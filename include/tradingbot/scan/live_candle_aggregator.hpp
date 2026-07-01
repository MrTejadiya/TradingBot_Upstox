#pragma once

#include "tradingbot/core/domain.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::scan {

class LiveCandleAggregator {
public:
    explicit LiveCandleAggregator(std::string interval = "days:1");

    void update(const core::QuoteSnapshot& quote);
    std::optional<core::Candle> current_candle(const core::InstrumentKey& key) const;
    std::vector<core::Candle> current_candles() const;
    void clear();

private:
    std::string interval_;
    std::map<std::string, core::Candle> candles_;
    mutable std::mutex mutex_;
};

core::TimePoint session_day_start(core::TimePoint timestamp);
std::vector<core::Candle> with_provisional_candle(const std::vector<core::Candle>& historical,
                                                  const std::optional<core::Candle>& provisional);

}  // namespace tradingbot::scan
