#pragma once

#include "tradingbot/scan/live_scanner_decision_pipeline.hpp"

#include <cstddef>

namespace tradingbot::persistence {
class PersistenceWorker;
}

namespace tradingbot::scan {

struct LiveScannerPersistenceResult {
    std::size_t signals_persisted{0};
    bool decision_persisted{false};
    std::size_t enqueue_failures{0};
};

LiveScannerPersistenceResult persist_live_scanner_decision_result(
    persistence::PersistenceWorker& persistence,
    const LiveScannerDecisionResult& result);

}  // namespace tradingbot::scan
