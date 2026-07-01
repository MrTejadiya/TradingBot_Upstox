#pragma once

#include "tradingbot/infra/candle_service.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"

#include <memory>
#include <string>

namespace tradingbot::persistence {

class SqliteCandleCache final : public infra::CandleCache {
public:
    SqliteCandleCache(std::shared_ptr<SqliteDatabase> database, std::string run_id);

    std::optional<std::vector<core::Candle>> get(const infra::CandleQuery& query) override;
    void put(const infra::CandleQuery& query, const std::vector<core::Candle>& candles) override;

private:
    std::shared_ptr<SqliteDatabase> database_;
    std::string run_id_;
};

}  // namespace tradingbot::persistence
