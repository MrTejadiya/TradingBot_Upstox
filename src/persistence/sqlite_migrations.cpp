#include "tradingbot/persistence/sqlite_migrations.hpp"

#include <algorithm>

namespace tradingbot::persistence {
namespace {

const std::vector<SqliteMigration> kMigrations = {
    {
        .version = 1,
        .name = "create_core_tables",
        .sql = R"sql(
CREATE TABLE IF NOT EXISTS bot_runs (
    run_id TEXT PRIMARY KEY,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    mode TEXT NOT NULL,
    config_hash TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS orders (
    broker_order_id TEXT PRIMARY KEY,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    side TEXT NOT NULL,
    quantity INTEGER NOT NULL,
    price REAL NOT NULL,
    status TEXT NOT NULL,
    rejection_reason TEXT,
    filled_quantity INTEGER NOT NULL DEFAULT 0,
    average_fill_price REAL,
    source_strategy TEXT,
    tag TEXT,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS risk_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    decision TEXT NOT NULL,
    reason_code TEXT NOT NULL,
    detail TEXT NOT NULL,
    created_at TEXT NOT NULL
);
)sql",
    },
    {
        .version = 2,
        .name = "create_strategy_and_audit_tables",
        .sql = R"sql(
CREATE TABLE IF NOT EXISTS strategy_signals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    action TEXT NOT NULL,
    confidence REAL NOT NULL,
    suggested_quantity INTEGER NOT NULL,
    strategy_name TEXT NOT NULL,
    reason TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    category TEXT NOT NULL,
    message TEXT NOT NULL,
    metadata TEXT,
    created_at TEXT NOT NULL
);
)sql",
    },
    {
        .version = 3,
        .name = "create_lookup_indexes",
        .sql = R"sql(
CREATE INDEX IF NOT EXISTS idx_orders_run_id ON orders(run_id);
CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);
CREATE INDEX IF NOT EXISTS idx_risk_events_run_id ON risk_events(run_id);
CREATE INDEX IF NOT EXISTS idx_strategy_signals_run_id ON strategy_signals(run_id);
CREATE INDEX IF NOT EXISTS idx_audit_events_run_id ON audit_events(run_id);
)sql",
    },
    {
        .version = 4,
        .name = "expand_audit_tables",
        .sql = R"sql(
CREATE TABLE IF NOT EXISTS instruments (
    instrument_key TEXT PRIMARY KEY,
    symbol TEXT NOT NULL,
    exchange TEXT NOT NULL,
    enabled INTEGER NOT NULL,
    quantity INTEGER NOT NULL,
    max_position_quantity INTEGER NOT NULL,
    manual_buy_price REAL,
    manual_target_price REAL,
    stop_loss_pct REAL,
    target_profit_pct REAL NOT NULL,
    trailing_stop_pct REAL,
    strategy_profile TEXT,
    notes TEXT,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS quote_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    ltp REAL NOT NULL,
    stale INTEGER NOT NULL,
    captured_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS candles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    interval TEXT NOT NULL,
    candle_at TEXT NOT NULL,
    open REAL NOT NULL,
    high REAL NOT NULL,
    low REAL NOT NULL,
    close REAL NOT NULL,
    volume INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS decisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    instrument_key TEXT NOT NULL,
    decision_type TEXT NOT NULL,
    confidence REAL NOT NULL,
    quantity INTEGER NOT NULL,
    price REAL,
    reason TEXT NOT NULL,
    source TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS api_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    method TEXT NOT NULL,
    url TEXT NOT NULL,
    status_code INTEGER,
    attempt_count INTEGER NOT NULL,
    retried INTEGER NOT NULL,
    redacted_request_metadata TEXT,
    created_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_quote_snapshots_run_instrument_time
    ON quote_snapshots(run_id, instrument_key, captured_at);
CREATE UNIQUE INDEX IF NOT EXISTS idx_candles_instrument_interval_time
    ON candles(instrument_key, interval, candle_at);
CREATE INDEX IF NOT EXISTS idx_decisions_run_instrument_time
    ON decisions(run_id, instrument_key, created_at);
CREATE INDEX IF NOT EXISTS idx_api_events_run_method_status_time
    ON api_events(run_id, method, status_code, created_at);
)sql",
    },
};

bool contains_version(const std::vector<int>& versions, int version) {
    return std::find(versions.begin(), versions.end(), version) != versions.end();
}

}  // namespace

std::vector<int> InMemoryMigrationStore::applied_versions() const {
    std::vector<int> versions;
    versions.reserve(applied_.size());
    for (const auto& migration : applied_) {
        versions.push_back(migration.version);
    }
    return versions;
}

void InMemoryMigrationStore::apply(const SqliteMigration& migration) {
    if (!contains_version(applied_versions(), migration.version)) {
        applied_.push_back(migration);
    }
}

const std::vector<SqliteMigration>& InMemoryMigrationStore::applied_migrations() const {
    return applied_;
}

const std::vector<SqliteMigration>& sqlite_migrations() {
    return kMigrations;
}

std::vector<SqliteMigration> pending_migrations(const std::vector<int>& applied_versions) {
    std::vector<SqliteMigration> pending;
    for (const auto& migration : sqlite_migrations()) {
        if (!contains_version(applied_versions, migration.version)) {
            pending.push_back(migration);
        }
    }
    return pending;
}

int apply_pending_migrations(MigrationStore& store) {
    auto count = 0;
    for (const auto& migration : pending_migrations(store.applied_versions())) {
        store.apply(migration);
        ++count;
    }
    return count;
}

}  // namespace tradingbot::persistence
