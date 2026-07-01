#include "tradingbot/paper/backtest_runner.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

class BuyBelowStrategy final : public tradingbot::strategy::Strategy {
public:
    std::string name() const override {
        return "buy_below";
    }

    tradingbot::strategy::StrategyEvaluation evaluate(const tradingbot::strategy::StrategyContext& context) const override {
        tradingbot::strategy::StrategyEvaluation evaluation;
        if (!context.portfolio.holdings.empty() || !context.quote || context.quote->ltp > 100.0) {
            return evaluation;
        }
        evaluation.signals.push_back({
            .instrument_key = context.instrument.key,
            .action = tradingbot::core::TradeAction::Buy,
            .confidence = 0.9,
            .suggested_quantity = 2,
            .suggested_entry_price = context.quote->ltp,
            .reason = "close is at buy threshold",
            .strategy_name = name(),
            .timestamp = context.evaluated_at,
        });
        return evaluation;
    }
};

class SellAboveStrategy final : public tradingbot::strategy::Strategy {
public:
    std::string name() const override {
        return "sell_above";
    }

    tradingbot::strategy::StrategyEvaluation evaluate(const tradingbot::strategy::StrategyContext& context) const override {
        tradingbot::strategy::StrategyEvaluation evaluation;
        if (context.portfolio.holdings.empty() || !context.quote || context.quote->ltp < 120.0) {
            return evaluation;
        }
        evaluation.signals.push_back({
            .instrument_key = context.instrument.key,
            .action = tradingbot::core::TradeAction::Sell,
            .confidence = 0.95,
            .suggested_quantity = context.portfolio.holdings.front().quantity,
            .suggested_entry_price = context.quote->ltp,
            .reason = "close reached sell threshold",
            .strategy_name = name(),
            .timestamp = context.evaluated_at,
        });
        return evaluation;
    }
};

tradingbot::core::Instrument instrument() {
    return {
        .key = {"NSE_EQ|INE002A01018"},
        .symbol = "RELIANCE",
        .enabled = true,
        .quantity = 2,
        .max_position_quantity = 10,
    };
}

std::vector<tradingbot::core::Candle> candles(std::initializer_list<double> closes) {
    std::vector<tradingbot::core::Candle> result;
    auto offset = 0;
    for (const auto close : closes) {
        result.push_back({
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .timestamp = tradingbot::core::Clock::time_point{} + std::chrono::hours{offset++},
            .open = close,
            .high = close,
            .low = close,
            .close = close,
            .volume = 1000,
            .interval = "1d",
        });
    }
    return result;
}

void replay_creates_buy_order_and_updates_portfolio() {
    BuyBelowStrategy buy;
    tradingbot::paper::BacktestRunner runner;
    const auto result = runner.run({
        .instrument = instrument(),
        .candles = candles({99.0, 105.0}),
        .starting_portfolio = {.available_funds = 1000.0},
        .strategies = {&buy},
    });

    require(result.steps.size() == 2, "runner should create one step per candle");
    require(result.steps.front().decision.has_value(), "buy step should record decision");
    require(result.steps.front().order.has_value(), "buy step should record paper order");
    require(result.steps.front().order->status == tradingbot::core::OrderStatus::Filled,
            "buy order should be locally filled");
    require(result.final_portfolio.holdings.size() == 1, "buy replay should create holding");
    require(result.final_portfolio.holdings.front().quantity == 2, "buy replay should hold filled quantity");
}

void replay_creates_sell_order_and_realized_profit() {
    SellAboveStrategy sell;
    tradingbot::paper::BacktestRunner runner;
    const auto result = runner.run({
        .instrument = instrument(),
        .candles = candles({119.0, 125.0}),
        .starting_portfolio =
            {
                .available_funds = 1000.0,
                .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 2, .average_buy_price = 100.0}},
            },
        .strategies = {&sell},
    });

    require(result.steps.back().decision.has_value(), "sell step should record decision");
    require(result.steps.back().order.has_value(), "sell step should record paper order");
    require(result.final_portfolio.holdings.empty(), "sell replay should close holding");
    require(result.realized_pnl == 50.0, "sell replay should realize profit");
    require(result.performance.total_equity == 1250.0, "final equity should include cash after sell");
}

void no_signal_replay_leaves_portfolio_unchanged() {
    tradingbot::strategy::NoopStrategy noop;
    tradingbot::paper::BacktestRunner runner;
    const auto result = runner.run({
        .instrument = instrument(),
        .candles = candles({101.0, 102.0}),
        .starting_portfolio = {.available_funds = 500.0},
        .strategies = {&noop},
    });

    require(result.steps.size() == 2, "no-signal replay should still record steps");
    require(!result.steps.front().decision.has_value(), "noop should not produce decision");
    require(!result.steps.back().order.has_value(), "noop should not produce order");
    require(result.final_portfolio.available_funds == 500.0, "noop should not change cash");
    require(result.final_portfolio.holdings.empty(), "noop should not change holdings");
}

}  // namespace

int main() {
    replay_creates_buy_order_and_updates_portfolio();
    replay_creates_sell_order_and_realized_profit();
    no_signal_replay_leaves_portfolio_unchanged();
    return 0;
}
