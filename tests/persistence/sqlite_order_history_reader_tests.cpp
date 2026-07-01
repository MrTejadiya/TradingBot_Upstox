#include "tradingbot/persistence/sqlite_order_history_reader.hpp"

#include "tradingbot/persistence/sqlite_migrations.hpp"
#include "tradingbot/persistence/sqlite_persistence_sink.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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

std::shared_ptr<tradingbot::persistence::SqliteDatabase> migrated_database(const std::filesystem::path& path) {
    auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(path.string());
    tradingbot::persistence::SqliteMigrationStore migrations(database);
    tradingbot::persistence::apply_pending_migrations(migrations);
    return database;
}

tradingbot::core::TimePoint at(int seconds) {
    return tradingbot::core::TimePoint{std::chrono::seconds{seconds}};
}

tradingbot::core::OrderRecord order(const std::string& id, tradingbot::core::TimePoint updated_at) {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = tradingbot::core::OrderSide::Buy,
                .quantity = 1,
                .price = 1100.0,
                .source_strategy = "manual",
                .run_id = "run-1",
            },
        .broker_order_id = id,
        .status = tradingbot::core::OrderStatus::Accepted,
        .updated_at = updated_at,
    };
}

void loads_orders_newest_first() {
    const auto path = test_db_path("tradingbot_sqlite_order_history_reader_loads.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqlitePersistenceSink sink(database, "run-1");
        sink.save_order(order("ORDER-OLD", at(1)));
        sink.save_order(order("ORDER-NEW", at(2)));

        tradingbot::persistence::SqliteOrderHistoryReader reader(database);
        const auto result = reader.load_orders();

        require(result.ok, "orders should load");
        require(result.orders.size() == 2, "two orders should load");
        require(result.orders[0].broker_order_id == "ORDER-NEW", "newest order should load first");
        require(result.orders[1].broker_order_id == "ORDER-OLD", "older order should load second");
    }
    std::filesystem::remove(path);
}

void loads_nullable_average_fill_price() {
    const auto path = test_db_path("tradingbot_sqlite_order_history_reader_nullable.sqlite3");
    {
        auto database = migrated_database(path);
        auto record = order("ORDER-FILLED", at(1));
        record.average_fill_price = 1101.5;
        record.filled_quantity = 1;
        tradingbot::persistence::SqlitePersistenceSink sink(database, "run-1");
        sink.save_order(record);
        sink.save_order(order("ORDER-OPEN", at(2)));

        tradingbot::persistence::SqliteOrderHistoryReader reader(database);
        const auto result = reader.load_orders();

        require(result.ok, "orders should load");
        require(!result.orders[0].average_fill_price, "null average fill price should load empty");
        require(result.orders[1].average_fill_price && *result.orders[1].average_fill_price == 1101.5,
                "average fill price should load");
    }
    std::filesystem::remove(path);
}

void returns_empty_history_for_empty_table() {
    const auto path = test_db_path("tradingbot_sqlite_order_history_reader_empty.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteOrderHistoryReader reader(database);
        const auto result = reader.load_orders();

        require(result.ok, "empty order history should load");
        require(result.orders.empty(), "empty table should return no orders");
    }
    std::filesystem::remove(path);
}

void malformed_persisted_row_returns_error() {
    const auto path = test_db_path("tradingbot_sqlite_order_history_reader_malformed.sqlite3");
    {
        auto database = migrated_database(path);
        require(database->exec("INSERT INTO orders("
                               "broker_order_id, run_id, instrument_key, side, quantity, price, status, "
                               "filled_quantity, updated_at) VALUES("
                               "'ORDER-BAD', 'run-1', 'NSE_EQ|INE002A01018', 'BAD_SIDE', 1, 1100, "
                               "'ACCEPTED', 0, '1970-01-01T00:00:01Z');"),
                "bad row should insert");

        tradingbot::persistence::SqliteOrderHistoryReader reader(database);
        const auto result = reader.load_orders();

        require(!result.ok, "malformed order history should fail");
        require(result.error.find("invalid side") != std::string::npos, "mapper error should be reported");
    }
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    loads_orders_newest_first();
    loads_nullable_average_fill_price();
    returns_empty_history_for_empty_table();
    malformed_persisted_row_returns_error();
    return 0;
}
