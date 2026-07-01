#include "tradingbot/scan/live_scanner_decision_pipeline.hpp"

#include <utility>

namespace tradingbot::scan {

LiveScannerDecisionPipeline::LiveScannerDecisionPipeline(LiveScannerDecisionConfig config)
    : config_(std::move(config)) {}

LiveScannerDecisionResult LiveScannerDecisionPipeline::decide(const std::vector<ProvisionalScanInput>& inputs,
                                                              const LiveRsiDivergenceEngine& rsi_engine,
                                                              core::TimePoint decided_at) const {
    const LiveScannerRankingPipeline ranking_pipeline(config_.ranking);
    LiveScannerDecisionResult result;
    result.ranking = ranking_pipeline.rank(inputs, rsi_engine, decided_at);
    result.selection =
        strategy::select_top_ranked_candidate(result.ranking.ranked_candidates, config_.selection, decided_at);
    return result;
}

}  // namespace tradingbot::scan
