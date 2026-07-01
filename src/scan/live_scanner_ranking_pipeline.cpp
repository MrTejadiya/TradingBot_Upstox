#include "tradingbot/scan/live_scanner_ranking_pipeline.hpp"

#include <utility>

namespace tradingbot::scan {

LiveScannerRankingPipeline::LiveScannerRankingPipeline(LiveScannerRankingConfig config) : config_(std::move(config)) {}

LiveScannerRankingResult LiveScannerRankingPipeline::rank(const std::vector<ProvisionalScanInput>& inputs,
                                                          const LiveRsiDivergenceEngine& rsi_engine,
                                                          core::TimePoint ranked_at) const {
    LiveScannerRankingResult result;
    result.rsi_results = rsi_engine.scan(inputs);

    result.signals = map_rsi_divergence_signals(inputs, result.rsi_results, ranked_at, config_.rsi_signals);

    const MacdCrossoverSignalScanner macd_scanner(config_.macd);
    auto macd_signals = macd_scanner.scan_many(inputs, rsi_engine.candle_store(), ranked_at);
    result.signals.insert(result.signals.end(), macd_signals.begin(), macd_signals.end());

    result.ranked_candidates = strategy::rank_scanner_signals(result.signals, config_.ranking, ranked_at);
    return result;
}

}  // namespace tradingbot::scan
