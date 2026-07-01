#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/scan/live_candle_aggregator.hpp"
#include "tradingbot/scan/provisional_rsi_divergence_scanner.hpp"

#include <cstddef>
#include <vector>

namespace tradingbot::scan {

struct LiveRsiDivergenceEngineConfig {
    ProvisionalRsiDivergenceConfig scanner;
    std::size_t partition_count{0};
};

class LiveRsiDivergenceEngine {
public:
    explicit LiveRsiDivergenceEngine(LiveRsiDivergenceEngineConfig config = {});

    std::size_t partition_count() const;
    bool on_quote(const core::QuoteSnapshot& quote);
    std::vector<ProvisionalDivergenceResult> scan(const std::vector<ProvisionalScanInput>& inputs) const;
    const PartitionedLiveCandleStore& candle_store() const;

private:
    PartitionedLiveCandleStore candle_store_;
    ProvisionalRsiDivergenceScanner scanner_;
};

}  // namespace tradingbot::scan
