#include "tradingbot/scan/live_rsi_divergence_engine.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
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

tradingbot::core::QuoteSnapshot quote(const std::string& key, double ltp, int days) {
    return {
        .instrument_key = {key},
        .timestamp = tp(days),
        .ltp = ltp,
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

void defaults_partition_count_to_available_workers() {
    tradingbot::scan::LiveRsiDivergenceEngine engine;

    require(engine.partition_count() == tradingbot::scan::available_worker_count(),
            "default live engine partition count should match available workers");
}

void scans_with_quote_driven_provisional_candle() {
    const auto bull_key = std::string{"NSE_EQ|BULL"};
    const auto bear_key = std::string{"NSE_EQ|BEAR"};
    tradingbot::scan::LiveRsiDivergenceEngine engine({
        .scanner = {.rsi_period = 3, .wing_size = 1, .worker_count = 2},
        .partition_count = 2,
    });

    require(engine.on_quote(quote(bull_key, 80.0, 11)), "valid live quote should update partitioned candle store");
    const auto results = engine.scan({
        {.instrument = instrument(bull_key, "BULL"),
         .historical_candles = candles_from_closes(bull_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
        {.instrument = instrument(bear_key, "BEAR"),
         .historical_candles = candles_from_closes(bear_key, {100, 105, 110, 112, 114, 122, 119, 124, 127, 119, 121, 123})},
    });

    require(results.size() == 2, "engine scan should return all results");
    require(results[0].instrument_key.value == bull_key, "engine scan should preserve first input order");
    require(results[1].instrument_key.value == bear_key, "engine scan should preserve second input order");
    require(results[0].provisional, "quote-driven scan should use live provisional candle");
    require(results[0].bullish_divergence, "quote-driven scan should detect bullish divergence");
    require(!results[1].provisional, "unquoted instrument should use closed candles only");
    require(results[1].bearish_divergence, "engine scan should detect bearish divergence");
}

void rejects_invalid_quotes_without_changing_scan_state() {
    const auto key = std::string{"NSE_EQ|BULL"};
    tradingbot::scan::LiveRsiDivergenceEngine engine({
        .scanner = {.rsi_period = 3, .wing_size = 1, .worker_count = 1},
        .partition_count = 1,
    });

    require(!engine.on_quote({.instrument_key = {key}, .timestamp = tp(11), .ltp = 0.0}),
            "invalid live quote should be rejected");
    const auto results = engine.scan({
        {.instrument = instrument(key, "BULL"),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    });

    require(results.size() == 1, "engine scan should still return result after invalid quote");
    require(!results[0].provisional, "invalid quote should not create provisional candle");
}

}  // namespace

int main() {
    defaults_partition_count_to_available_workers();
    scans_with_quote_driven_provisional_candle();
    rejects_invalid_quotes_without_changing_scan_state();
    return 0;
}
