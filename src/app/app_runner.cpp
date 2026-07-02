#include "tradingbot/app/app_runner.hpp"

#include "tradingbot/app/cli.hpp"
#include "tradingbot/infra/config.hpp"
#include "tradingbot/infra/instrument_csv.hpp"
#include "tradingbot/infra/upstox_instrument_json_cache.hpp"
#include "tradingbot/infra/upstox_instrument_json.hpp"
#include "tradingbot/infra/win_http_transport.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"
#include "tradingbot/persistence/sqlite_instrument_store.hpp"
#include "tradingbot/persistence/sqlite_order_history_reader.hpp"
#include "tradingbot/persistence/sqlite_persistence_sink.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace tradingbot::app {
namespace {

void apply_config_defaults(CliOptions& options, const infra::BotConfig& config) {
    if (!options.mode_overridden) {
        options.mode = config.app.mode;
    }
    if (!options.live_trading_enabled_overridden) {
        options.live_trading_enabled = config.app.live_trading_enabled;
    }
}

int report_config_errors(const infra::ConfigLoadResult& result, std::ostream& err) {
    for (const auto& error : result.errors) {
        err << error << "\n";
    }
    if (result.errors.empty()) {
        err << "failed to load config\n";
    }
    return 2;
}

std::string read_text_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string fnv1a64_hex(const std::string& text) {
    auto hash = 14695981039346656037ULL;
    for (const auto ch : text) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string run_id_for(core::TimePoint started_at, Mode mode) {
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(started_at.time_since_epoch()).count();
    return to_string(mode) + "-" + std::to_string(millis);
}

std::filesystem::path resolve_configured_path(const std::string& config_path, const std::string& configured_path) {
    std::filesystem::path path{configured_path};
    if (path.is_absolute()) {
        return path;
    }
    return std::filesystem::path{config_path}.parent_path() / path;
}

bool persist_configured_instruments(const std::shared_ptr<persistence::SqliteDatabase>& database,
                                    const std::string& config_path, const infra::BotConfig& config,
                                    std::ostream& err) {
    std::vector<core::Instrument> instruments;

    if (config.input.instrument_source == infra::InstrumentSource::Csv) {
        const auto instruments_path = resolve_configured_path(config_path, config.input.instruments_csv);
        const auto loaded = infra::load_instruments_csv_file(instruments_path.string());
        if (!loaded.ok) {
            for (const auto& error : loaded.errors) {
                err << error << "\n";
            }
            if (loaded.errors.empty()) {
                err << "failed to load instrument CSV\n";
            }
            return false;
        }
        instruments = loaded.instruments;
    } else {
        infra::UpstoxInstrumentJsonLoadOptions options;
        options.enabled = config.input.default_enabled;
        options.quantity = config.input.default_quantity;
        options.max_position_quantity = config.input.default_max_position_quantity;
        options.target_profit_pct = config.input.default_target_profit_pct;
        options.strategy_profile = config.input.default_strategy_profile;
        options.notes = config.input.default_notes;

        infra::UpstoxInstrumentJsonLoadResult loaded;
        if (!config.input.upstox_instruments_url.empty()) {
            const auto cache_path = resolve_configured_path(config_path, config.input.upstox_instruments_cache);
            infra::UpstoxInstrumentJsonCache cache(std::make_shared<infra::WinHttpTransport>());
            const auto acquired = cache.load({
                .url = config.input.upstox_instruments_url,
                .cache_path = cache_path.string(),
                .refresh = config.input.refresh_upstox_instruments,
                .allow_stale_cache = config.input.allow_stale_upstox_instruments_cache,
                .force_ipv4 = config.upstox.force_ipv4,
            });
            if (!acquired.ok) {
                err << acquired.error << "\n";
                return false;
            }
            loaded = infra::load_upstox_instruments_json_text(acquired.json_text, options);
        } else {
            const auto instruments_path = resolve_configured_path(config_path, config.input.upstox_instruments_json);
            loaded = infra::load_upstox_instruments_json_file(instruments_path.string(), options);
        }
        if (!loaded.ok) {
            for (const auto& error : loaded.errors) {
                err << error << "\n";
            }
            if (loaded.errors.empty()) {
                err << "failed to load Upstox instrument JSON\n";
            }
            return false;
        }
        instruments = loaded.instruments;
    }

    persistence::SqliteInstrumentStore store(database);
    store.upsert_all(instruments);
    return true;
}

}  // namespace

int run_app(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    const auto parsed = parse_cli(args);
    if (!parsed.ok) {
        err << parsed.error << "\n";
        print_usage(err);
        return 2;
    }

    auto options = parsed.options;
    if (options.help_requested || options.config_path.empty()) {
        return run_cli(options, out, err);
    }

    const auto config_text = read_text_file(options.config_path);
    const auto config = infra::load_config_file(options.config_path);
    if (!config.ok) {
        return report_config_errors(config, err);
    }

    apply_config_defaults(options, config.config);
    if (config.config.storage.sqlite_path.empty()) {
        return run_cli(options, out, err);
    }

    auto database = std::make_shared<persistence::SqliteDatabase>(config.config.storage.sqlite_path);
    if (!database->ok()) {
        err << database->error() << "\n";
        return 2;
    }

    try {
        persistence::SqliteMigrationStore migrations(database);
        persistence::apply_pending_migrations(migrations);
        if (options.mode != Mode::ShowOrders) {
            if (!persist_configured_instruments(database, options.config_path, config.config, err)) {
                return 2;
            }

            const auto started_at = core::Clock::now();
            const auto run_id = run_id_for(started_at, options.mode);
            persistence::SqlitePersistenceSink sink(database, run_id);
            core::BotRun run{
                .run_id = run_id,
                .started_at = started_at,
                .mode = to_string(options.mode),
                .config_hash = fnv1a64_hex(config_text),
            };
            sink.save_bot_run(run);
            const auto code = run_cli(options, out, err);
            run.ended_at = core::Clock::now();
            sink.save_bot_run(run);
            return code;
        }

        persistence::SqliteOrderHistoryReader reader(database);
        return run_cli(options, out, err, &reader);
    } catch (const std::exception& ex) {
        err << ex.what() << "\n";
        return 2;
    }
}

}  // namespace tradingbot::app
