#include "tradingbot/scan/live_scanner_ranking_pipeline.hpp"

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

tradingbot::scan::LiveRsiDivergenceEngine rsi_engine() {
    return tradingbot::scan::LiveRsiDivergenceEngine({
        .scanner = {.rsi_period = 3, .wing_size = 1, .worker_count = 2},
        .partition_count = 2,
    });
}

tradingbot::scan::LiveScannerRankingPipeline pipeline(std::size_t limit = 0) {
    return tradingbot::scan::LiveScannerRankingPipeline({
        .macd = {.fast_period = 3, .slow_period = 5, .signal_period = 3},
        .ranking = {.strategy_weights = {{"rsi_divergence", 1.0}, {"macd_bullish_cross", 1.3}}, .limit = limit},
    });
}

void ranks_rsi_and_macd_candidates_together() {
    const auto rsi_key = std::string{"NSE_EQ|RSI"};
    const auto macd_key = std::string{"NSE_EQ|MACD"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(rsi_key, 80.0, 11)), "RSI fixture should include live provisional candle");
    const std::vector<tradingbot::scan::ProvisionalScanInput> inputs{
        {.instrument = instrument(rsi_key, "RSI", 3),
         .historical_candles = candles_from_closes(rsi_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
        {.instrument = instrument(macd_key, "MACD", 4),
         .historical_candles =
             candles_from_closes(macd_key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95, 95})},
    };

    const auto result = pipeline().rank(inputs, engine, tradingbot::core::TimePoint{std::chrono::seconds{7}});

    require(result.rsi_results.size() == 2, "pipeline should retain RSI scan results");
    require(result.signals.size() == 2, "pipeline should combine RSI and MACD signals");
    require(result.ranked_candidates.size() == 2, "pipeline should rank both buy candidates");
    require(result.ranked_candidates[0].instrument_key.value == macd_key,
            "weighted MACD candidate should rank ahead of RSI candidate");
    require(result.ranked_candidates[0].ranked_at == tradingbot::core::TimePoint{std::chrono::seconds{7}},
            "rank timestamp should be retained");
}

void applies_top_n_limit() {
    const auto rsi_key = std::string{"NSE_EQ|RSI"};
    const auto macd_key = std::string{"NSE_EQ|MACD"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(rsi_key, 80.0, 11)), "RSI fixture should include live provisional candle");
    const std::vector<tradingbot::scan::ProvisionalScanInput> inputs{
        {.instrument = instrument(rsi_key, "RSI"),
         .historical_candles = candles_from_closes(rsi_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
        {.instrument = instrument(macd_key, "MACD"),
         .historical_candles =
             candles_from_closes(macd_key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95, 95})},
    };

    const auto result = pipeline(1).rank(inputs, engine, tradingbot::core::TimePoint{});

    require(result.ranked_candidates.size() == 1, "pipeline should apply ranking limit");
    require(result.ranked_candidates[0].instrument_key.value == macd_key, "top candidate should remain after limit");
}

void includes_live_provisional_rsi_candle() {
    const auto key = std::string{"NSE_EQ|LIVE_RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "live quote should update engine");

    const auto result = pipeline().rank({
        {.instrument = instrument(key, "LIVE_RSI"),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, tradingbot::core::TimePoint{});

    require(result.signals.size() == 1, "live RSI divergence should map to signal");
    require(result.signals[0].reason.find("live provisional candle") != std::string::npos,
            "signal should mention provisional candle");
    require(result.ranked_candidates.size() == 1, "live RSI signal should rank");
}

void returns_empty_ranking_when_no_scanner_signals_fire() {
    const auto key = std::string{"NSE_EQ|QUIET"};
    auto engine = rsi_engine();

    const auto result = pipeline().rank({
        {.instrument = instrument(key, "QUIET"),
         .historical_candles = candles_from_closes(key, {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111})},
    }, engine, tradingbot::core::TimePoint{});

    require(result.signals.empty(), "quiet input should produce no scanner signals");
    require(result.ranked_candidates.empty(), "quiet input should produce no ranked candidates");
}

}  // namespace

int main() {
    ranks_rsi_and_macd_candidates_together();
    applies_top_n_limit();
    includes_live_provisional_rsi_candle();
    returns_empty_ranking_when_no_scanner_signals_fire();
    return 0;
}
