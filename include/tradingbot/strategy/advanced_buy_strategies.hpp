#pragma once

#include "tradingbot/strategy/strategy.hpp"

namespace tradingbot::strategy {

struct EmaCrossoverConfig {
    int fast_period{9};
    int slow_period{21};
};

struct BreakoutConfig {
    int lookback_period{20};
    double breakout_pct{2.0};
};

struct VolumeSurgeConfig {
    int lookback_period{20};
    double multiplier{2.0};
};

class EmaCrossoverBuyStrategy final : public Strategy {
public:
    explicit EmaCrossoverBuyStrategy(EmaCrossoverConfig config = {});

    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;

private:
    EmaCrossoverConfig config_;
};

class BreakoutBuyStrategy final : public Strategy {
public:
    explicit BreakoutBuyStrategy(BreakoutConfig config = {});

    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;

private:
    BreakoutConfig config_;
};

class VolumeSurgeBuyStrategy final : public Strategy {
public:
    explicit VolumeSurgeBuyStrategy(VolumeSurgeConfig config = {});

    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;

private:
    VolumeSurgeConfig config_;
};

}  // namespace tradingbot::strategy

