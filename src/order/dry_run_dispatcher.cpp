#include "tradingbot/order/dry_run_dispatcher.hpp"

#include <sstream>

namespace tradingbot::order {

DispatchResult DryRunOrderDispatcher::dispatch(const core::OrderRequest& request, core::TimePoint dispatched_at) {
    core::OrderRecord record{
        .request = request,
        .broker_order_id = next_order_id(),
        .status = core::OrderStatus::Accepted,
        .filled_quantity = 0,
        .redacted_response_metadata = "dry_run=true",
        .updated_at = dispatched_at,
    };

    if (!core::is_valid_instrument_key(request.instrument_key)) {
        record.status = core::OrderStatus::Rejected;
        record.rejection_reason = "invalid instrument key";
    } else if (!core::has_positive_order_quantity(request)) {
        record.status = core::OrderStatus::Rejected;
        record.rejection_reason = "quantity must be positive";
    } else if (request.order_type == core::OrderType::Limit && request.price <= 0.0) {
        record.status = core::OrderStatus::Rejected;
        record.rejection_reason = "limit order price must be positive";
    }

    records_.push_back(record);
    return {.accepted = record.status == core::OrderStatus::Accepted, .record = record};
}

const std::vector<core::OrderRecord>& DryRunOrderDispatcher::records() const {
    return records_;
}

std::string DryRunOrderDispatcher::next_order_id() {
    std::ostringstream out;
    out << "dry-run-" << next_id_++;
    return out.str();
}

}  // namespace tradingbot::order

