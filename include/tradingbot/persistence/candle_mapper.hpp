#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>

namespace tradingbot::persistence {

struct StoredCandleRow {
    std::string run_id;
    std::string instrument_key;
    std::string interval;
    core::TimePoint candle_at{};
    core::Money open{0.0};
    core::Money high{0.0};
    core::Money low{0.0};
    core::Money close{0.0};
    core::Quantity volume{0};
};

StoredCandleRow map_candle_to_stored_row(const core::Candle& candle, const std::string& run_id);
core::Candle map_stored_candle_row(const StoredCandleRow& row);

}  // namespace tradingbot::persistence
