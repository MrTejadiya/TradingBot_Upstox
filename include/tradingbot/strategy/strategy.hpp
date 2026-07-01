#pragma once

#include "tradingbot/core/domain.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::strategy {

struct StrategyContext {
    core::Instrument instrument;
    std::vector<core::Candle> candles;
    std::optional<core::QuoteSnapshot> quote;
    core::PortfolioState portfolio;
    std::optional<std::chrono::seconds> max_quote_age;
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
std::chrono::seconds quote_freshness_window(const std::optional<std::chrono::seconds>& override);
bool is_usable_quote(const core::QuoteSnapshot& quote, core::TimePoint evaluated_at,
                     std::chrono::seconds max_age = std::chrono::minutes{5});
bool is_actionable_signal(const core::StrategySignal& signal);

}  // namespace tradingbot::strategy
