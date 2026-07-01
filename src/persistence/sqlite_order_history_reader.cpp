#include "tradingbot/persistence/sqlite_order_history_reader.hpp"

#include "tradingbot/persistence/order_history_mapper.hpp"
#include "tradingbot/persistence/sqlite_time.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace tradingbot::persistence {
namespace {

std::string required_text(const std::vector<std::optional<std::string>>& row, std::size_t index,
                          const std::string& column) {
    if (index >= row.size() || !row[index]) {
        throw std::runtime_error("orders." + column + " is required");
    }
    return *row[index];
}

core::Quantity required_quantity(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                 const std::string& column) {
    return std::stoll(required_text(row, index, column));
}

core::Money required_money(const std::vector<std::optional<std::string>>& row, std::size_t index,
                           const std::string& column) {
    return std::stod(required_text(row, index, column));
}

std::optional<core::Money> optional_money(const std::vector<std::optional<std::string>>& row, std::size_t index) {
    if (index >= row.size() || !row[index]) {
        return std::nullopt;
    }
    return std::stod(*row[index]);
}

core::TimePoint required_timestamp(const std::vector<std::optional<std::string>>& row, std::size_t index,
                                   const std::string& column) {
    const auto parsed = parse_sqlite_timestamp(required_text(row, index, column));
    if (!parsed) {
        throw std::runtime_error("orders." + column + " is not a valid SQLite timestamp");
    }
    return *parsed;
}

StoredOrderRow stored_order_from_query_row(const std::vector<std::optional<std::string>>& row) {
    return {
        .broker_order_id = required_text(row, 0, "broker_order_id"),
        .run_id = required_text(row, 1, "run_id"),
        .instrument_key = required_text(row, 2, "instrument_key"),
        .side = required_text(row, 3, "side"),
        .quantity = required_quantity(row, 4, "quantity"),
        .price = required_money(row, 5, "price"),
        .status = required_text(row, 6, "status"),
        .rejection_reason = row[7].value_or(""),
        .filled_quantity = required_quantity(row, 8, "filled_quantity"),
        .average_fill_price = optional_money(row, 9),
        .source_strategy = row[10].value_or(""),
        .tag = row[11].value_or(""),
        .updated_at = required_timestamp(row, 12, "updated_at"),
    };
}

}  // namespace

SqliteOrderHistoryReader::SqliteOrderHistoryReader(std::shared_ptr<SqliteDatabase> database)
    : database_(std::move(database)) {}

app::OrderHistoryLoadResult SqliteOrderHistoryReader::load_orders() {
    if (!database_ || !database_->ok()) {
        return {.ok = false, .error = "SQLite order history database is not open"};
    }

    try {
        const auto rows = database_->query_rows(
            "SELECT broker_order_id, run_id, instrument_key, side, quantity, price, status, rejection_reason, "
            "filled_quantity, average_fill_price, source_strategy, tag, updated_at "
            "FROM orders ORDER BY updated_at DESC, broker_order_id DESC;");
        std::vector<core::OrderRecord> orders;
        orders.reserve(rows.size());
        for (const auto& row : rows) {
            const auto mapped = map_stored_order_row(stored_order_from_query_row(row));
            if (!mapped.ok) {
                return {.ok = false, .error = mapped.error};
            }
            orders.push_back(mapped.order);
        }
        return {.ok = true, .orders = std::move(orders)};
    } catch (const std::exception& ex) {
        return {.ok = false, .error = ex.what()};
    }
}

}  // namespace tradingbot::persistence
