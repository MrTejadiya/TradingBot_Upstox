#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::strategy {

enum class SignalAggregationMode {
    FirstActionable,
    HighestConfidence,
    MajorityVote,
};

struct SignalAggregationRequest {
    core::InstrumentKey instrument_key;
    std::vector<core::StrategySignal> signals;
    SignalAggregationMode mode{SignalAggregationMode::HighestConfidence};
    core::TimePoint decided_at{};
};

struct SignalAggregationResult {
    std::optional<core::Decision> decision;
    std::vector<std::string> diagnostics;
};

SignalAggregationResult aggregate_signals(const SignalAggregationRequest& request);
std::string signal_aggregation_mode_name(SignalAggregationMode mode);

}  // namespace tradingbot::strategy

