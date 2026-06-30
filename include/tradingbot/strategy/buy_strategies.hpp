#pragma once

#include "tradingbot/strategy/strategy.hpp"

namespace tradingbot::strategy {

struct RsiOversoldConfig {
    int period{14};
    double threshold{30.0};
};

class ManualBuyStrategy final : public Strategy {
public:
    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;
};

class RsiOversoldBuyStrategy final : public Strategy {
public:
    explicit RsiOversoldBuyStrategy(RsiOversoldConfig config = {});

    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;

private:
    RsiOversoldConfig config_;
};

}  // namespace tradingbot::strategy

