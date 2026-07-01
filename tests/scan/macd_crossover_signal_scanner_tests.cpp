#include "tradingbot/scan/macd_crossover_signal_scanner.hpp"

#include "tradingbot/strategy/scanner_signal_ranker.hpp"
#include "tradingbot/strategy/strategy.hpp"

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

tradingbot::core::Instrument instrument(const std::string& key, const std::string& symbol,
                                        tradingbot::core::Quantity quantity = 2) {
    return {
        .key = {key},
        .symbol = symbol,
        .enabled = true,
        .quantity = quantity,
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

tradingbot::core::Candle candle(const std::string& key, double close, int days) {
    return {
        .instrument_key = {key},
        .timestamp = tp(days),
        .open = close,
        .high = close,
        .low = close,
        .close = close,
        .interval = "days:1",
    };
}

tradingbot::scan::MacdCrossoverSignalScanner scanner() {
    return tradingbot::scan::MacdCrossoverSignalScanner({
        .fast_period = 3,
        .slow_period = 5,
        .signal_period = 3,
    });
}

void detects_bullish_macd_histogram_cross() {
    const auto key = std::string{"NSE_EQ|BULL"};
    const auto signals = scanner().scan_one(
        {.instrument = instrument(key, "BULL", 3),
         .historical_candles = candles_from_closes(key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95, 95})},
        std::nullopt, tradingbot::core::TimePoint{std::chrono::seconds{7}});

    require(signals.size() == 1, "bullish MACD cross should emit one signal");
    require(signals[0].action == tradingbot::core::TradeAction::Buy, "bullish MACD cross should map to buy");
    require(signals[0].strategy_name == "macd_bullish_cross", "buy signal should use MACD bullish strategy name");
    require(signals[0].suggested_quantity == 3, "signal should use configured quantity");
    require(signals[0].suggested_entry_price && *signals[0].suggested_entry_price == 95.0,
            "signal should use latest close as entry price");
    require(signals[0].confidence == 0.70, "closed-candle MACD signal should use base confidence");
    require(signals[0].reason.find("bullish MACD histogram crossover") != std::string::npos,
            "reason should explain bullish crossover");
    require(tradingbot::strategy::is_actionable_signal(signals[0]), "MACD buy signal should be actionable");
}

void detects_bearish_macd_histogram_cross() {
    const auto key = std::string{"NSE_EQ|BEAR"};
    const auto signals = scanner().scan_one(
        {.instrument = instrument(key, "BEAR"),
         .historical_candles = candles_from_closes(key, {100, 99, 95, 94, 94, 93, 92, 93, 90, 94, 91, 88})},
        std::nullopt, tradingbot::core::TimePoint{});

    require(signals.size() == 1, "bearish MACD cross should emit one signal");
    require(signals[0].action == tradingbot::core::TradeAction::Sell, "bearish MACD cross should map to sell");
    require(signals[0].strategy_name == "macd_bearish_cross", "sell signal should use MACD bearish strategy name");
    require(signals[0].reason.find("bearish MACD histogram crossover") != std::string::npos,
            "reason should explain bearish crossover");
}

void skips_when_no_fresh_cross() {
    const auto key = std::string{"NSE_EQ|TREND"};
    const auto signals = scanner().scan_one(
        {.instrument = instrument(key, "TREND"),
         .historical_candles = candles_from_closes(key, {10, 9, 8, 7, 6, 5, 5, 6, 7, 8, 9, 10})},
        std::nullopt, tradingbot::core::TimePoint{});

    require(signals.empty(), "existing MACD direction without fresh cross should not emit signal");
}

void includes_live_provisional_candle() {
    const auto key = std::string{"NSE_EQ|LIVE"};
    const auto historical = candles_from_closes(key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95});
    const auto signals = scanner().scan_one(
        {.instrument = instrument(key, "LIVE"), .historical_candles = historical},
        candle(key, 95, 11), tradingbot::core::TimePoint{});

    require(signals.size() == 1, "live provisional candle should participate in MACD cross");
    require(signals[0].action == tradingbot::core::TradeAction::Buy, "provisional bullish cross should map to buy");
    require(signals[0].confidence == 0.73, "provisional MACD signal should include confidence bonus");
    require(signals[0].reason.find("live provisional candle") != std::string::npos,
            "reason should mention provisional candle");
}

void skips_invalid_inputs_and_config() {
    const auto key = std::string{"NSE_EQ|BAD"};
    auto disabled = instrument(key, "BAD");
    disabled.enabled = false;

    require(scanner()
                .scan_one({.instrument = disabled,
                           .historical_candles = candles_from_closes(key, {100, 103, 99, 97, 100, 102, 100, 101,
                                                                           98, 94, 95, 95})},
                          std::nullopt, tradingbot::core::TimePoint{})
                .empty(),
            "disabled instrument should not emit MACD signal");

    const tradingbot::scan::MacdCrossoverSignalScanner invalid_scanner({
        .fast_period = 5,
        .slow_period = 3,
        .signal_period = 3,
    });
    require(invalid_scanner
                .scan_one({.instrument = instrument(key, "BAD"),
                           .historical_candles = candles_from_closes(key, {100, 103, 99, 97, 100, 102, 100, 101,
                                                                           98, 94, 95, 95})},
                          std::nullopt, tradingbot::core::TimePoint{})
                .empty(),
            "invalid MACD config should fail closed");
}

void scan_many_feeds_ranker_compatible_signals() {
    const auto bull_key = std::string{"NSE_EQ|BULL"};
    const auto bear_key = std::string{"NSE_EQ|BEAR"};
    tradingbot::scan::PartitionedLiveCandleStore store(2);
    const std::vector<tradingbot::scan::ProvisionalScanInput> inputs{
        {.instrument = instrument(bull_key, "BULL"),
         .historical_candles = candles_from_closes(bull_key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95, 95})},
        {.instrument = instrument(bear_key, "BEAR"),
         .historical_candles = candles_from_closes(bear_key, {100, 99, 95, 94, 94, 93, 92, 93, 90, 94, 91, 88})},
    };

    const auto signals = scanner().scan_many(inputs, store, tradingbot::core::TimePoint{});
    const auto ranked = tradingbot::strategy::rank_scanner_signals(signals, {
        .strategy_weights = {{"macd_bullish_cross", 1.3}},
    });

    require(signals.size() == 2, "scan_many should emit buy and sell MACD signals");
    require(ranked.size() == 1, "default ranking should keep only buy candidate");
    require(ranked[0].instrument_key.value == bull_key, "MACD buy signal should rank as trade candidate");
}

}  // namespace

int main() {
    detects_bullish_macd_histogram_cross();
    detects_bearish_macd_histogram_cross();
    skips_when_no_fresh_cross();
    includes_live_provisional_candle();
    skips_invalid_inputs_and_config();
    scan_many_feeds_ranker_compatible_signals();
    return 0;
}
