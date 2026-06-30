#include "tradingbot/strategy/indicators.hpp"

#include <algorithm>
#include <numeric>

namespace tradingbot::strategy {
namespace {

bool invalid_period(int period) {
    return period <= 0;
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

}  // namespace tradingbot::strategy
