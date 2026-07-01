#include "tradingbot/strategy/sell_strategies.hpp"

#include <algorithm>
#include <optional>
#include <sstream>

namespace tradingbot::strategy {
namespace {

std::optional<core::Holding> current_holding(const StrategyContext& context) {
    for (const auto& holding : context.portfolio.holdings) {
        if (holding.instrument_key.value == context.instrument.key.value && holding.quantity > 0) {
            return holding;
        }
    }
    return std::nullopt;
}

std::optional<core::Money> current_price(const StrategyContext& context) {
    if (context.quote && is_usable_quote(*context.quote, context.evaluated_at)) {
        return context.quote->ltp;
    }
    return latest_close(context);
}

core::StrategySignal build_sell_signal(const StrategyContext& context, const core::Holding& holding,
                                       const std::string& strategy_name, double confidence,
                                       const std::string& reason, core::Money price) {
    return {
        .instrument_key = context.instrument.key,
        .action = core::TradeAction::Sell,
        .confidence = confidence,
        .suggested_quantity = holding.quantity,
        .suggested_entry_price = price,
        .reason = reason,
        .strategy_name = strategy_name,
        .timestamp = context.evaluated_at,
    };
}

core::Money high_water_mark(const StrategyContext& context, const core::Holding& holding,
                            std::optional<core::Money> price) {
    auto high = holding.average_buy_price;
    for (const auto& candle : context.candles) {
        if (candle.instrument_key.value == context.instrument.key.value) {
            high = std::max(high, candle.high > 0.0 ? candle.high : candle.close);
        }
    }
    if (price) {
        high = std::max(high, *price);
    }
    return high;
}

}  // namespace

std::string TargetProfitSellStrategy::name() const {
    return "target_profit_sell";
}

StrategyEvaluation TargetProfitSellStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    const auto holding = current_holding(context);
    if (!holding) {
        evaluation.diagnostics.push_back("target sell skipped: no current holding");
        return evaluation;
    }

    const auto price = current_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("target sell skipped: no quote or close price available");
        return evaluation;
    }

    const auto target = context.instrument.manual_target_price.value_or(
        holding->average_buy_price * (1.0 + (context.instrument.target_profit_pct / 100.0)));
    if (*price < target) {
        evaluation.diagnostics.push_back("target sell skipped: current price is below target");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "price " << *price << " reached target " << target;
    evaluation.signals.push_back(build_sell_signal(context, *holding, name(), 0.9, reason.str(), *price));
    return evaluation;
}

std::string StopLossSellStrategy::name() const {
    return "stop_loss_sell";
}

StrategyEvaluation StopLossSellStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    const auto holding = current_holding(context);
    if (!holding) {
        evaluation.diagnostics.push_back("stop loss skipped: no current holding");
        return evaluation;
    }
    if (!context.instrument.stop_loss_pct || *context.instrument.stop_loss_pct <= 0.0) {
        evaluation.diagnostics.push_back("stop loss skipped: stop loss percentage is not configured");
        return evaluation;
    }

    const auto price = current_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("stop loss skipped: no quote or close price available");
        return evaluation;
    }

    const auto stop_loss = holding->average_buy_price * (1.0 - (*context.instrument.stop_loss_pct / 100.0));
    if (*price > stop_loss) {
        evaluation.diagnostics.push_back("stop loss skipped: current price is above stop loss");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "price " << *price << " breached stop loss " << stop_loss;
    evaluation.signals.push_back(build_sell_signal(context, *holding, name(), 0.98, reason.str(), *price));
    return evaluation;
}

std::string TrailingStopSellStrategy::name() const {
    return "trailing_stop_sell";
}

StrategyEvaluation TrailingStopSellStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    const auto holding = current_holding(context);
    if (!holding) {
        evaluation.diagnostics.push_back("trailing stop skipped: no current holding");
        return evaluation;
    }
    if (!context.instrument.trailing_stop_pct || *context.instrument.trailing_stop_pct <= 0.0) {
        evaluation.diagnostics.push_back("trailing stop skipped: trailing stop percentage is not configured");
        return evaluation;
    }

    const auto price = current_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("trailing stop skipped: no quote or close price available");
        return evaluation;
    }

    const auto high = high_water_mark(context, *holding, price);
    if (high <= holding->average_buy_price) {
        evaluation.diagnostics.push_back("trailing stop skipped: no favorable price move yet");
        return evaluation;
    }

    const auto trailing_stop = high * (1.0 - (*context.instrument.trailing_stop_pct / 100.0));
    if (*price > trailing_stop) {
        evaluation.diagnostics.push_back("trailing stop skipped: current price is above trailing stop");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "price " << *price << " breached trailing stop " << trailing_stop << " from high " << high;
    evaluation.signals.push_back(build_sell_signal(context, *holding, name(), 0.92, reason.str(), *price));
    return evaluation;
}

}  // namespace tradingbot::strategy
