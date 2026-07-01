#include "tradingbot/persistence/decision_mapper.hpp"

#include <chrono>
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

tradingbot::core::Decision decision() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .type = tradingbot::core::DecisionType::Buy,
        .confidence = 0.82,
        .quantity = 3,
        .price = 100.5,
        .reason = "manual buy triggered",
        .source = "highest_confidence:manual_buy",
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

tradingbot::persistence::StoredDecisionRow stored_row() {
    return {
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .decision_type = "buy",
        .confidence = 0.82,
        .quantity = 3,
        .price = 100.5,
        .reason = "manual buy triggered",
        .source = "highest_confidence:manual_buy",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void maps_decision_to_stored_row() {
    const auto row = tradingbot::persistence::map_decision_to_stored_row(decision(), "run-1");

    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.decision_type == "buy", "decision type should store");
    require(row.confidence == 0.82, "confidence should store");
    require(row.quantity == 3, "quantity should store");
    require(row.price && *row.price == 100.5, "price should store");
    require(row.reason == "manual buy triggered", "reason should store");
    require(row.source == "highest_confidence:manual_buy", "source should store");
    require(row.created_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
}

void maps_stored_row_to_decision() {
    const auto result = tradingbot::persistence::map_stored_decision_row(stored_row());

    require(result.ok, "valid stored decision should map");
    require(result.decision.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(result.decision.type == tradingbot::core::DecisionType::Buy, "buy decision should map");
    require(result.decision.confidence == 0.82, "confidence should map");
    require(result.decision.quantity == 3, "quantity should map");
    require(result.decision.price && *result.decision.price == 100.5, "price should map");
    require(result.decision.reason == "manual buy triggered", "reason should map");
    require(result.decision.source == "highest_confidence:manual_buy", "source should map");
    require(result.decision.timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}},
            "timestamp should map");
}

void maps_sell_and_hold_decisions() {
    auto sell = stored_row();
    sell.decision_type = "sell";
    sell.price = 125.0;
    const auto sell_result = tradingbot::persistence::map_stored_decision_row(sell);
    require(sell_result.ok, "sell row should map");
    require(sell_result.decision.type == tradingbot::core::DecisionType::Sell, "sell decision should map");

    auto hold = stored_row();
    hold.decision_type = "hold";
    hold.price = std::nullopt;
    hold.quantity = 0;
    const auto hold_result = tradingbot::persistence::map_stored_decision_row(hold);
    require(hold_result.ok, "hold row should map");
    require(hold_result.decision.type == tradingbot::core::DecisionType::Hold, "hold decision should map");
    require(!hold_result.decision.price, "missing optional price should map");
}

void round_trips_decision_through_stored_row() {
    const auto row = tradingbot::persistence::map_decision_to_stored_row(decision(), "run-1");
    const auto result = tradingbot::persistence::map_stored_decision_row(row);

    require(result.ok, "decision should round trip");
    require(result.decision.instrument_key.value == decision().instrument_key.value, "instrument should round trip");
    require(result.decision.type == decision().type, "type should round trip");
    require(result.decision.confidence == decision().confidence, "confidence should round trip");
    require(result.decision.quantity == decision().quantity, "quantity should round trip");
    require(result.decision.price == decision().price, "price should round trip");
    require(result.decision.reason == decision().reason, "reason should round trip");
    require(result.decision.source == decision().source, "source should round trip");
    require(result.decision.timestamp == decision().timestamp, "timestamp should round trip");
}

void parses_all_decision_types() {
    const tradingbot::core::DecisionType types[]{
        tradingbot::core::DecisionType::Buy,
        tradingbot::core::DecisionType::Sell,
        tradingbot::core::DecisionType::Hold,
    };
    for (const auto type : types) {
        const auto stored = tradingbot::persistence::stored_decision_type_name(type);
        const auto parsed = tradingbot::persistence::parse_stored_decision_type(stored);
        require(parsed && *parsed == type, "decision type should round trip: " + stored);
    }
}

void invalid_decision_type_fails_closed() {
    auto row = stored_row();
    row.decision_type = "wait";

    const auto result = tradingbot::persistence::map_stored_decision_row(row);

    require(!result.ok, "invalid decision type should fail");
    require(result.error.find("invalid decision_type") != std::string::npos,
            "invalid decision type error should be clear");
}

}  // namespace

int main() {
    maps_decision_to_stored_row();
    maps_stored_row_to_decision();
    maps_sell_and_hold_decisions();
    round_trips_decision_through_stored_row();
    parses_all_decision_types();
    invalid_decision_type_fails_closed();
    return 0;
}
