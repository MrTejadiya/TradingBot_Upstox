#include "tradingbot/order/dry_run_dispatcher.hpp"
#include "tradingbot/order/order_queue.hpp"
#include "tradingbot/persistence/persistence_worker.hpp"
#include "tradingbot/strategy/buy_strategies.hpp"
#include "tradingbot/strategy/risk_manager.hpp"
#include "tradingbot/strategy/sell_strategies.hpp"
#include "tradingbot/strategy/signal_aggregation.hpp"

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

tradingbot::strategy::StrategyContext strategy_context() {
    return {
        .instrument = {
            .key = {"NSE_EQ|INE002A01018"},
            .symbol = "RELIANCE",
            .enabled = true,
            .quantity = 3,
            .max_position_quantity = 10,
            .manual_buy_price = 101.0,
            .target_profit_pct = 10.0,
        },
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 100.0}},
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 100.0},
        .portfolio = {.available_funds = 10000.0},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

tradingbot::strategy::StrategyContext sell_strategy_context() {
    auto context = strategy_context();
    context.instrument.manual_buy_price = std::nullopt;
    context.instrument.target_profit_pct = 10.0;
    context.candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 112.0}};
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 112.0};
    context.portfolio = {
        .available_funds = 10000.0,
        .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 3, .average_buy_price = 100.0}},
    };
    return context;
}

tradingbot::core::OrderRequest order_from_decision(const tradingbot::core::Decision& decision) {
    return {
        .instrument_key = decision.instrument_key,
        .side = decision.type == tradingbot::core::DecisionType::Sell ? tradingbot::core::OrderSide::Sell
                                                                      : tradingbot::core::OrderSide::Buy,
        .quantity = decision.quantity,
        .price = decision.price.value_or(0.0),
        .tag = "dry-run-integration",
        .source_strategy = decision.source,
        .run_id = "run-1",
    };
}

void persist_dry_run_artifacts(const tradingbot::strategy::StrategyContext& context,
                               const tradingbot::core::StrategySignal& signal,
                               const tradingbot::core::Decision& decision,
                               const tradingbot::core::RiskEvent& risk,
                               const tradingbot::core::OrderRecord& record,
                               const std::shared_ptr<tradingbot::persistence::InMemoryPersistenceSink>& sink) {
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker persistence(sink, migrations);
    require(persistence.start(), "persistence worker should start");
    require(persistence.persist_strategy_signal(signal), "strategy signal should enqueue for persistence");
    require(persistence.persist_decision(decision), "decision should enqueue for persistence");
    require(context.quote && persistence.persist_quote_snapshot(*context.quote),
            "quote snapshot should enqueue for persistence");
    require(persistence.persist_order(record), "dry-run record should enqueue for persistence");
    require(persistence.persist_risk_event(risk), "risk event should enqueue for persistence");
    persistence.drain();

    require(sink->strategy_signals().size() == 1, "strategy signal should persist");
    require(sink->decisions().size() == 1, "decision should persist");
    require(sink->quote_snapshots().size() == 1, "quote snapshot should persist");
    require(sink->orders().size() == 1, "dry-run order should persist");
    require(sink->risk_events().size() == 1, "risk event should persist");
    require(migrations.applied_migrations().size() == tradingbot::persistence::sqlite_migrations().size(),
            "integration should apply migrations");
}

void approved_dry_run_order_is_queued_dispatched_and_persisted() {
    const auto context = strategy_context();
    const auto strategy_result = tradingbot::strategy::ManualBuyStrategy{}.evaluate(context);
    require(strategy_result.signals.size() == 1, "manual buy should emit one dry-run signal");

    const auto aggregate = tradingbot::strategy::aggregate_signals({
        .instrument_key = context.instrument.key,
        .signals = strategy_result.signals,
        .mode = tradingbot::strategy::SignalAggregationMode::HighestConfidence,
        .decided_at = context.evaluated_at,
    });
    require(aggregate.decision.has_value(), "signal should aggregate into decision");

    const auto risk = tradingbot::strategy::RiskManager{}.evaluate({
        .instrument = context.instrument,
        .decision = *aggregate.decision,
        .portfolio = context.portfolio,
        .evaluated_at = context.evaluated_at,
    });
    require(risk.decision == tradingbot::core::RiskDecision::Approved, "risk should approve dry-run decision");

    tradingbot::order::OrderRequestPriorityQueue queue;
    queue.push(order_from_decision(*aggregate.decision), tradingbot::order::OrderPriority::Normal);
    const auto queued = queue.pop();
    require(queued.has_value(), "approved order should pop from queue");

    tradingbot::order::DryRunOrderDispatcher dispatcher;
    const auto dispatch = dispatcher.dispatch(queued->request, context.evaluated_at);
    require(dispatch.accepted, "dry-run dispatcher should accept valid order");
    require(dispatch.record.broker_order_id == "dry-run-1", "dry-run order id should be deterministic");

    auto sink = std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>();
    persist_dry_run_artifacts(context, strategy_result.signals.front(), *aggregate.decision, risk, dispatch.record, sink);
}

void approved_dry_run_sell_order_is_queued_dispatched_and_persisted() {
    const auto context = sell_strategy_context();
    const auto strategy_result = tradingbot::strategy::TargetProfitSellStrategy{}.evaluate(context);
    require(strategy_result.signals.size() == 1, "target sell should emit one dry-run signal");

    const auto aggregate = tradingbot::strategy::aggregate_signals({
        .instrument_key = context.instrument.key,
        .signals = strategy_result.signals,
        .mode = tradingbot::strategy::SignalAggregationMode::HighestConfidence,
        .decided_at = context.evaluated_at,
    });
    require(aggregate.decision.has_value(), "sell signal should aggregate into decision");
    require(aggregate.decision->type == tradingbot::core::DecisionType::Sell, "aggregated decision should be sell");

    const auto risk = tradingbot::strategy::RiskManager{}.evaluate({
        .instrument = context.instrument,
        .decision = *aggregate.decision,
        .portfolio = context.portfolio,
        .evaluated_at = context.evaluated_at,
    });
    require(risk.decision == tradingbot::core::RiskDecision::Approved, "risk should approve dry-run sell decision");

    tradingbot::order::OrderRequestPriorityQueue queue;
    queue.push(order_from_decision(*aggregate.decision), tradingbot::order::OrderPriority::High);
    const auto queued = queue.pop();
    require(queued.has_value(), "approved sell order should pop from queue");
    require(queued->request.side == tradingbot::core::OrderSide::Sell, "queued order should be sell side");

    tradingbot::order::DryRunOrderDispatcher dispatcher;
    const auto dispatch = dispatcher.dispatch(queued->request, context.evaluated_at);
    require(dispatch.accepted, "dry-run dispatcher should accept valid sell order");
    require(dispatch.record.request.side == tradingbot::core::OrderSide::Sell, "dispatched record should be sell side");

    auto sink = std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>();
    persist_dry_run_artifacts(context, strategy_result.signals.front(), *aggregate.decision, risk, dispatch.record, sink);
}

}  // namespace

int main() {
    approved_dry_run_order_is_queued_dispatched_and_persisted();
    approved_dry_run_sell_order_is_queued_dispatched_and_persisted();
    return 0;
}
