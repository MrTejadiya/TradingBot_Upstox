#include "tradingbot/persistence/risk_event_mapper.hpp"

namespace tradingbot::persistence {

std::string stored_risk_decision_name(core::RiskDecision decision) {
    return decision == core::RiskDecision::Approved ? "approved" : "rejected";
}

std::optional<core::RiskDecision> parse_stored_risk_decision(const std::string& value) {
    if (value == "approved") {
        return core::RiskDecision::Approved;
    }
    if (value == "rejected") {
        return core::RiskDecision::Rejected;
    }
    return std::nullopt;
}

StoredRiskEventRow map_risk_event_to_stored_row(const core::RiskEvent& event, const std::string& run_id) {
    return {
        .run_id = run_id,
        .instrument_key = event.instrument_key.value,
        .decision = stored_risk_decision_name(event.decision),
        .reason_code = event.reason_code,
        .detail = event.detail,
        .created_at = event.timestamp,
    };
}

RiskEventMapResult map_stored_risk_event_row(const StoredRiskEventRow& row) {
    const auto decision = parse_stored_risk_decision(row.decision);
    if (!decision) {
        return {.ok = false, .error = "stored risk event has invalid decision: " + row.decision};
    }

    return {
        .ok = true,
        .event =
            {
                .instrument_key = {row.instrument_key},
                .decision = *decision,
                .reason_code = row.reason_code,
                .detail = row.detail,
                .timestamp = row.created_at,
            },
    };
}

}  // namespace tradingbot::persistence
