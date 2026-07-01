#include "tradingbot/scan/provisional_rsi_divergence_scanner.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::TimePoint tp(int days) {
    return tradingbot::core::TimePoint{std::chrono::seconds{days * 86400}};
}

tradingbot::core::Instrument instrument(const std::string& key, const std::string& symbol) {
    return {
        .key = {key},
        .symbol = symbol,
        .enabled = true,
        .quantity = 1,
    };
}

std::vector<tradingbot::core::Candle> candles_from_closes(const std::string& key, const std::vector<double>& closes) {
    std::vector<tradingbot::core::Candle> candles;
    candles.reserve(closes.size());
    for (auto index = std::size_t{0}; index < closes.size(); ++index) {
        candles.push_back({
            .instrument_key = {key},
            .timestamp = tp(static_cast<int>(index)),
            .open = closes[index],
            .high = closes[index],
            .low = closes[index],
            .close = closes[index],
            .interval = "days:1",
        });
    }
    return candles;
}

tradingbot::core::QuoteSnapshot quote(const std::string& key, double ltp, int days) {
    return {
        .instrument_key = {key},
        .timestamp = tp(days),
        .ltp = ltp,
    };
}

void detects_bullish_divergence_with_provisional_candle() {
    const auto key = std::string{"NSE_EQ|BULL"};
    auto historical = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82});
    tradingbot::scan::LiveCandleAggregator aggregator;
    aggregator.update(quote(key, 80.0, 11));
    tradingbot::scan::ProvisionalRsiDivergenceScanner scanner({.rsi_period = 3, .wing_size = 1, .worker_count = 2});

    const auto result = scanner.scan_one({.instrument = instrument(key, "BULL"), .historical_candles = historical},
                                         aggregator);

    require(result.ok, "bullish scan should be valid");
    require(result.provisional, "scan should include provisional candle");
    require(result.bullish_divergence, "bullish divergence should detect lower low with higher RSI");
    require(!result.bearish_divergence, "bullish fixture should not be bearish");
}

void detects_bearish_divergence_from_closed_candles() {
    const auto key = std::string{"NSE_EQ|BEAR"};
    const auto historical = candles_from_closes(key, {100, 105, 110, 112, 114, 122, 119, 124, 127, 119, 121, 123});
    tradingbot::scan::LiveCandleAggregator aggregator;
    tradingbot::scan::ProvisionalRsiDivergenceScanner scanner({.rsi_period = 3, .wing_size = 1, .worker_count = 2});

    const auto result = scanner.scan_one({.instrument = instrument(key, "BEAR"), .historical_candles = historical},
                                         aggregator);

    require(result.ok, "bearish scan should be valid");
    require(!result.provisional, "scan should use closed candles only");
    require(!result.bullish_divergence, "bearish fixture should not be bullish");
    require(result.bearish_divergence, "bearish divergence should detect higher high with lower RSI");
}

void reports_insufficient_candles() {
    const auto key = std::string{"NSE_EQ|SHORT"};
    tradingbot::scan::LiveCandleAggregator aggregator;
    tradingbot::scan::ProvisionalRsiDivergenceScanner scanner({.rsi_period = 14, .wing_size = 1, .worker_count = 1});

    const auto result = scanner.scan_one(
        {.instrument = instrument(key, "SHORT"), .historical_candles = candles_from_closes(key, {100, 101, 102})},
        aggregator);

    require(!result.ok, "short history should not scan");
    require(result.diagnostic.find("insufficient") != std::string::npos, "diagnostic should explain shortage");
}

void assigns_same_instrument_key_to_same_owner_partition() {
    const auto first = tradingbot::scan::owner_partition("NSE_EQ|INE002A01018", 10);
    const auto second = tradingbot::scan::owner_partition("NSE_EQ|INE002A01018", 10);

    require(first == second, "same instrument key should map to the same owner partition");
    require(first < 10, "owner partition should be within worker range");
}

void partitions_inputs_by_instrument_owner() {
    const auto partitions = tradingbot::scan::partition_scan_inputs({
        {.instrument = instrument("NSE_EQ|SAME", "SAME_A")},
        {.instrument = instrument("NSE_EQ|OTHER", "OTHER")},
        {.instrument = instrument("NSE_EQ|SAME", "SAME_B")},
    }, 4);
    std::optional<std::size_t> first_bucket;
    std::optional<std::size_t> third_bucket;

    for (auto bucket = std::size_t{0}; bucket < partitions.size(); ++bucket) {
        for (const auto input_index : partitions[bucket]) {
            if (input_index == 0) {
                first_bucket = bucket;
            }
            if (input_index == 2) {
                third_bucket = bucket;
            }
        }
    }

    require(partitions.size() == 4, "partition count should match worker count");
    require(first_bucket.has_value(), "first input should be assigned to a partition");
    require(third_bucket.has_value(), "third input should be assigned to a partition");
    require(first_bucket == third_bucket, "same instrument key should stay in one owner partition");
}

void defaults_worker_count_to_available_cores() {
    require(tradingbot::scan::available_worker_count() >= 1, "available worker count should have a safe fallback");

    const auto bull_key = std::string{"NSE_EQ|BULL"};
    tradingbot::scan::LiveCandleAggregator aggregator;
    aggregator.update(quote(bull_key, 80.0, 11));
    tradingbot::scan::ProvisionalRsiDivergenceScanner scanner({.rsi_period = 3, .wing_size = 1});

    const auto results = scanner.scan_parallel({
        {.instrument = instrument(bull_key, "BULL"),
         .historical_candles = candles_from_closes(bull_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, aggregator);

    require(results.size() == 1, "default scanner should process inputs with auto worker count");
    require(results[0].bullish_divergence, "default scanner should preserve scan behavior");
}

void scans_multiple_instruments_in_parallel_with_stable_order() {
    const auto bull_key = std::string{"NSE_EQ|BULL"};
    const auto bear_key = std::string{"NSE_EQ|BEAR"};
    tradingbot::scan::LiveCandleAggregator aggregator;
    aggregator.update(quote(bull_key, 80.0, 11));
    tradingbot::scan::ProvisionalRsiDivergenceScanner scanner({.rsi_period = 3, .wing_size = 1, .worker_count = 4});

    const auto results = scanner.scan_parallel({
        {.instrument = instrument(bull_key, "BULL"),
         .historical_candles = candles_from_closes(bull_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
        {.instrument = instrument(bear_key, "BEAR"),
         .historical_candles = candles_from_closes(bear_key, {100, 105, 110, 112, 114, 122, 119, 124, 127, 119, 121, 123})},
    }, aggregator);

    require(results.size() == 2, "parallel scan should return both results");
    require(results[0].instrument_key.value == bull_key, "parallel output should preserve first input order");
    require(results[1].instrument_key.value == bear_key, "parallel output should preserve second input order");
    require(results[0].bullish_divergence, "first result should be bullish");
    require(results[1].bearish_divergence, "second result should be bearish");
}

}  // namespace

int main() {
    detects_bullish_divergence_with_provisional_candle();
    detects_bearish_divergence_from_closed_candles();
    reports_insufficient_candles();
    assigns_same_instrument_key_to_same_owner_partition();
    partitions_inputs_by_instrument_owner();
    defaults_worker_count_to_available_cores();
    scans_multiple_instruments_in_parallel_with_stable_order();
    return 0;
}
