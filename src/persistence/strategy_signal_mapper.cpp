#include "tradingbot/persistence/strategy_signal_mapper.hpp"

namespace tradingbot::persistence {

std::string stored_trade_action_name(core::TradeAction action) {
    return action == core::TradeAction::Buy ? "buy" : "sell";
}

std::optional<core::TradeAction> parse_stored_trade_action(const std::string& value) {
    if (value == "buy") {
        return core::TradeAction::Buy;
    }
    if (value == "sell") {
        return core::TradeAction::Sell;
    }
    return std::nullopt;
}

StoredStrategySignalRow map_strategy_signal_to_stored_row(const core::StrategySignal& signal,
                                                          const std::string& run_id) {
    return {
        .run_id = run_id,
        .instrument_key = signal.instrument_key.value,
        .action = stored_trade_action_name(signal.action),
        .confidence = signal.confidence,
        .suggested_quantity = signal.suggested_quantity,
        .suggested_entry_price = signal.suggested_entry_price,
        .suggested_target_price = signal.suggested_target_price,
        .suggested_stop_loss = signal.suggested_stop_loss,
        .strategy_name = signal.strategy_name,
        .reason = signal.reason,
        .created_at = signal.timestamp,
    };
}

StrategySignalMapResult map_stored_strategy_signal_row(const StoredStrategySignalRow& row) {
    const auto action = parse_stored_trade_action(row.action);
    if (!action) {
        return {.ok = false, .error = "stored strategy signal has invalid action: " + row.action};
    }

    return {
        .ok = true,
        .signal =
            {
                .instrument_key = {row.instrument_key},
                .action = *action,
                .confidence = row.confidence,
                .suggested_quantity = row.suggested_quantity,
                .suggested_entry_price = row.suggested_entry_price,
                .suggested_target_price = row.suggested_target_price,
                .suggested_stop_loss = row.suggested_stop_loss,
                .reason = row.reason,
                .strategy_name = row.strategy_name,
                .timestamp = row.created_at,
            },
    };
}

}  // namespace tradingbot::persistence
