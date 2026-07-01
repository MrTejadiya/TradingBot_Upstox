#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/persistence/sqlite_migrations.hpp"
#include "tradingbot/runtime/worker_group.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tradingbot::persistence {

struct AuditEvent {
    std::string run_id;
    std::string category;
    std::string message;
    std::string metadata;
    core::TimePoint created_at{};
};

class PersistenceSink {
public:
    virtual ~PersistenceSink() = default;
    virtual void save_order(const core::OrderRecord& order) = 0;
    virtual void save_risk_event(const core::RiskEvent& event) = 0;
    virtual void save_audit_event(const AuditEvent& event) = 0;
};

class InMemoryPersistenceSink final : public PersistenceSink {
public:
    void save_order(const core::OrderRecord& order) override;
    void save_risk_event(const core::RiskEvent& event) override;
    void save_audit_event(const AuditEvent& event) override;

    const std::vector<core::OrderRecord>& orders() const;
    const std::vector<core::RiskEvent>& risk_events() const;
    const std::vector<AuditEvent>& audit_events() const;

private:
    std::vector<core::OrderRecord> orders_;
    std::vector<core::RiskEvent> risk_events_;
    std::vector<AuditEvent> audit_events_;
};

class PersistenceWorker {
public:
    PersistenceWorker(std::shared_ptr<PersistenceSink> sink, MigrationStore& migrations, std::size_t worker_count = 1);

    bool start();
    bool persist_order(core::OrderRecord order);
    bool persist_risk_event(core::RiskEvent event);
    bool persist_audit_event(AuditEvent event);
    void drain();
    void stop();
    std::vector<std::string> errors() const;

private:
    std::shared_ptr<PersistenceSink> sink_;
    MigrationStore& migrations_;
    runtime::WorkerGroup workers_;
    bool started_{false};
};

}  // namespace tradingbot::persistence

