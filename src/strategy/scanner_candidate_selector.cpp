#include "tradingbot/strategy/scanner_candidate_selector.hpp"

#include <sstream>

namespace tradingbot::strategy {
namespace {

core::DecisionType decision_type_for(core::TradeAction action) {
    return action == core::TradeAction::Buy ? core::DecisionType::Buy : core::DecisionType::Sell;
}

std::string scanner_names(const std::vector<std::string>& names) {
    std::ostringstream out;
    for (auto index = std::size_t{0}; index < names.size(); ++index) {
        if (index != 0) {
            out << ",";
        }
        out << names[index];
    }
    return out.str();
}

}  // namespace

ScannerCandidateSelectionResult select_top_ranked_candidate(
    const std::vector<ScannerCandidateRank>& ranked_candidates,
    ScannerCandidateSelectionConfig config,
    core::TimePoint decided_at) {
    ScannerCandidateSelectionResult result;
    if (ranked_candidates.empty()) {
        result.diagnostics.push_back("no ranked scanner candidates available");
        return result;
    }

    const auto& candidate = ranked_candidates.front();
    if (candidate.action != config.expected_action) {
        result.diagnostics.push_back("top scanner candidate action does not match expected action");
        return result;
    }
    if (candidate.score < config.minimum_score) {
        result.diagnostics.push_back("top scanner candidate score is below minimum");
        return result;
    }
    if (candidate.suggested_quantity <= 0) {
        result.diagnostics.push_back("top scanner candidate has no positive suggested quantity");
        return result;
    }
    if (!candidate.suggested_entry_price) {
        result.diagnostics.push_back("top scanner candidate has no suggested entry price");
        return result;
    }

    std::ostringstream reason;
    reason << "selected top scanner candidate; score=" << candidate.score
           << "; strongest_confidence=" << candidate.strongest_confidence
           << "; signal_count=" << candidate.signal_count
           << "; scanners=" << scanner_names(candidate.strategy_names);

    result.decision = core::Decision{
        .instrument_key = candidate.instrument_key,
        .type = decision_type_for(candidate.action),
        .confidence = candidate.strongest_confidence,
        .quantity = candidate.suggested_quantity,
        .price = candidate.suggested_entry_price,
        .reason = reason.str(),
        .source = config.source,
        .timestamp = decided_at,
    };
    return result;
}

}  // namespace tradingbot::strategy
