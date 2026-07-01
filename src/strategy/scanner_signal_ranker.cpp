#include "tradingbot/strategy/scanner_signal_ranker.hpp"

#include "tradingbot/strategy/strategy.hpp"

#include <algorithm>
#include <unordered_map>

namespace tradingbot::strategy {
namespace {

struct CandidateBuilder {
    ScannerCandidateRank candidate;
};

double weight_for(const std::unordered_map<std::string, double>& weights, const std::string& strategy_name) {
    const auto found = weights.find(strategy_name);
    if (found == weights.end() || found->second <= 0.0) {
        return 1.0;
    }
    return found->second;
}

bool has_strategy_name(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

std::vector<ScannerCandidateRank> rank_scanner_signals(const std::vector<core::StrategySignal>& signals,
                                                       ScannerSignalRankConfig config, core::TimePoint ranked_at) {
    std::unordered_map<std::string, CandidateBuilder> builders;
    for (const auto& signal : signals) {
        if (signal.action != config.action || !is_actionable_signal(signal)) {
            continue;
        }

        auto& builder = builders[signal.instrument_key.value];
        auto& candidate = builder.candidate;
        if (candidate.instrument_key.value.empty()) {
            candidate.instrument_key = signal.instrument_key;
            candidate.action = signal.action;
            candidate.ranked_at = ranked_at;
        }

        candidate.score += signal.confidence * weight_for(config.strategy_weights, signal.strategy_name);
        candidate.strongest_confidence = std::max(candidate.strongest_confidence, signal.confidence);
        ++candidate.signal_count;
        if (!has_strategy_name(candidate.strategy_names, signal.strategy_name)) {
            candidate.strategy_names.push_back(signal.strategy_name);
        }
        if (!candidate.suggested_entry_price || signal.confidence >= candidate.strongest_confidence) {
            candidate.suggested_entry_price = signal.suggested_entry_price;
        }
    }

    std::vector<ScannerCandidateRank> ranked;
    ranked.reserve(builders.size());
    for (auto& [_, builder] : builders) {
        std::sort(builder.candidate.strategy_names.begin(), builder.candidate.strategy_names.end());
        ranked.push_back(std::move(builder.candidate));
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.strongest_confidence != right.strongest_confidence) {
            return left.strongest_confidence > right.strongest_confidence;
        }
        if (left.signal_count != right.signal_count) {
            return left.signal_count > right.signal_count;
        }
        return left.instrument_key.value < right.instrument_key.value;
    });

    if (config.limit > 0 && ranked.size() > config.limit) {
        ranked.resize(config.limit);
    }
    return ranked;
}

}  // namespace tradingbot::strategy
