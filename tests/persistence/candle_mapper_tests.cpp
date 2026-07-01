#include "tradingbot/persistence/candle_mapper.hpp"

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

tradingbot::core::Candle candle() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
        .open = 100.0,
        .high = 110.0,
        .low = 95.0,
        .close = 105.0,
        .volume = 12345,
        .interval = "days:1",
    };
}

tradingbot::persistence::StoredCandleRow stored_row() {
    return {
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .interval = "days:1",
        .candle_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
        .open = 100.0,
        .high = 110.0,
        .low = 95.0,
        .close = 105.0,
        .volume = 12345,
    };
}

void maps_candle_to_stored_row() {
    const auto row = tradingbot::persistence::map_candle_to_stored_row(candle(), "run-1");

    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.interval == "days:1", "interval should store");
    require(row.candle_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
    require(row.open == 100.0, "open should store");
    require(row.high == 110.0, "high should store");
    require(row.low == 95.0, "low should store");
    require(row.close == 105.0, "close should store");
    require(row.volume == 12345, "volume should store");
}

void maps_stored_row_to_candle() {
    const auto mapped = tradingbot::persistence::map_stored_candle_row(stored_row());

    require(mapped.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(mapped.interval == "days:1", "interval should map");
    require(mapped.timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should map");
    require(mapped.open == 100.0, "open should map");
    require(mapped.high == 110.0, "high should map");
    require(mapped.low == 95.0, "low should map");
    require(mapped.close == 105.0, "close should map");
    require(mapped.volume == 12345, "volume should map");
}

void round_trips_candle_through_stored_row() {
    const auto row = tradingbot::persistence::map_candle_to_stored_row(candle(), "run-1");
    const auto mapped = tradingbot::persistence::map_stored_candle_row(row);

    require(mapped.instrument_key.value == candle().instrument_key.value, "instrument should round trip");
    require(mapped.interval == candle().interval, "interval should round trip");
    require(mapped.timestamp == candle().timestamp, "timestamp should round trip");
    require(mapped.open == candle().open, "open should round trip");
    require(mapped.high == candle().high, "high should round trip");
    require(mapped.low == candle().low, "low should round trip");
    require(mapped.close == candle().close, "close should round trip");
    require(mapped.volume == candle().volume, "volume should round trip");
}

}  // namespace

int main() {
    maps_candle_to_stored_row();
    maps_stored_row_to_candle();
    round_trips_candle_through_stored_row();
    return 0;
}
