#include "tradingbot/strategy/market_session.hpp"

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

void default_window_allows_regular_market_orders() {
    const auto result = tradingbot::strategy::MarketSessionChecker{}.evaluate({
        .weekday = tradingbot::strategy::Weekday::Wednesday,
        .hour = 9,
        .minute = 15,
    });

    require(result.market_open, "market should be open at default open time");
    require(result.order_allowed, "orders should be allowed at default open time");
    require(result.reason_code == "ORDER_ALLOWED", "allowed reason should be stable");
}

void rejects_before_open() {
    const auto result = tradingbot::strategy::MarketSessionChecker{}.evaluate({
        .weekday = tradingbot::strategy::Weekday::Wednesday,
        .hour = 9,
        .minute = 14,
    });

    require(!result.market_open, "market should be closed before open");
    require(!result.order_allowed, "orders should not be allowed before open");
    require(result.reason_code == "BEFORE_OPEN", "before open reason should be stable");
}

void rejects_after_last_order_time_while_market_is_open() {
    const auto result = tradingbot::strategy::MarketSessionChecker{}.evaluate({
        .weekday = tradingbot::strategy::Weekday::Wednesday,
        .hour = 15,
        .minute = 21,
    });

    require(result.market_open, "market should still be open after last order time");
    require(!result.order_allowed, "new orders should be blocked after order window closes");
    require(result.reason_code == "ORDER_WINDOW_CLOSED", "order window reason should be stable");
}

void rejects_at_market_close() {
    const auto result = tradingbot::strategy::MarketSessionChecker{}.evaluate({
        .weekday = tradingbot::strategy::Weekday::Wednesday,
        .hour = 15,
        .minute = 30,
    });

    require(!result.market_open, "market should be closed at close boundary");
    require(!result.order_allowed, "orders should not be allowed at close boundary");
    require(result.reason_code == "AFTER_CLOSE", "after close reason should be stable");
}

void rejects_weekends_by_default() {
    const auto result = tradingbot::strategy::MarketSessionChecker{}.evaluate({
        .weekday = tradingbot::strategy::Weekday::Saturday,
        .hour = 10,
        .minute = 0,
    });

    require(!result.market_open, "weekends should be closed by default");
    require(result.reason_code == "WEEKEND", "weekend reason should be stable");
}

void supports_custom_window() {
    const tradingbot::strategy::MarketSessionChecker checker({
        .open_minute_of_day = 10 * 60,
        .close_minute_of_day = 12 * 60,
        .last_order_minute_of_day = 11 * 60 + 45,
        .allow_weekend_trading = true,
    });
    const auto result = checker.evaluate({
        .weekday = tradingbot::strategy::Weekday::Saturday,
        .hour = 10,
        .minute = 30,
    });

    require(result.market_open, "custom weekend-enabled window should open");
    require(result.order_allowed, "custom weekend-enabled window should allow orders");
}

}  // namespace

int main() {
    default_window_allows_regular_market_orders();
    rejects_before_open();
    rejects_after_last_order_time_while_market_is_open();
    rejects_at_market_close();
    rejects_weekends_by_default();
    supports_custom_window();
    return 0;
}

