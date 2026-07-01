#include "tradingbot/strategy/sell_strategies.hpp"

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

tradingbot::strategy::StrategyContext base_context() {
    return {
        .instrument = {
            .key = {"NSE_EQ|INE002A01018"},
            .symbol = "RELIANCE",
            .enabled = true,
            .quantity = 3,
            .manual_target_price = 115.0,
            .stop_loss_pct = 5.0,
            .target_profit_pct = 10.0,
            .trailing_stop_pct = 4.0,
        },
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .high = 120.0, .close = 100.0}},
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 116.0},
        .portfolio = {.available_funds = 50000.0,
                      .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 7, .average_buy_price = 100.0}}},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

void target_profit_emits_sell_signal_when_target_is_reached() {
    const auto evaluation = tradingbot::strategy::TargetProfitSellStrategy{}.evaluate(base_context());

    require(evaluation.signals.size() == 1, "target sell should emit one signal");
    const auto& signal = evaluation.signals.front();
    require(tradingbot::strategy::is_actionable_signal(signal), "target sell signal should be actionable");
    require(signal.action == tradingbot::core::TradeAction::Sell, "target signal should be sell action");
    require(signal.strategy_name == "target_profit_sell", "target strategy name should be present");
    require(signal.suggested_quantity == 7, "target sell quantity should come from holding");
    require(signal.suggested_entry_price == 116.0, "target sell should use fresh quote price");
}

void target_profit_uses_default_profit_percentage() {
    auto context = base_context();
    context.instrument.manual_target_price = std::nullopt;
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 111.0};

    const auto evaluation = tradingbot::strategy::TargetProfitSellStrategy{}.evaluate(context);

    require(evaluation.signals.size() == 1, "target sell should use target profit percent");
}

void target_profit_falls_back_to_latest_close_when_quote_is_old() {
    auto context = base_context();
    context.instrument.manual_target_price = 99.0;
    context.quote = tradingbot::core::QuoteSnapshot{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = context.evaluated_at - std::chrono::minutes{6},
        .ltp = 90.0,
    };

    const auto evaluation = tradingbot::strategy::TargetProfitSellStrategy{}.evaluate(context);

    require(evaluation.signals.size() == 1, "target sell should use latest close when quote is old");
    require(evaluation.signals.front().suggested_entry_price == 100.0, "old quote should fall back to latest close");
}

void stop_loss_emits_sell_signal_when_price_breaches_stop() {
    auto context = base_context();
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 94.0};

    const auto evaluation = tradingbot::strategy::StopLossSellStrategy{}.evaluate(context);

    require(evaluation.signals.size() == 1, "stop loss should emit one signal");
    const auto& signal = evaluation.signals.front();
    require(signal.action == tradingbot::core::TradeAction::Sell, "stop loss signal should be sell action");
    require(signal.strategy_name == "stop_loss_sell", "stop loss strategy name should be present");
    require(signal.suggested_quantity == 7, "stop loss quantity should come from holding");
}

void sell_strategies_skip_without_holding() {
    auto context = base_context();
    context.portfolio.holdings.clear();

    const auto target = tradingbot::strategy::TargetProfitSellStrategy{}.evaluate(context);
    const auto stop = tradingbot::strategy::StopLossSellStrategy{}.evaluate(context);

    require(target.signals.empty(), "target strategy should skip without holding");
    require(stop.signals.empty(), "stop loss strategy should skip without holding");
    require(!target.diagnostics.empty(), "target skip should include diagnostic");
    require(!stop.diagnostics.empty(), "stop skip should include diagnostic");
}

void stop_loss_skips_when_not_configured() {
    auto context = base_context();
    context.instrument.stop_loss_pct = std::nullopt;

    const auto evaluation = tradingbot::strategy::StopLossSellStrategy{}.evaluate(context);

    require(evaluation.signals.empty(), "stop loss should skip without configured percentage");
    require(!evaluation.diagnostics.empty(), "stop loss skip should include diagnostic");
}

void trailing_stop_emits_sell_signal_after_reversal_from_high() {
    auto context = base_context();
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 115.0};

    const auto evaluation = tradingbot::strategy::TrailingStopSellStrategy{}.evaluate(context);

    require(evaluation.signals.size() == 1, "trailing stop should emit one signal");
    const auto& signal = evaluation.signals.front();
    require(signal.action == tradingbot::core::TradeAction::Sell, "trailing stop signal should be sell action");
    require(signal.strategy_name == "trailing_stop_sell", "trailing stop strategy name should be present");
    require(signal.suggested_quantity == 7, "trailing stop quantity should come from holding");
    require(signal.reason.find("trailing stop") != std::string::npos, "reason should explain trailing stop");
}

void trailing_stop_skips_before_threshold_is_breached() {
    auto context = base_context();
    context.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 118.0};

    const auto evaluation = tradingbot::strategy::TrailingStopSellStrategy{}.evaluate(context);

    require(evaluation.signals.empty(), "trailing stop should skip above trailing threshold");
    require(!evaluation.diagnostics.empty(), "trailing stop skip should include diagnostic");
}

void trailing_stop_skips_when_not_configured() {
    auto context = base_context();
    context.instrument.trailing_stop_pct = std::nullopt;

    const auto evaluation = tradingbot::strategy::TrailingStopSellStrategy{}.evaluate(context);

    require(evaluation.signals.empty(), "trailing stop should skip without configured percentage");
    require(!evaluation.diagnostics.empty(), "trailing stop skip should include diagnostic");
}

}  // namespace

int main() {
    target_profit_emits_sell_signal_when_target_is_reached();
    target_profit_uses_default_profit_percentage();
    target_profit_falls_back_to_latest_close_when_quote_is_old();
    stop_loss_emits_sell_signal_when_price_breaches_stop();
    sell_strategies_skip_without_holding();
    stop_loss_skips_when_not_configured();
    trailing_stop_emits_sell_signal_after_reversal_from_high();
    trailing_stop_skips_before_threshold_is_breached();
    trailing_stop_skips_when_not_configured();
    return 0;
}
