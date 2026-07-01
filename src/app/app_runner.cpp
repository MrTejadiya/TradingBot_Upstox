#include "tradingbot/app/app_runner.hpp"

#include "tradingbot/app/cli.hpp"
#include "tradingbot/infra/config.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"
#include "tradingbot/persistence/sqlite_order_history_reader.hpp"

#include <iostream>
#include <memory>
#include <string>

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

    const auto config = infra::load_config_file(options.config_path);
    if (!config.ok) {
        return report_config_errors(config, err);
    }

    apply_config_defaults(options, config.config);
    if (options.mode != Mode::ShowOrders || config.config.storage.sqlite_path.empty()) {
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
        persistence::SqliteOrderHistoryReader reader(database);
        return run_cli(options, out, err, &reader);
    } catch (const std::exception& ex) {
        err << ex.what() << "\n";
        return 2;
    }
}

}  // namespace tradingbot::app
