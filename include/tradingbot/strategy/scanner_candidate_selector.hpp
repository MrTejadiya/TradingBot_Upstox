#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/strategy/scanner_signal_ranker.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::strategy {

struct ScannerCandidateSelectionConfig {
    core::TradeAction expected_action{core::TradeAction::Buy};
    double minimum_score{0.0};
    std::string source{"scanner_ranker"};
};

struct ScannerCandidateSelectionResult {
    std::optional<core::Decision> decision;
    std::vector<std::string> diagnostics;
};

ScannerCandidateSelectionResult select_top_ranked_candidate(
    const std::vector<ScannerCandidateRank>& ranked_candidates,
    ScannerCandidateSelectionConfig config = {},
    core::TimePoint decided_at = core::TimePoint{});

}  // namespace tradingbot::strategy
