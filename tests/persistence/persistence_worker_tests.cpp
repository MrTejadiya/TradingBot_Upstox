#include "tradingbot/persistence/persistence_worker.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

class ThrowingSink final : public tradingbot::persistence::PersistenceSink {
public:
    void save_order(const tradingbot::core::OrderRecord&) override {
        throw std::runtime_error("order write failed");
    }
    void save_risk_event(const tradingbot::core::RiskEvent&) override {}
    void save_audit_event(const tradingbot::persistence::AuditEvent&) override {}
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::OrderRecord order_record() {
    return {
        .request = {.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 1, .price = 100.0, .run_id = "run-1"},
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Accepted,
        .updated_at = tradingbot::core::Clock::now(),
    };
}

tradingbot::core::RiskEvent risk_event() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .decision = tradingbot::core::RiskDecision::Approved,
        .reason_code = "APPROVED",
        .detail = "ok",
        .timestamp = tradingbot::core::Clock::now(),
    };
}

void start_applies_pending_migrations() {
    auto sink = std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>();
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker(sink, migrations);

    require(worker.start(), "worker should start with sink");
    require(migrations.applied_migrations().size() == tradingbot::persistence::sqlite_migrations().size(),
            "worker start should apply migrations");
}

void persists_records_asynchronously() {
    auto sink = std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>();
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker(sink, migrations, 1);
    worker.start();

    require(worker.persist_order(order_record()), "order should enqueue");
    require(worker.persist_risk_event(risk_event()), "risk event should enqueue");
    require(worker.persist_audit_event({
                .run_id = "run-1",
                .category = "test",
                .message = "hello",
                .metadata = "{}",
                .created_at = tradingbot::core::Clock::now(),
            }),
            "audit event should enqueue");
    worker.drain();

    require(sink->orders().size() == 1, "order should persist");
    require(sink->risk_events().size() == 1, "risk event should persist");
    require(sink->audit_events().size() == 1, "audit event should persist");
}

void rejects_writes_before_start() {
    auto sink = std::make_shared<tradingbot::persistence::InMemoryPersistenceSink>();
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker(sink, migrations);

    require(!worker.persist_order(order_record()), "worker should reject writes before start");
}

void captures_sink_errors() {
    auto sink = std::make_shared<ThrowingSink>();
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker(sink, migrations);
    worker.start();

    require(worker.persist_order(order_record()), "throwing order should still enqueue");
    worker.drain();

    require(worker.errors().size() == 1, "sink exception should be captured");
    require(worker.errors().front() == "order write failed", "sink exception message should be retained");
}

void fails_start_without_sink() {
    tradingbot::persistence::InMemoryMigrationStore migrations;
    tradingbot::persistence::PersistenceWorker worker(nullptr, migrations);

    require(!worker.start(), "worker should not start without sink");
}

}  // namespace

int main() {
    start_applies_pending_migrations();
    persists_records_asynchronously();
    rejects_writes_before_start();
    captures_sink_errors();
    fails_start_without_sink();
    return 0;
}

