#pragma once

#include "tradingbot/core/domain.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::strategy {

struct ExitEngineRequest {
    core::Instrument instrument;
    core::Holding holding;
    std::optional<core::QuoteSnapshot> quote;
    std::vector<core::Candle> candles;
    std::vector<core::StrategySignal> strategy_signals;
    std::vector<core::RiskEvent> risk_events;
    std::optional<std::chrono::seconds> max_holding_duration;
    bool emergency_exit{false};
    core::TimePoint evaluated_at{};
};

struct ExitEngineResult {
    std::optional<core::Decision> decision;
    std::optional<core::ExitReason> exit_reason;
    std::vector<std::string> diagnostics;
};

class ExitEngine {
public:
    ExitEngineResult evaluate(const ExitEngineRequest& request) const;
};

std::string exit_reason_name(core::ExitReason reason);

}  // namespace tradingbot::strategy
