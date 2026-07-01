#include "tradingbot/strategy/advanced_buy_strategies.hpp"

#include "tradingbot/strategy/indicators.hpp"

#include <optional>
#include <sstream>
#include <utility>
#include <vector>

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

std::optional<core::Money> entry_price(const StrategyContext& context) {
    if (context.quote && context.quote->ltp > 0.0 && !context.quote->stale) {
        return context.quote->ltp;
    }
    return latest_close(context);
}

std::optional<core::Money> target_price(const StrategyContext& context, core::Money price) {
    if (context.instrument.manual_target_price) {
        return context.instrument.manual_target_price;
    }
    if (context.instrument.target_profit_pct > 0.0) {
        return price * (1.0 + (context.instrument.target_profit_pct / 100.0));
    }
    return std::nullopt;
}

std::optional<core::Money> stop_loss_price(const StrategyContext& context, core::Money price) {
    if (!context.instrument.stop_loss_pct || *context.instrument.stop_loss_pct <= 0.0) {
        return std::nullopt;
    }
    return price * (1.0 - (*context.instrument.stop_loss_pct / 100.0));
}

bool can_buy(const StrategyContext& context) {
    return context.instrument.enabled && core::is_valid_instrument_key(context.instrument.key) &&
           context.instrument.quantity > 0;
}

core::StrategySignal build_buy_signal(const StrategyContext& context, const std::string& strategy_name,
                                      double confidence, const std::string& reason, core::Money price) {
    return {
        .instrument_key = context.instrument.key,
        .action = core::TradeAction::Buy,
        .confidence = confidence,
        .suggested_quantity = context.instrument.quantity,
        .suggested_entry_price = price,
        .suggested_target_price = target_price(context, price),
        .suggested_stop_loss = stop_loss_price(context, price),
        .reason = reason,
        .strategy_name = strategy_name,
        .timestamp = context.evaluated_at,
    };
}

}  // namespace

EmaCrossoverBuyStrategy::EmaCrossoverBuyStrategy(EmaCrossoverConfig config) : config_(config) {}

std::string EmaCrossoverBuyStrategy::name() const {
    return "ema_crossover_buy";
}

StrategyEvaluation EmaCrossoverBuyStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    if (!can_buy(context)) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: instrument is disabled or quantity/key is invalid");
        return evaluation;
    }
    if (config_.fast_period <= 0 || config_.slow_period <= 0 || config_.fast_period >= config_.slow_period) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: invalid EMA crossover configuration");
        return evaluation;
    }

    const auto closes = close_prices(context.candles);
    if (closes.size() < static_cast<std::size_t>(config_.slow_period + 2)) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: insufficient candle data");
        return evaluation;
    }

    const std::vector<double> previous_closes(closes.begin(), closes.end() - 1);
    const auto previous_fast = exponential_moving_average(previous_closes, config_.fast_period);
    const auto previous_slow = exponential_moving_average(previous_closes, config_.slow_period);
    const auto current_fast = exponential_moving_average(closes, config_.fast_period);
    const auto current_slow = exponential_moving_average(closes, config_.slow_period);
    if (!previous_fast || !previous_slow || !current_fast || !current_slow) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: insufficient candle data");
        return evaluation;
    }

    if (!(*previous_fast <= *previous_slow && *current_fast > *current_slow)) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: bullish crossover not confirmed");
        return evaluation;
    }

    const auto price = entry_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("EMA crossover buy skipped: no quote or close price available");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "fast EMA crossed above slow EMA: previous " << *previous_fast << "/" << *previous_slow << ", current "
           << *current_fast << "/" << *current_slow;
    evaluation.signals.push_back(build_buy_signal(context, name(), 0.8, reason.str(), *price));
    return evaluation;
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
