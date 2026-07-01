#include "tradingbot/persistence/sqlite_database.hpp"

#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct sqlite3_stmt;

namespace tradingbot::persistence {
namespace {

constexpr int kSqliteOk = 0;
constexpr int kSqliteRow = 100;
constexpr int kSqliteDone = 101;

using SqliteExecCallback = int (*)(void*, int, char**, char**);

struct SqliteApi {
    using OpenFn = int (*)(const char*, sqlite3**);
    using CloseFn = int (*)(sqlite3*);
    using ErrmsgFn = const char* (*)(sqlite3*);
    using ExecFn = int (*)(sqlite3*, const char*, SqliteExecCallback, void*, char**);
    using FreeFn = void (*)(void*);
    using PrepareFn = int (*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    using StepFn = int (*)(sqlite3_stmt*);
    using ColumnIntFn = int (*)(sqlite3_stmt*, int);
    using FinalizeFn = int (*)(sqlite3_stmt*);

    OpenFn open{nullptr};
    CloseFn close{nullptr};
    ErrmsgFn errmsg{nullptr};
    ExecFn exec{nullptr};
    FreeFn free{nullptr};
    PrepareFn prepare_v2{nullptr};
    StepFn step{nullptr};
    ColumnIntFn column_int{nullptr};
    FinalizeFn finalize{nullptr};
    std::string error;
};

#ifdef _WIN32
template <typename Fn>
bool load_symbol(HMODULE module, const char* name, Fn& target, std::string& error) {
    target = reinterpret_cast<Fn>(GetProcAddress(module, name));
    if (target == nullptr) {
        error = std::string{"missing SQLite symbol: "} + name;
        return false;
    }
    return true;
}

SqliteApi& sqlite_api() {
    static SqliteApi api = [] {
        SqliteApi loaded;
        HMODULE module = LoadLibraryA("sqlite3.dll");
        if (module == nullptr) {
            module = LoadLibraryA("libsqlite3-0.dll");
        }
        if (module == nullptr) {
            loaded.error = "could not load SQLite runtime DLL";
            return loaded;
        }

        if (!load_symbol(module, "sqlite3_open", loaded.open, loaded.error) ||
            !load_symbol(module, "sqlite3_close", loaded.close, loaded.error) ||
            !load_symbol(module, "sqlite3_errmsg", loaded.errmsg, loaded.error) ||
            !load_symbol(module, "sqlite3_exec", loaded.exec, loaded.error) ||
            !load_symbol(module, "sqlite3_free", loaded.free, loaded.error) ||
            !load_symbol(module, "sqlite3_prepare_v2", loaded.prepare_v2, loaded.error) ||
            !load_symbol(module, "sqlite3_step", loaded.step, loaded.error) ||
            !load_symbol(module, "sqlite3_column_int", loaded.column_int, loaded.error) ||
            !load_symbol(module, "sqlite3_finalize", loaded.finalize, loaded.error)) {
            return loaded;
        }
        return loaded;
    }();
    return api;
}
#else
SqliteApi& sqlite_api() {
    static SqliteApi api{.error = "SQLite dynamic loading is only implemented for Windows builds"};
    return api;
}
#endif

std::string quote_sql_text(const std::string& value) {
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

}  // namespace

SqliteDatabase::SqliteDatabase(std::string path) {
    const auto& sqlite = sqlite_api();
    if (sqlite.open == nullptr) {
        error_ = sqlite.error;
        return;
    }

    if (sqlite.open(path.c_str(), &db_) != kSqliteOk) {
        error_ = db_ != nullptr ? sqlite.errmsg(db_) : "could not open SQLite database";
        if (db_ != nullptr) {
            sqlite.close(db_);
            db_ = nullptr;
        }
    }
}

SqliteDatabase::~SqliteDatabase() {
    if (db_ != nullptr) {
        sqlite_api().close(db_);
    }
}

SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept : db_(other.db_), error_(std::move(other.error_)) {
    other.db_ = nullptr;
}

SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept {
    if (this != &other) {
        if (db_ != nullptr) {
            sqlite_api().close(db_);
        }
        db_ = other.db_;
        error_ = std::move(other.error_);
        other.db_ = nullptr;
    }
    return *this;
}

bool SqliteDatabase::ok() const {
    return db_ != nullptr;
}

const std::string& SqliteDatabase::error() const {
    return error_;
}

bool SqliteDatabase::exec(const std::string& sql) {
    if (db_ == nullptr) {
        error_ = "SQLite database is not open";
        return false;
    }

    const auto& sqlite = sqlite_api();
    char* error_message = nullptr;
    const auto result = sqlite.exec(db_, sql.c_str(), nullptr, nullptr, &error_message);
    if (result != kSqliteOk) {
        error_ = error_message != nullptr ? error_message : sqlite.errmsg(db_);
        sqlite.free(error_message);
        return false;
    }
    error_.clear();
    return true;
}

std::vector<int> SqliteDatabase::query_ints(const std::string& sql) const {
    if (db_ == nullptr) {
        throw std::runtime_error("SQLite database is not open");
    }

    const auto& sqlite = sqlite_api();
    sqlite3_stmt* statement = nullptr;
    if (sqlite.prepare_v2(db_, sql.c_str(), -1, &statement, nullptr) != kSqliteOk) {
        throw std::runtime_error(sqlite.errmsg(db_));
    }

    std::vector<int> values;
    while (true) {
        const auto result = sqlite.step(statement);
        if (result == kSqliteRow) {
            values.push_back(sqlite.column_int(statement, 0));
            continue;
        }
        if (result == kSqliteDone) {
            break;
        }
        const std::string error = sqlite.errmsg(db_);
        sqlite.finalize(statement);
        throw std::runtime_error(error);
    }

    sqlite.finalize(statement);
    return values;
}

bool SqliteDatabase::table_exists(const std::string& table_name) const {
    const auto values = query_ints("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = " +
                                  quote_sql_text(table_name) + ";");
    return !values.empty() && values.front() > 0;
}

SqliteMigrationStore::SqliteMigrationStore(std::shared_ptr<SqliteDatabase> database) : database_(std::move(database)) {
    if (!database_ || !database_->ok()) {
        throw std::runtime_error("SQLite migration store requires an open database");
    }
    if (!database_->exec("CREATE TABLE IF NOT EXISTS schema_migrations ("
                         "version INTEGER PRIMARY KEY, "
                         "name TEXT NOT NULL);")) {
        throw std::runtime_error(database_->error());
    }
}

std::vector<int> SqliteMigrationStore::applied_versions() const {
    return database_->query_ints("SELECT version FROM schema_migrations ORDER BY version;");
}

void SqliteMigrationStore::apply(const SqliteMigration& migration) {
    if (!database_->exec("BEGIN IMMEDIATE;")) {
        throw std::runtime_error(database_->error());
    }

    if (!database_->exec(migration.sql) ||
        !database_->exec("INSERT OR IGNORE INTO schema_migrations(version, name) VALUES(" +
                         std::to_string(migration.version) + ", " + quote_sql_text(migration.name) + ");")) {
        const auto error = database_->error();
        database_->exec("ROLLBACK;");
        throw std::runtime_error(error);
    }

    if (!database_->exec("COMMIT;")) {
        const auto error = database_->error();
        database_->exec("ROLLBACK;");
        throw std::runtime_error(error);
    }
}

}  // namespace tradingbot::persistence
