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

const std::vector<core::OrderRecord>& InMemoryPersistenceSink::orders() const {
    return orders_;
}

const std::vector<core::RiskEvent>& InMemoryPersistenceSink::risk_events() const {
    return risk_events_;
}

const std::vector<AuditEvent>& InMemoryPersistenceSink::audit_events() const {
    return audit_events_;
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

