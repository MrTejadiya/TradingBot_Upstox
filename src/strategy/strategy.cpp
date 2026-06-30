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

bool is_actionable_signal(const core::StrategySignal& signal) {
    return core::is_valid_instrument_key(signal.instrument_key) && signal.confidence > 0.0 &&
           signal.confidence <= 1.0 && signal.suggested_quantity > 0 && !signal.reason.empty() &&
           !signal.strategy_name.empty();
}

}  // namespace tradingbot::strategy

