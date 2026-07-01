#pragma once

#include "tradingbot/persistence/persistence_worker.hpp"
#include "tradingbot/scan/live_scanner_decision_pipeline.hpp"
#include "tradingbot/scan/live_scanner_persistence.hpp"

#include <vector>

namespace tradingbot::scan {

struct LiveScannerRuntimeCycleResult {
    LiveScannerDecisionResult decision;
    LiveScannerPersistenceResult persistence;
};

LiveScannerRuntimeCycleResult run_live_scanner_runtime_cycle(
    const std::vector<ProvisionalScanInput>& inputs,
    const LiveRsiDivergenceEngine& rsi_engine,
    persistence::PersistenceWorker& persistence,
    core::TimePoint decided_at,
    LiveScannerDecisionConfig config = {});

}  // namespace tradingbot::scan
