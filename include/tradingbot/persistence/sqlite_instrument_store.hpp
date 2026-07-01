#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::persistence {

class SqliteInstrumentStore {
public:
    explicit SqliteInstrumentStore(std::shared_ptr<SqliteDatabase> database);

    void upsert(const core::Instrument& instrument, core::TimePoint updated_at = core::Clock::now());
    void upsert_all(const std::vector<core::Instrument>& instruments,
                    core::TimePoint updated_at = core::Clock::now());
    std::vector<core::Instrument> load_all() const;
    std::optional<core::Instrument> find_by_key(const core::InstrumentKey& key) const;

private:
    std::shared_ptr<SqliteDatabase> database_;
};

}  // namespace tradingbot::persistence
