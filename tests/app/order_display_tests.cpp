#include "tradingbot/app/order_display.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::OrderRecord order_record() {
    return {
        .request = {
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .side = tradingbot::core::OrderSide::Sell,
            .quantity = 3,
            .price = 101.5,
        },
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Filled,
    };
}

void prints_empty_state() {
    std::ostringstream out;

    tradingbot::app::print_orders({}, out);

    require(out.str() == "No orders found.\n", "empty order list should print empty state");
}

void prints_order_table() {
    std::ostringstream out;

    tradingbot::app::print_orders({order_record()}, out);

    require(out.str().find("ORDER_ID") != std::string::npos, "table should include header");
    require(out.str().find("ORDER-1") != std::string::npos, "table should include order id");
    require(out.str().find("NSE_EQ|INE002A01018") != std::string::npos, "table should include instrument");
    require(out.str().find("sell") != std::string::npos, "table should include side");
    require(out.str().find("filled") != std::string::npos, "table should include status");
}

void names_all_terminal_statuses() {
    require(tradingbot::app::order_status_name(tradingbot::core::OrderStatus::Rejected) == "rejected",
            "rejected status name should be stable");
    require(tradingbot::app::order_status_name(tradingbot::core::OrderStatus::Cancelled) == "cancelled",
            "cancelled status name should be stable");
    require(tradingbot::app::order_status_name(tradingbot::core::OrderStatus::TimedOut) == "timed_out",
            "timed out status name should be stable");
}

}  // namespace

int main() {
    prints_empty_state();
    prints_order_table();
    names_all_terminal_statuses();
    return 0;
}

