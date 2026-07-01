#include "tradingbot/scan/live_candle_aggregator.hpp"

#include <chrono>
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

tradingbot::core::TimePoint tp(int seconds) {
    return tradingbot::core::TimePoint{std::chrono::seconds{seconds}};
}

tradingbot::core::QuoteSnapshot quote(double ltp, int seconds = 100) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = tp(seconds),
        .ltp = ltp,
    };
}

tradingbot::core::Candle candle(double close, int seconds = 0) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = tp(seconds),
        .open = close,
        .high = close,
        .low = close,
        .close = close,
        .interval = "days:1",
    };
}

void updates_open_high_low_close_for_current_session() {
    tradingbot::scan::LiveCandleAggregator aggregator;

    aggregator.update(quote(100.0));
    aggregator.update(quote(105.0));
    aggregator.update(quote(98.0));
    aggregator.update(quote(102.0));

    const auto current = aggregator.current_candle({"NSE_EQ|INE002A01018"});

    require(current.has_value(), "current candle should exist");
    require(current->open == 100.0, "open should come from first quote");
    require(current->high == 105.0, "high should track max quote");
    require(current->low == 98.0, "low should track min quote");
    require(current->close == 102.0, "close should track latest quote");
    require(current->timestamp == tradingbot::scan::session_day_start(tp(100)), "timestamp should be session day start");
}

void ignores_stale_and_invalid_quotes() {
    tradingbot::scan::LiveCandleAggregator aggregator;
    aggregator.update({.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 0.0});
    aggregator.update({.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 100.0, .stale = true});

    require(!aggregator.current_candle({"NSE_EQ|INE002A01018"}), "invalid quotes should not create a candle");
}

void replaces_matching_historical_candle_with_provisional() {
    const std::vector<tradingbot::core::Candle> historical{candle(99.0, 0)};
    const auto combined = tradingbot::scan::with_provisional_candle(historical, candle(101.0, 0));

    require(combined.size() == 1, "same-session provisional candle should replace historical candle");
    require(combined.front().close == 101.0, "replacement candle should use provisional close");
}

void appends_new_provisional_candle() {
    const std::vector<tradingbot::core::Candle> historical{candle(99.0, 0)};
    const auto combined = tradingbot::scan::with_provisional_candle(historical, candle(101.0, 86400));

    require(combined.size() == 2, "new-session provisional candle should append");
    require(combined.back().close == 101.0, "appended candle should use provisional close");
}

}  // namespace

int main() {
    updates_open_high_low_close_for_current_session();
    ignores_stale_and_invalid_quotes();
    replaces_matching_historical_candle_with_provisional();
    appends_new_provisional_candle();
    return 0;
}
