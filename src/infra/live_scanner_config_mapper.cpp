#include "tradingbot/infra/live_scanner_config_mapper.hpp"

namespace tradingbot::infra {

scan::LiveRsiDivergenceEngineConfig to_live_rsi_divergence_engine_config(const LiveScannerConfig& config) {
    return {
        .scanner =
            {
                .rsi_period = config.rsi_period,
                .wing_size = config.wing_size,
                .worker_count = config.worker_count,
            },
        .partition_count = config.partition_count,
    };
}

scan::LiveScannerDecisionConfig to_live_scanner_decision_config(const LiveScannerConfig& config) {
    return {
        .ranking =
            {
                .macd =
                    {
                        .fast_period = config.macd_fast_period,
                        .slow_period = config.macd_slow_period,
                        .signal_period = config.macd_signal_period,
                    },
                .ranking =
                    {
                        .strategy_weights = config.strategy_weights,
                        .limit = config.top_n,
                    },
            },
        .selection = {.minimum_score = config.minimum_score},
    };
}

}  // namespace tradingbot::infra
