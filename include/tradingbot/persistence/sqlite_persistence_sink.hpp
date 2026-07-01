#pragma once

#include "tradingbot/persistence/persistence_worker.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"

#include <memory>
#include <string>

namespace tradingbot::persistence {

class SqlitePersistenceSink final : public PersistenceSink {
public:
    SqlitePersistenceSink(std::shared_ptr<SqliteDatabase> database, std::string run_id);

    void save_bot_run(const core::BotRun& run);
    void save_order(const core::OrderRecord& order) override;
    void save_risk_event(const core::RiskEvent& event) override;
    void save_audit_event(const AuditEvent& event) override;
    void save_strategy_signal(const core::StrategySignal& signal) override;
    void save_decision(const core::Decision& decision) override;
    void save_quote_snapshot(const core::QuoteSnapshot& quote) override;
    void save_candle(const core::Candle& candle) override;
    void save_api_event(const infra::ApiEvent& event) override;

private:
    void exec_or_throw(const std::string& sql);

    std::shared_ptr<SqliteDatabase> database_;
    std::string run_id_;
};

}  // namespace tradingbot::persistence
