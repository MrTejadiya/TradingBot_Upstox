#include "tradingbot/persistence/sqlite_database.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

std::filesystem::path test_db_path(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path;
}

void opens_database_and_executes_sql() {
    const auto path = test_db_path("tradingbot_sqlite_database_open_test.sqlite3");
    {
        tradingbot::persistence::SqliteDatabase database(path.string());

        require(database.ok(), "database should open");
        require(database.exec("CREATE TABLE sample(id INTEGER PRIMARY KEY);"), "SQL should execute");
        require(database.table_exists("sample"), "created table should exist");
    }

    std::filesystem::remove(path);
}

void reports_sql_errors() {
    const auto path = test_db_path("tradingbot_sqlite_database_error_test.sqlite3");
    {
        tradingbot::persistence::SqliteDatabase database(path.string());

        require(database.ok(), "database should open");
        require(!database.exec("CREATE TABLE broken("), "invalid SQL should fail");
        require(!database.error().empty(), "invalid SQL should report an error");
    }

    std::filesystem::remove(path);
}

void migration_store_applies_migrations_to_database() {
    const auto path = test_db_path("tradingbot_sqlite_database_migration_test.sqlite3");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(path.string());
        tradingbot::persistence::SqliteMigrationStore store(database);

        auto applied = 0;
        try {
            applied = tradingbot::persistence::apply_pending_migrations(store);
        } catch (const std::exception& ex) {
            std::cerr << "FAILED: migration apply threw: " << ex.what() << "\n";
            std::exit(1);
        }

        require(applied == 4, "all migrations should apply");
        require(store.applied_versions().size() == 4, "migration versions should persist");
        require(database->table_exists("schema_migrations"), "schema migrations table should exist");
        require(database->table_exists("bot_runs"), "bot runs table should exist");
        require(database->table_exists("orders"), "orders table should exist");
        require(database->table_exists("audit_events"), "audit events table should exist");
        require(database->table_exists("instruments"), "instruments table should exist");
        require(database->table_exists("api_events"), "api events table should exist");
    }

    std::filesystem::remove(path);
}

void migration_store_is_idempotent() {
    const auto path = test_db_path("tradingbot_sqlite_database_idempotent_test.sqlite3");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(path.string());
        tradingbot::persistence::SqliteMigrationStore store(database);

        auto first_count = 0;
        auto second_count = 0;
        try {
            first_count = tradingbot::persistence::apply_pending_migrations(store);
            second_count = tradingbot::persistence::apply_pending_migrations(store);
        } catch (const std::exception& ex) {
            std::cerr << "FAILED: idempotent migration apply threw: " << ex.what() << "\n";
            std::exit(1);
        }

        require(first_count == 4, "first apply should apply all migrations");
        require(second_count == 0, "second apply should skip migrations");
        require(store.applied_versions().size() == 4, "migration versions should not duplicate");
    }

    std::filesystem::remove(path);
}

}  // namespace

int main() {
    try {
        opens_database_and_executes_sql();
        reports_sql_errors();
        migration_store_applies_migrations_to_database();
        migration_store_is_idempotent();
    } catch (const std::exception& ex) {
        std::cerr << "FAILED: unhandled exception: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
