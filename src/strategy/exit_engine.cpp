#include "tradingbot/strategy/exit_engine.hpp"

#include "tradingbot/strategy/strategy.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace tradingbot::strategy {
namespace {

bool applies_to_instrument(const core::InstrumentKey& left, const core::InstrumentKey& right) {
    return left.value == right.value;
}

std::optional<core::Money> current_price(const ExitEngineRequest& request) {
    if (request.quote && is_usable_quote(*request.quote, request.evaluated_at, quote_freshness_window(request.max_quote_age))) {
        return request.quote->ltp;
    }
    return std::nullopt;
}

core::Money high_water_mark(const ExitEngineRequest& request, std::optional<core::Money> price) {
    auto high = request.holding.average_buy_price;
    for (const auto& candle : request.candles) {
        if (applies_to_instrument(candle.instrument_key, request.instrument.key)) {
            high = std::max(high, candle.high > 0.0 ? candle.high : candle.close);
        }
    }
    if (price) {
        high = std::max(high, *price);
    }
    return high;
}

core::Decision build_exit_decision(const ExitEngineRequest& request, core::ExitReason reason,
                                   const std::string& detail, double confidence,
                                   std::optional<core::Money> price) {
    return {
        .instrument_key = request.instrument.key,
        .type = core::DecisionType::Sell,
        .confidence = confidence,
        .quantity = request.holding.quantity,
        .price = price,
        .reason = detail,
        .source = "exit_engine:" + exit_reason_name(reason),
        .timestamp = request.evaluated_at,
    };
}

bool has_rejected_risk_event(const ExitEngineRequest& request) {
    for (const auto& event : request.risk_events) {
        if (applies_to_instrument(event.instrument_key, request.instrument.key) &&
            event.decision == core::RiskDecision::Rejected) {
            return true;
        }
    }
    return false;
}

bool exceeds_max_holding_duration(const ExitEngineRequest& request) {
    if (!request.max_holding_duration || *request.max_holding_duration <= std::chrono::seconds{0}) {
        return false;
    }
    if (request.holding.acquired_at == core::TimePoint{}) {
        return false;
    }
    return request.evaluated_at - request.holding.acquired_at >= *request.max_holding_duration;
}

std::optional<core::StrategySignal> strongest_sell_signal(const ExitEngineRequest& request) {
    std::optional<core::StrategySignal> selected;
    for (const auto& signal : request.strategy_signals) {
        if (!applies_to_instrument(signal.instrument_key, request.instrument.key) ||
            signal.action != core::TradeAction::Sell || !is_actionable_signal(signal)) {
            continue;
        }
        if (!selected || signal.confidence > selected->confidence) {
            selected = signal;
        }
    }
    return selected;
}

std::optional<core::StrategySignal> strongest_reached_strategy_target(const ExitEngineRequest& request,
                                                                      core::Money price) {
    std::optional<core::StrategySignal> selected;
    for (const auto& signal : request.strategy_signals) {
        if (!applies_to_instrument(signal.instrument_key, request.instrument.key) ||
            !is_actionable_signal(signal) || !signal.suggested_target_price ||
            price < *signal.suggested_target_price) {
            continue;
        }
        if (!selected || signal.confidence > selected->confidence) {
            selected = signal;
        }
    }
    return selected;
}

}  // namespace

ExitEngineResult ExitEngine::evaluate(const ExitEngineRequest& request) const {
    ExitEngineResult result;
    if (!core::is_valid_instrument_key(request.instrument.key)) {
        result.diagnostics.push_back("exit skipped: invalid instrument key");
        return result;
    }
    if (!applies_to_instrument(request.holding.instrument_key, request.instrument.key) || request.holding.quantity <= 0) {
        result.diagnostics.push_back("exit skipped: no positive holding for instrument");
        return result;
    }

    const auto price = current_price(request);
    if (!price) {
        result.diagnostics.push_back("exit engine has no fresh current price");
    }

    if (request.emergency_exit || has_rejected_risk_event(request)) {
        result.exit_reason = core::ExitReason::EmergencyRisk;
        result.decision = build_exit_decision(request, *result.exit_reason, "emergency risk exit", 1.0, price);
        return result;
    }

    if (price && request.instrument.stop_loss_pct && *request.instrument.stop_loss_pct > 0.0) {
        const auto stop_loss = request.holding.average_buy_price * (1.0 - (*request.instrument.stop_loss_pct / 100.0));
        if (*price <= stop_loss) {
            std::ostringstream reason;
            reason << "price " << *price << " breached stop loss " << stop_loss;
            result.exit_reason = core::ExitReason::StopLoss;
            result.decision = build_exit_decision(request, *result.exit_reason, reason.str(), 0.98, price);
            return result;
        }
    }

    if (price && request.instrument.manual_target_price && *price >= *request.instrument.manual_target_price) {
        std::ostringstream reason;
        reason << "price " << *price << " reached manual target " << *request.instrument.manual_target_price;
        result.exit_reason = core::ExitReason::ManualTarget;
        result.decision = build_exit_decision(request, *result.exit_reason, reason.str(), 0.94, price);
        return result;
    }

    if (price && request.instrument.target_profit_pct > 0.0) {
        const auto target = request.holding.average_buy_price * (1.0 + (request.instrument.target_profit_pct / 100.0));
        if (*price >= target) {
            std::ostringstream reason;
            reason << "price " << *price << " reached fixed target " << target;
            result.exit_reason = core::ExitReason::FixedProfitTarget;
            result.decision = build_exit_decision(request, *result.exit_reason, reason.str(), 0.9, price);
            return result;
        }
    }

    if (price) {
        const auto strategy_target = strongest_reached_strategy_target(request, *price);
        if (strategy_target) {
            std::ostringstream reason;
            reason << "price " << *price << " reached strategy target "
                   << *strategy_target->suggested_target_price << " from " << strategy_target->strategy_name;
            result.exit_reason = core::ExitReason::StrategyTarget;
            result.decision = build_exit_decision(request, *result.exit_reason, reason.str(),
                                                  strategy_target->confidence, price);
            return result;
        }
    }

    const auto strategy_signal = strongest_sell_signal(request);
    if (strategy_signal) {
        result.exit_reason = core::ExitReason::StrategySignal;
        result.decision = {
            .instrument_key = request.instrument.key,
            .type = core::DecisionType::Sell,
            .confidence = strategy_signal->confidence,
            .quantity = request.holding.quantity,
            .price = strategy_signal->suggested_entry_price.value_or(price.value_or(0.0)),
            .reason = strategy_signal->reason,
            .source = "exit_engine:strategy_signal:" + strategy_signal->strategy_name,
            .timestamp = request.evaluated_at,
        };
        return result;
    }

    if (price && request.instrument.trailing_stop_pct && *request.instrument.trailing_stop_pct > 0.0) {
        const auto high = high_water_mark(request, price);
        if (high > request.holding.average_buy_price) {
            const auto trailing_stop = high * (1.0 - (*request.instrument.trailing_stop_pct / 100.0));
            if (*price <= trailing_stop) {
                std::ostringstream reason;
                reason << "price " << *price << " breached trailing stop " << trailing_stop << " from high " << high;
                result.exit_reason = core::ExitReason::TrailingStop;
                result.decision = build_exit_decision(request, *result.exit_reason, reason.str(), 0.86, price);
                return result;
            }
        }
    }

    if (exceeds_max_holding_duration(request)) {
        std::ostringstream reason;
        reason << "holding duration reached configured maximum of " << request.max_holding_duration->count()
               << " seconds";
        result.exit_reason = core::ExitReason::MaximumHoldingDuration;
        result.decision = build_exit_decision(request, *result.exit_reason, reason.str(), 0.72, price);
        return result;
    }

    result.diagnostics.push_back("exit skipped: no exit rule matched");
    return result;
}

std::string exit_reason_name(core::ExitReason reason) {
    switch (reason) {
        case core::ExitReason::EmergencyRisk:
            return "emergency_risk";
        case core::ExitReason::StopLoss:
            return "stop_loss";
        case core::ExitReason::ManualTarget:
            return "manual_target";
        case core::ExitReason::FixedProfitTarget:
            return "fixed_profit_target";
        case core::ExitReason::StrategyTarget:
            return "strategy_target";
        case core::ExitReason::StrategySignal:
            return "strategy_signal";
        case core::ExitReason::TrailingStop:
            return "trailing_stop";
        case core::ExitReason::MaximumHoldingDuration:
            return "maximum_holding_duration";
    }
    return "unknown";
}

}  // namespace tradingbot::strategy
