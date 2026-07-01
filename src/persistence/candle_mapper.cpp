#include "tradingbot/persistence/candle_mapper.hpp"

namespace tradingbot::persistence {

StoredCandleRow map_candle_to_stored_row(const core::Candle& candle, const std::string& run_id) {
    return {
        .run_id = run_id,
        .instrument_key = candle.instrument_key.value,
        .interval = candle.interval,
        .candle_at = candle.timestamp,
        .open = candle.open,
        .high = candle.high,
        .low = candle.low,
        .close = candle.close,
        .volume = candle.volume,
    };
}

core::Candle map_stored_candle_row(const StoredCandleRow& row) {
    return {
        .instrument_key = {row.instrument_key},
        .timestamp = row.candle_at,
        .open = row.open,
        .high = row.high,
        .low = row.low,
        .close = row.close,
        .volume = row.volume,
        .interval = row.interval,
    };
}

}  // namespace tradingbot::persistence
