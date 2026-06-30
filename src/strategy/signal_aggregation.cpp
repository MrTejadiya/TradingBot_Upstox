#include "tradingbot/strategy/signal_aggregation.hpp"

#include "tradingbot/strategy/strategy.hpp"

#include <algorithm>
#include <sstream>

namespace tradingbot::strategy {
namespace {

core::DecisionType decision_type_for(core::TradeAction action) {
    return action == core::TradeAction::Buy ? core::DecisionType::Buy : core::DecisionType::Sell;
}

core::Decision decision_from_signal(const core::StrategySignal& signal, const std::string& source,
                                    core::TimePoint decided_at) {
    return {
        .instrument_key = signal.instrument_key,
        .type = decision_type_for(signal.action),
        .confidence = signal.confidence,
        .quantity = signal.suggested_quantity,
        .price = signal.suggested_entry_price,
        .reason = signal.reason,
        .source = source,
        .timestamp = decided_at,
    };
}

std::vector<core::StrategySignal> actionable_signals(const SignalAggregationRequest& request,
                                                     std::vector<std::string>& diagnostics) {
    std::vector<core::StrategySignal> signals;
    for (const auto& signal : request.signals) {
        if (signal.instrument_key.value != request.instrument_key.value) {
            diagnostics.push_back("ignored signal for different instrument");
            continue;
        }
        if (!is_actionable_signal(signal)) {
            diagnostics.push_back("ignored non-actionable signal from " + signal.strategy_name);
            continue;
        }
        signals.push_back(signal);
    }
    return signals;
}

std::optional<core::StrategySignal> majority_signal(const std::vector<core::StrategySignal>& signals) {
    auto buy_count = 0;
    auto sell_count = 0;
    for (const auto& signal : signals) {
        if (signal.action == core::TradeAction::Buy) {
            ++buy_count;
        } else {
            ++sell_count;
        }
    }
    if (buy_count == sell_count) {
        return std::nullopt;
    }

    const auto winning_action = buy_count > sell_count ? core::TradeAction::Buy : core::TradeAction::Sell;
    return *std::max_element(signals.begin(), signals.end(), [winning_action](const auto& left, const auto& right) {
        const auto left_wins = left.action == winning_action;
        const auto right_wins = right.action == winning_action;
        if (left_wins != right_wins) {
            return !left_wins;
        }
        return left.confidence < right.confidence;
    });
}

}  // namespace

SignalAggregationResult aggregate_signals(const SignalAggregationRequest& request) {
    SignalAggregationResult result;
    const auto signals = actionable_signals(request, result.diagnostics);
    if (signals.empty()) {
        result.diagnostics.push_back("no actionable signals available");
        return result;
    }

    std::optional<core::StrategySignal> selected;
    switch (request.mode) {
        case SignalAggregationMode::FirstActionable:
            selected = signals.front();
            break;
        case SignalAggregationMode::HighestConfidence:
            selected = *std::max_element(signals.begin(), signals.end(), [](const auto& left, const auto& right) {
                return left.confidence < right.confidence;
            });
            break;
        case SignalAggregationMode::MajorityVote:
            selected = majority_signal(signals);
            if (!selected) {
                result.diagnostics.push_back("majority vote tied");
                return result;
            }
            break;
    }

    std::ostringstream source;
    source << signal_aggregation_mode_name(request.mode) << ":" << selected->strategy_name;
    result.decision = decision_from_signal(*selected, source.str(), request.decided_at);
    return result;
}

std::string signal_aggregation_mode_name(SignalAggregationMode mode) {
    switch (mode) {
        case SignalAggregationMode::FirstActionable:
            return "first_actionable";
        case SignalAggregationMode::HighestConfidence:
            return "highest_confidence";
        case SignalAggregationMode::MajorityVote:
            return "majority_vote";
    }
    return "unknown";
}

}  // namespace tradingbot::strategy

