#include "tradingbot/strategy/risk_manager.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::Instrument instrument() {
    return {
        .key = {"NSE_EQ|INE002A01018"},
        .symbol = "RELIANCE",
        .enabled = true,
        .quantity = 3,
        .max_position_quantity = 10,
    };
}

tradingbot::core::Decision buy_decision() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .type = tradingbot::core::DecisionType::Buy,
        .confidence = 0.8,
        .quantity = 3,
        .price = 100.0,
        .reason = "buy",
        .source = "test",
        .timestamp = tradingbot::core::Clock::now(),
    };
}

tradingbot::strategy::RiskManagerRequest base_request() {
    return {
        .instrument = instrument(),
        .decision = buy_decision(),
        .portfolio = {.available_funds = 1000.0,
                      .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 5, .average_buy_price = 90.0}}},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

void approves_valid_buy_decision() {
    const auto event = tradingbot::strategy::RiskManager{}.evaluate(base_request());

    require(event.decision == tradingbot::core::RiskDecision::Approved, "valid buy should be approved");
    require(event.reason_code == "APPROVED", "approved event should carry code");
}

void rejects_disabled_instrument() {
    auto request = base_request();
    request.instrument.enabled = false;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.decision == tradingbot::core::RiskDecision::Rejected, "disabled instrument should reject");
    require(event.reason_code == "INSTRUMENT_DISABLED", "disabled reason should be reported");
}

void rejects_duplicate_open_order() {
    auto request = base_request();
    request.portfolio.open_orders = {{
        .request = {.instrument_key = {"NSE_EQ|INE002A01018"}, .side = tradingbot::core::OrderSide::Buy},
        .status = tradingbot::core::OrderStatus::Accepted,
    }};

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.reason_code == "DUPLICATE_OPEN_ORDER", "duplicate open order should reject");
}

void rejects_insufficient_funds() {
    auto request = base_request();
    request.portfolio.available_funds = 100.0;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.reason_code == "INSUFFICIENT_FUNDS", "insufficient funds should reject buy");
}

void rejects_quantity_above_limit() {
    auto request = base_request();
    request.decision.quantity = 11;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.reason_code == "MAX_POSITION_EXCEEDED", "quantity above max should reject");
}

void approves_valid_sell_against_holding() {
    auto request = base_request();
    request.decision.type = tradingbot::core::DecisionType::Sell;
    request.decision.quantity = 5;
    request.decision.price = 110.0;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.decision == tradingbot::core::RiskDecision::Approved, "valid sell should be approved");
}

void rejects_sell_above_holding() {
    auto request = base_request();
    request.decision.type = tradingbot::core::DecisionType::Sell;
    request.decision.quantity = 6;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.reason_code == "SELL_QUANTITY_EXCEEDS_HOLDING", "sell above holding should reject");
}

void rejects_hold_decision() {
    auto request = base_request();
    request.decision.type = tradingbot::core::DecisionType::Hold;

    const auto event = tradingbot::strategy::RiskManager{}.evaluate(request);

    require(event.reason_code == "HOLD_DECISION", "hold decision should reject as not orderable");
}

}  // namespace

int main() {
    approves_valid_buy_decision();
    rejects_disabled_instrument();
    rejects_duplicate_open_order();
    rejects_insufficient_funds();
    rejects_quantity_above_limit();
    approves_valid_sell_against_holding();
    rejects_sell_above_holding();
    rejects_hold_decision();
    return 0;
}

