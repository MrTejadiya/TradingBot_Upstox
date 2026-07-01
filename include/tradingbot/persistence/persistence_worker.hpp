#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"
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
    virtual void save_strategy_signal(const core::StrategySignal& signal) = 0;
    virtual void save_decision(const core::Decision& decision) = 0;
    virtual void save_quote_snapshot(const core::QuoteSnapshot& quote) = 0;
    virtual void save_candle(const core::Candle& candle) = 0;
    virtual void save_api_event(const infra::ApiEvent& event) = 0;
};

class InMemoryPersistenceSink final : public PersistenceSink {
public:
    void save_order(const core::OrderRecord& order) override;
    void save_risk_event(const core::RiskEvent& event) override;
    void save_audit_event(const AuditEvent& event) override;
    void save_strategy_signal(const core::StrategySignal& signal) override;
    void save_decision(const core::Decision& decision) override;
    void save_quote_snapshot(const core::QuoteSnapshot& quote) override;
    void save_candle(const core::Candle& candle) override;
    void save_api_event(const infra::ApiEvent& event) override;

    const std::vector<core::OrderRecord>& orders() const;
    const std::vector<core::RiskEvent>& risk_events() const;
    const std::vector<AuditEvent>& audit_events() const;
    const std::vector<core::StrategySignal>& strategy_signals() const;
    const std::vector<core::Decision>& decisions() const;
    const std::vector<core::QuoteSnapshot>& quote_snapshots() const;
    const std::vector<core::Candle>& candles() const;
    const std::vector<infra::ApiEvent>& api_events() const;

private:
    std::vector<core::OrderRecord> orders_;
    std::vector<core::RiskEvent> risk_events_;
    std::vector<AuditEvent> audit_events_;
    std::vector<core::StrategySignal> strategy_signals_;
    std::vector<core::Decision> decisions_;
    std::vector<core::QuoteSnapshot> quote_snapshots_;
    std::vector<core::Candle> candles_;
    std::vector<infra::ApiEvent> api_events_;
};

class PersistenceWorker {
public:
    PersistenceWorker(std::shared_ptr<PersistenceSink> sink, MigrationStore& migrations, std::size_t worker_count = 1);

    bool start();
    bool persist_order(core::OrderRecord order);
    bool persist_risk_event(core::RiskEvent event);
    bool persist_audit_event(AuditEvent event);
    bool persist_strategy_signal(core::StrategySignal signal);
    bool persist_decision(core::Decision decision);
    bool persist_quote_snapshot(core::QuoteSnapshot quote);
    bool persist_candle(core::Candle candle);
    bool persist_api_event(infra::ApiEvent event);
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
