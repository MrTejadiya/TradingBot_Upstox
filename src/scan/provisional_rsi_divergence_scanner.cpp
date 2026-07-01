#include "tradingbot/scan/provisional_rsi_divergence_scanner.hpp"

#include "tradingbot/runtime/worker_group.hpp"
#include "tradingbot/strategy/indicators.hpp"

#include <algorithm>
#include <mutex>
#include <optional>

namespace tradingbot::scan {
namespace {

std::vector<std::optional<double>> rsi_series(const std::vector<double>& values, int period) {
    std::vector<std::optional<double>> output(values.size());
    if (period <= 0 || values.size() <= static_cast<std::size_t>(period)) {
        return output;
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

    const auto current_rsi = [&] {
        if (average_loss == 0.0) {
            return 100.0;
        }
        if (average_gain == 0.0) {
            return 0.0;
        }
        const auto relative_strength = average_gain / average_loss;
        return 100.0 - (100.0 / (1.0 + relative_strength));
    };

    output[static_cast<std::size_t>(period)] = current_rsi();
    for (auto index = static_cast<std::size_t>(period) + 1; index < values.size(); ++index) {
        const auto change = values[index] - values[index - 1];
        const auto gain = std::max(change, 0.0);
        const auto loss = std::max(-change, 0.0);
        average_gain = ((average_gain * static_cast<double>(period - 1)) + gain) / static_cast<double>(period);
        average_loss = ((average_loss * static_cast<double>(period - 1)) + loss) / static_cast<double>(period);
        output[index] = current_rsi();
    }
    return output;
}

bool has_bullish_divergence(const std::vector<double>& prices, const std::vector<std::optional<double>>& oscillator,
                            int wing_size) {
    const auto lows = strategy::pivot_lows(prices, wing_size);
    std::vector<strategy::PivotPoint> valid_lows;
    for (const auto& low : lows) {
        if (low.index < oscillator.size() && oscillator[low.index]) {
            valid_lows.push_back(low);
        }
    }
    if (valid_lows.size() < 2U) {
        return false;
    }
    const auto& previous = valid_lows[valid_lows.size() - 2U];
    const auto& current = valid_lows.back();
    return current.value < previous.value && *oscillator[current.index] > *oscillator[previous.index];
}

bool has_bearish_divergence(const std::vector<double>& prices, const std::vector<std::optional<double>>& oscillator,
                            int wing_size) {
    const auto highs = strategy::pivot_highs(prices, wing_size);
    std::vector<strategy::PivotPoint> valid_highs;
    for (const auto& high : highs) {
        if (high.index < oscillator.size() && oscillator[high.index]) {
            valid_highs.push_back(high);
        }
    }
    if (valid_highs.size() < 2U) {
        return false;
    }
    const auto& previous = valid_highs[valid_highs.size() - 2U];
    const auto& current = valid_highs.back();
    return current.value > previous.value && *oscillator[current.index] < *oscillator[previous.index];
}

}  // namespace

ProvisionalRsiDivergenceScanner::ProvisionalRsiDivergenceScanner(ProvisionalRsiDivergenceConfig config)
    : config_(config) {
    if (config_.worker_count == 0) {
        config_.worker_count = 1;
    }
}

ProvisionalDivergenceResult ProvisionalRsiDivergenceScanner::scan_one(
    const ProvisionalScanInput& input, const LiveCandleAggregator& aggregator) const {
    const auto live_candle = aggregator.current_candle(input.instrument.key);
    auto candles = with_provisional_candle(input.historical_candles, live_candle);
    const auto provisional = live_candle.has_value();
    ProvisionalDivergenceResult result{
        .instrument_key = input.instrument.key,
        .symbol = input.instrument.symbol,
        .provisional = provisional,
        .candle_count = candles.size(),
    };

    if (config_.rsi_period <= 0 || config_.wing_size <= 0) {
        result.diagnostic = "invalid RSI divergence scanner configuration";
        return result;
    }
    if (candles.size() <= static_cast<std::size_t>(config_.rsi_period + (config_.wing_size * 2))) {
        result.diagnostic = "insufficient candles for RSI divergence scan";
        return result;
    }

    const auto closes = strategy::close_prices(candles);
    const auto rsi = rsi_series(closes, config_.rsi_period);
    const auto latest_rsi = rsi.empty() ? std::optional<double>{} : rsi.back();
    if (!latest_rsi) {
        result.diagnostic = "latest RSI is unavailable";
        return result;
    }

    result.ok = true;
    result.latest_close = closes.back();
    result.latest_rsi = *latest_rsi;
    result.bullish_divergence = has_bullish_divergence(closes, rsi, config_.wing_size);
    result.bearish_divergence = has_bearish_divergence(closes, rsi, config_.wing_size);
    result.diagnostic = result.provisional ? "provisional scan includes live candle" : "scan uses closed candles only";
    return result;
}

std::vector<ProvisionalDivergenceResult> ProvisionalRsiDivergenceScanner::scan_parallel(
    const std::vector<ProvisionalScanInput>& inputs, const LiveCandleAggregator& aggregator) const {
    std::vector<ProvisionalDivergenceResult> results(inputs.size());
    runtime::WorkerGroup workers(config_.worker_count);
    std::mutex result_mutex;

    for (auto index = std::size_t{0}; index < inputs.size(); ++index) {
        workers.submit([&, index] {
            auto result = scan_one(inputs[index], aggregator);
            std::lock_guard lock(result_mutex);
            results[index] = std::move(result);
        });
    }
    workers.drain();

    const auto errors = workers.errors();
    if (!errors.empty()) {
        for (auto& result : results) {
            if (result.instrument_key.value.empty()) {
                result.diagnostic = errors.front();
            }
        }
    }
    return results;
}

}  // namespace tradingbot::scan
