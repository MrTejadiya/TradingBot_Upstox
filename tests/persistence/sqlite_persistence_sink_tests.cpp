#include "tradingbot/persistence/sqlite_persistence_sink.hpp"

#include "tradingbot/persistence/sqlite_migrations.hpp"

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

int count_rows(const tradingbot::persistence::SqliteDatabase& database, const std::string& table,
               const std::string& where = "") {
    const auto values = database.query_ints("SELECT COUNT(*) FROM " + table + where + ";");
    return values.empty() ? 0 : values.front();
}

tradingbot::core::TimePoint at(int seconds) {
    return tradingbot::core::TimePoint{std::chrono::seconds{seconds}};
}

tradingbot::core::OrderRecord order() {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = tradingbot::core::OrderSide::Buy,
                .quantity = 1,
                .price = 1100.0,
                .tag = "dry-run",
                .source_strategy = "manual",
                .run_id = "run-1",
            },
        .broker_order_id = "dry-run-1",
        .status = tradingbot::core::OrderStatus::Accepted,
        .filled_quantity = 0,
        .updated_at = at(1),
    };
}

void writes_all_worker_events_to_sqlite() {
    const auto path = test_db_path("tradingbot_sqlite_persistence_sink_test.sqlite3");
    {
        auto database = std::make_shared<tradingbot::persistence::SqliteDatabase>(path.string());
        tradingbot::persistence::SqliteMigrationStore migrations(database);
        tradingbot::persistence::apply_pending_migrations(migrations);
        tradingbot::persistence::SqlitePersistenceSink sink(database, "run-1");

        sink.save_order(order());
        sink.save_risk_event({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .decision = tradingbot::core::RiskDecision::Rejected,
            .reason_code = "max_position",
            .detail = "don't buy",
            .timestamp = at(2),
        });
        sink.save_audit_event({
            .run_id = "run-1",
            .category = "risk",
            .message = "risk rejected",
            .metadata = R"({"reason":"max_position"})",
            .created_at = at(3),
        });
        sink.save_strategy_signal({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .action = tradingbot::core::TradeAction::Buy,
            .confidence = 0.8,
            .suggested_quantity = 1,
            .suggested_entry_price = 1100.0,
            .suggested_target_price = 1200.0,
            .suggested_stop_loss = 1050.0,
            .reason = "manual threshold",
            .strategy_name = "manual",
            .timestamp = at(4),
        });
        sink.save_decision({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .type = tradingbot::core::DecisionType::Buy,
            .confidence = 0.8,
            .quantity = 1,
            .price = 1100.0,
            .reason = "manual threshold",
            .source = "aggregator",
            .timestamp = at(5),
        });
        sink.save_quote_snapshot({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .timestamp = at(6),
            .ltp = 1110.0,
            .stale = false,
        });
        sink.save_candle({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .timestamp = at(7),
            .open = 1000.0,
            .high = 1120.0,
            .low = 990.0,
            .close = 1110.0,
            .volume = 10000,
            .interval = "1minute",
        });
        sink.save_api_event({
            .method = "GET",
            .url = "https://api.upstox.com/v2/user/get-funds-and-margin",
            .status_code = 200,
            .attempt_count = 1,
            .retried = false,
            .redacted_request_metadata = R"({"authorization":"redacted"})",
        });

        require(count_rows(*database, "orders", " WHERE run_id = 'run-1'") == 1, "order should persist");
        require(count_rows(*database, "risk_events", " WHERE run_id = 'run-1'") == 1, "risk event should persist");
        require(count_rows(*database, "audit_events", " WHERE run_id = 'run-1'") == 1, "audit event should persist");
        require(count_rows(*database, "strategy_signals", " WHERE run_id = 'run-1'") == 1,
                "strategy signal should persist");
        require(count_rows(*database, "decisions", " WHERE run_id = 'run-1'") == 1, "decision should persist");
        require(count_rows(*database, "quote_snapshots", " WHERE run_id = 'run-1'") == 1,
                "quote snapshot should persist");
        require(count_rows(*database, "candles", " WHERE run_id = 'run-1'") == 1, "candle should persist");
        require(count_rows(*database, "api_events", " WHERE run_id = 'run-1'") == 1, "API event should persist");
        require(count_rows(*database, "risk_events", " WHERE detail = 'don''t buy'") == 1,
                "SQL text should escape apostrophes");
    }
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    writes_all_worker_events_to_sqlite();
    return 0;
}
