#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <vector>

namespace tradingbot::strategy {

struct MacdValue {
    double macd{0.0};
    double signal{0.0};
    double histogram{0.0};
};

std::vector<double> close_prices(const std::vector<core::Candle>& candles);
std::optional<double> simple_moving_average(const std::vector<double>& values, int period);
std::optional<double> exponential_moving_average(const std::vector<double>& values, int period);
std::optional<double> relative_strength_index(const std::vector<double>& values, int period = 14);
std::optional<MacdValue> moving_average_convergence_divergence(const std::vector<double>& values, int fast_period = 12,
                                                               int slow_period = 26, int signal_period = 9);

}  // namespace tradingbot::strategy

