#include "tradingbot/persistence/sqlite_persistence_sink.hpp"

#include "tradingbot/persistence/api_event_mapper.hpp"
#include "tradingbot/persistence/audit_event_mapper.hpp"
#include "tradingbot/persistence/bot_run_mapper.hpp"
#include "tradingbot/persistence/candle_mapper.hpp"
#include "tradingbot/persistence/decision_mapper.hpp"
#include "tradingbot/persistence/order_history_mapper.hpp"
#include "tradingbot/persistence/quote_snapshot_mapper.hpp"
#include "tradingbot/persistence/risk_event_mapper.hpp"
#include "tradingbot/persistence/sqlite_time.hpp"
#include "tradingbot/persistence/strategy_signal_mapper.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace tradingbot::persistence {
namespace {

std::string sql_text(const std::string& value) {
    std::string quoted{"'"};
    for (const auto ch : value) {
        quoted += ch;
        if (ch == '\'') {
            quoted += '\'';
        }
    }
    quoted += '\'';
    return quoted;
}

std::string sql_timestamp(core::TimePoint timestamp) {
    return sql_text(format_sqlite_timestamp(timestamp));
}

std::string sql_optional_money(std::optional<core::Money> value) {
    if (!value) {
        return "NULL";
    }
    std::ostringstream out;
    out << *value;
    return out.str();
}

std::string sql_optional_timestamp(std::optional<core::TimePoint> value) {
    if (!value) {
        return "NULL";
    }
    return sql_timestamp(*value);
}

}  // namespace

SqlitePersistenceSink::SqlitePersistenceSink(std::shared_ptr<SqliteDatabase> database, std::string run_id)
    : database_(std::move(database)), run_id_(std::move(run_id)) {
    if (!database_ || !database_->ok()) {
        throw std::runtime_error("SQLite persistence sink requires an open database");
    }
    if (run_id_.empty()) {
        throw std::runtime_error("SQLite persistence sink requires a run id");
    }
}

void SqlitePersistenceSink::exec_or_throw(const std::string& sql) {
    if (!database_->exec(sql)) {
        throw std::runtime_error(database_->error());
    }
}

void SqlitePersistenceSink::save_bot_run(const core::BotRun& run) {
    const auto row = map_bot_run_to_stored_row(run);
    exec_or_throw("INSERT OR REPLACE INTO bot_runs(run_id, started_at, ended_at, mode, config_hash) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_timestamp(row.started_at) + ", " +
                  sql_optional_timestamp(row.ended_at) + ", " + sql_text(row.mode) + ", " +
                  sql_text(row.config_hash) + ");");
}

void SqlitePersistenceSink::save_order(const core::OrderRecord& order) {
    const auto row = map_order_record_to_stored_row(order);
    exec_or_throw("INSERT OR REPLACE INTO orders("
                  "broker_order_id, run_id, instrument_key, side, quantity, price, status, rejection_reason, "
                  "filled_quantity, average_fill_price, source_strategy, tag, updated_at) VALUES(" +
                  sql_text(row.broker_order_id) + ", " + sql_text(row.run_id) + ", " +
                  sql_text(row.instrument_key) + ", " + sql_text(row.side) + ", " +
                  std::to_string(row.quantity) + ", " + std::to_string(row.price) + ", " +
                  sql_text(row.status) + ", " + sql_text(row.rejection_reason) + ", " +
                  std::to_string(row.filled_quantity) + ", " + sql_optional_money(row.average_fill_price) + ", " +
                  sql_text(row.source_strategy) + ", " + sql_text(row.tag) + ", " + sql_timestamp(row.updated_at) +
                  ");");
}

void SqlitePersistenceSink::save_risk_event(const core::RiskEvent& event) {
    const auto row = map_risk_event_to_stored_row(event, run_id_);
    exec_or_throw("INSERT INTO risk_events(run_id, instrument_key, decision, reason_code, detail, created_at) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.instrument_key) + ", " + sql_text(row.decision) + ", " +
                  sql_text(row.reason_code) + ", " + sql_text(row.detail) + ", " + sql_timestamp(row.created_at) +
                  ");");
}

void SqlitePersistenceSink::save_audit_event(const AuditEvent& event) {
    const auto row = map_audit_event_to_stored_row(event);
    exec_or_throw("INSERT INTO audit_events(run_id, category, message, metadata, created_at) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.category) + ", " + sql_text(row.message) + ", " +
                  sql_text(row.metadata) + ", " + sql_timestamp(row.created_at) + ");");
}

void SqlitePersistenceSink::save_strategy_signal(const core::StrategySignal& signal) {
    const auto row = map_strategy_signal_to_stored_row(signal, run_id_);
    exec_or_throw("INSERT INTO strategy_signals("
                  "run_id, instrument_key, action, confidence, suggested_quantity, suggested_entry_price, "
                  "suggested_target_price, suggested_stop_loss, strategy_name, reason, created_at) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.instrument_key) + ", " + sql_text(row.action) + ", " +
                  std::to_string(row.confidence) + ", " + std::to_string(row.suggested_quantity) + ", " +
                  sql_optional_money(row.suggested_entry_price) + ", " +
                  sql_optional_money(row.suggested_target_price) + ", " + sql_optional_money(row.suggested_stop_loss) +
                  ", " + sql_text(row.strategy_name) + ", " + sql_text(row.reason) + ", " +
                  sql_timestamp(row.created_at) + ");");
}

void SqlitePersistenceSink::save_decision(const core::Decision& decision) {
    const auto row = map_decision_to_stored_row(decision, run_id_);
    exec_or_throw("INSERT INTO decisions("
                  "run_id, instrument_key, decision_type, confidence, quantity, price, reason, source, created_at) "
                  "VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.instrument_key) + ", " +
                  sql_text(row.decision_type) + ", " + std::to_string(row.confidence) + ", " +
                  std::to_string(row.quantity) + ", " + sql_optional_money(row.price) + ", " +
                  sql_text(row.reason) + ", " + sql_text(row.source) + ", " + sql_timestamp(row.created_at) + ");");
}

void SqlitePersistenceSink::save_quote_snapshot(const core::QuoteSnapshot& quote) {
    const auto row = map_quote_snapshot_to_stored_row(quote, run_id_);
    exec_or_throw("INSERT INTO quote_snapshots(run_id, instrument_key, ltp, stale, captured_at) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.instrument_key) + ", " + std::to_string(row.ltp) +
                  ", " + std::to_string(row.stale) + ", " + sql_timestamp(row.captured_at) + ");");
}

void SqlitePersistenceSink::save_candle(const core::Candle& candle) {
    const auto row = map_candle_to_stored_row(candle, run_id_);
    exec_or_throw("INSERT OR REPLACE INTO candles("
                  "run_id, instrument_key, interval, candle_at, open, high, low, close, volume) VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.instrument_key) + ", " + sql_text(row.interval) + ", " +
                  sql_timestamp(row.candle_at) + ", " + std::to_string(row.open) + ", " + std::to_string(row.high) +
                  ", " + std::to_string(row.low) + ", " + std::to_string(row.close) + ", " +
                  std::to_string(row.volume) + ");");
}

void SqlitePersistenceSink::save_api_event(const infra::ApiEvent& event) {
    const auto row = map_api_event_to_stored_row(event, run_id_, core::Clock::now());
    exec_or_throw("INSERT INTO api_events("
                  "run_id, method, url, status_code, attempt_count, retried, redacted_request_metadata, created_at) "
                  "VALUES(" +
                  sql_text(row.run_id) + ", " + sql_text(row.method) + ", " + sql_text(row.url) + ", " +
                  std::to_string(row.status_code) + ", " + std::to_string(row.attempt_count) + ", " +
                  std::to_string(row.retried) + ", " + sql_text(row.redacted_request_metadata) + ", " +
                  sql_timestamp(row.created_at) + ");");
}

}  // namespace tradingbot::persistence
