#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/scan/provisional_rsi_divergence_scanner.hpp"

#include <string>
#include <vector>

namespace tradingbot::scan {

struct RsiDivergenceSignalConfig {
    double bullish_confidence{0.72};
    double bearish_confidence{0.72};
    double provisional_confidence_bonus{0.03};
    std::string strategy_name{"rsi_divergence"};
};

std::vector<core::StrategySignal> map_rsi_divergence_signals(
    const std::vector<ProvisionalScanInput>& inputs, const std::vector<ProvisionalDivergenceResult>& results,
    core::TimePoint timestamp, RsiDivergenceSignalConfig config = {});

}  // namespace tradingbot::scan
