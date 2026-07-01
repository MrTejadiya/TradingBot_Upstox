#include "tradingbot/infra/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

std::string valid_config_json() {
    return R"json({
        "app": {
            "mode": "dry-run",
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
            "candle_intervals": ["1d", "1h", "15m"]
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
            "max_daily_traded_value": 100000.0
        },
        "rate_limits": {
            "order_api": {
                "safe_requests_per_second": 2.0
            }
        },
        "storage": {
            "sqlite_path": "bot.sqlite3"
        },
        "logging": {
            "log_directory": "logs"
        }
    })json";
}

void loads_valid_json_config() {
    const auto result = tradingbot::infra::load_config_from_json(valid_config_json());

    require(result.ok, "valid config should load");
    require(result.config.app.mode == tradingbot::app::Mode::DryRun, "mode should parse");
    require(!result.config.app.live_trading_enabled, "live flag should parse");
    require(result.config.upstox.access_token_env == "UPSTOX_ACCESS_TOKEN", "token env should parse");
    require(result.config.upstox.force_ipv4, "force IPv4 flag should parse");
    require(result.config.input.instruments_csv == "instruments.csv", "CSV path should parse");
    require(result.config.market_data.candle_intervals.size() == 3, "candle intervals should parse");
    require(result.config.strategies.buy_signal_mode == "weighted_score", "buy signal mode should parse");
    require(result.config.exit_rules.default_target_profit_pct == 10.0, "target profit should parse");
    require(result.config.exit_rules.max_holding_duration_hours == 720.0, "max holding duration should parse");
    require(result.config.risk.max_orders_per_day == 20, "daily order count should parse");
    require(result.config.rate_limits.order_api_safe_requests_per_second == 2.0, "order API safe limit should parse");
    require(result.config.storage.sqlite_path == "bot.sqlite3", "SQLite path should parse");
    require(result.config.logging.log_directory == "logs", "log directory should parse");
}

void reports_missing_required_sections() {
    const auto result = tradingbot::infra::load_config_from_json(R"json({"app": {"mode": "dry-run", "live_trading_enabled": false}})json");

    require(!result.ok, "config with missing sections should fail");
    require(!result.errors.empty(), "missing sections should produce errors");
    require(result.errors.front().find("missing required section") != std::string::npos,
            "missing section error should be clear");
}

void rejects_invalid_live_flag_type() {
    auto json = valid_config_json();
    const auto from = std::string{"\"live_trading_enabled\": false"};
    const auto to = std::string{"\"live_trading_enabled\": \"false\""};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "string live flag should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("app.live_trading_enabled") != std::string::npos;
    }
    require(found, "live flag type error should be reported");
}

void rejects_unknown_mode() {
    auto json = valid_config_json();
    const auto from = std::string{"\"mode\": \"dry-run\""};
    const auto to = std::string{"\"mode\": \"intraday\""};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "unknown mode should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("app.mode") != std::string::npos;
    }
    require(found, "mode validation error should be reported");
}

void rejects_invalid_json() {
    const auto result = tradingbot::infra::load_config_from_json("{ not json");

    require(!result.ok, "invalid JSON should fail");
    require(!result.errors.empty(), "invalid JSON should report an error");
    require(result.errors.front().find("invalid JSON") != std::string::npos, "invalid JSON error should be clear");
}

void loads_config_from_file() {
    const auto path = std::string{"config_test_tmp.json"};
    {
        std::ofstream file(path);
        file << valid_config_json();
    }

    const auto result = tradingbot::infra::load_config_file(path);
    std::remove(path.c_str());

    require(result.ok, "valid config file should load");
    require(result.config.storage.sqlite_path == "bot.sqlite3", "file-loaded config should be parsed");
}

}  // namespace

int main() {
    loads_valid_json_config();
    reports_missing_required_sections();
    rejects_invalid_live_flag_type();
    rejects_unknown_mode();
    rejects_invalid_json();
    loads_config_from_file();
    return 0;
}
