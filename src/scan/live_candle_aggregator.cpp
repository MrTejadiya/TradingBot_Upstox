#include "tradingbot/scan/live_candle_aggregator.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>

namespace tradingbot::scan {

core::TimePoint session_day_start(core::TimePoint timestamp) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch());
    const auto day = seconds.count() / 86400;
    return core::TimePoint{std::chrono::seconds{day * 86400}};
}

LiveCandleAggregator::LiveCandleAggregator(std::string interval) : interval_(std::move(interval)) {}

void LiveCandleAggregator::update(const core::QuoteSnapshot& quote) {
    if (quote.instrument_key.value.empty() || quote.ltp <= 0.0 || quote.stale) {
        return;
    }

    const auto timestamp = quote.timestamp == core::TimePoint{} ? core::Clock::now() : quote.timestamp;
    const auto candle_time = session_day_start(timestamp);
    std::lock_guard lock(mutex_);
    auto& candle = candles_[quote.instrument_key.value];
    if (candle.timestamp != candle_time || candle.instrument_key.value != quote.instrument_key.value) {
        candle = {
            .instrument_key = quote.instrument_key,
            .timestamp = candle_time,
            .open = quote.ltp,
            .high = quote.ltp,
            .low = quote.ltp,
            .close = quote.ltp,
            .volume = 0,
            .interval = interval_,
        };
        return;
    }

    candle.high = std::max(candle.high, quote.ltp);
    candle.low = candle.low <= 0.0 ? quote.ltp : std::min(candle.low, quote.ltp);
    candle.close = quote.ltp;
}

std::optional<core::Candle> LiveCandleAggregator::current_candle(const core::InstrumentKey& key) const {
    std::lock_guard lock(mutex_);
    const auto found = candles_.find(key.value);
    if (found == candles_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<core::Candle> LiveCandleAggregator::current_candles() const {
    std::lock_guard lock(mutex_);
    std::vector<core::Candle> result;
    result.reserve(candles_.size());
    for (const auto& [_, candle] : candles_) {
        result.push_back(candle);
    }
    return result;
}

void LiveCandleAggregator::clear() {
    std::lock_guard lock(mutex_);
    candles_.clear();
}

std::vector<core::Candle> with_provisional_candle(const std::vector<core::Candle>& historical,
                                                  const std::optional<core::Candle>& provisional) {
    auto candles = historical;
    if (!provisional) {
        return candles;
    }
    if (!candles.empty() && candles.back().timestamp == provisional->timestamp &&
        candles.back().instrument_key.value == provisional->instrument_key.value &&
        candles.back().interval == provisional->interval) {
        candles.back() = *provisional;
    } else {
        candles.push_back(*provisional);
    }
    return candles;
}

}  // namespace tradingbot::scan
