#include "tradingbot/persistence/decision_mapper.hpp"

namespace tradingbot::persistence {

std::string stored_decision_type_name(core::DecisionType type) {
    switch (type) {
        case core::DecisionType::Buy:
            return "buy";
        case core::DecisionType::Sell:
            return "sell";
        case core::DecisionType::Hold:
            return "hold";
    }
    return "unknown";
}

std::optional<core::DecisionType> parse_stored_decision_type(const std::string& value) {
    if (value == "buy") {
        return core::DecisionType::Buy;
    }
    if (value == "sell") {
        return core::DecisionType::Sell;
    }
    if (value == "hold") {
        return core::DecisionType::Hold;
    }
    return std::nullopt;
}

StoredDecisionRow map_decision_to_stored_row(const core::Decision& decision, const std::string& run_id) {
    return {
        .run_id = run_id,
        .instrument_key = decision.instrument_key.value,
        .decision_type = stored_decision_type_name(decision.type),
        .confidence = decision.confidence,
        .quantity = decision.quantity,
        .price = decision.price,
        .reason = decision.reason,
        .source = decision.source,
        .created_at = decision.timestamp,
    };
}

DecisionMapResult map_stored_decision_row(const StoredDecisionRow& row) {
    const auto type = parse_stored_decision_type(row.decision_type);
    if (!type) {
        return {.ok = false, .error = "stored decision has invalid decision_type: " + row.decision_type};
    }

    return {
        .ok = true,
        .decision =
            {
                .instrument_key = {row.instrument_key},
                .type = *type,
                .confidence = row.confidence,
                .quantity = row.quantity,
                .price = row.price,
                .reason = row.reason,
                .source = row.source,
                .timestamp = row.created_at,
            },
    };
}

}  // namespace tradingbot::persistence
