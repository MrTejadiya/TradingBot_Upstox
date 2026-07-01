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
    require(tradingbot::persistence::parse_stored_order_status("pending").has_value(), "pending should parse");
    require(tradingbot::persistence::parse_stored_order_status("accepted").has_value(), "accepted should parse");
    require(tradingbot::persistence::parse_stored_order_status("partially_filled").has_value(),
            "partially filled should parse");
    require(tradingbot::persistence::parse_stored_order_status("cancelled").has_value(), "cancelled should parse");
    require(tradingbot::persistence::parse_stored_order_status("timed_out").has_value(), "timed out should parse");
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
    maps_rejected_sell_order();
    maps_filled_order_with_average_price();
    parses_all_stored_statuses();
    invalid_stored_values_fail_closed();
    return 0;
}
