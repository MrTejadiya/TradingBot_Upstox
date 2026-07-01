#include "tradingbot/persistence/persistence_worker.hpp"
#include "tradingbot/scan/live_scanner_persistence.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::TimePoint tp(int seconds = 1) {
    return tradingbot::core::TimePoint{std::chrono::seconds{seconds}};
}

tradingbot::core::StrategySignal signal(const std::string& key, const std::string& strategy_name) {
    return {
        .instrument_key = {key},
        .action = tradingbot::core::TradeAction::Buy,
        .confidence = 0.8,
        .suggested_quantity = 2,
        .suggested_entry_price = 100.0,
        .reason = "scanner candidate",
        .strategy_name = strategy_name,
        .timestamp = tp(),
    };
}

tradingbot::core::Decision decision(const std::string& key) {
    return {
        .instrument_key = {key},
        .type = tradingbot::core::DecisionType::Buy,
        .quantity = 2,
        .price = 100.0,
        .source = "live_scanner_decision_pipeline",
        .timestamp = tp(),
    };
}

struct Harness {
    std::shared_ptr<tradingbot::persistence::InMemoryPersistenceSink> sink{
        std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>()};
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker{sink, migrations};
};

void persists_scanner_signals_and_selected_decision() {
    Harness harness;
    require(harness.worker.start(), "persistence worker should start");

    tradingbot::scan::LiveScannerDecisionResult result;
    result.ranking.signals.push_back(signal("NSE_EQ|ONE", "rsi_divergence"));
    result.ranking.signals.push_back(signal("NSE_EQ|TWO", "macd_bullish_cross"));
    result.selection.decision = decision("NSE_EQ|TWO");

    const auto persisted = tradingbot::scan::persist_live_scanner_decision_result(harness.worker, result);
    harness.worker.drain();

    require(persisted.signals_persisted == 2, "all scanner signals should enqueue");
    require(persisted.decision_persisted, "selected scanner decision should enqueue");
    require(persisted.enqueue_failures == 0, "started worker should not report enqueue failures");
    require(harness.sink->strategy_signals().size() == 2, "scanner signals should persist");
    require(harness.sink->decisions().size() == 1, "scanner decision should persist");
    require(harness.sink->orders().empty(), "scanner persistence must not place orders");
}

void fail_closed_result_persists_signals_without_decision_or_orders() {
    Harness harness;
    require(harness.worker.start(), "persistence worker should start");

    tradingbot::scan::LiveScannerDecisionResult result;
    result.ranking.signals.push_back(signal("NSE_EQ|ONE", "rsi_divergence"));
    result.selection.diagnostics.push_back("top scanner candidate score 0.4 below minimum 0.7");

    const auto persisted = tradingbot::scan::persist_live_scanner_decision_result(harness.worker, result);
    harness.worker.drain();

    require(persisted.signals_persisted == 1, "signals should still persist for review");
    require(!persisted.decision_persisted, "missing selected decision should not persist decision");
    require(persisted.enqueue_failures == 0, "no-decision result should not be an enqueue failure");
    require(harness.sink->strategy_signals().size() == 1, "fail-closed scanner signal should persist");
    require(harness.sink->decisions().empty(), "fail-closed scanner result should persist no decision");
    require(harness.sink->orders().empty(), "fail-closed scanner result must not place orders");
}

void reports_enqueue_failures_when_worker_is_not_started() {
    Harness harness;

    tradingbot::scan::LiveScannerDecisionResult result;
    result.ranking.signals.push_back(signal("NSE_EQ|ONE", "rsi_divergence"));
    result.selection.decision = decision("NSE_EQ|ONE");

    const auto persisted = tradingbot::scan::persist_live_scanner_decision_result(harness.worker, result);

    require(persisted.signals_persisted == 0, "unstarted worker should not enqueue signals");
    require(!persisted.decision_persisted, "unstarted worker should not enqueue decision");
    require(persisted.enqueue_failures == 2, "signal and decision enqueue failures should be counted");
    require(harness.sink->strategy_signals().empty(), "failed enqueue should persist no signals");
    require(harness.sink->decisions().empty(), "failed enqueue should persist no decisions");
    require(harness.sink->orders().empty(), "failed enqueue must not place orders");
}

}  // namespace

int main() {
    persists_scanner_signals_and_selected_decision();
    fail_closed_result_persists_signals_without_decision_or_orders();
    reports_enqueue_failures_when_worker_is_not_started();
    return 0;
}
