#include "tradingbot/persistence/sqlite_instrument_store.hpp"

#include "tradingbot/persistence/sqlite_migrations.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

tradingbot::core::Instrument instrument(const std::string& key, const std::string& symbol) {
    return {
        .key = {key},
        .symbol = symbol,
        .exchange = tradingbot::core::Exchange::NseEq,
        .enabled = true,
        .quantity = 2,
        .max_position_quantity = 10,
        .manual_buy_price = 2400.5,
        .manual_target_price = 2700.0,
        .stop_loss_pct = 3.0,
        .target_profit_pct = 10.0,
        .trailing_stop_pct = 2.5,
        .strategy_profile = "delivery",
        .notes = "operator's quoted note",
    };
}

void empty_store_loads_empty_results() {
    const auto path = test_db_path("tradingbot_sqlite_instrument_store_empty.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteInstrumentStore store(database);

        require(store.load_all().empty(), "empty store should load no instruments");
        require(!store.find_by_key({"NSE_EQ|INE002A01018"}), "missing key should return no instrument");
    }
    std::filesystem::remove(path);
}

void upserts_and_loads_instruments_in_key_order() {
    const auto path = test_db_path("tradingbot_sqlite_instrument_store_load.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteInstrumentStore store(database);
        store.upsert(instrument("NSE_EQ|INE467B01029", "TCS"), tradingbot::core::TimePoint{std::chrono::seconds{7}});
        store.upsert(instrument("NSE_EQ|INE002A01018", "RELIANCE"),
                     tradingbot::core::TimePoint{std::chrono::seconds{8}});

        const auto instruments = store.load_all();
        const auto reliance = store.find_by_key({"NSE_EQ|INE002A01018"});

        require(instruments.size() == 2, "both instruments should load");
        require(instruments.front().key.value == "NSE_EQ|INE002A01018", "load_all should sort by key");
        require(instruments.front().symbol == "RELIANCE", "first symbol should load");
        require(reliance.has_value(), "find_by_key should load existing key");
        require(reliance->manual_buy_price && *reliance->manual_buy_price == 2400.5, "manual buy should load");
        require(reliance->manual_target_price && *reliance->manual_target_price == 2700.0,
                "manual target should load");
        require(reliance->stop_loss_pct && *reliance->stop_loss_pct == 3.0, "stop loss should load");
        require(reliance->trailing_stop_pct && *reliance->trailing_stop_pct == 2.5, "trailing stop should load");
        require(reliance->notes == "operator's quoted note", "quoted text should round trip");
    }
    std::filesystem::remove(path);
}

void upsert_replaces_existing_row() {
    const auto path = test_db_path("tradingbot_sqlite_instrument_store_replace.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteInstrumentStore store(database);
        store.upsert(instrument("NSE_EQ|INE002A01018", "RELIANCE"));

        auto updated = instrument("NSE_EQ|INE002A01018", "RELIANCE");
        updated.enabled = false;
        updated.quantity = 5;
        updated.manual_buy_price = std::nullopt;
        updated.notes = "replacement";
        store.upsert(updated);

        const auto instruments = store.load_all();

        require(instruments.size() == 1, "replacement should not duplicate the instrument");
        require(!instruments.front().enabled, "enabled flag should replace");
        require(instruments.front().quantity == 5, "quantity should replace");
        require(!instruments.front().manual_buy_price, "optional manual buy should clear");
        require(instruments.front().notes == "replacement", "notes should replace");
    }
    std::filesystem::remove(path);
}

void upsert_all_rolls_back_when_one_instrument_fails() {
    const auto path = test_db_path("tradingbot_sqlite_instrument_store_batch_rollback.sqlite3");
    {
        auto database = migrated_database(path);
        require(database->exec(
                    "CREATE TRIGGER block_blocked_symbol BEFORE INSERT ON instruments "
                    "WHEN NEW.symbol = 'BLOCKED' BEGIN SELECT RAISE(FAIL, 'blocked instrument'); END;"),
                "blocking trigger should create");
        tradingbot::persistence::SqliteInstrumentStore store(database);

        bool threw = false;
        try {
            store.upsert_all({
                instrument("NSE_EQ|INE002A01018", "RELIANCE"),
                instrument("NSE_EQ|INE467B01029", "BLOCKED"),
            });
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()}.find("blocked instrument") != std::string::npos;
        }

        require(threw, "blocked instrument should fail batch import");
        require(store.load_all().empty(), "failed batch import should roll back earlier instruments");
    }
    std::filesystem::remove(path);
}

void malformed_persisted_row_reports_mapper_error() {
    const auto path = test_db_path("tradingbot_sqlite_instrument_store_malformed.sqlite3");
    {
        auto database = migrated_database(path);
        const auto inserted = database->exec(
            "INSERT INTO instruments(instrument_key, symbol, exchange, enabled, quantity, max_position_quantity, "
            "target_profit_pct, updated_at) VALUES('NSE_EQ|BAD', 'BAD', 'BAD_EXCHANGE', 1, 1, 1, 10.0, "
            "'1970-01-01T00:00:00Z');");
        require(inserted, "malformed fixture row should insert");

        tradingbot::persistence::SqliteInstrumentStore store(database);
        bool threw = false;
        try {
            (void)store.load_all();
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()}.find("invalid exchange") != std::string::npos;
        }

        require(threw, "invalid stored exchange should surface mapper error");
    }
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    empty_store_loads_empty_results();
    upserts_and_loads_instruments_in_key_order();
    upsert_replaces_existing_row();
    upsert_all_rolls_back_when_one_instrument_fails();
    malformed_persisted_row_reports_mapper_error();
    return 0;
}
