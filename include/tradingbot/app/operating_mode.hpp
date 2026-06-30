#pragma once

#include "tradingbot/app/cli.hpp"

#include <string>

namespace tradingbot::app {

struct OperatingModeConfig {
    Mode mode{Mode::DryRun};
    bool live_trading_enabled{false};
    bool live_trading_confirmed{false};
};

struct OperatingModeValidation {
    bool ok{false};
    bool market_calls_allowed{false};
    bool live_orders_allowed{false};
    std::string error;
};

OperatingModeValidation validate_operating_mode(const OperatingModeConfig& config);

}  // namespace tradingbot::app

