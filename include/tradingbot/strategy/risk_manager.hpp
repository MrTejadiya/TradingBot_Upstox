#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>

namespace tradingbot::strategy {

struct RiskManagerRequest {
    core::Instrument instrument;
    core::Decision decision;
    core::PortfolioState portfolio;
    int orders_placed_today{0};
    int max_orders_per_day{0};
    core::Money max_order_value{0.0};
    core::Money traded_value_today{0.0};
    core::Money max_daily_traded_value{0.0};
    core::TimePoint evaluated_at{};
};

class RiskManager {
public:
    core::RiskEvent evaluate(const RiskManagerRequest& request) const;
};

std::string risk_decision_name(core::RiskDecision decision);

}  // namespace tradingbot::strategy
