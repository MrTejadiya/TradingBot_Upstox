#include "tradingbot/scan/rsi_divergence_signal_mapper.hpp"

#include <algorithm>
#include <sstream>

namespace tradingbot::scan {
namespace {

double confidence(double base, bool provisional, double bonus) {
    return std::clamp(base + (provisional ? bonus : 0.0), 0.0, 1.0);
}

std::string reason_for(const ProvisionalDivergenceResult& result, const std::string& direction) {
    std::ostringstream out;
    out << direction << " RSI divergence";
    if (result.provisional) {
        out << " with live provisional candle";
    }
    out << "; latest_close=" << result.latest_close << "; latest_rsi=" << result.latest_rsi;
    return out.str();
}

core::StrategySignal signal_for(const ProvisionalScanInput& input, const ProvisionalDivergenceResult& result,
                                core::TradeAction action, double signal_confidence,
                                const RsiDivergenceSignalConfig& config, core::TimePoint timestamp,
                                const std::string& direction) {
    return {
        .instrument_key = input.instrument.key,
        .action = action,
        .confidence = signal_confidence,
        .suggested_quantity = input.instrument.quantity,
        .suggested_entry_price = result.latest_close,
        .reason = reason_for(result, direction),
        .strategy_name = config.strategy_name,
        .timestamp = timestamp,
    };
}

}  // namespace

std::vector<core::StrategySignal> map_rsi_divergence_signals(
    const std::vector<ProvisionalScanInput>& inputs, const std::vector<ProvisionalDivergenceResult>& results,
    core::TimePoint timestamp, RsiDivergenceSignalConfig config) {
    if (inputs.size() != results.size()) {
        return {};
    }

    std::vector<core::StrategySignal> signals;
    for (auto index = std::size_t{0}; index < results.size(); ++index) {
        const auto& input = inputs[index];
        const auto& result = results[index];
        if (!result.ok || result.instrument_key.value != input.instrument.key.value) {
            continue;
        }

        if (result.bullish_divergence) {
            signals.push_back(signal_for(input, result, core::TradeAction::Buy,
                                         confidence(config.bullish_confidence, result.provisional,
                                                    config.provisional_confidence_bonus),
                                         config, timestamp, "bullish"));
        }
        if (result.bearish_divergence) {
            signals.push_back(signal_for(input, result, core::TradeAction::Sell,
                                         confidence(config.bearish_confidence, result.provisional,
                                                    config.provisional_confidence_bonus),
                                         config, timestamp, "bearish"));
        }
    }
    return signals;
}

}  // namespace tradingbot::scan
