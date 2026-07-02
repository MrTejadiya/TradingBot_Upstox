#pragma once

#include "tradingbot/app/cli.hpp"
#include "tradingbot/core/domain.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace tradingbot::infra {

enum class InstrumentSource {
    Csv,
    UpstoxJson,
};

struct AppConfig {
    app::Mode mode{app::Mode::DryRun};
    bool live_trading_enabled{false};
};

struct UpstoxConfig {
    std::string access_token_env;
    bool force_ipv4{false};
};

struct InputConfig {
    InstrumentSource instrument_source{InstrumentSource::Csv};
    std::string instruments_csv;
    std::string upstox_instruments_json;
    bool default_enabled{true};
    core::Quantity default_quantity{1};
    core::Quantity default_max_position_quantity{1};
    core::Percent default_target_profit_pct{10.0};
    std::string default_strategy_profile;
    std::string default_notes{"imported from Upstox instrument JSON"};
};

struct MarketDataConfig {
    std::vector<std::string> candle_intervals;
    double max_quote_age_seconds{300.0};
};

struct StrategiesConfig {
    std::string buy_signal_mode;
    std::string sell_signal_mode;
    double min_buy_score{0.0};
};

struct LiveScannerConfig {
    std::size_t worker_count{0};
    std::size_t partition_count{0};
    int rsi_period{14};
    int wing_size{1};
    int macd_fast_period{12};
    int macd_slow_period{26};
    int macd_signal_period{9};
    double minimum_score{0.0};
    std::size_t top_n{0};
    std::unordered_map<std::string, double> strategy_weights;
};

struct ExitRulesConfig {
    double default_target_profit_pct{10.0};
    double default_stop_loss_pct{0.0};
    double max_holding_duration_hours{0.0};
};

struct RiskConfig {
    int max_orders_per_day{0};
    double max_order_value{0.0};
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
    LiveScannerConfig live_scanner;
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
