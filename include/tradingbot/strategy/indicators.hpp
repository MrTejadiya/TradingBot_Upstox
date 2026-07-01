#pragma once

#include "tradingbot/core/domain.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace tradingbot::strategy {

struct MacdValue {
    double macd{0.0};
    double signal{0.0};
    double histogram{0.0};
};

struct PivotPoint {
    std::size_t index{0};
    double value{0.0};
};

struct TrendLine {
    std::size_t start_index{0};
    std::size_t end_index{0};
    double slope{0.0};
    double intercept{0.0};
};

std::vector<double> close_prices(const std::vector<core::Candle>& candles);
std::optional<double> simple_moving_average(const std::vector<double>& values, int period);
std::optional<double> exponential_moving_average(const std::vector<double>& values, int period);
std::optional<double> relative_strength_index(const std::vector<double>& values, int period = 14);
std::optional<MacdValue> moving_average_convergence_divergence(const std::vector<double>& values, int fast_period = 12,
                                                               int slow_period = 26, int signal_period = 9);
std::vector<PivotPoint> pivot_lows(const std::vector<double>& values, int wing_size = 1);
std::vector<PivotPoint> pivot_highs(const std::vector<double>& values, int wing_size = 1);
bool has_bullish_divergence(const std::vector<double>& prices, const std::vector<double>& oscillator,
                            int wing_size = 1);
bool has_bearish_divergence(const std::vector<double>& prices, const std::vector<double>& oscillator,
                            int wing_size = 1);
std::optional<TrendLine> support_trendline(const std::vector<double>& values, int wing_size = 1);
std::optional<TrendLine> resistance_trendline(const std::vector<double>& values, int wing_size = 1);
double trendline_value_at(const TrendLine& line, std::size_t index);

}  // namespace tradingbot::strategy
