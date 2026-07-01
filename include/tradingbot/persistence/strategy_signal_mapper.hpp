#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredStrategySignalRow {
    std::string run_id;
    std::string instrument_key;
    std::string action;
    double confidence{0.0};
    core::Quantity suggested_quantity{0};
    std::optional<core::Money> suggested_entry_price;
    std::optional<core::Money> suggested_target_price;
    std::optional<core::Money> suggested_stop_loss;
    std::string strategy_name;
    std::string reason;
    core::TimePoint created_at{};
};

struct StrategySignalMapResult {
    bool ok{false};
    core::StrategySignal signal;
    std::string error;
};

StrategySignalMapResult map_stored_strategy_signal_row(const StoredStrategySignalRow& row);
StoredStrategySignalRow map_strategy_signal_to_stored_row(const core::StrategySignal& signal,
                                                          const std::string& run_id);
std::string stored_trade_action_name(core::TradeAction action);
std::optional<core::TradeAction> parse_stored_trade_action(const std::string& value);

}  // namespace tradingbot::persistence
