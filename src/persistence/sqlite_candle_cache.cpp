#include "tradingbot/persistence/sqlite_candle_cache.hpp"

#include "tradingbot/persistence/candle_mapper.hpp"
#include "tradingbot/persistence/sqlite_persistence_sink.hpp"
#include "tradingbot/persistence/sqlite_time.hpp"

#include <stdexcept>
#include <utility>

namespace tradingbot::persistence {
namespace {

std::string sql_text(const std::string& value) {
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

std::string cache_interval(const infra::CandleQuery& query) {
    return query.unit + ":" + std::to_string(query.interval);
}

std::string required_text(const std::vector<std::optional<std::string>>& row, std::size_t index,
                          const std::string& column) {
    if (index >= row.size() || !row[index]) {
        throw std::runtime_error("candles." + column + " is required");
    }
    return *row[index];
}

core::Money required_money(const std::vector<std::optional<std::string>>& row, std::size_t index,
                           const std::string& column) {
    return std::stod(required_text(row, index, column));
}

core::Quantity required_quantity(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                 const std::string& column) {
    return std::stoll(required_text(row, index, column));
}

core::TimePoint required_timestamp(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                   const std::string& column) {
    const auto parsed = parse_sqlite_timestamp(required_text(row, index, column));
    if (!parsed) {
        throw std::runtime_error("candles." + column + " is not a valid SQLite timestamp");
    }
    return *parsed;
}

StoredCandleRow stored_candle_from_query_row(const std::vector<std::optional<std::string>>& row) {
    return {
        .run_id = required_text(row, 0, "run_id"),
        .instrument_key = required_text(row, 1, "instrument_key"),
        .interval = required_text(row, 2, "interval"),
        .candle_at = required_timestamp(row, 3, "candle_at"),
        .open = required_money(row, 4, "open"),
        .high = required_money(row, 5, "high"),
        .low = required_money(row, 6, "low"),
        .close = required_money(row, 7, "close"),
        .volume = required_quantity(row, 8, "volume"),
    };
}

}  // namespace

SqliteCandleCache::SqliteCandleCache(std::shared_ptr<SqliteDatabase> database, std::string run_id)
    : database_(std::move(database)), run_id_(std::move(run_id)) {
    if (!database_ || !database_->ok()) {
        throw std::runtime_error("SQLite candle cache requires an open database");
    }
    if (run_id_.empty()) {
        throw std::runtime_error("SQLite candle cache requires a run id");
    }
}

std::optional<std::vector<core::Candle>> SqliteCandleCache::get(const infra::CandleQuery& query) {
    const auto rows = database_->query_rows(
        "SELECT run_id, instrument_key, interval, candle_at, open, high, low, close, volume "
        "FROM candles WHERE instrument_key = " +
        sql_text(query.instrument_key.value) + " AND interval = " + sql_text(cache_interval(query)) +
        " ORDER BY candle_at ASC;");

    if (rows.empty()) {
        return std::nullopt;
    }

    std::vector<core::Candle> candles;
    candles.reserve(rows.size());
    for (const auto& row : rows) {
        candles.push_back(map_stored_candle_row(stored_candle_from_query_row(row)));
    }
    return candles;
}

void SqliteCandleCache::put(const infra::CandleQuery&, const std::vector<core::Candle>& candles) {
    SqlitePersistenceSink sink(database_, run_id_);
    for (const auto& candle : candles) {
        sink.save_candle(candle);
    }
}

}  // namespace tradingbot::persistence
