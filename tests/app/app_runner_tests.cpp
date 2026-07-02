#include "tradingbot/app/app_runner.hpp"

#include "tradingbot/persistence/sqlite_database.hpp"
#include "tradingbot/persistence/sqlite_migrations.hpp"
#include "tradingbot/persistence/sqlite_persistence_sink.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

std::filesystem::path test_path(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path;
}

std::string json_path(const std::filesystem::path& path) {
    auto value = path.string();
    for (auto& ch : value) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return value;
}

std::string valid_config_json(const std::filesystem::path& sqlite_path, const std::string& mode = "show-orders",
                              const std::string& instruments_csv = "instruments.csv") {
    return R"json({
        "app": {
            "mode": ")json" + mode +
           R"json(",
            "live_trading_enabled": false
        },
        "upstox": {
            "access_token_env": "UPSTOX_ACCESS_TOKEN",
            "force_ipv4": true
        },
        "input": {
            "instruments_csv": ")json" +
           instruments_csv + R"json("
        },
        "market_data": {
            "candle_intervals": ["1d"],
            "max_quote_age_seconds": 300.0
        },
        "strategies": {
            "buy_signal_mode": "weighted_score",
            "sell_signal_mode": "first_exit_wins",
            "min_buy_score": 0.65
        },
        "exit_rules": {
            "default_target_profit_pct": 10.0,
            "default_stop_loss_pct": 3.0,
            "max_holding_duration_hours": 720.0
        },
        "risk": {
            "max_orders_per_day": 20,
            "max_order_value": 25000.0,
            "max_daily_traded_value": 100000.0
        },
        "rate_limits": {
            "order_api": {
                "safe_requests_per_second": 2.0
            }
        },
        "storage": {
            "sqlite_path": ")json" +
           json_path(sqlite_path) + R"json("
        },
        "logging": {
            "log_directory": "logs"
        }
    })json";
}

void write_config(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path);
    file << text;
}

void write_instruments_csv(const std::filesystem::path& path) {
    std::ofstream file(path);
    file << "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct,notes\n";
    file << "NSE_EQ|INE002A01018,RELIANCE,true,2,10,10.0,configured instrument\n";
}

void write_duplicate_listing_instruments_csv(const std::filesystem::path& path) {
    std::ofstream file(path);
    file << "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct,notes\n";
    file << "BSE_EQ|INE002A01018,RELIANCE_BSE,true,1,2,9.0,bse duplicate\n";
    file << "NSE_EQ|INE002A01018,RELIANCE_NSE,true,3,12,11.0,nse preferred\n";
}

tradingbot::core::OrderRecord order_record() {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = tradingbot::core::OrderSide::Buy,
                .quantity = 1,
                .price = 1100.0,
                .run_id = "run-1",
            },
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Accepted,
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void seed_order_database(const std::filesystem::path& path) {
    auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(path.string());
    tradingbot::persistence::SqliteMigrationStore migrations(database);
    tradingbot::persistence::apply_pending_migrations(migrations);
    tradingbot::persistence::SqlitePersistenceSink sink(database, "run-1");
    sink.save_order(order_record());
}

int count_rows(const tradingbot::persistence::SqliteDatabase& database, const std::string& table,
               const std::string& where = "") {
    const auto values = database.query_ints("SELECT COUNT(*) FROM " + table + where + ";");
    return values.empty() ? 0 : values.front();
}

void configured_show_orders_reads_sqlite_history() {
    const auto db_path = test_path("tradingbot_app_runner_show_orders.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_show_orders.json");
    seed_order_database(db_path);
    write_config(config_path, valid_config_json(db_path));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code == 0, "configured show-orders should run");
    require(err.str().empty(), "configured show-orders should not write errors");
    require(out.str().find("ORDER-1") != std::string::npos, "configured show-orders should render SQLite order");

    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void configured_show_orders_does_not_create_bot_run() {
    const auto db_path = test_path("tradingbot_app_runner_show_orders_readonly.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_show_orders_readonly.json");
    seed_order_database(db_path);
    write_config(config_path, valid_config_json(db_path));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code == 0, "configured show-orders should run");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(db_path.string());
        require(count_rows(*database, "bot_runs") == 0, "show-orders should not create bot run rows");
    }

    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void configured_dry_run_records_bot_run_lifecycle() {
    const auto db_path = test_path("tradingbot_app_runner_dry_run_lifecycle.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_dry_run_lifecycle.json");
    const auto instruments_path = test_path("tradingbot_app_runner_instruments.csv");
    write_instruments_csv(instruments_path);
    write_config(config_path, valid_config_json(db_path, "dry-run", instruments_path.filename().string()));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code == 0, "configured dry-run should run");
    require(out.str().find("dry-run mode selected") != std::string::npos, "dry-run output should render");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(db_path.string());
        require(count_rows(*database, "bot_runs") == 1, "dry-run should create one bot run row");
        require(count_rows(*database, "bot_runs", " WHERE mode = 'dry-run' AND ended_at IS NOT NULL") == 1,
                "dry-run should complete bot run row");
        require(count_rows(*database, "bot_runs", " WHERE config_hash LIKE 'fnv1a64:%'") == 1,
                "dry-run should store deterministic config hash");
    }

    std::filesystem::remove(instruments_path);
    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void configured_dry_run_persists_instruments_relative_to_config() {
    const auto db_path = test_path("tradingbot_app_runner_instrument_persistence.sqlite3");
    const auto config_dir = std::filesystem::temp_directory_path();
    const auto config_path = config_dir / "tradingbot_app_runner_relative_config.json";
    const auto instruments_path = config_dir / "tradingbot_app_runner_relative_instruments.csv";
    write_instruments_csv(instruments_path);
    write_config(config_path, valid_config_json(db_path, "dry-run", instruments_path.filename().string()));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code == 0, "configured dry-run with relative instrument CSV should run");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(db_path.string());
        require(count_rows(*database, "instruments") == 1, "dry-run should persist configured instruments");
        require(count_rows(*database, "instruments", " WHERE symbol = 'RELIANCE' AND notes = 'configured instrument'") ==
                    1,
                "persisted instrument fields should load from CSV");
    }

    std::filesystem::remove(instruments_path);
    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void configured_dry_run_persists_nse_preferred_instrument_universe() {
    const auto db_path = test_path("tradingbot_app_runner_nse_preferred.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_nse_preferred.json");
    const auto instruments_path = test_path("tradingbot_app_runner_nse_preferred.csv");
    write_duplicate_listing_instruments_csv(instruments_path);
    write_config(config_path, valid_config_json(db_path, "dry-run", instruments_path.filename().string()));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code == 0, "configured dry-run with NSE/BSE duplicate listing should run");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(db_path.string());
        require(count_rows(*database, "instruments") == 1, "duplicate NSE/BSE listing should persist one row");
        require(count_rows(*database, "instruments",
                           " WHERE instrument_key = 'NSE_EQ|INE002A01018' AND symbol = 'RELIANCE_NSE' "
                           "AND quantity = 3 AND notes = 'nse preferred'") == 1,
                "persisted instrument should use NSE row data");
        require(count_rows(*database, "instruments", " WHERE instrument_key LIKE 'BSE_EQ|%'") == 0,
                "BSE duplicate should not persist");
    }

    std::filesystem::remove(instruments_path);
    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void configured_dry_run_fails_when_instrument_csv_is_missing() {
    const auto db_path = test_path("tradingbot_app_runner_missing_instruments.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_missing_instruments.json");
    write_config(config_path, valid_config_json(db_path, "dry-run", "missing-instruments.csv"));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string()}, out, err);

    require(code != 0, "missing instrument CSV should fail configured dry-run");
    require(out.str().empty(), "missing instrument CSV should not write normal output");
    require(err.str().find("unable to open instrument CSV file") != std::string::npos,
            "missing instrument CSV error should be clear");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(db_path.string());
        require(count_rows(*database, "bot_runs") == 0, "failed startup should not create bot run rows");
    }

    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void cli_mode_overrides_config_mode() {
    const auto db_path = test_path("tradingbot_app_runner_override.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_override.json");
    const auto instruments_path = test_path("tradingbot_app_runner_override_instruments.csv");
    write_instruments_csv(instruments_path);
    write_config(config_path, valid_config_json(db_path, "show-orders", instruments_path.filename().string()));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string(), "--mode", "dry-run"}, out, err);

    require(code == 0, "CLI mode override should run");
    require(out.str().find("dry-run mode selected") != std::string::npos, "CLI mode should override config mode");
    require(out.str().find("No orders found") == std::string::npos, "show-orders should not run when overridden");

    std::filesystem::remove(instruments_path);
    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void config_load_errors_are_reported() {
    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", "missing-config.json"}, out, err);

    require(code != 0, "missing config should fail");
    require(out.str().empty(), "missing config should not write output");
    require(!err.str().empty(), "missing config should write an error");
}

}  // namespace

int main() {
    configured_show_orders_reads_sqlite_history();
    configured_show_orders_does_not_create_bot_run();
    configured_dry_run_records_bot_run_lifecycle();
    configured_dry_run_persists_instruments_relative_to_config();
    configured_dry_run_persists_nse_preferred_instrument_universe();
    configured_dry_run_fails_when_instrument_csv_is_missing();
    cli_mode_overrides_config_mode();
    config_load_errors_are_reported();
    return 0;
}
