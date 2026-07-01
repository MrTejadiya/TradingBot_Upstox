#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredDecisionRow {
    std::string run_id;
    std::string instrument_key;
    std::string decision_type;
    double confidence{0.0};
    core::Quantity quantity{0};
    std::optional<core::Money> price;
    std::string reason;
    std::string source;
    core::TimePoint created_at{};
};

struct DecisionMapResult {
    bool ok{false};
    core::Decision decision;
    std::string error;
};

DecisionMapResult map_stored_decision_row(const StoredDecisionRow& row);
StoredDecisionRow map_decision_to_stored_row(const core::Decision& decision, const std::string& run_id);
std::string stored_decision_type_name(core::DecisionType type);
std::optional<core::DecisionType> parse_stored_decision_type(const std::string& value);

}  // namespace tradingbot::persistence
