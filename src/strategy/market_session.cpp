#include "tradingbot/strategy/market_session.hpp"

namespace tradingbot::strategy {

MarketSessionChecker::MarketSessionChecker(MarketSessionConfig config) : config_(config) {}

MarketSessionResult MarketSessionChecker::evaluate(const LocalMarketTime& local_time) const {
    if (!config_.allow_weekend_trading && is_weekend(local_time.weekday)) {
        return {
            .market_open = false,
            .order_allowed = false,
            .reason_code = "WEEKEND",
            .detail = "market session is closed on weekends",
        };
    }

    const auto minute = minute_of_day(local_time);
    if (minute < config_.open_minute_of_day) {
        return {
            .market_open = false,
            .order_allowed = false,
            .reason_code = "BEFORE_OPEN",
            .detail = "current time is before market open",
        };
    }
    if (minute >= config_.close_minute_of_day) {
        return {
            .market_open = false,
            .order_allowed = false,
            .reason_code = "AFTER_CLOSE",
            .detail = "current time is at or after market close",
        };
    }
    if (minute > config_.last_order_minute_of_day) {
        return {
            .market_open = true,
            .order_allowed = false,
            .reason_code = "ORDER_WINDOW_CLOSED",
            .detail = "market is open but new order window has closed",
        };
    }

    return {
        .market_open = true,
        .order_allowed = true,
        .reason_code = "ORDER_ALLOWED",
        .detail = "market is open and order window is active",
    };
}

int minute_of_day(const LocalMarketTime& local_time) {
    return (local_time.hour * 60) + local_time.minute;
}

bool is_weekend(Weekday weekday) {
    return weekday == Weekday::Saturday || weekday == Weekday::Sunday;
}

}  // namespace tradingbot::strategy

