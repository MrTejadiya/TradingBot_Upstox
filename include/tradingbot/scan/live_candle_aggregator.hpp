#pragma once

#include "tradingbot/core/domain.hpp"

#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
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

class PartitionedLiveCandleStore {
public:
    explicit PartitionedLiveCandleStore(std::size_t partition_count, std::string interval = "days:1");

    std::size_t partition_count() const;
    std::size_t owner_for(const core::InstrumentKey& key) const;

    bool update(std::size_t owner_partition, const core::QuoteSnapshot& quote);
    bool update(const core::QuoteSnapshot& quote);
    std::optional<core::Candle> current_candle(std::size_t owner_partition, const core::InstrumentKey& key) const;
    std::vector<core::Candle> current_candles(std::size_t owner_partition) const;
    void clear(std::size_t owner_partition);
    void clear_all();

private:
    // No internal mutex: callers must route each key's updates and reads through its owner partition.
    std::string interval_;
    std::vector<std::unordered_map<std::string, core::Candle>> partitions_;
};

core::TimePoint session_day_start(core::TimePoint timestamp);
std::vector<core::Candle> with_provisional_candle(const std::vector<core::Candle>& historical,
                                                  const std::optional<core::Candle>& provisional);

}  // namespace tradingbot::scan
