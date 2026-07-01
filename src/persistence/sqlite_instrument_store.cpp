#include "tradingbot/persistence/sqlite_instrument_store.hpp"

#include "tradingbot/persistence/instrument_mapper.hpp"
#include "tradingbot/persistence/sqlite_time.hpp"

#include <sstream>
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

std::string sql_timestamp(core::TimePoint timestamp) {
    return sql_text(format_sqlite_timestamp(timestamp));
}

std::string sql_optional_number(std::optional<double> value) {
    if (!value) {
        return "NULL";
    }
    std::ostringstream out;
    out << *value;
    return out.str();
}

std::string required_text(const std::vector<std::optional<std::string>>& row, std::size_t index,
                          const std::string& column) {
    if (index >= row.size() || !row[index]) {
        throw std::runtime_error("instruments." + column + " is required");
    }
    return *row[index];
}

std::optional<std::string> optional_text(const std::vector<std::optional<std::string>>& row, std::size_t index) {
    if (index >= row.size()) {
        return std::nullopt;
    }
    return row[index];
}

int required_int(const std::vector<std::optional<std::string>>& row, std::size_t index, const std::string& column) {
    return std::stoi(required_text(row, index, column));
}

core::Quantity required_quantity(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                 const std::string& column) {
    return std::stoll(required_text(row, index, column));
}

core::Percent required_percent(const std::vector<std::optional<std::string>>& row, std::size_t index,
                               const std::string& column) {
    return std::stod(required_text(row, index, column));
}

std::optional<double> optional_number(const std::vector<std::optional<std::string>>& row, std::size_t index) {
    const auto value = optional_text(row, index);
    if (!value) {
        return std::nullopt;
    }
    return std::stod(*value);
}

core::TimePoint required_timestamp(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                   const std::string& column) {
    const auto parsed = parse_sqlite_timestamp(required_text(row, index, column));
    if (!parsed) {
        throw std::runtime_error("instruments." + column + " is not a valid SQLite timestamp");
    }
    return *parsed;
}

StoredInstrumentRow stored_instrument_from_query_row(const std::vector<std::optional<std::string>>& row) {
    return {
        .instrument_key = required_text(row, 0, "instrument_key"),
        .symbol = required_text(row, 1, "symbol"),
        .exchange = required_text(row, 2, "exchange"),
        .enabled = required_int(row, 3, "enabled"),
        .quantity = required_quantity(row, 4, "quantity"),
        .max_position_quantity = required_quantity(row, 5, "max_position_quantity"),
        .manual_buy_price = optional_number(row, 6),
        .manual_target_price = optional_number(row, 7),
        .stop_loss_pct = optional_number(row, 8),
        .target_profit_pct = required_percent(row, 9, "target_profit_pct"),
        .trailing_stop_pct = optional_number(row, 10),
        .strategy_profile = optional_text(row, 11).value_or(""),
        .notes = optional_text(row, 12).value_or(""),
        .updated_at = required_timestamp(row, 13, "updated_at"),
    };
}

std::string instrument_select_sql() {
    return "SELECT instrument_key, symbol, exchange, enabled, quantity, max_position_quantity, "
           "manual_buy_price, manual_target_price, stop_loss_pct, target_profit_pct, trailing_stop_pct, "
           "strategy_profile, notes, updated_at FROM instruments";
}

core::Instrument map_or_throw(const StoredInstrumentRow& row) {
    const auto mapped = map_stored_instrument_row(row);
    if (!mapped.ok) {
        throw std::runtime_error(mapped.error);
    }
    return mapped.instrument;
}

}  // namespace

SqliteInstrumentStore::SqliteInstrumentStore(std::shared_ptr<SqliteDatabase> database)
    : database_(std::move(database)) {
    if (!database_ || !database_->ok()) {
        throw std::runtime_error("SQLite instrument store requires an open database");
    }
}

void SqliteInstrumentStore::upsert(const core::Instrument& instrument, core::TimePoint updated_at) {
    const auto row = map_instrument_to_stored_row(instrument, updated_at);
    const auto sql =
        "INSERT OR REPLACE INTO instruments("
        "instrument_key, symbol, exchange, enabled, quantity, max_position_quantity, manual_buy_price, "
        "manual_target_price, stop_loss_pct, target_profit_pct, trailing_stop_pct, strategy_profile, notes, "
        "updated_at) VALUES(" +
        sql_text(row.instrument_key) + ", " + sql_text(row.symbol) + ", " + sql_text(row.exchange) + ", " +
        std::to_string(row.enabled) + ", " + std::to_string(row.quantity) + ", " +
        std::to_string(row.max_position_quantity) + ", " + sql_optional_number(row.manual_buy_price) + ", " +
        sql_optional_number(row.manual_target_price) + ", " + sql_optional_number(row.stop_loss_pct) + ", " +
        std::to_string(row.target_profit_pct) + ", " + sql_optional_number(row.trailing_stop_pct) + ", " +
        sql_text(row.strategy_profile) + ", " + sql_text(row.notes) + ", " + sql_timestamp(row.updated_at) + ");";

    if (!database_->exec(sql)) {
        throw std::runtime_error(database_->error());
    }
}

void SqliteInstrumentStore::upsert_all(const std::vector<core::Instrument>& instruments, core::TimePoint updated_at) {
    for (const auto& instrument : instruments) {
        upsert(instrument, updated_at);
    }
}

std::vector<core::Instrument> SqliteInstrumentStore::load_all() const {
    const auto rows = database_->query_rows(instrument_select_sql() + " ORDER BY instrument_key ASC;");
    std::vector<core::Instrument> instruments;
    instruments.reserve(rows.size());
    for (const auto& row : rows) {
        instruments.push_back(map_or_throw(stored_instrument_from_query_row(row)));
    }
    return instruments;
}

std::optional<core::Instrument> SqliteInstrumentStore::find_by_key(const core::InstrumentKey& key) const {
    const auto rows = database_->query_rows(instrument_select_sql() + " WHERE instrument_key = " + sql_text(key.value) +
                                            " ORDER BY instrument_key ASC LIMIT 1;");
    if (rows.empty()) {
        return std::nullopt;
    }
    return map_or_throw(stored_instrument_from_query_row(rows.front()));
}

}  // namespace tradingbot::persistence
