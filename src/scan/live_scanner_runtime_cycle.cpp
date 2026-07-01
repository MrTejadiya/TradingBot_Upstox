#include "tradingbot/scan/live_scanner_runtime_cycle.hpp"

#include <utility>

namespace tradingbot::scan {

LiveScannerRuntimeCycleResult run_live_scanner_runtime_cycle(
    const std::vector<ProvisionalScanInput>& inputs,
    const LiveRsiDivergenceEngine& rsi_engine,
    persistence::PersistenceWorker& persistence,
    core::TimePoint decided_at,
    LiveScannerDecisionConfig config) {
    LiveScannerRuntimeCycleResult result;
    result.decision = LiveScannerDecisionPipeline(std::move(config)).decide(inputs, rsi_engine, decided_at);
    result.persistence = persist_live_scanner_decision_result(persistence, result.decision);
    return result;
}

}  // namespace tradingbot::scan
