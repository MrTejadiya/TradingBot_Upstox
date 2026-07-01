#pragma once

#include "tradingbot/core/domain.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tradingbot::strategy {

struct ScannerSignalRankConfig {
    core::TradeAction action{core::TradeAction::Buy};
    std::unordered_map<std::string, double> strategy_weights;
    std::size_t limit{0};
};

struct ScannerCandidateRank {
    core::InstrumentKey instrument_key;
    core::TradeAction action{core::TradeAction::Buy};
    double score{0.0};
    double strongest_confidence{0.0};
    std::size_t signal_count{0};
    std::vector<std::string> strategy_names;
    core::Quantity suggested_quantity{0};
    std::optional<core::Money> suggested_entry_price;
    core::TimePoint ranked_at{};
};

std::vector<ScannerCandidateRank> rank_scanner_signals(const std::vector<core::StrategySignal>& signals,
                                                       ScannerSignalRankConfig config = {},
                                                       core::TimePoint ranked_at = core::TimePoint{});

}  // namespace tradingbot::strategy
