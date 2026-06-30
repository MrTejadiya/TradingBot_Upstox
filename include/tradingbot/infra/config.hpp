#pragma once

#include "tradingbot/app/cli.hpp"

#include <string>
#include <vector>

namespace tradingbot::infra {

struct AppConfig {
    app::Mode mode{app::Mode::DryRun};
    bool live_trading_enabled{false};
};

struct UpstoxConfig {
    std::string access_token_env;
};

struct InputConfig {
    std::string instruments_csv;
};

struct MarketDataConfig {
    std::vector<std::string> candle_intervals;
};

struct StrategiesConfig {
    std::string buy_signal_mode;
    std::string sell_signal_mode;
    double min_buy_score{0.0};
};

struct ExitRulesConfig {
    double default_target_profit_pct{10.0};
    double default_stop_loss_pct{0.0};
};

struct RiskConfig {
    int max_orders_per_day{0};
    double max_daily_traded_value{0.0};
};

struct RateLimitsConfig {
    double order_api_safe_requests_per_second{0.0};
};

struct StorageConfig {
    std::string sqlite_path;
};

struct LoggingConfig {
    std::string log_directory;
};

struct BotConfig {
    AppConfig app;
    UpstoxConfig upstox;
    InputConfig input;
    MarketDataConfig market_data;
    StrategiesConfig strategies;
    ExitRulesConfig exit_rules;
    RiskConfig risk;
    RateLimitsConfig rate_limits;
    StorageConfig storage;
    LoggingConfig logging;
};

struct ConfigLoadResult {
    bool ok{false};
    BotConfig config;
    std::vector<std::string> errors;
};

ConfigLoadResult load_config_file(const std::string& path);
ConfigLoadResult load_config_from_json(const std::string& json_text);

}  // namespace tradingbot::infra

