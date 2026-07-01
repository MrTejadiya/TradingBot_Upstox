#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>

namespace tradingbot::strategy {

struct RiskManagerRequest {
    core::Instrument instrument;
    core::Decision decision;
    core::PortfolioState portfolio;
    core::Money max_order_value{0.0};
    core::TimePoint evaluated_at{};
};

class RiskManager {
public:
    core::RiskEvent evaluate(const RiskManagerRequest& request) const;
};

std::string risk_decision_name(core::RiskDecision decision);

}  // namespace tradingbot::strategy
