#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredRiskEventRow {
    std::string run_id;
    std::string instrument_key;
    std::string decision;
    std::string reason_code;
    std::string detail;
    core::TimePoint created_at{};
};

struct RiskEventMapResult {
    bool ok{false};
    core::RiskEvent event;
    std::string error;
};

RiskEventMapResult map_stored_risk_event_row(const StoredRiskEventRow& row);
StoredRiskEventRow map_risk_event_to_stored_row(const core::RiskEvent& event, const std::string& run_id);
std::string stored_risk_decision_name(core::RiskDecision decision);
std::optional<core::RiskDecision> parse_stored_risk_decision(const std::string& value);

}  // namespace tradingbot::persistence
