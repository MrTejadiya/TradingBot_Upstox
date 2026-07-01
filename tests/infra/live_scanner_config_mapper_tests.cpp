#include "tradingbot/infra/live_scanner_config_mapper.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void maps_full_live_scanner_config() {
    tradingbot::infra::LiveScannerConfig config{
        .worker_count = 0,
        .partition_count = 16,
        .rsi_period = 11,
        .wing_size = 2,
        .macd_fast_period = 5,
        .macd_slow_period = 13,
        .macd_signal_period = 4,
        .minimum_score = 0.72,
        .top_n = 12,
        .strategy_weights = {{"rsi_divergence", 1.0}, {"macd_bullish_cross", 1.4}},
    };

    const auto rsi = tradingbot::infra::to_live_rsi_divergence_engine_config(config);
    require(rsi.scanner.worker_count == 0, "worker_count zero sentinel should be preserved");
    require(rsi.partition_count == 16, "partition count should map");
    require(rsi.scanner.rsi_period == 11, "RSI period should map");
    require(rsi.scanner.wing_size == 2, "RSI wing size should map");

    const auto decision = tradingbot::infra::to_live_scanner_decision_config(config);
    require(decision.ranking.macd.fast_period == 5, "MACD fast period should map");
    require(decision.ranking.macd.slow_period == 13, "MACD slow period should map");
    require(decision.ranking.macd.signal_period == 4, "MACD signal period should map");
    require(decision.ranking.ranking.limit == 12, "top-N should map to rank limit");
    require(decision.ranking.ranking.strategy_weights.at("macd_bullish_cross") == 1.4,
            "strategy weights should map");
    require(decision.selection.minimum_score == 0.72, "minimum score should map");
}

void maps_default_live_scanner_config() {
    const tradingbot::infra::LiveScannerConfig config;

    const auto rsi = tradingbot::infra::to_live_rsi_divergence_engine_config(config);
    require(rsi.scanner.worker_count == 0, "default worker_count should keep CPU-default sentinel");
    require(rsi.partition_count == 0, "default partition count should map");
    require(rsi.scanner.rsi_period == 14, "default RSI period should map");
    require(rsi.scanner.wing_size == 1, "default wing size should map");

    const auto decision = tradingbot::infra::to_live_scanner_decision_config(config);
    require(decision.ranking.macd.fast_period == 12, "default MACD fast period should map");
    require(decision.ranking.macd.slow_period == 26, "default MACD slow period should map");
    require(decision.ranking.macd.signal_period == 9, "default MACD signal period should map");
    require(decision.ranking.ranking.limit == 0, "default top-N should map");
    require(decision.ranking.ranking.strategy_weights.empty(), "default weights should be empty");
    require(decision.selection.minimum_score == 0.0, "default minimum score should map");
}

}  // namespace

int main() {
    maps_full_live_scanner_config();
    maps_default_live_scanner_config();
    return 0;
}
