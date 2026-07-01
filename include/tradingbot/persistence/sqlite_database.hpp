#pragma once

#include "tradingbot/persistence/sqlite_migrations.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace tradingbot::persistence {

class SqliteDatabase {
public:
    explicit SqliteDatabase(std::string path);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&& other) noexcept;
    SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;

    bool ok() const;
    const std::string& error() const;
    bool exec(const std::string& sql);
    std::vector<int> query_ints(const std::string& sql) const;
    std::vector<std::vector<std::optional<std::string>>> query_rows(const std::string& sql) const;
    bool table_exists(const std::string& table_name) const;

private:
    sqlite3* db_{nullptr};
    std::string error_;
};

class SqliteMigrationStore final : public MigrationStore {
public:
    explicit SqliteMigrationStore(std::shared_ptr<SqliteDatabase> database);

    std::vector<int> applied_versions() const override;
    void apply(const SqliteMigration& migration) override;

private:
    std::shared_ptr<SqliteDatabase> database_;
};

}  // namespace tradingbot::persistence
