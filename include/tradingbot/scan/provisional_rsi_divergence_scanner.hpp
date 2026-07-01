#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/scan/instrument_partitioner.hpp"
#include "tradingbot/scan/live_candle_aggregator.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace tradingbot::scan {

struct ProvisionalScanInput {
    core::Instrument instrument;
    std::vector<core::Candle> historical_candles;
};

struct ProvisionalDivergenceResult {
    core::InstrumentKey instrument_key;
    std::string symbol;
    bool ok{false};
    bool provisional{false};
    bool bullish_divergence{false};
    bool bearish_divergence{false};
    double latest_close{0.0};
    double latest_rsi{0.0};
    std::size_t candle_count{0};
    std::string diagnostic;
};

struct ProvisionalRsiDivergenceConfig {
    int rsi_period{14};
    int wing_size{1};
    std::size_t worker_count{0};
};

class ProvisionalRsiDivergenceScanner {
public:
    explicit ProvisionalRsiDivergenceScanner(ProvisionalRsiDivergenceConfig config = {});

    ProvisionalDivergenceResult scan_one(const ProvisionalScanInput& input,
                                         const LiveCandleAggregator& aggregator) const;
    ProvisionalDivergenceResult scan_one(const ProvisionalScanInput& input,
                                         const std::optional<core::Candle>& live_candle) const;
    std::vector<ProvisionalDivergenceResult> scan_parallel(const std::vector<ProvisionalScanInput>& inputs,
                                                           const LiveCandleAggregator& aggregator) const;
    std::vector<ProvisionalDivergenceResult> scan_parallel(const std::vector<ProvisionalScanInput>& inputs,
                                                           const PartitionedLiveCandleStore& candle_store) const;

private:
    ProvisionalRsiDivergenceConfig config_;
};

std::vector<std::vector<std::size_t>> partition_scan_inputs(const std::vector<ProvisionalScanInput>& inputs,
                                                            std::size_t partition_count);

}  // namespace tradingbot::scan
