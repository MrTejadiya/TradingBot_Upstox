#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/paper/paper_portfolio_simulator.hpp"
#include "tradingbot/strategy/signal_aggregation.hpp"
#include "tradingbot/strategy/strategy.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::paper {

struct BacktestRequest {
    core::Instrument instrument;
    std::vector<core::Candle> candles;
    core::PortfolioState starting_portfolio;
    std::vector<const strategy::Strategy*> strategies;
    strategy::SignalAggregationMode aggregation_mode{strategy::SignalAggregationMode::HighestConfidence};
    std::string run_id{"paper-backtest"};
};

struct BacktestStep {
    core::TimePoint evaluated_at{};
    std::vector<core::StrategySignal> signals;
    std::optional<core::Decision> decision;
    std::optional<core::OrderRecord> order;
    std::vector<std::string> diagnostics;
    core::PortfolioState portfolio;
};

struct BacktestResult {
    std::vector<BacktestStep> steps;
    core::PortfolioState final_portfolio;
    core::Money realized_pnl{0.0};
    PaperPerformanceSnapshot performance;
    std::vector<std::string> diagnostics;
};

class BacktestRunner {
public:
    BacktestResult run(const BacktestRequest& request) const;
};

}  // namespace tradingbot::paper
