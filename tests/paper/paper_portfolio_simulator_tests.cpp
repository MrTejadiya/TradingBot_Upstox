#include "tradingbot/paper/paper_portfolio_simulator.hpp"

#include <cmath>
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

void require_near(double actual, double expected, double tolerance, const std::string& message) {
    require(std::fabs(actual - expected) <= tolerance, message);
}

tradingbot::core::OrderRecord filled_order(tradingbot::core::OrderSide side, tradingbot::core::Quantity quantity,
                                           tradingbot::core::Money price) {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = side,
                .quantity = quantity,
                .price = price,
                .tag = "paper-test",
                .source_strategy = "test",
                .run_id = "run-1",
            },
        .broker_order_id = "paper-1",
        .status = tradingbot::core::OrderStatus::Filled,
        .filled_quantity = quantity,
        .average_fill_price = price,
        .updated_at = tradingbot::core::Clock::now(),
    };
}

void buy_fill_reduces_cash_and_adds_holding() {
    tradingbot::paper::PaperPortfolioSimulator simulator({.available_funds = 10000.0});

    const auto result = simulator.apply_fill(filled_order(tradingbot::core::OrderSide::Buy, 5, 100.0));

    require(result.applied, "buy fill should apply");
    require_near(result.portfolio.available_funds, 9500.0, 0.0001, "buy should reduce cash");
    require(result.portfolio.holdings.size() == 1, "buy should add holding");
    require(result.portfolio.holdings.front().quantity == 5, "buy should add filled quantity");
    require_near(result.portfolio.holdings.front().average_buy_price, 100.0, 0.0001,
                 "buy should set average price");
}

void buy_fill_averages_existing_holding() {
    tradingbot::core::PortfolioState starting{
        .available_funds = 10000.0,
        .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 5, .average_buy_price = 100.0}},
    };
    tradingbot::paper::PaperPortfolioSimulator simulator(starting);

    const auto result = simulator.apply_fill(filled_order(tradingbot::core::OrderSide::Buy, 5, 120.0));

    require(result.applied, "second buy should apply");
    require(result.portfolio.holdings.front().quantity == 10, "buy should combine quantities");
    require_near(result.portfolio.holdings.front().average_buy_price, 110.0, 0.0001,
                 "buy should weighted-average price");
}

void sell_fill_reduces_holding_and_records_realized_pnl() {
    tradingbot::core::PortfolioState starting{
        .available_funds = 1000.0,
        .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 5, .average_buy_price = 100.0}},
    };
    tradingbot::paper::PaperPortfolioSimulator simulator(starting);

    const auto result = simulator.apply_fill(filled_order(tradingbot::core::OrderSide::Sell, 3, 125.0));

    require(result.applied, "sell fill should apply");
    require_near(result.portfolio.available_funds, 1375.0, 0.0001, "sell should increase cash");
    require(result.portfolio.holdings.front().quantity == 2, "sell should reduce holding");
    require_near(result.realized_pnl, 75.0, 0.0001, "sell should record realized profit");
}

void ignored_order_statuses_do_not_mutate_portfolio() {
    tradingbot::paper::PaperPortfolioSimulator simulator({.available_funds = 1000.0});
    auto rejected = filled_order(tradingbot::core::OrderSide::Buy, 2, 100.0);
    rejected.status = tradingbot::core::OrderStatus::Rejected;
    rejected.rejection_reason = "risk rejected";

    const auto result = simulator.apply_fill(rejected);

    require(!result.applied, "rejected order should not apply");
    require_near(simulator.portfolio().available_funds, 1000.0, 0.0001, "rejected order should not change cash");
    require(simulator.portfolio().holdings.empty(), "rejected order should not add holding");
}

void insufficient_cash_and_holdings_fail_closed() {
    tradingbot::paper::PaperPortfolioSimulator cash_limited({.available_funds = 100.0});
    const auto buy = cash_limited.apply_fill(filled_order(tradingbot::core::OrderSide::Buy, 2, 100.0));
    require(!buy.applied, "cash-limited buy should reject");
    require(buy.rejection_reason == "insufficient paper funds", "cash-limited buy should explain rejection");

    tradingbot::paper::PaperPortfolioSimulator holdings_limited({.available_funds = 1000.0});
    const auto sell = holdings_limited.apply_fill(filled_order(tradingbot::core::OrderSide::Sell, 1, 100.0));
    require(!sell.applied, "sell without holding should reject");
    require(sell.rejection_reason == "insufficient paper holding", "sell without holding should explain rejection");
}

void calculates_unrealized_and_total_equity_from_quotes() {
    tradingbot::core::PortfolioState starting{
        .available_funds = 1000.0,
        .holdings = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 4, .average_buy_price = 100.0}},
    };
    tradingbot::paper::PaperPortfolioSimulator simulator(starting);

    const auto snapshot = simulator.performance({{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .ltp = 125.0,
    }});

    require_near(snapshot.holdings_cost, 400.0, 0.0001, "snapshot should include holding cost");
    require_near(snapshot.holdings_market_value, 500.0, 0.0001, "snapshot should mark holdings to quote");
    require_near(snapshot.unrealized_pnl, 100.0, 0.0001, "snapshot should calculate unrealized PnL");
    require_near(snapshot.total_equity, 1500.0, 0.0001, "snapshot should combine cash and market value");
}

}  // namespace

int main() {
    buy_fill_reduces_cash_and_adds_holding();
    buy_fill_averages_existing_holding();
    sell_fill_reduces_holding_and_records_realized_pnl();
    ignored_order_statuses_do_not_mutate_portfolio();
    insufficient_cash_and_holdings_fail_closed();
    calculates_unrealized_and_total_equity_from_quotes();
    return 0;
}
