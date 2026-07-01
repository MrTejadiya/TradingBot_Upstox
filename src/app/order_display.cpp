#include "tradingbot/app/order_display.hpp"

#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace tradingbot::app {
namespace {

std::string format_timestamp(core::TimePoint timestamp) {
    const auto time = core::Clock::to_time_t(timestamp);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

}  // namespace

void print_orders(const std::vector<core::OrderRecord>& orders, std::ostream& out) {
    if (orders.empty()) {
        out << "No orders found.\n";
        return;
    }

    out << std::left << std::setw(14) << "RUN_ID" << std::setw(18) << "ORDER_ID" << std::setw(24) << "INSTRUMENT"
        << std::setw(8) << "SIDE" << std::setw(10) << "QTY" << std::setw(12) << "PRICE" << std::setw(18) << "STATUS"
        << std::setw(28) << "REASON" << "UPDATED_AT\n";
    for (const auto& order : orders) {
        out << std::left << std::setw(14) << order.request.run_id << std::setw(18) << order.broker_order_id
            << std::setw(24) << order.request.instrument_key.value << std::setw(8) << order_side_name(order.request.side)
            << std::setw(10) << order.request.quantity << std::setw(12) << order.request.price << std::setw(18)
            << order_status_name(order.status) << std::setw(28) << order.rejection_reason
            << format_timestamp(order.updated_at) << "\n";
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
