#pragma once

#include "tradingbot/strategy/strategy.hpp"

namespace tradingbot::strategy {

class TargetProfitSellStrategy final : public Strategy {
public:
    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;
};

class StopLossSellStrategy final : public Strategy {
public:
    std::string name() const override;
    StrategyEvaluation evaluate(const StrategyContext& context) const override;
};

}  // namespace tradingbot::strategy

