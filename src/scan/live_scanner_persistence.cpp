#include "tradingbot/scan/live_scanner_persistence.hpp"

#include "tradingbot/persistence/persistence_worker.hpp"

namespace tradingbot::scan {

LiveScannerPersistenceResult persist_live_scanner_decision_result(
    persistence::PersistenceWorker& persistence,
    const LiveScannerDecisionResult& result) {
    LiveScannerPersistenceResult persisted;
    for (const auto& signal : result.ranking.signals) {
        if (persistence.persist_strategy_signal(signal)) {
            ++persisted.signals_persisted;
        } else {
            ++persisted.enqueue_failures;
        }
    }

    if (result.selection.decision) {
        if (persistence.persist_decision(*result.selection.decision)) {
            persisted.decision_persisted = true;
        } else {
            ++persisted.enqueue_failures;
        }
    }

    return persisted;
}

}  // namespace tradingbot::scan
