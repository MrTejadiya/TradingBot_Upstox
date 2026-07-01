#include "tradingbot/app/order_display.hpp"

#include <chrono>
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
            .run_id = "run-1",
        },
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Rejected,
        .rejection_reason = "risk gate rejected",
        .redacted_response_metadata = "hidden metadata",
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
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

    const auto text = out.str();
    require(text.find("RUN_ID") != std::string::npos, "table should include run id header");
    require(text.find("ORDER_ID") != std::string::npos, "table should include order id header");
    require(text.find("REASON") != std::string::npos, "table should include rejection reason header");
    require(text.find("UPDATED_AT") != std::string::npos, "table should include updated timestamp header");
    require(text.find("run-1") != std::string::npos, "table should include run id");
    require(text.find("ORDER-1") != std::string::npos, "table should include order id");
    require(text.find("NSE_EQ|INE002A01018") != std::string::npos, "table should include instrument");
    require(text.find("sell") != std::string::npos, "table should include side");
    require(text.find("rejected") != std::string::npos, "table should include status");
    require(text.find("risk gate rejected") != std::string::npos, "table should include rejection reason");
    require(text.find("1970-01-01T00:00:01Z") != std::string::npos, "table should include UTC updated timestamp");
    require(text.find("hidden metadata") == std::string::npos, "table should not include API metadata");
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
