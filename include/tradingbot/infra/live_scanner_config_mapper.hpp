#pragma once

#include "tradingbot/infra/config.hpp"
#include "tradingbot/scan/live_rsi_divergence_engine.hpp"
#include "tradingbot/scan/live_scanner_decision_pipeline.hpp"

namespace tradingbot::infra {

scan::LiveRsiDivergenceEngineConfig to_live_rsi_divergence_engine_config(const LiveScannerConfig& config);

scan::LiveScannerDecisionConfig to_live_scanner_decision_config(const LiveScannerConfig& config);

}  // namespace tradingbot::infra
