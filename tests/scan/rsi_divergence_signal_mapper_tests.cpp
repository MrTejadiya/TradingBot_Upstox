#include "tradingbot/scan/rsi_divergence_signal_mapper.hpp"

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

tradingbot::core::Instrument instrument(const std::string& key, const std::string& symbol,
                                        tradingbot::core::Quantity quantity = 5) {
    return {
        .key = {key},
        .symbol = symbol,
        .enabled = true,
        .quantity = quantity,
    };
}

tradingbot::scan::ProvisionalScanInput input(const std::string& key, const std::string& symbol,
                                             tradingbot::core::Quantity quantity = 5) {
    return {.instrument = instrument(key, symbol, quantity)};
}

tradingbot::scan::ProvisionalDivergenceResult result(const std::string& key) {
    return {
        .instrument_key = {key},
        .symbol = "TEST",
        .ok = true,
        .latest_close = 101.5,
        .latest_rsi = 42.25,
        .candle_count = 20,
    };
}

void maps_bullish_divergence_to_buy_signal() {
    auto scan_result = result("NSE_EQ|BULL");
    scan_result.provisional = true;
    scan_result.bullish_divergence = true;
    const auto timestamp = tradingbot::core::TimePoint{std::chrono::seconds{7}};

    const auto signals = tradingbot::scan::map_rsi_divergence_signals(
        {input("NSE_EQ|BULL", "BULL", 3)}, {scan_result}, timestamp);

    require(signals.size() == 1, "bullish divergence should emit one signal");
    require(signals[0].instrument_key.value == "NSE_EQ|BULL", "signal should keep instrument key");
    require(signals[0].action == tradingbot::core::TradeAction::Buy, "bullish divergence should map to buy");
    require(signals[0].suggested_quantity == 3, "signal should use configured instrument quantity");
    require(signals[0].suggested_entry_price && *signals[0].suggested_entry_price == 101.5,
            "signal should use latest close as entry price");
    require(signals[0].confidence == 0.75, "provisional bullish signal should include confidence bonus");
    require(signals[0].strategy_name == "rsi_divergence", "strategy name should be set");
    require(signals[0].reason.find("bullish RSI divergence") != std::string::npos, "reason should mention bullish");
    require(signals[0].reason.find("live provisional candle") != std::string::npos,
            "reason should mention provisional candle");
    require(signals[0].timestamp == timestamp, "timestamp should be retained");
}

void maps_bearish_divergence_to_sell_signal() {
    auto scan_result = result("NSE_EQ|BEAR");
    scan_result.bearish_divergence = true;
    const auto timestamp = tradingbot::core::TimePoint{std::chrono::seconds{9}};

    const auto signals = tradingbot::scan::map_rsi_divergence_signals(
        {input("NSE_EQ|BEAR", "BEAR", 4)}, {scan_result}, timestamp);

    require(signals.size() == 1, "bearish divergence should emit one signal");
    require(signals[0].action == tradingbot::core::TradeAction::Sell, "bearish divergence should map to sell");
    require(signals[0].suggested_quantity == 4, "sell signal should use configured quantity");
    require(signals[0].confidence == 0.72, "closed-candle bearish signal should use base confidence");
    require(signals[0].reason.find("bearish RSI divergence") != std::string::npos, "reason should mention bearish");
}

void skips_non_actionable_results() {
    auto clean = result("NSE_EQ|CLEAN");
    auto invalid = result("NSE_EQ|INVALID");
    invalid.ok = false;

    const auto signals = tradingbot::scan::map_rsi_divergence_signals(
        {input("NSE_EQ|CLEAN", "CLEAN"), input("NSE_EQ|INVALID", "INVALID")},
        {clean, invalid}, tradingbot::core::TimePoint{});

    require(signals.empty(), "non-divergent and invalid results should not emit signals");
}

void fails_closed_for_mismatched_batches() {
    auto scan_result = result("NSE_EQ|BULL");
    scan_result.bullish_divergence = true;

    const auto signals = tradingbot::scan::map_rsi_divergence_signals(
        {input("NSE_EQ|BULL", "BULL"), input("NSE_EQ|EXTRA", "EXTRA")},
        {scan_result}, tradingbot::core::TimePoint{});

    require(signals.empty(), "mismatched batches should fail closed");
}

void skips_result_for_different_instrument_key() {
    auto scan_result = result("NSE_EQ|OTHER");
    scan_result.bullish_divergence = true;

    const auto signals = tradingbot::scan::map_rsi_divergence_signals(
        {input("NSE_EQ|BULL", "BULL")}, {scan_result}, tradingbot::core::TimePoint{});

    require(signals.empty(), "different instrument key should be ignored");
}

}  // namespace

int main() {
    maps_bullish_divergence_to_buy_signal();
    maps_bearish_divergence_to_sell_signal();
    skips_non_actionable_results();
    fails_closed_for_mismatched_batches();
    skips_result_for_different_instrument_key();
    return 0;
}
