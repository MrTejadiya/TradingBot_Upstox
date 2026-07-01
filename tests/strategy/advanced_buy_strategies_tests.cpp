#include "tradingbot/strategy/advanced_buy_strategies.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void require_optional_near(const std::optional<double>& actual, double expected, const std::string& message) {
    require(actual.has_value(), message);
    require(std::fabs(*actual - expected) <= 0.0001, message);
}

std::vector<tradingbot::core::Candle> candles_from_closes(const std::vector<double>& closes) {
    std::vector<tradingbot::core::Candle> candles;
    for (const auto close : closes) {
        candles.push_back({.instrument_key = {"NSE_EQ|INE002A01018"}, .high = close, .close = close, .volume = 1000});
    }
    return candles;
}

tradingbot::strategy::StrategyContext sample_context() {
    return {
        .instrument = {
            .key = {"NSE_EQ|INE002A01018"},
            .symbol = "RELIANCE",
            .enabled = true,
            .quantity = 3,
            .manual_target_price = 115.0,
            .stop_loss_pct = 5.0,
            .target_profit_pct = 10.0,
        },
        .candles = candles_from_closes({100.0, 99.0, 98.0, 97.0, 110.0}),
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 111.0},
        .portfolio = {.available_funds = 50000.0},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

void strategies_have_stable_names() {
    require(tradingbot::strategy::EmaCrossoverBuyStrategy{}.name() == "ema_crossover_buy",
            "EMA crossover strategy should expose stable name");
    require(tradingbot::strategy::BreakoutBuyStrategy{}.name() == "breakout_buy",
            "breakout placeholder should expose stable name");
    require(tradingbot::strategy::VolumeSurgeBuyStrategy{}.name() == "volume_surge_buy",
            "volume surge placeholder should expose stable name");
}

void ema_crossover_emits_buy_signal_on_bullish_cross() {
    const auto evaluation =
        tradingbot::strategy::EmaCrossoverBuyStrategy{{.fast_period = 2, .slow_period = 3}}.evaluate(sample_context());

    require(evaluation.signals.size() == 1, "EMA crossover should emit a signal on bullish cross");
    const auto& signal = evaluation.signals.front();
    require(tradingbot::strategy::is_actionable_signal(signal), "EMA crossover signal should be actionable");
    require(signal.strategy_name == "ema_crossover_buy", "signal should identify EMA crossover strategy");
    require(signal.suggested_quantity == 3, "quantity should come from instrument");
    require(signal.suggested_entry_price == 111.0, "entry should use fresh quote");
    require_optional_near(signal.suggested_target_price, 115.0, "manual target should be retained");
    require_optional_near(signal.suggested_stop_loss, 105.45, "stop loss should derive from entry");
    require(signal.reason.find("fast EMA crossed above slow EMA") != std::string::npos,
            "reason should explain crossover");
}

void ema_crossover_skips_when_cross_is_not_confirmed() {
    auto context = sample_context();
    context.candles = candles_from_closes({100.0, 101.0, 102.0, 103.0, 104.0});

    const auto evaluation =
        tradingbot::strategy::EmaCrossoverBuyStrategy{{.fast_period = 2, .slow_period = 3}}.evaluate(context);

    require(evaluation.signals.empty(), "EMA crossover should skip without a fresh bullish cross");
    require(evaluation.diagnostics.front().find("not confirmed") != std::string::npos,
            "skip diagnostic should explain missing crossover");
}

void ema_crossover_skips_when_data_is_insufficient() {
    auto context = sample_context();
    context.candles = candles_from_closes({100.0, 99.0, 110.0});

    const auto evaluation =
        tradingbot::strategy::EmaCrossoverBuyStrategy{{.fast_period = 2, .slow_period = 3}}.evaluate(context);

    require(evaluation.signals.empty(), "EMA crossover should skip insufficient data");
    require(evaluation.diagnostics.front().find("insufficient") != std::string::npos,
            "skip diagnostic should mention insufficient data");
}

void breakout_emits_buy_signal_when_price_clears_resistance() {
    auto context = sample_context();
    context.candles = candles_from_closes({100.0, 104.0, 103.0, 105.0, 108.0});
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 108.0};

    const auto evaluation =
        tradingbot::strategy::BreakoutBuyStrategy{{.lookback_period = 4, .breakout_pct = 2.0}}.evaluate(context);

    require(evaluation.signals.size() == 1, "breakout should emit a signal above resistance threshold");
    const auto& signal = evaluation.signals.front();
    require(tradingbot::strategy::is_actionable_signal(signal), "breakout signal should be actionable");
    require(signal.strategy_name == "breakout_buy", "signal should identify breakout strategy");
    require(signal.suggested_entry_price == 108.0, "entry should use fresh quote");
    require(signal.reason.find("cleared resistance") != std::string::npos, "reason should explain breakout");
}

void breakout_skips_when_price_has_not_cleared_threshold() {
    auto context = sample_context();
    context.candles = candles_from_closes({100.0, 104.0, 103.0, 105.0, 106.0});
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 106.0};

    const auto evaluation =
        tradingbot::strategy::BreakoutBuyStrategy{{.lookback_period = 4, .breakout_pct = 2.0}}.evaluate(context);

    require(evaluation.signals.empty(), "breakout should skip below threshold");
    require(evaluation.diagnostics.front().find("threshold") != std::string::npos,
            "skip diagnostic should explain threshold");
}

void breakout_skips_when_data_is_insufficient() {
    auto context = sample_context();
    context.candles = candles_from_closes({100.0, 108.0});

    const auto evaluation =
        tradingbot::strategy::BreakoutBuyStrategy{{.lookback_period = 4, .breakout_pct = 2.0}}.evaluate(context);

    require(evaluation.signals.empty(), "breakout should skip insufficient data");
    require(evaluation.diagnostics.front().find("insufficient") != std::string::npos,
            "skip diagnostic should mention insufficient data");
}

void remaining_placeholders_emit_diagnostics_but_no_signals() {
    const auto context = sample_context();
    std::vector<std::unique_ptr<tradingbot::strategy::Strategy>> strategies;
    strategies.push_back(std::make_unique<tradingbot::strategy::VolumeSurgeBuyStrategy>());

    for (const auto& strategy : strategies) {
        const auto evaluation = strategy->evaluate(context);
        require(evaluation.signals.empty(), strategy->name() + " should not emit placeholder signals");
        require(evaluation.diagnostics.size() == 1, strategy->name() + " should emit one diagnostic");
    }
}

void invalid_configuration_is_reported() {
    const auto context = sample_context();
    const auto ema = tradingbot::strategy::EmaCrossoverBuyStrategy{{.fast_period = 21, .slow_period = 9}}.evaluate(context);
    const auto breakout = tradingbot::strategy::BreakoutBuyStrategy{{.lookback_period = 0, .breakout_pct = 2.0}}.evaluate(context);
    const auto volume =
        tradingbot::strategy::VolumeSurgeBuyStrategy{{.lookback_period = 20, .multiplier = 1.0}}.evaluate(context);

    require(ema.diagnostics.front().find("invalid") != std::string::npos, "EMA invalid config should be diagnostic");
    require(breakout.diagnostics.front().find("invalid") != std::string::npos,
            "breakout invalid config should be diagnostic");
    require(volume.diagnostics.front().find("invalid") != std::string::npos,
            "volume invalid config should be diagnostic");
}

}  // namespace

int main() {
    strategies_have_stable_names();
    ema_crossover_emits_buy_signal_on_bullish_cross();
    ema_crossover_skips_when_cross_is_not_confirmed();
    ema_crossover_skips_when_data_is_insufficient();
    breakout_emits_buy_signal_when_price_clears_resistance();
    breakout_skips_when_price_has_not_cleared_threshold();
    breakout_skips_when_data_is_insufficient();
    remaining_placeholders_emit_diagnostics_but_no_signals();
    invalid_configuration_is_reported();
    return 0;
}
