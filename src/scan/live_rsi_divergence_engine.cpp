#include "tradingbot/scan/live_rsi_divergence_engine.hpp"

#include "tradingbot/scan/instrument_partitioner.hpp"

namespace tradingbot::scan {

LiveRsiDivergenceEngine::LiveRsiDivergenceEngine(LiveRsiDivergenceEngineConfig config)
    : candle_store_(config.partition_count == 0 ? available_worker_count() : config.partition_count),
      scanner_(config.scanner) {}

std::size_t LiveRsiDivergenceEngine::partition_count() const {
    return candle_store_.partition_count();
}

bool LiveRsiDivergenceEngine::on_quote(const core::QuoteSnapshot& quote) {
    return candle_store_.update(quote);
}

std::vector<ProvisionalDivergenceResult> LiveRsiDivergenceEngine::scan(
    const std::vector<ProvisionalScanInput>& inputs) const {
    return scanner_.scan_parallel(inputs, candle_store_);
}

const PartitionedLiveCandleStore& LiveRsiDivergenceEngine::candle_store() const {
    return candle_store_;
}

}  // namespace tradingbot::scan
