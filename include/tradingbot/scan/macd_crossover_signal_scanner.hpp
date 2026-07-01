#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/scan/provisional_rsi_divergence_scanner.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::scan {

struct MacdCrossoverSignalConfig {
    int fast_period{12};
    int slow_period{26};
    int signal_period{9};
    double bullish_confidence{0.70};
    double bearish_confidence{0.70};
    double provisional_confidence_bonus{0.03};
    std::string bullish_strategy_name{"macd_bullish_cross"};
    std::string bearish_strategy_name{"macd_bearish_cross"};
};

class MacdCrossoverSignalScanner {
public:
    explicit MacdCrossoverSignalScanner(MacdCrossoverSignalConfig config = {});

    std::vector<core::StrategySignal> scan_one(const ProvisionalScanInput& input,
                                               const std::optional<core::Candle>& live_candle,
                                               core::TimePoint timestamp) const;
    std::vector<core::StrategySignal> scan_many(const std::vector<ProvisionalScanInput>& inputs,
                                                const PartitionedLiveCandleStore& candle_store,
                                                core::TimePoint timestamp) const;

private:
    MacdCrossoverSignalConfig config_;
};

}  // namespace tradingbot::scan
