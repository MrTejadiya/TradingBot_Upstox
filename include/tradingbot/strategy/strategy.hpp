#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::strategy {

struct StrategyContext {
    core::Instrument instrument;
    std::vector<core::Candle> candles;
    std::optional<core::QuoteSnapshot> quote;
    core::PortfolioState portfolio;
    core::TimePoint evaluated_at{};
};

struct StrategyEvaluation {
    std::vector<core::StrategySignal> signals;
    std::vector<std::string> diagnostics;
};

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::string name() const = 0;
    virtual StrategyEvaluation evaluate(const StrategyContext& context) const = 0;
};

class NoopStrategy final : public Strategy {
public:
    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;
};

bool has_minimum_candles(const StrategyContext& context, std::size_t minimum_count);
std::optional<core::Money> latest_close(const StrategyContext& context);
bool is_actionable_signal(const core::StrategySignal& signal);

}  // namespace tradingbot::strategy

