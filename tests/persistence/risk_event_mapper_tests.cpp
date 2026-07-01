#include "tradingbot/persistence/risk_event_mapper.hpp"

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

tradingbot::core::RiskEvent risk_event() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .decision = tradingbot::core::RiskDecision::Approved,
        .reason_code = "APPROVED",
        .detail = "risk checks passed",
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

tradingbot::persistence::StoredRiskEventRow stored_row() {
    return {
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .decision = "approved",
        .reason_code = "APPROVED",
        .detail = "risk checks passed",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void maps_risk_event_to_stored_row() {
    const auto row = tradingbot::persistence::map_risk_event_to_stored_row(risk_event(), "run-1");

    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.decision == "approved", "approved decision should store");
    require(row.reason_code == "APPROVED", "reason code should store");
    require(row.detail == "risk checks passed", "detail should store");
    require(row.created_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
}

void maps_stored_row_to_risk_event() {
    const auto result = tradingbot::persistence::map_stored_risk_event_row(stored_row());

    require(result.ok, "valid stored risk event should map");
    require(result.event.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(result.event.decision == tradingbot::core::RiskDecision::Approved, "approved decision should map");
    require(result.event.reason_code == "APPROVED", "reason code should map");
    require(result.event.detail == "risk checks passed", "detail should map");
    require(result.event.timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should map");
}

void maps_rejected_decision() {
    auto row = stored_row();
    row.decision = "rejected";
    row.reason_code = "INSUFFICIENT_FUNDS";
    row.detail = "available funds below order value";

    const auto result = tradingbot::persistence::map_stored_risk_event_row(row);

    require(result.ok, "valid rejected risk event should map");
    require(result.event.decision == tradingbot::core::RiskDecision::Rejected, "rejected decision should map");
    require(result.event.reason_code == "INSUFFICIENT_FUNDS", "rejected reason should map");
    require(result.event.detail == "available funds below order value", "rejected detail should map");
}

void round_trips_risk_event_through_stored_row() {
    const auto row = tradingbot::persistence::map_risk_event_to_stored_row(risk_event(), "run-1");
    const auto result = tradingbot::persistence::map_stored_risk_event_row(row);

    require(result.ok, "risk event should round trip");
    require(result.event.instrument_key.value == risk_event().instrument_key.value, "instrument should round trip");
    require(result.event.decision == risk_event().decision, "decision should round trip");
    require(result.event.reason_code == risk_event().reason_code, "reason code should round trip");
    require(result.event.detail == risk_event().detail, "detail should round trip");
    require(result.event.timestamp == risk_event().timestamp, "timestamp should round trip");
}

void parses_all_risk_decisions() {
    require(tradingbot::persistence::stored_risk_decision_name(tradingbot::core::RiskDecision::Approved) == "approved",
            "approved decision should serialize");
    require(tradingbot::persistence::stored_risk_decision_name(tradingbot::core::RiskDecision::Rejected) == "rejected",
            "rejected decision should serialize");
    require(tradingbot::persistence::parse_stored_risk_decision("approved") ==
                tradingbot::core::RiskDecision::Approved,
            "approved decision should parse");
    require(tradingbot::persistence::parse_stored_risk_decision("rejected") ==
                tradingbot::core::RiskDecision::Rejected,
            "rejected decision should parse");
}

void invalid_decision_fails_closed() {
    auto row = stored_row();
    row.decision = "maybe";

    const auto result = tradingbot::persistence::map_stored_risk_event_row(row);

    require(!result.ok, "invalid risk decision should fail");
    require(result.error.find("invalid decision") != std::string::npos, "invalid decision error should be clear");
}

}  // namespace

int main() {
    maps_risk_event_to_stored_row();
    maps_stored_row_to_risk_event();
    maps_rejected_decision();
    round_trips_risk_event_through_stored_row();
    parses_all_risk_decisions();
    invalid_decision_fails_closed();
    return 0;
}
