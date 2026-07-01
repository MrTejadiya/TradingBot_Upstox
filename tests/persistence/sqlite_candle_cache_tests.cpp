#include "tradingbot/persistence/sqlite_candle_cache.hpp"

#include "tradingbot/persistence/sqlite_migrations.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeTransport final : public tradingbot::infra::HttpTransport {
public:
    std::vector<tradingbot::infra::HttpResponse> responses;
    std::vector<tradingbot::infra::HttpRequest> requests;

    tradingbot::infra::HttpResponse send(const tradingbot::infra::HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            return {.status_code = 500, .body = "missing fake response"};
        }
        auto response = responses.front();
        responses.erase(responses.begin());
        return response;
    }
};

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

tradingbot::infra::CandleQuery query() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .unit = "days",
        .interval = 1,
        .from_date = "2026-06-01",
        .to_date = "2026-06-30",
    };
}

tradingbot::core::Candle candle(int seconds, double close) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{seconds}},
        .open = close - 1.0,
        .high = close + 1.0,
        .low = close - 2.0,
        .close = close,
        .volume = 1000,
        .interval = "days:1",
    };
}

std::string valid_candle_body() {
    return R"json({
        "status": "success",
        "data": {
            "candles": [
                ["2026-06-30T00:00:00+05:30", 100.0, 110.0, 95.0, 105.0, 12345]
            ]
        }
    })json";
}

void misses_empty_cache() {
    const auto path = test_db_path("tradingbot_sqlite_candle_cache_miss.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteCandleCache cache(database, "run-1");

        require(!cache.get(query()), "empty cache should miss");
    }
    std::filesystem::remove(path);
}

void hits_after_put_in_timestamp_order() {
    const auto path = test_db_path("tradingbot_sqlite_candle_cache_hit.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteCandleCache cache(database, "run-1");
        cache.put(query(), {candle(2, 102.0), candle(1, 101.0)});

        const auto cached = cache.get(query());

        require(cached.has_value(), "cache should hit after put");
        require(cached->size() == 2, "both candles should load");
        require(cached->front().timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}},
                "candles should load in timestamp order");
        require(cached->front().close == 101.0, "first close should load");
        require(cached->back().close == 102.0, "second close should load");
    }
    std::filesystem::remove(path);
}

void replaces_duplicate_candle_key() {
    const auto path = test_db_path("tradingbot_sqlite_candle_cache_replace.sqlite3");
    {
        auto database = migrated_database(path);
        tradingbot::persistence::SqliteCandleCache cache(database, "run-1");
        cache.put(query(), {candle(1, 101.0)});
        cache.put(query(), {candle(1, 111.0)});

        const auto cached = cache.get(query());

        require(cached.has_value(), "cache should hit after replacement");
        require(cached->size() == 1, "duplicate candle key should replace");
        require(cached->front().close == 111.0, "replacement close should load");
    }
    std::filesystem::remove(path);
}

void candle_service_uses_sqlite_cache_after_first_fetch() {
    const auto path = test_db_path("tradingbot_sqlite_candle_cache_service.sqlite3");
    {
        auto database = migrated_database(path);
        auto cache = std::make_shared<tradingbot::persistence::SqliteCandleCache>(database, "run-1");
        auto transport = std::make_shared<FakeTransport>();
        transport->responses = {{.status_code = 200, .body = valid_candle_body()}};
        auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api.upstox.com", "token", transport);
        tradingbot::infra::CandleService service(client, cache);

        const auto first = service.fetch_candles(query());
        const auto second = service.fetch_candles(query());

        require(first.ok, "first fetch should succeed");
        require(!first.cache_hit, "first fetch should not be cache hit");
        require(second.ok, "second fetch should succeed");
        require(second.cache_hit, "second fetch should use SQLite cache");
        require(transport->requests.size() == 1, "SQLite cache should avoid second API call");
    }
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    misses_empty_cache();
    hits_after_put_in_timestamp_order();
    replaces_duplicate_candle_key();
    candle_service_uses_sqlite_cache_after_first_fetch();
    return 0;
}
