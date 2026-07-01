#include "tradingbot/strategy/buy_strategies.hpp"

#include "tradingbot/strategy/indicators.hpp"

#include <sstream>
#include <utility>

namespace tradingbot::strategy {
namespace {

std::optional<core::Money> entry_price(const StrategyContext& context) {
    if (context.quote && is_usable_quote(*context.quote, context.evaluated_at, quote_freshness_window(context.max_quote_age))) {
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

std::string ManualBuyStrategy::name() const {
    return "manual_buy";
}

StrategyEvaluation ManualBuyStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    if (!can_buy(context)) {
        evaluation.diagnostics.push_back("manual buy skipped: instrument is disabled or quantity/key is invalid");
        return evaluation;
    }
    if (!context.instrument.manual_buy_price) {
        evaluation.diagnostics.push_back("manual buy skipped: manual buy price is not configured");
        return evaluation;
    }

    const auto price = entry_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("manual buy skipped: no quote or close price available");
        return evaluation;
    }
    if (*price > *context.instrument.manual_buy_price) {
        evaluation.diagnostics.push_back("manual buy skipped: current price is above manual buy price");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "price " << *price << " is at or below manual buy price " << *context.instrument.manual_buy_price;
    evaluation.signals.push_back(build_buy_signal(context, name(), 0.95, reason.str(), *price));
    return evaluation;
}

RsiOversoldBuyStrategy::RsiOversoldBuyStrategy(RsiOversoldConfig config) : config_(config) {}

std::string RsiOversoldBuyStrategy::name() const {
    return "rsi_oversold";
}

StrategyEvaluation RsiOversoldBuyStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    if (!can_buy(context)) {
        evaluation.diagnostics.push_back("RSI buy skipped: instrument is disabled or quantity/key is invalid");
        return evaluation;
    }
    if (config_.period <= 0 || config_.threshold <= 0.0 || config_.threshold >= 100.0) {
        evaluation.diagnostics.push_back("RSI buy skipped: invalid RSI configuration");
        return evaluation;
    }

    const auto closes = close_prices(context.candles);
    const auto rsi = relative_strength_index(closes, config_.period);
    if (!rsi) {
        evaluation.diagnostics.push_back("RSI buy skipped: insufficient candle data");
        return evaluation;
    }
    if (*rsi > config_.threshold) {
        evaluation.diagnostics.push_back("RSI buy skipped: RSI is above oversold threshold");
        return evaluation;
    }

    const auto price = entry_price(context);
    if (!price) {
        evaluation.diagnostics.push_back("RSI buy skipped: no quote or close price available");
        return evaluation;
    }

    std::ostringstream reason;
    reason << "RSI " << *rsi << " is at or below oversold threshold " << config_.threshold;
    evaluation.signals.push_back(build_buy_signal(context, name(), 0.75, reason.str(), *price));
    return evaluation;
}

}  // namespace tradingbot::strategy
