#include "tradingbot/scan/live_scanner_decision_pipeline.hpp"

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

tradingbot::scan::LiveScannerDecisionPipeline pipeline(double minimum_score = 0.0) {
    return tradingbot::scan::LiveScannerDecisionPipeline({
        .ranking =
            {
                .macd = {.fast_period = 3, .slow_period = 5, .signal_period = 3},
                .ranking = {.strategy_weights = {{"rsi_divergence", 1.0}, {"macd_bullish_cross", 1.3}}},
            },
        .selection = {.minimum_score = minimum_score, .source = "live_scanner_decision_pipeline"},
    });
}

void selects_top_ranked_candidate_as_decision() {
    const auto key = std::string{"NSE_EQ|RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "RSI fixture should include live provisional candle");
    const auto decided_at = tradingbot::core::TimePoint{std::chrono::seconds{17}};

    const auto result = pipeline().decide({
        {.instrument = instrument(key, "RSI", 3),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, decided_at);

    require(result.ranking.rsi_results.size() == 1, "decision pipeline should retain RSI scan results");
    require(result.ranking.signals.size() == 1, "decision pipeline should retain raw scanner signals");
    require(result.ranking.ranked_candidates.size() == 1, "decision pipeline should retain ranked candidates");
    require(result.selection.decision.has_value(), "top ranked scanner candidate should select decision");
    require(result.selection.decision->instrument_key.value == key, "decision should use ranked instrument");
    require(result.selection.decision->type == tradingbot::core::DecisionType::Buy, "decision should be buy");
    require(result.selection.decision->quantity == 3, "decision should use scanner quantity");
    require(result.selection.decision->price && *result.selection.decision->price == 80.0,
            "decision should use scanner price");
    require(result.selection.decision->source == "live_scanner_decision_pipeline", "decision source should be set");
    require(result.selection.decision->timestamp == decided_at, "decision timestamp should be retained");
}

void fails_closed_when_no_scanner_signals_fire() {
    const auto key = std::string{"NSE_EQ|QUIET"};
    auto engine = rsi_engine();

    const auto result = pipeline().decide({
        {.instrument = instrument(key, "QUIET"),
         .historical_candles = candles_from_closes(key, {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111})},
    }, engine, tradingbot::core::TimePoint{});

    require(result.ranking.signals.empty(), "quiet input should emit no scanner signals");
    require(!result.selection.decision.has_value(), "no signal should fail closed");
    require(!result.selection.diagnostics.empty(), "no signal failure should include diagnostic");
}

void fails_closed_when_score_is_below_minimum() {
    const auto key = std::string{"NSE_EQ|RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "RSI fixture should include live provisional candle");

    const auto result = pipeline(10.0).decide({
        {.instrument = instrument(key, "RSI"),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, tradingbot::core::TimePoint{});

    require(!result.selection.decision.has_value(), "low score should fail closed");
    require(!result.ranking.ranked_candidates.empty(), "ranking should still be available for review");
}

void weighted_macd_candidate_can_win_selection() {
    const auto rsi_key = std::string{"NSE_EQ|RSI"};
    const auto macd_key = std::string{"NSE_EQ|MACD"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(rsi_key, 80.0, 11)), "RSI fixture should include live provisional candle");

    const auto result = pipeline().decide({
        {.instrument = instrument(rsi_key, "RSI"),
         .historical_candles = candles_from_closes(rsi_key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
        {.instrument = instrument(macd_key, "MACD"),
         .historical_candles =
             candles_from_closes(macd_key, {100, 103, 99, 97, 100, 102, 100, 101, 98, 94, 95, 95})},
    }, engine, tradingbot::core::TimePoint{});

    require(result.selection.decision.has_value(), "combined scanner ranking should select decision");
    require(result.selection.decision->instrument_key.value == macd_key,
            "weighted MACD candidate should be selected over RSI");
}

}  // namespace

int main() {
    selects_top_ranked_candidate_as_decision();
    fails_closed_when_no_scanner_signals_fire();
    fails_closed_when_score_is_below_minimum();
    weighted_macd_candidate_can_win_selection();
    return 0;
}
