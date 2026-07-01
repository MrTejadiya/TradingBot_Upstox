#include "tradingbot/persistence/order_history_mapper.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::persistence::StoredOrderRow stored_row() {
    return {
        .broker_order_id = "ORDER-1",
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .side = "buy",
        .quantity = 3,
        .price = 100.5,
        .status = "accepted",
        .source_strategy = "manual_buy",
        .tag = "dry-run",
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

tradingbot::core::OrderRecord order_record() {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = tradingbot::core::OrderSide::Sell,
                .quantity = 3,
                .price = 100.5,
                .tag = "dry-run",
                .source_strategy = "manual_sell",
                .run_id = "run-1",
            },
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Filled,
        .rejection_reason = "",
        .filled_quantity = 3,
        .average_fill_price = 101.25,
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void maps_accepted_buy_order() {
    const auto result = tradingbot::persistence::map_stored_order_row(stored_row());

    require(result.ok, "valid stored row should map");
    require(result.order.broker_order_id == "ORDER-1", "broker id should map");
    require(result.order.request.run_id == "run-1", "run id should map");
    require(result.order.request.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(result.order.request.side == tradingbot::core::OrderSide::Buy, "buy side should map");
    require(result.order.request.quantity == 3, "quantity should map");
    require(result.order.request.price == 100.5, "price should map");
    require(result.order.status == tradingbot::core::OrderStatus::Accepted, "accepted status should map");
    require(result.order.request.source_strategy == "manual_buy", "source strategy should map");
    require(result.order.request.tag == "dry-run", "tag should map");
}

void maps_order_record_to_stored_row() {
    const auto row = tradingbot::persistence::map_order_record_to_stored_row(order_record());

    require(row.broker_order_id == "ORDER-1", "broker id should store");
    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.side == "sell", "side should store with stable vocabulary");
    require(row.quantity == 3, "quantity should store");
    require(row.price == 100.5, "price should store");
    require(row.status == "filled", "status should store with stable vocabulary");
    require(row.filled_quantity == 3, "filled quantity should store");
    require(row.average_fill_price && *row.average_fill_price == 101.25, "average fill price should store");
    require(row.source_strategy == "manual_sell", "source strategy should store");
    require(row.tag == "dry-run", "tag should store");
    require(row.updated_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
}

void round_trips_order_record_through_stored_row() {
    const auto row = tradingbot::persistence::map_order_record_to_stored_row(order_record());
    const auto result = tradingbot::persistence::map_stored_order_row(row);

    require(result.ok, "stored row from order should map back");
    require(result.order.broker_order_id == "ORDER-1", "broker id should round trip");
    require(result.order.request.side == tradingbot::core::OrderSide::Sell, "side should round trip");
    require(result.order.status == tradingbot::core::OrderStatus::Filled, "status should round trip");
    require(result.order.filled_quantity == 3, "filled quantity should round trip");
    require(result.order.average_fill_price && *result.order.average_fill_price == 101.25,
            "average fill price should round trip");
    require(result.order.request.source_strategy == "manual_sell", "source strategy should round trip");
    require(result.order.request.tag == "dry-run", "tag should round trip");
    require(result.order.updated_at == tradingbot::core::TimePoint{std::chrono::seconds{1}},
            "timestamp should round trip");
}

void maps_rejected_sell_order() {
    auto row = stored_row();
    row.side = "sell";
    row.status = "rejected";
    row.rejection_reason = "risk gate rejected";

    const auto result = tradingbot::persistence::map_stored_order_row(row);

    require(result.ok, "valid rejected sell row should map");
    require(result.order.request.side == tradingbot::core::OrderSide::Sell, "sell side should map");
    require(result.order.status == tradingbot::core::OrderStatus::Rejected, "rejected status should map");
    require(result.order.rejection_reason == "risk gate rejected", "rejection reason should map");
}

void maps_filled_order_with_average_price() {
    auto row = stored_row();
    row.status = "filled";
    row.filled_quantity = 3;
    row.average_fill_price = 101.25;

    const auto result = tradingbot::persistence::map_stored_order_row(row);

    require(result.ok, "valid filled row should map");
    require(result.order.status == tradingbot::core::OrderStatus::Filled, "filled status should map");
    require(result.order.filled_quantity == 3, "filled quantity should map");
    require(result.order.average_fill_price && *result.order.average_fill_price == 101.25,
            "average fill price should map");
}

void parses_all_stored_statuses() {
    const tradingbot::core::OrderStatus statuses[]{
        tradingbot::core::OrderStatus::Pending,         tradingbot::core::OrderStatus::Accepted,
        tradingbot::core::OrderStatus::Rejected,        tradingbot::core::OrderStatus::PartiallyFilled,
        tradingbot::core::OrderStatus::Filled,          tradingbot::core::OrderStatus::Cancelled,
        tradingbot::core::OrderStatus::TimedOut,
    };
    for (const auto status : statuses) {
        const auto stored = tradingbot::persistence::stored_order_status_name(status);
        const auto parsed = tradingbot::persistence::parse_stored_order_status(stored);
        require(parsed && *parsed == status, "stored status should round trip: " + stored);
    }
}

void parses_all_stored_sides() {
    require(tradingbot::persistence::stored_order_side_name(tradingbot::core::OrderSide::Buy) == "buy",
            "buy side should serialize");
    require(tradingbot::persistence::stored_order_side_name(tradingbot::core::OrderSide::Sell) == "sell",
            "sell side should serialize");
    require(tradingbot::persistence::parse_stored_order_side("buy") == tradingbot::core::OrderSide::Buy,
            "buy side should parse");
    require(tradingbot::persistence::parse_stored_order_side("sell") == tradingbot::core::OrderSide::Sell,
            "sell side should parse");
}

void invalid_stored_values_fail_closed() {
    auto bad_side = stored_row();
    bad_side.side = "hold";
    const auto side_result = tradingbot::persistence::map_stored_order_row(bad_side);
    require(!side_result.ok, "invalid side should fail");
    require(side_result.error.find("invalid side") != std::string::npos, "invalid side error should be clear");

    auto bad_status = stored_row();
    bad_status.status = "mystery";
    const auto status_result = tradingbot::persistence::map_stored_order_row(bad_status);
    require(!status_result.ok, "invalid status should fail");
    require(status_result.error.find("invalid status") != std::string::npos, "invalid status error should be clear");
}

}  // namespace

int main() {
    maps_accepted_buy_order();
    maps_order_record_to_stored_row();
    round_trips_order_record_through_stored_row();
    maps_rejected_sell_order();
    maps_filled_order_with_average_price();
    parses_all_stored_statuses();
    parses_all_stored_sides();
    invalid_stored_values_fail_closed();
    return 0;
}
