#pragma once

#include "tradingbot/scan/live_scanner_ranking_pipeline.hpp"
#include "tradingbot/strategy/scanner_candidate_selector.hpp"

namespace tradingbot::scan {

struct LiveScannerDecisionConfig {
    LiveScannerRankingConfig ranking;
    strategy::ScannerCandidateSelectionConfig selection;
};

struct LiveScannerDecisionResult {
    LiveScannerRankingResult ranking;
    strategy::ScannerCandidateSelectionResult selection;
};

class LiveScannerDecisionPipeline {
public:
    explicit LiveScannerDecisionPipeline(LiveScannerDecisionConfig config = {});

    LiveScannerDecisionResult decide(const std::vector<ProvisionalScanInput>& inputs,
                                     const LiveRsiDivergenceEngine& rsi_engine,
                                     core::TimePoint decided_at) const;

private:
    LiveScannerDecisionConfig config_;
};

}  // namespace tradingbot::scan
