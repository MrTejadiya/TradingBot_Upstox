#include "tradingbot/app/operating_mode.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void validate_mode_allows_no_market_calls() {
    const auto result = tradingbot::app::validate_operating_mode({
        .mode = tradingbot::app::Mode::Validate,
    });

    require(result.ok, "validate mode should be valid");
    require(!result.market_calls_allowed, "validate mode should not allow market calls");
    require(!result.live_orders_allowed, "validate mode should not allow live orders");
}

void dry_run_is_default_safe_mode() {
    const auto result = tradingbot::app::validate_operating_mode({});

    require(result.ok, "default mode should be valid");
    require(result.market_calls_allowed, "dry-run may use market data");
    require(!result.live_orders_allowed, "dry-run must not allow live orders");
}

void live_requires_enabled_flag() {
    const auto result = tradingbot::app::validate_operating_mode({
        .mode = tradingbot::app::Mode::Live,
    });

    require(!result.ok, "live mode should fail without enabled flag");
    require(result.error.find("live_trading_enabled=true") != std::string::npos, "error should name enabled flag");
    require(!result.live_orders_allowed, "failed live validation must not allow live orders");
}

void live_requires_explicit_confirmation() {
    const auto result = tradingbot::app::validate_operating_mode({
        .mode = tradingbot::app::Mode::Live,
        .live_trading_enabled = true,
    });

    require(!result.ok, "live mode should fail without explicit confirmation");
    require(result.error.find("explicit live trading confirmation") != std::string::npos, "error should name confirmation");
    require(!result.live_orders_allowed, "unconfirmed live mode must not allow live orders");
}

void live_allows_orders_only_after_both_gates() {
    const auto result = tradingbot::app::validate_operating_mode({
        .mode = tradingbot::app::Mode::Live,
        .live_trading_enabled = true,
        .live_trading_confirmed = true,
    });

    require(result.ok, "live mode should pass when both gates are set");
    require(result.market_calls_allowed, "valid live mode should allow market calls");
    require(result.live_orders_allowed, "valid live mode should allow live orders");
}

void show_orders_does_not_allow_market_calls_or_live_orders() {
    const auto result = tradingbot::app::validate_operating_mode({
        .mode = tradingbot::app::Mode::ShowOrders,
    });

    require(result.ok, "show-orders mode should be valid");
    require(!result.market_calls_allowed, "show-orders should not call market APIs");
    require(!result.live_orders_allowed, "show-orders should not place live orders");
}

}  // namespace

int main() {
    validate_mode_allows_no_market_calls();
    dry_run_is_default_safe_mode();
    live_requires_enabled_flag();
    live_requires_explicit_confirmation();
    live_allows_orders_only_after_both_gates();
    show_orders_does_not_allow_market_calls_or_live_orders();
    return 0;
}

