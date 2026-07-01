#include "tradingbot/scan/macd_crossover_signal_scanner.hpp"

#include "tradingbot/scan/live_candle_aggregator.hpp"
#include "tradingbot/strategy/indicators.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace tradingbot::scan {
namespace {

bool valid_config(const MacdCrossoverSignalConfig& config) {
    return config.fast_period > 0 && config.slow_period > 0 && config.signal_period > 0 &&
           config.fast_period < config.slow_period;
}

double confidence(double base, bool provisional, double bonus) {
    return std::clamp(base + (provisional ? bonus : 0.0), 0.0, 1.0);
}

core::StrategySignal build_signal(const ProvisionalScanInput& input, core::TradeAction action, double signal_confidence,
                                  double latest_close, const strategy::MacdValue& current_macd,
                                  const std::string& strategy_name, bool provisional, core::TimePoint timestamp) {
    std::ostringstream reason;
    reason << (action == core::TradeAction::Buy ? "bullish" : "bearish") << " MACD histogram crossover";
    if (provisional) {
        reason << " with live provisional candle";
    }
    reason << "; macd=" << current_macd.macd << "; signal=" << current_macd.signal
           << "; histogram=" << current_macd.histogram;

    return {
        .instrument_key = input.instrument.key,
        .action = action,
        .confidence = signal_confidence,
        .suggested_quantity = input.instrument.quantity,
        .suggested_entry_price = latest_close,
        .reason = reason.str(),
        .strategy_name = strategy_name,
        .timestamp = timestamp,
    };
}

}  // namespace

MacdCrossoverSignalScanner::MacdCrossoverSignalScanner(MacdCrossoverSignalConfig config) : config_(std::move(config)) {}

std::vector<core::StrategySignal> MacdCrossoverSignalScanner::scan_one(
    const ProvisionalScanInput& input, const std::optional<core::Candle>& live_candle, core::TimePoint timestamp) const {
    if (!valid_config(config_) || !input.instrument.enabled || input.instrument.quantity <= 0 ||
        !core::is_valid_instrument_key(input.instrument.key)) {
        return {};
    }

    const auto candles = with_provisional_candle(input.historical_candles, live_candle);
    if (candles.size() < static_cast<std::size_t>(config_.slow_period + config_.signal_period + 1)) {
        return {};
    }

    const auto closes = strategy::close_prices(candles);
    const std::vector<double> previous_closes(closes.begin(), closes.end() - 1);
    const auto previous_macd = strategy::moving_average_convergence_divergence(
        previous_closes, config_.fast_period, config_.slow_period, config_.signal_period);
    const auto current_macd = strategy::moving_average_convergence_divergence(
        closes, config_.fast_period, config_.slow_period, config_.signal_period);
    if (!previous_macd || !current_macd) {
        return {};
    }

    const auto provisional = live_candle.has_value();
    if (previous_macd->histogram <= 0.0 && current_macd->histogram > 0.0) {
        return {build_signal(input, core::TradeAction::Buy,
                             confidence(config_.bullish_confidence, provisional, config_.provisional_confidence_bonus),
                             closes.back(), *current_macd, config_.bullish_strategy_name, provisional, timestamp)};
    }
    if (previous_macd->histogram >= 0.0 && current_macd->histogram < 0.0) {
        return {build_signal(input, core::TradeAction::Sell,
                             confidence(config_.bearish_confidence, provisional, config_.provisional_confidence_bonus),
                             closes.back(), *current_macd, config_.bearish_strategy_name, provisional, timestamp)};
    }
    return {};
}

std::vector<core::StrategySignal> MacdCrossoverSignalScanner::scan_many(
    const std::vector<ProvisionalScanInput>& inputs, const PartitionedLiveCandleStore& candle_store,
    core::TimePoint timestamp) const {
    std::vector<core::StrategySignal> signals;
    for (const auto& input : inputs) {
        const auto owner = candle_store.owner_for(input.instrument.key);
        const auto live_candle = candle_store.current_candle(owner, input.instrument.key);
        auto instrument_signals = scan_one(input, live_candle, timestamp);
        signals.insert(signals.end(), instrument_signals.begin(), instrument_signals.end());
    }
    return signals;
}

}  // namespace tradingbot::scan
