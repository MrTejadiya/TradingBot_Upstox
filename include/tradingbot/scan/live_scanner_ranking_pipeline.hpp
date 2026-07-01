#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/scan/live_rsi_divergence_engine.hpp"
#include "tradingbot/scan/macd_crossover_signal_scanner.hpp"
#include "tradingbot/scan/rsi_divergence_signal_mapper.hpp"
#include "tradingbot/strategy/scanner_signal_ranker.hpp"

#include <vector>

namespace tradingbot::scan {

struct LiveScannerRankingConfig {
    RsiDivergenceSignalConfig rsi_signals;
    MacdCrossoverSignalConfig macd;
    strategy::ScannerSignalRankConfig ranking;
};

struct LiveScannerRankingResult {
    std::vector<ProvisionalDivergenceResult> rsi_results;
    std::vector<core::StrategySignal> signals;
    std::vector<strategy::ScannerCandidateRank> ranked_candidates;
};

class LiveScannerRankingPipeline {
public:
    explicit LiveScannerRankingPipeline(LiveScannerRankingConfig config = {});

    LiveScannerRankingResult rank(const std::vector<ProvisionalScanInput>& inputs,
                                  const LiveRsiDivergenceEngine& rsi_engine,
                                  core::TimePoint ranked_at) const;

private:
    LiveScannerRankingConfig config_;
};

}  // namespace tradingbot::scan
