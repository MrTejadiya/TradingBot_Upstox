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

std::string valid_config_json(const std::filesystem::path& sqlite_path, const std::string& mode = "show-orders") {
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
            "instruments_csv": "instruments.csv"
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
    write_config(config_path, valid_config_json(db_path, "dry-run"));

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

    std::filesystem::remove(config_path);
    std::filesystem::remove(db_path);
}

void cli_mode_overrides_config_mode() {
    const auto db_path = test_path("tradingbot_app_runner_override.sqlite3");
    const auto config_path = test_path("tradingbot_app_runner_override.json");
    write_config(config_path, valid_config_json(db_path, "show-orders"));

    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_app({"--config", config_path.string(), "--mode", "dry-run"}, out, err);

    require(code == 0, "CLI mode override should run");
    require(out.str().find("dry-run mode selected") != std::string::npos, "CLI mode should override config mode");
    require(out.str().find("No orders found") == std::string::npos, "show-orders should not run when overridden");

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
    cli_mode_overrides_config_mode();
    config_load_errors_are_reported();
    return 0;
}
