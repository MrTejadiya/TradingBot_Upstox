#include "tradingbot/strategy/advanced_buy_strategies.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 100.0, .volume = 1000},
                    {.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 102.0, .volume = 1500}},
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 103.0},
        .portfolio = {.available_funds = 50000.0},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

void placeholders_have_stable_names() {
    require(tradingbot::strategy::EmaCrossoverBuyStrategy{}.name() == "ema_crossover_buy",
            "EMA crossover placeholder should expose stable name");
    require(tradingbot::strategy::BreakoutBuyStrategy{}.name() == "breakout_buy",
            "breakout placeholder should expose stable name");
    require(tradingbot::strategy::VolumeSurgeBuyStrategy{}.name() == "volume_surge_buy",
            "volume surge placeholder should expose stable name");
}

void placeholders_emit_diagnostics_but_no_signals() {
    const auto context = sample_context();
    std::vector<std::unique_ptr<tradingbot::strategy::Strategy>> strategies;
    strategies.push_back(std::make_unique<tradingbot::strategy::EmaCrossoverBuyStrategy>());
    strategies.push_back(std::make_unique<tradingbot::strategy::BreakoutBuyStrategy>());
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
    placeholders_have_stable_names();
    placeholders_emit_diagnostics_but_no_signals();
    invalid_configuration_is_reported();
    return 0;
}

