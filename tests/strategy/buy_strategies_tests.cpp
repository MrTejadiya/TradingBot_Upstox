#include "tradingbot/strategy/buy_strategies.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
        candles.push_back({.instrument_key = {"NSE_EQ|INE002A01018"}, .close = close});
    }
    return candles;
}

tradingbot::strategy::StrategyContext base_context() {
    return {
        .instrument = {
            .key = {"NSE_EQ|INE002A01018"},
            .symbol = "RELIANCE",
            .enabled = true,
            .quantity = 3,
            .manual_buy_price = 101.0,
            .manual_target_price = 115.0,
            .stop_loss_pct = 5.0,
            .target_profit_pct = 10.0,
        },
        .candles = candles_from_closes({105.0, 104.0, 103.0, 102.0, 101.0}),
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 100.0},
        .portfolio = {.available_funds = 50000.0},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

void manual_buy_emits_signal_at_or_below_configured_price() {
    tradingbot::strategy::ManualBuyStrategy strategy;
    const auto evaluation = strategy.evaluate(base_context());

    require(evaluation.signals.size() == 1, "manual buy should emit a signal");
    const auto& signal = evaluation.signals.front();
    require(tradingbot::strategy::is_actionable_signal(signal), "manual buy signal should be actionable");
    require(signal.strategy_name == "manual_buy", "strategy name should be manual_buy");
    require(signal.suggested_quantity == 3, "quantity should come from instrument config");
    require(signal.suggested_entry_price == 100.0, "entry should use fresh quote");
    require_optional_near(signal.suggested_target_price, 115.0, "manual target should be retained");
    require_optional_near(signal.suggested_stop_loss, 95.0, "stop loss should derive from configured percentage");
}

void manual_buy_skips_when_price_is_too_high() {
    auto context = base_context();
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 102.0};

    const auto evaluation = tradingbot::strategy::ManualBuyStrategy{}.evaluate(context);

    require(evaluation.signals.empty(), "manual buy should skip above configured price");
    require(!evaluation.diagnostics.empty(), "manual buy skip should include diagnostic");
}

void manual_buy_falls_back_to_latest_close_when_quote_is_old() {
    auto context = base_context();
    context.quote = tradingbot::core::QuoteSnapshot{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = context.evaluated_at - std::chrono::minutes{6},
        .ltp = 150.0,
    };

    const auto evaluation = tradingbot::strategy::ManualBuyStrategy{}.evaluate(context);

    require(evaluation.signals.size() == 1, "manual buy should use latest close when quote is old");
    require(evaluation.signals.front().suggested_entry_price == 101.0, "old quote should fall back to latest close");
}

void rsi_oversold_emits_signal_when_threshold_is_met() {
    auto context = base_context();
    context.instrument.manual_target_price = std::nullopt;
    context.candles = candles_from_closes({100.0, 99.0, 98.0, 97.0, 96.0, 95.0});
    context.quote = std::nullopt;

    const auto evaluation = tradingbot::strategy::RsiOversoldBuyStrategy{{.period = 5, .threshold = 30.0}}.evaluate(context);

    require(evaluation.signals.size() == 1, "RSI oversold should emit a signal");
    const auto& signal = evaluation.signals.front();
    require(signal.strategy_name == "rsi_oversold", "strategy name should be rsi_oversold");
    require(signal.suggested_entry_price == 95.0, "entry should fall back to latest close");
    require_optional_near(signal.suggested_target_price, 104.5, "default target should derive from target profit percent");
    require_optional_near(signal.suggested_stop_loss, 90.25, "stop loss should derive from latest close");
}

void rsi_oversold_skips_when_data_is_insufficient() {
    auto context = base_context();
    context.candles = candles_from_closes({100.0, 99.0});

    const auto evaluation = tradingbot::strategy::RsiOversoldBuyStrategy{{.period = 5, .threshold = 30.0}}.evaluate(context);

    require(evaluation.signals.empty(), "RSI strategy should skip insufficient data");
    require(!evaluation.diagnostics.empty(), "RSI skip should include diagnostic");
}

void strategies_skip_disabled_instruments() {
    auto context = base_context();
    context.instrument.enabled = false;

    require(tradingbot::strategy::ManualBuyStrategy{}.evaluate(context).signals.empty(),
            "manual strategy should skip disabled instrument");
    require(tradingbot::strategy::RsiOversoldBuyStrategy{{.period = 5, .threshold = 30.0}}.evaluate(context).signals.empty(),
            "RSI strategy should skip disabled instrument");
}

}  // namespace

int main() {
    manual_buy_emits_signal_at_or_below_configured_price();
    manual_buy_skips_when_price_is_too_high();
    manual_buy_falls_back_to_latest_close_when_quote_is_old();
    rsi_oversold_emits_signal_when_threshold_is_met();
    rsi_oversold_skips_when_data_is_insufficient();
    strategies_skip_disabled_instruments();
    return 0;
}
