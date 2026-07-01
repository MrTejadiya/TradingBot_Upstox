#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <vector>

namespace tradingbot::paper {

struct PaperSimulationResult {
    bool applied{false};
    std::string rejection_reason;
    core::PortfolioState portfolio;
    core::Money realized_pnl{0.0};
};

struct PaperPerformanceSnapshot {
    core::Money available_funds{0.0};
    core::Money holdings_cost{0.0};
    core::Money holdings_market_value{0.0};
    core::Money realized_pnl{0.0};
    core::Money unrealized_pnl{0.0};
    core::Money total_equity{0.0};
};

class PaperPortfolioSimulator {
public:
    explicit PaperPortfolioSimulator(core::PortfolioState starting_portfolio = {});

    PaperSimulationResult apply_fill(const core::OrderRecord& record);
    PaperPerformanceSnapshot performance(const std::vector<core::QuoteSnapshot>& quotes) const;

    const core::PortfolioState& portfolio() const;
    core::Money realized_pnl() const;

private:
    core::PortfolioState portfolio_;
    core::Money realized_pnl_{0.0};
};

}  // namespace tradingbot::paper
