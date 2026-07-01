#include "tradingbot/strategy/strategy.hpp"

namespace tradingbot::strategy {

std::string NoopStrategy::name() const {
    return "noop";
}

StrategyEvaluation NoopStrategy::evaluate(const StrategyContext& context) const {
    StrategyEvaluation evaluation;
    evaluation.diagnostics.push_back("noop strategy skipped " + context.instrument.symbol);
    return evaluation;
}

bool has_minimum_candles(const StrategyContext& context, std::size_t minimum_count) {
    return context.candles.size() >= minimum_count;
}

std::optional<core::Money> latest_close(const StrategyContext& context) {
    if (context.candles.empty()) {
        return std::nullopt;
    }
    return context.candles.back().close;
}

std::chrono::seconds quote_freshness_window(const std::optional<std::chrono::seconds>& override) {
    if (override && *override > std::chrono::seconds{0}) {
        return *override;
    }
    return std::chrono::minutes{5};
}

bool is_usable_quote(const core::QuoteSnapshot& quote, core::TimePoint evaluated_at, std::chrono::seconds max_age) {
    if (quote.stale || quote.ltp <= 0.0) {
        return false;
    }
    if (quote.timestamp == core::TimePoint{}) {
        return true;
    }
    if (evaluated_at == core::TimePoint{}) {
        return false;
    }
    return quote.timestamp >= evaluated_at || evaluated_at - quote.timestamp <= max_age;
}

bool is_actionable_signal(const core::StrategySignal& signal) {
    return core::is_valid_instrument_key(signal.instrument_key) && signal.confidence > 0.0 &&
           signal.confidence <= 1.0 && signal.suggested_quantity > 0 && !signal.reason.empty() &&
           !signal.strategy_name.empty();
}

}  // namespace tradingbot::strategy
