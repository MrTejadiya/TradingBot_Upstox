#include "tradingbot/persistence/persistence_worker.hpp"

#include <stdexcept>
#include <utility>

namespace tradingbot::persistence {

void InMemoryPersistenceSink::save_order(const core::OrderRecord& order) {
    orders_.push_back(order);
}

void InMemoryPersistenceSink::save_risk_event(const core::RiskEvent& event) {
    risk_events_.push_back(event);
}

void InMemoryPersistenceSink::save_audit_event(const AuditEvent& event) {
    audit_events_.push_back(event);
}

void InMemoryPersistenceSink::save_strategy_signal(const core::StrategySignal& signal) {
    strategy_signals_.push_back(signal);
}

void InMemoryPersistenceSink::save_decision(const core::Decision& decision) {
    decisions_.push_back(decision);
}

void InMemoryPersistenceSink::save_quote_snapshot(const core::QuoteSnapshot& quote) {
    quote_snapshots_.push_back(quote);
}

void InMemoryPersistenceSink::save_candle(const core::Candle& candle) {
    candles_.push_back(candle);
}

void InMemoryPersistenceSink::save_api_event(const infra::ApiEvent& event) {
    api_events_.push_back(event);
}

const std::vector<core::OrderRecord>& InMemoryPersistenceSink::orders() const {
    return orders_;
}

const std::vector<core::RiskEvent>& InMemoryPersistenceSink::risk_events() const {
    return risk_events_;
}

const std::vector<AuditEvent>& InMemoryPersistenceSink::audit_events() const {
    return audit_events_;
}

const std::vector<core::StrategySignal>& InMemoryPersistenceSink::strategy_signals() const {
    return strategy_signals_;
}

const std::vector<core::Decision>& InMemoryPersistenceSink::decisions() const {
    return decisions_;
}

const std::vector<core::QuoteSnapshot>& InMemoryPersistenceSink::quote_snapshots() const {
    return quote_snapshots_;
}

const std::vector<core::Candle>& InMemoryPersistenceSink::candles() const {
    return candles_;
}

const std::vector<infra::ApiEvent>& InMemoryPersistenceSink::api_events() const {
    return api_events_;
}

PersistenceWorker::PersistenceWorker(std::shared_ptr<PersistenceSink> sink, MigrationStore& migrations,
                                     std::size_t worker_count)
    : sink_(std::move(sink)), migrations_(migrations), workers_(worker_count) {}

bool PersistenceWorker::start() {
    if (!sink_) {
        return false;
    }
    apply_pending_migrations(migrations_);
    started_ = true;
    return true;
}

bool PersistenceWorker::persist_order(core::OrderRecord order) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, order = std::move(order)] {
        sink->save_order(order);
    });
}

bool PersistenceWorker::persist_risk_event(core::RiskEvent event) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, event = std::move(event)] {
        sink->save_risk_event(event);
    });
}

bool PersistenceWorker::persist_audit_event(AuditEvent event) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, event = std::move(event)] {
        sink->save_audit_event(event);
    });
}

bool PersistenceWorker::persist_strategy_signal(core::StrategySignal signal) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, signal = std::move(signal)] {
        sink->save_strategy_signal(signal);
    });
}

bool PersistenceWorker::persist_decision(core::Decision decision) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, decision = std::move(decision)] {
        sink->save_decision(decision);
    });
}

bool PersistenceWorker::persist_quote_snapshot(core::QuoteSnapshot quote) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, quote = std::move(quote)] {
        sink->save_quote_snapshot(quote);
    });
}

bool PersistenceWorker::persist_candle(core::Candle candle) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, candle = std::move(candle)] {
        sink->save_candle(candle);
    });
}

bool PersistenceWorker::persist_api_event(infra::ApiEvent event) {
    if (!started_ || !sink_) {
        return false;
    }
    return workers_.submit([sink = sink_, event = std::move(event)] {
        sink->save_api_event(event);
    });
}

void PersistenceWorker::drain() {
    workers_.drain();
}

void PersistenceWorker::stop() {
    workers_.stop();
}

std::vector<std::string> PersistenceWorker::errors() const {
    return workers_.errors();
}

}  // namespace tradingbot::persistence
