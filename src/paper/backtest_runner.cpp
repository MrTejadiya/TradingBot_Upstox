#include "tradingbot/paper/backtest_runner.hpp"

#include <sstream>

namespace tradingbot::paper {
namespace {

core::QuoteSnapshot quote_from_candle(const core::Candle& candle) {
    return {
        .instrument_key = candle.instrument_key,
        .timestamp = candle.timestamp,
        .ltp = candle.close,
    };
}

std::optional<core::OrderSide> side_for_decision(core::DecisionType type) {
    if (type == core::DecisionType::Buy) {
        return core::OrderSide::Buy;
    }
    if (type == core::DecisionType::Sell) {
        return core::OrderSide::Sell;
    }
    return std::nullopt;
}

std::string order_id(std::size_t index) {
    std::ostringstream out;
    out << "paper-backtest-" << (index + 1U);
    return out.str();
}

core::OrderRecord order_from_decision(const core::Decision& decision, core::OrderSide side, core::Money fallback_price,
                                      const std::string& run_id, std::size_t index) {
    const auto price = decision.price.value_or(fallback_price);
    return {
        .request =
            {
                .instrument_key = decision.instrument_key,
                .side = side,
                .quantity = decision.quantity,
                .price = price,
                .tag = "paper-backtest",
                .source_strategy = decision.source,
                .run_id = run_id,
            },
        .broker_order_id = order_id(index),
        .status = core::OrderStatus::Filled,
        .filled_quantity = decision.quantity,
        .average_fill_price = price,
        .redacted_response_metadata = "paper_backtest=true",
        .updated_at = decision.timestamp,
    };
}

}  // namespace

BacktestResult BacktestRunner::run(const BacktestRequest& request) const {
    BacktestResult result;
    PaperPortfolioSimulator simulator(request.starting_portfolio);
    std::vector<core::QuoteSnapshot> latest_quotes;
    latest_quotes.reserve(1);

    for (std::size_t index = 0; index < request.candles.size(); ++index) {
        const auto& candle = request.candles[index];
        auto quote = quote_from_candle(candle);
        latest_quotes = {quote};

        BacktestStep step{
            .evaluated_at = candle.timestamp,
            .portfolio = simulator.portfolio(),
        };

        strategy::StrategyContext context{
            .instrument = request.instrument,
            .candles = std::vector<core::Candle>(request.candles.begin(), request.candles.begin() + index + 1),
            .quote = quote,
            .portfolio = simulator.portfolio(),
            .evaluated_at = candle.timestamp,
        };

        for (const auto* strategy : request.strategies) {
            if (!strategy) {
                step.diagnostics.push_back("ignored null strategy");
                continue;
            }
            const auto evaluation = strategy->evaluate(context);
            step.signals.insert(step.signals.end(), evaluation.signals.begin(), evaluation.signals.end());
            step.diagnostics.insert(step.diagnostics.end(), evaluation.diagnostics.begin(), evaluation.diagnostics.end());
        }

        const auto aggregation = strategy::aggregate_signals({
            .instrument_key = request.instrument.key,
            .signals = step.signals,
            .mode = request.aggregation_mode,
            .decided_at = candle.timestamp,
        });
        step.diagnostics.insert(step.diagnostics.end(), aggregation.diagnostics.begin(), aggregation.diagnostics.end());
        step.decision = aggregation.decision;

        if (step.decision) {
            const auto side = side_for_decision(step.decision->type);
            if (!side) {
                step.diagnostics.push_back("ignored hold decision");
            } else {
                auto order = order_from_decision(*step.decision, *side, candle.close, request.run_id, index);
                auto simulation = simulator.apply_fill(order);
                if (!simulation.applied) {
                    order.status = core::OrderStatus::Rejected;
                    order.rejection_reason =
                        simulation.rejection_reason.empty() ? "paper simulation rejected order" : simulation.rejection_reason;
                    order.filled_quantity = 0;
                    order.average_fill_price = std::nullopt;
                    step.diagnostics.push_back(order.rejection_reason);
                }
                step.order = order;
            }
        }

        step.portfolio = simulator.portfolio();
        result.steps.push_back(step);
    }

    result.final_portfolio = simulator.portfolio();
    result.realized_pnl = simulator.realized_pnl();
    result.performance = simulator.performance(latest_quotes);
    return result;
}

}  // namespace tradingbot::paper
