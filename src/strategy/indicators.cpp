#include "tradingbot/strategy/indicators.hpp"

#include <algorithm>
#include <numeric>

namespace tradingbot::strategy {
namespace {

bool invalid_period(int period) {
    return period <= 0;
}

bool is_pivot_low(const std::vector<double>& values, std::size_t index, int wing_size) {
    for (auto offset = 1; offset <= wing_size; ++offset) {
        if (values[index] >= values[index - static_cast<std::size_t>(offset)] ||
            values[index] >= values[index + static_cast<std::size_t>(offset)]) {
            return false;
        }
    }
    return true;
}

bool is_pivot_high(const std::vector<double>& values, std::size_t index, int wing_size) {
    for (auto offset = 1; offset <= wing_size; ++offset) {
        if (values[index] <= values[index - static_cast<std::size_t>(offset)] ||
            values[index] <= values[index + static_cast<std::size_t>(offset)]) {
            return false;
        }
    }
    return true;
}

bool has_enough_pivot_data(const std::vector<double>& values, int wing_size) {
    return !invalid_period(wing_size) && values.size() >= (static_cast<std::size_t>(wing_size) * 2U) + 1U;
}

std::optional<TrendLine> line_from_last_two_pivots(const std::vector<PivotPoint>& pivots) {
    if (pivots.size() < 2U) {
        return std::nullopt;
    }

    const auto& start = pivots[pivots.size() - 2U];
    const auto& end = pivots.back();
    const auto delta_index = static_cast<double>(end.index) - static_cast<double>(start.index);
    if (delta_index == 0.0) {
        return std::nullopt;
    }

    const auto slope = (end.value - start.value) / delta_index;
    const auto intercept = start.value - (slope * static_cast<double>(start.index));
    return TrendLine{.start_index = start.index, .end_index = end.index, .slope = slope, .intercept = intercept};
}

std::optional<std::vector<double>> exponential_moving_average_series(const std::vector<double>& values, int period) {
    if (invalid_period(period) || values.size() < static_cast<std::size_t>(period)) {
        return std::nullopt;
    }

    std::vector<double> series;
    const std::vector<double> seed_values(values.begin(), values.begin() + period);
    const auto seed = simple_moving_average(seed_values, period);
    if (!seed) {
        return std::nullopt;
    }

    const auto multiplier = 2.0 / (static_cast<double>(period) + 1.0);
    auto ema = *seed;
    series.push_back(ema);
    for (auto index = static_cast<std::size_t>(period); index < values.size(); ++index) {
        ema = ((values[index] - ema) * multiplier) + ema;
        series.push_back(ema);
    }
    return series;
}

}  // namespace

std::vector<double> close_prices(const std::vector<core::Candle>& candles) {
    std::vector<double> values;
    values.reserve(candles.size());
    for (const auto& candle : candles) {
        values.push_back(candle.close);
    }
    return values;
}

std::optional<double> simple_moving_average(const std::vector<double>& values, int period) {
    if (invalid_period(period) || values.size() < static_cast<std::size_t>(period)) {
        return std::nullopt;
    }

    const auto begin = values.end() - period;
    const auto sum = std::accumulate(begin, values.end(), 0.0);
    return sum / static_cast<double>(period);
}

std::optional<double> exponential_moving_average(const std::vector<double>& values, int period) {
    auto series = exponential_moving_average_series(values, period);
    if (!series || series->empty()) {
        return std::nullopt;
    }
    return series->back();
}

std::optional<double> relative_strength_index(const std::vector<double>& values, int period) {
    if (invalid_period(period) || values.size() <= static_cast<std::size_t>(period)) {
        return std::nullopt;
    }

    double average_gain = 0.0;
    double average_loss = 0.0;
    for (auto index = std::size_t{1}; index <= static_cast<std::size_t>(period); ++index) {
        const auto change = values[index] - values[index - 1];
        if (change >= 0.0) {
            average_gain += change;
        } else {
            average_loss += -change;
        }
    }
    average_gain /= static_cast<double>(period);
    average_loss /= static_cast<double>(period);

    for (auto index = static_cast<std::size_t>(period) + 1; index < values.size(); ++index) {
        const auto change = values[index] - values[index - 1];
        const auto gain = std::max(change, 0.0);
        const auto loss = std::max(-change, 0.0);
        average_gain = ((average_gain * (period - 1)) + gain) / static_cast<double>(period);
        average_loss = ((average_loss * (period - 1)) + loss) / static_cast<double>(period);
    }

    if (average_loss == 0.0) {
        return 100.0;
    }
    if (average_gain == 0.0) {
        return 0.0;
    }

    const auto relative_strength = average_gain / average_loss;
    return 100.0 - (100.0 / (1.0 + relative_strength));
}

std::optional<MacdValue> moving_average_convergence_divergence(const std::vector<double>& values, int fast_period,
                                                               int slow_period, int signal_period) {
    if (invalid_period(fast_period) || invalid_period(slow_period) || invalid_period(signal_period) ||
        fast_period >= slow_period) {
        return std::nullopt;
    }

    const auto fast_series = exponential_moving_average_series(values, fast_period);
    const auto slow_series = exponential_moving_average_series(values, slow_period);
    if (!fast_series || !slow_series || slow_series->empty()) {
        return std::nullopt;
    }

    const auto offset = fast_series->size() - slow_series->size();
    std::vector<double> macd_series;
    macd_series.reserve(slow_series->size());
    for (std::size_t index = 0; index < slow_series->size(); ++index) {
        macd_series.push_back((*fast_series)[index + offset] - (*slow_series)[index]);
    }

    const auto signal = exponential_moving_average(macd_series, signal_period);
    if (!signal) {
        return std::nullopt;
    }

    const auto macd = macd_series.back();
    return MacdValue{.macd = macd, .signal = *signal, .histogram = macd - *signal};
}

std::vector<PivotPoint> pivot_lows(const std::vector<double>& values, int wing_size) {
    std::vector<PivotPoint> pivots;
    if (!has_enough_pivot_data(values, wing_size)) {
        return pivots;
    }

    for (auto index = static_cast<std::size_t>(wing_size); index + static_cast<std::size_t>(wing_size) < values.size();
         ++index) {
        if (is_pivot_low(values, index, wing_size)) {
            pivots.push_back(PivotPoint{.index = index, .value = values[index]});
        }
    }
    return pivots;
}

std::vector<PivotPoint> pivot_highs(const std::vector<double>& values, int wing_size) {
    std::vector<PivotPoint> pivots;
    if (!has_enough_pivot_data(values, wing_size)) {
        return pivots;
    }

    for (auto index = static_cast<std::size_t>(wing_size); index + static_cast<std::size_t>(wing_size) < values.size();
         ++index) {
        if (is_pivot_high(values, index, wing_size)) {
            pivots.push_back(PivotPoint{.index = index, .value = values[index]});
        }
    }
    return pivots;
}

bool has_bullish_divergence(const std::vector<double>& prices, const std::vector<double>& oscillator, int wing_size) {
    if (prices.size() != oscillator.size()) {
        return false;
    }

    const auto lows = pivot_lows(prices, wing_size);
    if (lows.size() < 2U) {
        return false;
    }

    const auto& previous = lows[lows.size() - 2U];
    const auto& current = lows.back();
    return current.value < previous.value && oscillator[current.index] > oscillator[previous.index];
}

bool has_bearish_divergence(const std::vector<double>& prices, const std::vector<double>& oscillator, int wing_size) {
    if (prices.size() != oscillator.size()) {
        return false;
    }

    const auto highs = pivot_highs(prices, wing_size);
    if (highs.size() < 2U) {
        return false;
    }

    const auto& previous = highs[highs.size() - 2U];
    const auto& current = highs.back();
    return current.value > previous.value && oscillator[current.index] < oscillator[previous.index];
}

std::optional<TrendLine> support_trendline(const std::vector<double>& values, int wing_size) {
    return line_from_last_two_pivots(pivot_lows(values, wing_size));
}

std::optional<TrendLine> resistance_trendline(const std::vector<double>& values, int wing_size) {
    return line_from_last_two_pivots(pivot_highs(values, wing_size));
}

double trendline_value_at(const TrendLine& line, std::size_t index) {
    return (line.slope * static_cast<double>(index)) + line.intercept;
}

}  // namespace tradingbot::strategy
