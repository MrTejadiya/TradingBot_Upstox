#include "tradingbot/strategy/strategy.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::strategy::StrategyContext sample_context() {
    return {
        .instrument = {.key = {"NSE_EQ|INE002A01018"}, .symbol = "RELIANCE", .enabled = true, .quantity = 3},
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 100.0},
                    {.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 102.5}},
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 103.0},
        .portfolio = {.available_funds = 25000.0},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

class FixedBuyStrategy final : public tradingbot::strategy::Strategy {
public:
    std::string name() const override {
        return "fixed_buy";
    }

    tradingbot::strategy::StrategyEvaluation evaluate(const tradingbot::strategy::StrategyContext& context) const override {
        return {
            .signals = {{
                .instrument_key = context.instrument.key,
                .action = tradingbot::core::TradeAction::Buy,
                .confidence = 0.7,
                .suggested_quantity = 2,
                .suggested_entry_price = tradingbot::strategy::latest_close(context),
                .reason = "test buy signal",
                .strategy_name = name(),
                .timestamp = context.evaluated_at,
            }},
            .diagnostics = {"fixed buy evaluated"},
        };
    }
};

void noop_strategy_returns_no_signals() {
    tradingbot::strategy::NoopStrategy strategy;
    const auto evaluation = strategy.evaluate(sample_context());

    require(strategy.name() == "noop", "noop strategy should expose stable name");
    require(evaluation.signals.empty(), "noop should not emit signals");
    require(!evaluation.diagnostics.empty(), "noop should explain skipped evaluation");
}

void strategy_interface_supports_actionable_signals() {
    FixedBuyStrategy strategy;
    const auto evaluation = strategy.evaluate(sample_context());

    require(strategy.name() == "fixed_buy", "strategy name should be stable");
    require(evaluation.signals.size() == 1, "strategy should emit one signal");
    require(tradingbot::strategy::is_actionable_signal(evaluation.signals.front()), "signal should be actionable");
    require(evaluation.signals.front().suggested_entry_price == 102.5, "strategy should use latest close helper");
}

void context_helpers_report_candle_availability() {
    const auto context = sample_context();

    require(tradingbot::strategy::has_minimum_candles(context, 2), "context should have two candles");
    require(!tradingbot::strategy::has_minimum_candles(context, 3), "context should not have three candles");
    require(tradingbot::strategy::latest_close(context) == 102.5, "latest close should return trailing candle close");
}

void quote_freshness_rejects_bad_or_old_quotes() {
    const auto now = tradingbot::core::Clock::now();

    require(tradingbot::strategy::is_usable_quote({.ltp = 100.0}, now), "untimestamped positive quote should be usable");
    require(!tradingbot::strategy::is_usable_quote({.ltp = 0.0}, now), "non-positive quote should not be usable");
    require(!tradingbot::strategy::is_usable_quote({.ltp = 100.0, .stale = true}, now),
            "explicitly stale quote should not be usable");
    require(tradingbot::strategy::is_usable_quote({.timestamp = now - std::chrono::minutes{4}, .ltp = 100.0}, now),
            "recent timestamped quote should be usable");
    require(!tradingbot::strategy::is_usable_quote({.timestamp = now - std::chrono::minutes{6}, .ltp = 100.0}, now),
            "old timestamped quote should not be usable");
    require(tradingbot::strategy::is_usable_quote({.timestamp = now - std::chrono::minutes{6}, .ltp = 100.0}, now,
                                                 std::chrono::minutes{10}),
            "custom freshness window should allow a quote within override age");
}

void signal_validation_rejects_unsafe_signals() {
    tradingbot::core::StrategySignal signal{
        .instrument_key = {"bad-key"},
        .action = tradingbot::core::TradeAction::Buy,
        .confidence = 1.2,
        .suggested_quantity = 0,
        .reason = "",
        .strategy_name = "",
        .timestamp = tradingbot::core::Clock::now(),
    };

    require(!tradingbot::strategy::is_actionable_signal(signal), "invalid signal should not be actionable");
}

}  // namespace

int main() {
    noop_strategy_returns_no_signals();
    strategy_interface_supports_actionable_signals();
    context_helpers_report_candle_availability();
    quote_freshness_rejects_bad_or_old_quotes();
    signal_validation_rejects_unsafe_signals();
    return 0;
}
