#include "tradingbot/strategy/sell_strategies.hpp"

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
        },
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .close = 100.0}},
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

}  // namespace

int main() {
    target_profit_emits_sell_signal_when_target_is_reached();
    target_profit_uses_default_profit_percentage();
    stop_loss_emits_sell_signal_when_price_breaches_stop();
    sell_strategies_skip_without_holding();
    stop_loss_skips_when_not_configured();
    return 0;
}

