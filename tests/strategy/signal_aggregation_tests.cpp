#include "tradingbot/strategy/signal_aggregation.hpp"

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

tradingbot::core::StrategySignal signal(tradingbot::core::TradeAction action, double confidence,
                                        const std::string& strategy_name) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .action = action,
        .confidence = confidence,
        .suggested_quantity = 3,
        .suggested_entry_price = 101.0,
        .reason = strategy_name + " reason",
        .strategy_name = strategy_name,
        .timestamp = tradingbot::core::Clock::now(),
    };
}

void highest_confidence_selects_strongest_signal() {
    const auto result = tradingbot::strategy::aggregate_signals({
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .signals = {signal(tradingbot::core::TradeAction::Buy, 0.6, "manual_buy"),
                    signal(tradingbot::core::TradeAction::Sell, 0.9, "stop_loss_sell")},
        .mode = tradingbot::strategy::SignalAggregationMode::HighestConfidence,
        .decided_at = tradingbot::core::Clock::now(),
    });

    require(result.decision.has_value(), "highest confidence should produce decision");
    require(result.decision->type == tradingbot::core::DecisionType::Sell, "highest confidence should select sell");
    require(result.decision->source == "highest_confidence:stop_loss_sell", "source should include mode and strategy");
}

void first_actionable_preserves_input_order() {
    const auto result = tradingbot::strategy::aggregate_signals({
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .signals = {signal(tradingbot::core::TradeAction::Buy, 0.6, "manual_buy"),
                    signal(tradingbot::core::TradeAction::Sell, 0.9, "stop_loss_sell")},
        .mode = tradingbot::strategy::SignalAggregationMode::FirstActionable,
        .decided_at = tradingbot::core::Clock::now(),
    });

    require(result.decision.has_value(), "first actionable should produce decision");
    require(result.decision->type == tradingbot::core::DecisionType::Buy, "first actionable should select first signal");
}

void majority_vote_selects_highest_confidence_within_winning_side() {
    const auto result = tradingbot::strategy::aggregate_signals({
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .signals = {signal(tradingbot::core::TradeAction::Buy, 0.55, "rsi_oversold"),
                    signal(tradingbot::core::TradeAction::Buy, 0.85, "manual_buy"),
                    signal(tradingbot::core::TradeAction::Sell, 0.95, "target_profit_sell")},
        .mode = tradingbot::strategy::SignalAggregationMode::MajorityVote,
        .decided_at = tradingbot::core::Clock::now(),
    });

    require(result.decision.has_value(), "majority vote should produce decision");
    require(result.decision->type == tradingbot::core::DecisionType::Buy, "majority vote should select buy side");
    require(result.decision->source == "majority_vote:manual_buy", "majority should select strongest winning signal");
}

void aggregation_filters_invalid_and_other_instruments() {
    auto invalid = signal(tradingbot::core::TradeAction::Buy, 0.5, "invalid");
    invalid.suggested_quantity = 0;
    auto other = signal(tradingbot::core::TradeAction::Buy, 0.9, "other");
    other.instrument_key = {"NSE_EQ|OTHER"};

    const auto result = tradingbot::strategy::aggregate_signals({
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .signals = {invalid, other},
        .mode = tradingbot::strategy::SignalAggregationMode::HighestConfidence,
        .decided_at = tradingbot::core::Clock::now(),
    });

    require(!result.decision.has_value(), "invalid signals should not produce decision");
    require(result.diagnostics.size() == 3, "filtering should include diagnostics plus empty result");
}

void majority_vote_reports_tie() {
    const auto result = tradingbot::strategy::aggregate_signals({
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .signals = {signal(tradingbot::core::TradeAction::Buy, 0.8, "manual_buy"),
                    signal(tradingbot::core::TradeAction::Sell, 0.8, "target_profit_sell")},
        .mode = tradingbot::strategy::SignalAggregationMode::MajorityVote,
        .decided_at = tradingbot::core::Clock::now(),
    });

    require(!result.decision.has_value(), "majority tie should not produce decision");
    require(result.diagnostics.front().find("tied") != std::string::npos, "majority tie should be diagnostic");
}

}  // namespace

int main() {
    highest_confidence_selects_strongest_signal();
    first_actionable_preserves_input_order();
    majority_vote_selects_highest_confidence_within_winning_side();
    aggregation_filters_invalid_and_other_instruments();
    majority_vote_reports_tie();
    return 0;
}

