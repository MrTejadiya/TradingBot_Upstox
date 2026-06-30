#include "tradingbot/strategy/advanced_buy_strategies.hpp"

#include <sstream>
#include <utility>

namespace tradingbot::strategy {
namespace {

StrategyEvaluation placeholder_evaluation(const StrategyContext& context, const std::string& strategy_name,
                                          const std::string& detail) {
    StrategyEvaluation evaluation;
    std::ostringstream diagnostic;
    diagnostic << strategy_name << " placeholder skipped " << context.instrument.symbol << ": " << detail;
    evaluation.diagnostics.push_back(diagnostic.str());
    return evaluation;
}

}  // namespace

EmaCrossoverBuyStrategy::EmaCrossoverBuyStrategy(EmaCrossoverConfig config) : config_(config) {}

std::string EmaCrossoverBuyStrategy::name() const {
    return "ema_crossover_buy";
}

StrategyEvaluation EmaCrossoverBuyStrategy::evaluate(const StrategyContext& context) const {
    if (config_.fast_period <= 0 || config_.slow_period <= 0 || config_.fast_period >= config_.slow_period) {
        return placeholder_evaluation(context, name(), "invalid EMA crossover configuration");
    }
    return placeholder_evaluation(context, name(), "rule not enabled until crossover confirmation is implemented");
}

BreakoutBuyStrategy::BreakoutBuyStrategy(BreakoutConfig config) : config_(config) {}

std::string BreakoutBuyStrategy::name() const {
    return "breakout_buy";
}

StrategyEvaluation BreakoutBuyStrategy::evaluate(const StrategyContext& context) const {
    if (config_.lookback_period <= 0 || config_.breakout_pct <= 0.0) {
        return placeholder_evaluation(context, name(), "invalid breakout configuration");
    }
    return placeholder_evaluation(context, name(), "rule not enabled until breakout confirmation is implemented");
}

VolumeSurgeBuyStrategy::VolumeSurgeBuyStrategy(VolumeSurgeConfig config) : config_(config) {}

std::string VolumeSurgeBuyStrategy::name() const {
    return "volume_surge_buy";
}

StrategyEvaluation VolumeSurgeBuyStrategy::evaluate(const StrategyContext& context) const {
    if (config_.lookback_period <= 0 || config_.multiplier <= 1.0) {
        return placeholder_evaluation(context, name(), "invalid volume surge configuration");
    }
    return placeholder_evaluation(context, name(), "rule not enabled until volume confirmation is implemented");
}

}  // namespace tradingbot::strategy

