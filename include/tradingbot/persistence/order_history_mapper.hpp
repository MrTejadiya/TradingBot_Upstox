#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredOrderRow {
    std::string broker_order_id;
    std::string run_id;
    std::string instrument_key;
    std::string side;
    core::Quantity quantity{0};
    core::Money price{0.0};
    std::string status;
    std::string rejection_reason;
    core::Quantity filled_quantity{0};
    std::optional<core::Money> average_fill_price;
    std::string source_strategy;
    std::string tag;
    core::TimePoint updated_at{};
};

struct OrderHistoryMapResult {
    bool ok{false};
    core::OrderRecord order;
    std::string error;
};

OrderHistoryMapResult map_stored_order_row(const StoredOrderRow& row);
std::optional<core::OrderSide> parse_stored_order_side(const std::string& value);
std::optional<core::OrderStatus> parse_stored_order_status(const std::string& value);

}  // namespace tradingbot::persistence
