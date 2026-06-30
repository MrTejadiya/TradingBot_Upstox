#include "tradingbot/strategy/sell_strategies.hpp"

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
    if (context.quote && context.quote->ltp > 0.0 && !context.quote->stale) {
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

}  // namespace tradingbot::strategy

