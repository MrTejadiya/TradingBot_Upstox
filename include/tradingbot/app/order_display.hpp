#pragma once

#include "tradingbot/core/domain.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace tradingbot::app {

void print_orders(const std::vector<core::OrderRecord>& orders, std::ostream& out);
std::string order_status_name(core::OrderStatus status);
std::string order_side_name(core::OrderSide side);

}  // namespace tradingbot::app

