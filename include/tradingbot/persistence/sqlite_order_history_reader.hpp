#pragma once

#include "tradingbot/app/cli.hpp"
#include "tradingbot/persistence/sqlite_database.hpp"

#include <memory>

namespace tradingbot::persistence {

class SqliteOrderHistoryReader final : public app::OrderHistoryReader {
public:
    explicit SqliteOrderHistoryReader(std::shared_ptr<SqliteDatabase> database);

    app::OrderHistoryLoadResult load_orders() override;

private:
    std::shared_ptr<SqliteDatabase> database_;
};

}  // namespace tradingbot::persistence
