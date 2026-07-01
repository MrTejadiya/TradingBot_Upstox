#include "tradingbot/strategy/risk_manager.hpp"

#include <optional>

namespace tradingbot::strategy {
namespace {

core::RiskEvent risk_event(const RiskManagerRequest& request, core::RiskDecision decision,
                           const std::string& reason_code, const std::string& detail) {
    return {
        .instrument_key = request.instrument.key,
        .decision = decision,
        .reason_code = reason_code,
        .detail = detail,
        .timestamp = request.evaluated_at,
    };
}

bool same_instrument(const core::InstrumentKey& left, const core::InstrumentKey& right) {
    return left.value == right.value;
}

bool has_duplicate_open_order(const RiskManagerRequest& request) {
    const auto expected_side = request.decision.type == core::DecisionType::Buy ? core::OrderSide::Buy : core::OrderSide::Sell;
    for (const auto& order : request.portfolio.open_orders) {
        if (same_instrument(order.request.instrument_key, request.instrument.key) && order.request.side == expected_side &&
            (order.status == core::OrderStatus::Pending || order.status == core::OrderStatus::Accepted ||
             order.status == core::OrderStatus::PartiallyFilled)) {
            return true;
        }
    }
    return false;
}

std::optional<core::Holding> current_holding(const RiskManagerRequest& request) {
    for (const auto& holding : request.portfolio.holdings) {
        if (same_instrument(holding.instrument_key, request.instrument.key) && holding.quantity > 0) {
            return holding;
        }
    }
    return std::nullopt;
}

bool has_sufficient_funds(const RiskManagerRequest& request) {
    if (!request.decision.price) {
        return false;
    }
    return (*request.decision.price * static_cast<double>(request.decision.quantity)) <= request.portfolio.available_funds;
}

}  // namespace

core::RiskEvent RiskManager::evaluate(const RiskManagerRequest& request) const {
    if (!core::is_valid_instrument_key(request.instrument.key) ||
        !same_instrument(request.instrument.key, request.decision.instrument_key)) {
        return risk_event(request, core::RiskDecision::Rejected, "INVALID_INSTRUMENT",
                          "decision instrument does not match a valid configured instrument");
    }
    if (!request.instrument.enabled) {
        return risk_event(request, core::RiskDecision::Rejected, "INSTRUMENT_DISABLED",
                          "instrument is disabled in configuration");
    }
    if (request.decision.type == core::DecisionType::Hold) {
        return risk_event(request, core::RiskDecision::Rejected, "HOLD_DECISION", "hold decision is not orderable");
    }
    if (request.decision.quantity <= 0) {
        return risk_event(request, core::RiskDecision::Rejected, "INVALID_QUANTITY",
                          "decision quantity must be positive");
    }
    if (request.instrument.max_position_quantity > 0 &&
        request.decision.quantity > request.instrument.max_position_quantity) {
        return risk_event(request, core::RiskDecision::Rejected, "MAX_POSITION_EXCEEDED",
                          "decision quantity exceeds instrument max position quantity");
    }
    if (has_duplicate_open_order(request)) {
        return risk_event(request, core::RiskDecision::Rejected, "DUPLICATE_OPEN_ORDER",
                          "matching open order already exists for instrument and side");
    }

    if (request.decision.type == core::DecisionType::Buy) {
        if (!request.decision.price || *request.decision.price <= 0.0) {
            return risk_event(request, core::RiskDecision::Rejected, "MISSING_PRICE",
                              "buy decision requires a positive price");
        }
        if (!has_sufficient_funds(request)) {
            return risk_event(request, core::RiskDecision::Rejected, "INSUFFICIENT_FUNDS",
                              "available funds do not cover requested buy value");
        }
    }

    if (request.decision.type == core::DecisionType::Sell) {
        const auto holding = current_holding(request);
        if (!holding) {
            return risk_event(request, core::RiskDecision::Rejected, "NO_HOLDING",
                              "sell decision requires an existing holding");
        }
        if (request.decision.quantity > holding->quantity) {
            return risk_event(request, core::RiskDecision::Rejected, "SELL_QUANTITY_EXCEEDS_HOLDING",
                              "sell quantity exceeds current holding quantity");
        }
    }

    return risk_event(request, core::RiskDecision::Approved, "APPROVED", "decision passed risk checks");
}

std::string risk_decision_name(core::RiskDecision decision) {
    switch (decision) {
        case core::RiskDecision::Approved:
            return "approved";
        case core::RiskDecision::Rejected:
            return "rejected";
    }
    return "unknown";
}

}  // namespace tradingbot::strategy

