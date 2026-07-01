#include "tradingbot/app/order_display.hpp"

#include <iomanip>
#include <ostream>

namespace tradingbot::app {

void print_orders(const std::vector<core::OrderRecord>& orders, std::ostream& out) {
    if (orders.empty()) {
        out << "No orders found.\n";
        return;
    }

    out << std::left << std::setw(18) << "ORDER_ID" << std::setw(24) << "INSTRUMENT" << std::setw(8) << "SIDE"
        << std::setw(10) << "QTY" << std::setw(12) << "PRICE" << "STATUS\n";
    for (const auto& order : orders) {
        out << std::left << std::setw(18) << order.broker_order_id << std::setw(24) << order.request.instrument_key.value
            << std::setw(8) << order_side_name(order.request.side) << std::setw(10) << order.request.quantity
            << std::setw(12) << order.request.price << order_status_name(order.status) << "\n";
    }
}

std::string order_status_name(core::OrderStatus status) {
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

std::string order_side_name(core::OrderSide side) {
    return side == core::OrderSide::Buy ? "buy" : "sell";
}

}  // namespace tradingbot::app

