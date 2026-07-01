#include "tradingbot/persistence/order_history_mapper.hpp"

namespace tradingbot::persistence {

std::string stored_order_side_name(core::OrderSide side) {
    return side == core::OrderSide::Buy ? "buy" : "sell";
}

std::string stored_order_status_name(core::OrderStatus status) {
    switch (status) {
        case core::OrderStatus::Pending:
            return "pending";
        case core::OrderStatus::Accepted:
            return "accepted";
        case core::OrderStatus::Rejected:
            return "rejected";
        case core::OrderStatus::PartiallyFilled:
            return "partially_filled";
        case core::OrderStatus::Filled:
            return "filled";
        case core::OrderStatus::Cancelled:
            return "cancelled";
        case core::OrderStatus::TimedOut:
            return "timed_out";
    }
    return "unknown";
}

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

StoredOrderRow map_order_record_to_stored_row(const core::OrderRecord& order) {
    return {
        .broker_order_id = order.broker_order_id,
        .run_id = order.request.run_id,
        .instrument_key = order.request.instrument_key.value,
        .side = stored_order_side_name(order.request.side),
        .quantity = order.request.quantity,
        .price = order.request.price,
        .status = stored_order_status_name(order.status),
        .rejection_reason = order.rejection_reason,
        .filled_quantity = order.filled_quantity,
        .average_fill_price = order.average_fill_price,
        .source_strategy = order.request.source_strategy,
        .tag = order.request.tag,
        .updated_at = order.updated_at,
    };
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
