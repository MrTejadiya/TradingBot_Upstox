#include "tradingbot/persistence/sqlite_migrations.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void migrations_are_ordered_and_named() {
    const auto& migrations = tradingbot::persistence::sqlite_migrations();

    require(migrations.size() == 4, "four baseline migrations should exist");
    require(migrations[0].version == 1, "first migration should be version 1");
    require(migrations[1].version == 2, "second migration should be version 2");
    require(migrations[2].version == 3, "third migration should be version 3");
    require(migrations[3].version == 4, "fourth migration should be version 4");
    require(migrations[0].name == "create_core_tables", "first migration name should be stable");
    require(migrations[3].name == "expand_audit_tables", "fourth migration name should be stable");
}

void core_schema_contains_required_tables() {
    const auto& sql = tradingbot::persistence::sqlite_migrations().front().sql;

    require(sql.find("CREATE TABLE IF NOT EXISTS bot_runs") != std::string::npos, "bot_runs table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS orders") != std::string::npos, "orders table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS risk_events") != std::string::npos, "risk_events table should exist");
    require(sql.find("broker_order_id TEXT PRIMARY KEY") != std::string::npos, "orders should key by broker id");
}

void expanded_schema_contains_audit_tables() {
    const auto& sql = tradingbot::persistence::sqlite_migrations().back().sql;

    require(sql.find("CREATE TABLE IF NOT EXISTS instruments") != std::string::npos, "instruments table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS quote_snapshots") != std::string::npos,
            "quote snapshots table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS candles") != std::string::npos, "candles table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS decisions") != std::string::npos, "decisions table should exist");
    require(sql.find("CREATE TABLE IF NOT EXISTS api_events") != std::string::npos, "api events table should exist");
    require(sql.find("redacted_metadata TEXT") != std::string::npos, "API event metadata should be redacted");
}

void pending_migrations_skip_applied_versions() {
    const auto pending = tradingbot::persistence::pending_migrations({1});

    require(pending.size() == 3, "three migrations should remain after version 1");
    require(pending.front().version == 2, "pending should start at version 2");
}

void applies_pending_migrations_idempotently() {
    tradingbot::persistence::InMemoryMigrationStore store;

    const auto first_count = tradingbot::persistence::apply_pending_migrations(store);
    const auto second_count = tradingbot::persistence::apply_pending_migrations(store);

    require(first_count == 4, "first apply should apply all migrations");
    require(second_count == 0, "second apply should be idempotent");
    require(store.applied_migrations().size() == 4, "store should retain applied migrations");
}

void index_migration_contains_lookup_indexes() {
    const auto& sql = tradingbot::persistence::sqlite_migrations()[2].sql;

    require(sql.find("idx_orders_run_id") != std::string::npos, "orders run index should exist");
    require(sql.find("idx_orders_status") != std::string::npos, "orders status index should exist");
    require(sql.find("idx_audit_events_run_id") != std::string::npos, "audit run index should exist");
}

void expanded_schema_contains_lookup_indexes() {
    const auto& sql = tradingbot::persistence::sqlite_migrations().back().sql;

    require(sql.find("idx_quote_snapshots_run_instrument_time") != std::string::npos,
            "quote snapshot lookup index should exist");
    require(sql.find("idx_candles_instrument_interval_time") != std::string::npos, "candle lookup index should exist");
    require(sql.find("idx_decisions_run_instrument_time") != std::string::npos, "decision lookup index should exist");
    require(sql.find("idx_api_events_run_category_time") != std::string::npos, "API event lookup index should exist");
}

}  // namespace

int main() {
    migrations_are_ordered_and_named();
    core_schema_contains_required_tables();
    expanded_schema_contains_audit_tables();
    pending_migrations_skip_applied_versions();
    applies_pending_migrations_idempotently();
    index_migration_contains_lookup_indexes();
    expanded_schema_contains_lookup_indexes();
    return 0;
}
