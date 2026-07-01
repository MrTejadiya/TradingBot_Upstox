#include "tradingbot/persistence/persistence_worker.hpp"
#include "tradingbot/scan/live_scanner_runtime_cycle.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::TimePoint tp(int days) {
    return tradingbot::core::TimePoint{std::chrono::seconds{days * 86400}};
}

tradingbot::core::Instrument instrument(const std::string& key, const std::string& symbol,
                                        tradingbot::core::Quantity quantity = 2) {
    return {
        .key = {key},
        .symbol = symbol,
        .enabled = true,
        .quantity = quantity,
    };
}

tradingbot::core::QuoteSnapshot quote(const std::string& key, double ltp, int days) {
    return {
        .instrument_key = {key},
        .timestamp = tp(days),
        .ltp = ltp,
    };
}

std::vector<tradingbot::core::Candle> candles_from_closes(const std::string& key, const std::vector<double>& closes) {
    std::vector<tradingbot::core::Candle> candles;
    candles.reserve(closes.size());
    for (auto index = std::size_t{0}; index < closes.size(); ++index) {
        candles.push_back({
            .instrument_key = {key},
            .timestamp = tp(static_cast<int>(index)),
            .open = closes[index],
            .high = closes[index],
            .low = closes[index],
            .close = closes[index],
            .interval = "days:1",
        });
    }
    return candles;
}

tradingbot::scan::LiveRsiDivergenceEngine rsi_engine() {
    return tradingbot::scan::LiveRsiDivergenceEngine({
        .scanner = {.rsi_period = 3, .wing_size = 1, .worker_count = 2},
        .partition_count = 2,
    });
}

tradingbot::scan::LiveScannerDecisionConfig config(double minimum_score = 0.0) {
    return {
        .ranking =
            {
                .macd = {.fast_period = 3, .slow_period = 5, .signal_period = 3},
                .ranking = {.strategy_weights = {{"rsi_divergence", 1.0}, {"macd_bullish_cross", 1.3}}},
            },
        .selection = {.minimum_score = minimum_score, .source = "live_scanner_runtime_cycle"},
    };
}

struct Harness {
    std::shared_ptr<tradingbot::persistence::InMemoryPersistenceSink> sink{
        std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>()};
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker{sink, migrations};
};

void persists_selected_live_scanner_decision() {
    const auto key = std::string{"NSE_EQ|RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "runtime fixture should include live provisional candle");
    Harness harness;
    require(harness.worker.start(), "persistence worker should start");

    const auto result = tradingbot::scan::run_live_scanner_runtime_cycle({
        {.instrument = instrument(key, "RSI", 3),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, harness.worker, tradingbot::core::TimePoint{std::chrono::seconds{17}}, config());
    harness.worker.drain();

    require(result.decision.selection.decision.has_value(), "runtime cycle should produce scanner decision");
    require(result.persistence.signals_persisted == 1, "runtime cycle should enqueue scanner signal");
    require(result.persistence.decision_persisted, "runtime cycle should enqueue scanner decision");
    require(result.persistence.enqueue_failures == 0, "started worker should not report enqueue failures");
    require(harness.sink->strategy_signals().size() == 1, "runtime cycle should persist scanner signal");
    require(harness.sink->decisions().size() == 1, "runtime cycle should persist scanner decision");
    require(harness.sink->orders().empty(), "runtime cycle must not place orders");
}

void persists_fail_closed_signals_without_decision() {
    const auto key = std::string{"NSE_EQ|RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "runtime fixture should include live provisional candle");
    Harness harness;
    require(harness.worker.start(), "persistence worker should start");

    const auto result = tradingbot::scan::run_live_scanner_runtime_cycle({
        {.instrument = instrument(key, "RSI", 3),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, harness.worker, tradingbot::core::TimePoint{}, config(10.0));
    harness.worker.drain();

    require(!result.decision.selection.decision.has_value(), "minimum score should fail closed");
    require(!result.decision.ranking.signals.empty(), "scanner signal should remain reviewable");
    require(result.persistence.signals_persisted == 1, "fail-closed cycle should persist scanner signal");
    require(!result.persistence.decision_persisted, "fail-closed cycle should not persist decision");
    require(harness.sink->strategy_signals().size() == 1, "fail-closed signal should persist");
    require(harness.sink->decisions().empty(), "fail-closed result should persist no decision");
    require(harness.sink->orders().empty(), "fail-closed runtime cycle must not place orders");
}

void reports_enqueue_failures_for_unstarted_worker() {
    const auto key = std::string{"NSE_EQ|RSI"};
    auto engine = rsi_engine();
    require(engine.on_quote(quote(key, 80.0, 11)), "runtime fixture should include live provisional candle");
    Harness harness;

    const auto result = tradingbot::scan::run_live_scanner_runtime_cycle({
        {.instrument = instrument(key, "RSI", 3),
         .historical_candles = candles_from_closes(key, {100, 103, 101, 106, 98, 96, 94, 86, 88, 80, 82})},
    }, engine, harness.worker, tradingbot::core::TimePoint{}, config());

    require(result.decision.selection.decision.has_value(), "decision pipeline should still run before persistence");
    require(result.persistence.signals_persisted == 0, "unstarted worker should enqueue no signals");
    require(!result.persistence.decision_persisted, "unstarted worker should enqueue no decision");
    require(result.persistence.enqueue_failures == 2, "signal and decision enqueue failures should be counted");
    require(harness.sink->orders().empty(), "enqueue failure path must not place orders");
}

}  // namespace

int main() {
    persists_selected_live_scanner_decision();
    persists_fail_closed_signals_without_decision();
    reports_enqueue_failures_for_unstarted_worker();
    return 0;
}
