#include "tradingbot/strategy/scanner_candidate_selector.hpp"

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

tradingbot::strategy::ScannerCandidateRank candidate() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .action = tradingbot::core::TradeAction::Buy,
        .score = 1.42,
        .strongest_confidence = 0.82,
        .signal_count = 2,
        .strategy_names = {"macd_bullish_cross", "rsi_divergence"},
        .suggested_quantity = 3,
        .suggested_entry_price = 101.5,
        .ranked_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
    };
}

void selects_top_buy_candidate_as_decision() {
    const auto decided_at = tradingbot::core::TimePoint{std::chrono::seconds{9}};
    const auto result = tradingbot::strategy::select_top_ranked_candidate(
        {candidate()},
        {.minimum_score = 1.0, .source = "live_scanner_ranker"},
        decided_at);

    require(result.decision.has_value(), "valid top candidate should produce a decision");
    require(result.decision->instrument_key.value == "NSE_EQ|INE002A01018", "decision should keep instrument key");
    require(result.decision->type == tradingbot::core::DecisionType::Buy, "buy candidate should map to buy decision");
    require(result.decision->confidence == 0.82, "decision should use strongest confidence");
    require(result.decision->quantity == 3, "decision should use ranked quantity");
    require(result.decision->price && *result.decision->price == 101.5, "decision should use ranked price");
    require(result.decision->source == "live_scanner_ranker", "decision source should be configurable");
    require(result.decision->reason.find("macd_bullish_cross") != std::string::npos,
            "decision reason should include scanner names");
    require(result.decision->timestamp == decided_at, "decision timestamp should be retained");
}

void fails_closed_for_empty_candidates() {
    const auto result = tradingbot::strategy::select_top_ranked_candidate({});

    require(!result.decision.has_value(), "empty ranking should not produce decision");
    require(!result.diagnostics.empty(), "empty ranking should explain rejection");
}

void fails_closed_for_wrong_action() {
    auto ranked = candidate();
    ranked.action = tradingbot::core::TradeAction::Sell;
    const auto result = tradingbot::strategy::select_top_ranked_candidate({ranked});

    require(!result.decision.has_value(), "wrong action should not produce buy decision");
}

void fails_closed_for_low_score() {
    const auto result = tradingbot::strategy::select_top_ranked_candidate({candidate()}, {.minimum_score = 2.0});

    require(!result.decision.has_value(), "low score should not produce decision");
}

void fails_closed_for_missing_quantity_or_price() {
    auto no_quantity = candidate();
    no_quantity.suggested_quantity = 0;
    auto no_price = candidate();
    no_price.suggested_entry_price = std::nullopt;

    require(!tradingbot::strategy::select_top_ranked_candidate({no_quantity}).decision.has_value(),
            "missing quantity should fail closed");
    require(!tradingbot::strategy::select_top_ranked_candidate({no_price}).decision.has_value(),
            "missing price should fail closed");
}

void can_select_sell_candidate_when_expected() {
    auto ranked = candidate();
    ranked.action = tradingbot::core::TradeAction::Sell;
    const auto result = tradingbot::strategy::select_top_ranked_candidate(
        {ranked}, {.expected_action = tradingbot::core::TradeAction::Sell});

    require(result.decision.has_value(), "expected sell candidate should produce decision");
    require(result.decision->type == tradingbot::core::DecisionType::Sell, "sell candidate should map to sell decision");
}

}  // namespace

int main() {
    selects_top_buy_candidate_as_decision();
    fails_closed_for_empty_candidates();
    fails_closed_for_wrong_action();
    fails_closed_for_low_score();
    fails_closed_for_missing_quantity_or_price();
    can_select_sell_candidate_when_expected();
    return 0;
}
