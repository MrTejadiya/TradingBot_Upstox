#include "tradingbot/infra/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        },
        "market_data": {
            "candle_intervals": ["1d", "1h", "15m"],
            "max_quote_age_seconds": 300.0
        },
        "strategies": {
            "buy_signal_mode": "weighted_score",
            "sell_signal_mode": "first_exit_wins",
            "min_buy_score": 0.65
        },
        "live_scanner": {
            "worker_count": 0,
            "partition_count": 16,
            "rsi_period": 14,
            "wing_size": 2,
            "macd_fast_period": 12,
            "macd_slow_period": 26,
            "macd_signal_period": 9,
            "minimum_score": 0.75,
            "top_n": 25,
            "strategy_weights": {
                "rsi_divergence": 1.0,
                "macd_bullish_cross": 1.3
            }
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
    require(result.config.input.instrument_source == tradingbot::infra::InstrumentSource::Csv,
            "CSV source should parse");
    require(result.config.input.instruments_csv == "instruments.csv", "CSV path should parse");
    require(result.config.market_data.candle_intervals.size() == 3, "candle intervals should parse");
    require(result.config.market_data.max_quote_age_seconds == 300.0, "quote freshness should parse");
    require(result.config.strategies.buy_signal_mode == "weighted_score", "buy signal mode should parse");
    require(result.config.live_scanner.worker_count == 0, "scanner worker count should preserve CPU-default sentinel");
    require(result.config.live_scanner.partition_count == 16, "scanner partition count should parse");
    require(result.config.live_scanner.rsi_period == 14, "scanner RSI period should parse");
    require(result.config.live_scanner.wing_size == 2, "scanner wing size should parse");
    require(result.config.live_scanner.macd_fast_period == 12, "scanner MACD fast period should parse");
    require(result.config.live_scanner.macd_slow_period == 26, "scanner MACD slow period should parse");
    require(result.config.live_scanner.macd_signal_period == 9, "scanner MACD signal period should parse");
    require(result.config.live_scanner.minimum_score == 0.75, "scanner minimum score should parse");
    require(result.config.live_scanner.top_n == 25, "scanner top-N should parse");
    require(result.config.live_scanner.strategy_weights.at("macd_bullish_cross") == 1.3,
            "scanner strategy weights should parse");
    require(result.config.exit_rules.default_target_profit_pct == 10.0, "target profit should parse");
    require(result.config.exit_rules.max_holding_duration_hours == 720.0, "max holding duration should parse");
    require(result.config.risk.max_orders_per_day == 20, "daily order count should parse");
    require(result.config.risk.max_order_value == 25000.0, "max order value should parse");
    require(result.config.rate_limits.order_api_safe_requests_per_second == 2.0, "order API safe limit should parse");
    require(result.config.storage.sqlite_path == "bot.sqlite3", "SQLite path should parse");
    require(result.config.logging.log_directory == "logs", "log directory should parse");
}

void loads_upstox_json_input_config() {
    auto json = valid_config_json();
    const auto from = std::string{R"json("input": {
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        })json"};
    const auto to = std::string{R"json("input": {
            "instrument_source": "upstox_json",
            "upstox_instruments_json": "data/upstox_complete.json",
            "upstox_instruments_url": "https://assets.upstox.com/complete.json.gz",
            "upstox_instruments_cache": "cache/upstox_complete.json",
            "refresh_upstox_instruments": true,
            "allow_stale_upstox_instruments_cache": false,
            "default_enabled": false,
            "default_quantity": 2,
            "default_max_position_qty": 8,
            "default_target_profit_pct": 6.5,
            "default_strategy_profile": "scanner",
            "default_notes": "complete universe"
        })json"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(result.ok, "Upstox JSON input config should load");
    require(result.config.input.instrument_source == tradingbot::infra::InstrumentSource::UpstoxJson,
            "Upstox JSON source should parse");
    require(result.config.input.upstox_instruments_json == "data/upstox_complete.json", "JSON path should parse");
    require(result.config.input.upstox_instruments_url == "https://assets.upstox.com/complete.json.gz",
            "JSON URL should parse");
    require(result.config.input.upstox_instruments_cache == "cache/upstox_complete.json", "cache path should parse");
    require(result.config.input.refresh_upstox_instruments, "refresh flag should parse");
    require(!result.config.input.allow_stale_upstox_instruments_cache, "stale cache flag should parse");
    require(!result.config.input.default_enabled, "default enabled should parse");
    require(result.config.input.default_quantity == 2, "default quantity should parse");
    require(result.config.input.default_max_position_quantity == 8, "default max position should parse");
    require(result.config.input.default_target_profit_pct == 6.5, "default target should parse");
    require(result.config.input.default_strategy_profile == "scanner", "default profile should parse");
    require(result.config.input.default_notes == "complete universe", "default notes should parse");
}

void rejects_unknown_instrument_source() {
    auto json = valid_config_json();
    const auto from = std::string{"\"instrument_source\": \"csv\""};
    const auto to = std::string{"\"instrument_source\": \"manual\""};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "unknown instrument source should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("input.instrument_source") != std::string::npos;
    }
    require(found, "instrument source error should be reported");
}

void rejects_missing_upstox_json_path_for_upstox_source() {
    auto json = valid_config_json();
    const auto from = std::string{R"json("input": {
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        })json"};
    const auto to = std::string{R"json("input": {
            "instrument_source": "upstox_json"
        })json"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "Upstox source without path should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("input.upstox_instruments_json") != std::string::npos;
    }
    require(found, "missing Upstox JSON path error should be reported");
}

void allows_upstox_url_with_cache_without_local_json_path() {
    auto json = valid_config_json();
    const auto from = std::string{R"json("input": {
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        })json"};
    const auto to = std::string{R"json("input": {
            "instrument_source": "upstox_json",
            "upstox_instruments_url": "https://assets.upstox.com/complete.json.gz",
            "upstox_instruments_cache": "cache/upstox_complete.json"
        })json"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(result.ok, "Upstox URL plus cache should load without local JSON path");
    require(result.config.input.upstox_instruments_json.empty(), "local JSON path should remain empty");
    require(result.config.input.upstox_instruments_url == "https://assets.upstox.com/complete.json.gz",
            "URL should parse");
    require(result.config.input.upstox_instruments_cache == "cache/upstox_complete.json", "cache should parse");
    require(!result.config.input.refresh_upstox_instruments, "refresh should default false");
    require(result.config.input.allow_stale_upstox_instruments_cache, "stale cache fallback should default true");
}

void rejects_upstox_url_without_cache_path() {
    auto json = valid_config_json();
    const auto from = std::string{R"json("input": {
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        })json"};
    const auto to = std::string{R"json("input": {
            "instrument_source": "upstox_json",
            "upstox_instruments_url": "https://assets.upstox.com/complete.json.gz"
        })json"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "Upstox URL without cache should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("input.upstox_instruments_cache") != std::string::npos;
    }
    require(found, "missing cache path error should be reported");
}

void rejects_invalid_import_defaults() {
    auto json = valid_config_json();
    const auto from = std::string{R"json("input": {
            "instrument_source": "csv",
            "instruments_csv": "instruments.csv"
        })json"};
    const auto to = std::string{R"json("input": {
            "instrument_source": "upstox_json",
            "upstox_instruments_json": "data/upstox_complete.json",
            "default_quantity": 0,
            "default_max_position_qty": 1.5,
            "default_target_profit_pct": -1
        })json"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "invalid import defaults should fail");
    require(result.errors.size() >= 3, "invalid default fields should report all errors");
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

void rejects_invalid_live_scanner_values() {
    auto json = valid_config_json();
    const auto from = std::string{"\"rsi_period\": 14"};
    const auto to = std::string{"\"rsi_period\": 0"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "invalid scanner RSI period should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("live_scanner.rsi_period") != std::string::npos;
    }
    require(found, "scanner period validation error should be reported");
}

void rejects_invalid_live_scanner_weights() {
    auto json = valid_config_json();
    const auto from = std::string{"\"macd_bullish_cross\": 1.3"};
    const auto to = std::string{"\"macd_bullish_cross\": -1.3"};
    json.replace(json.find(from), from.size(), to);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(!result.ok, "negative scanner weight should fail");
    bool found = false;
    for (const auto& error : result.errors) {
        found = found || error.find("live_scanner.strategy_weights") != std::string::npos;
    }
    require(found, "scanner weight validation error should be reported");
}

void allows_missing_live_scanner_section() {
    auto json = valid_config_json();
    const auto start = json.find("        \"live_scanner\": {");
    const auto end = json.find("        \"exit_rules\": {");
    json.erase(start, end - start);

    const auto result = tradingbot::infra::load_config_from_json(json);

    require(result.ok, "missing optional live_scanner section should still load");
    require(result.config.live_scanner.worker_count == 0, "missing scanner config should keep defaults");
    require(result.config.live_scanner.rsi_period == 14, "missing scanner config should keep RSI default");
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

void published_example_config_loads() {
    const auto path = std::filesystem::path{TRADINGBOT_SOURCE_DIR} / "config.example.json";
    const auto result = tradingbot::infra::load_config_file(path.string());

    if (!result.ok) {
        for (const auto& error : result.errors) {
            std::cerr << error << "\n";
        }
    }
    require(result.ok, "published config.example.json should load");
    require(result.config.live_scanner.minimum_score == 0.75, "example live scanner config should parse");
    require(result.config.live_scanner.strategy_weights.at("macd_bullish_cross") == 1.3,
            "example scanner weights should parse");
}

}  // namespace

int main() {
    loads_valid_json_config();
    loads_upstox_json_input_config();
    rejects_unknown_instrument_source();
    rejects_missing_upstox_json_path_for_upstox_source();
    allows_upstox_url_with_cache_without_local_json_path();
    rejects_upstox_url_without_cache_path();
    rejects_invalid_import_defaults();
    reports_missing_required_sections();
    rejects_invalid_live_flag_type();
    rejects_unknown_mode();
    rejects_invalid_live_scanner_values();
    rejects_invalid_live_scanner_weights();
    allows_missing_live_scanner_section();
    rejects_invalid_json();
    loads_config_from_file();
    published_example_config_loads();
    return 0;
}
