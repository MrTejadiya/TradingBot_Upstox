#pragma once

#include <string>
#include <vector>

namespace tradingbot::persistence {

struct SqliteMigration {
    int version{0};
    std::string name;
    std::string sql;
};

class MigrationStore {
public:
    virtual ~MigrationStore() = default;
    virtual std::vector<int> applied_versions() const = 0;
    virtual void apply(const SqliteMigration& migration) = 0;
};

class InMemoryMigrationStore final : public MigrationStore {
public:
    std::vector<int> applied_versions() const override;
    void apply(const SqliteMigration& migration) override;
    const std::vector<SqliteMigration>& applied_migrations() const;

private:
    std::vector<SqliteMigration> applied_;
};

const std::vector<SqliteMigration>& sqlite_migrations();
std::vector<SqliteMigration> pending_migrations(const std::vector<int>& applied_versions);
int apply_pending_migrations(MigrationStore& store);

}  // namespace tradingbot::persistence

