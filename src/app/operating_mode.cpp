#include "tradingbot/app/operating_mode.hpp"

namespace tradingbot::app {

OperatingModeValidation validate_operating_mode(const OperatingModeConfig& config) {
    switch (config.mode) {
        case Mode::Validate:
            return {.ok = true, .market_calls_allowed = false, .live_orders_allowed = false};
        case Mode::DryRun:
            return {.ok = true, .market_calls_allowed = true, .live_orders_allowed = false};
        case Mode::Paper:
            return {.ok = true, .market_calls_allowed = true, .live_orders_allowed = false};
        case Mode::ShowOrders:
            return {.ok = true, .market_calls_allowed = false, .live_orders_allowed = false};
        case Mode::Live:
            if (!config.live_trading_enabled) {
                return {
                    .ok = false,
                    .market_calls_allowed = false,
                    .live_orders_allowed = false,
                    .error = "live mode requires live_trading_enabled=true",
                };
            }
            if (!config.live_trading_confirmed) {
                return {
                    .ok = false,
                    .market_calls_allowed = false,
                    .live_orders_allowed = false,
                    .error = "live mode requires explicit live trading confirmation",
                };
            }
            return {.ok = true, .market_calls_allowed = true, .live_orders_allowed = true};
    }

    return {
        .ok = false,
        .market_calls_allowed = false,
        .live_orders_allowed = false,
        .error = "unsupported operating mode",
    };
}

}  // namespace tradingbot::app

