#include "tradingbot/persistence/order_history_mapper.hpp"

namespace tradingbot::persistence {

std::optional<core::OrderSide> parse_stored_order_side(const std::string& value) {
    if (value == "buy") {
        return core::OrderSide::Buy;
    }
    if (value == "sell") {
        return core::OrderSide::Sell;
    }
    return std::nullopt;
}

std::optional<core::OrderStatus> parse_stored_order_status(const std::string& value) {
    if (value == "pending") {
        return core::OrderStatus::Pending;
    }
    if (value == "accepted") {
        return core::OrderStatus::Accepted;
    }
    if (value == "rejected") {
        return core::OrderStatus::Rejected;
    }
    if (value == "partially_filled") {
        return core::OrderStatus::PartiallyFilled;
    }
    if (value == "filled") {
        return core::OrderStatus::Filled;
    }
    if (value == "cancelled") {
        return core::OrderStatus::Cancelled;
    }
    if (value == "timed_out") {
        return core::OrderStatus::TimedOut;
    }
    return std::nullopt;
}

OrderHistoryMapResult map_stored_order_row(const StoredOrderRow& row) {
    const auto side = parse_stored_order_side(row.side);
    if (!side) {
        return {.ok = false, .error = "stored order has invalid side: " + row.side};
    }

    const auto status = parse_stored_order_status(row.status);
    if (!status) {
        return {.ok = false, .error = "stored order has invalid status: " + row.status};
    }

    return {
        .ok = true,
        .order =
            {
                .request =
                    {
                        .instrument_key = {row.instrument_key},
                        .side = *side,
                        .quantity = row.quantity,
                        .price = row.price,
                        .tag = row.tag,
                        .source_strategy = row.source_strategy,
                        .run_id = row.run_id,
                    },
                .broker_order_id = row.broker_order_id,
                .status = *status,
                .rejection_reason = row.rejection_reason,
                .filled_quantity = row.filled_quantity,
                .average_fill_price = row.average_fill_price,
                .updated_at = row.updated_at,
            },
    };
}

}  // namespace tradingbot::persistence
