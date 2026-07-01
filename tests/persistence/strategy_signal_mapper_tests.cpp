#include "tradingbot/persistence/strategy_signal_mapper.hpp"

#include <chrono>
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

tradingbot::core::StrategySignal signal() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .action = tradingbot::core::TradeAction::Buy,
        .confidence = 0.82,
        .suggested_quantity = 3,
        .suggested_entry_price = 100.5,
        .suggested_target_price = 110.0,
        .suggested_stop_loss = 95.0,
        .reason = "manual buy triggered",
        .strategy_name = "manual_buy",
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

tradingbot::persistence::StoredStrategySignalRow stored_row() {
    return {
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .action = "buy",
        .confidence = 0.82,
        .suggested_quantity = 3,
        .suggested_entry_price = 100.5,
        .suggested_target_price = 110.0,
        .suggested_stop_loss = 95.0,
        .strategy_name = "manual_buy",
        .reason = "manual buy triggered",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void maps_signal_to_stored_row() {
    const auto row = tradingbot::persistence::map_strategy_signal_to_stored_row(signal(), "run-1");

    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.action == "buy", "action should store");
    require(row.confidence == 0.82, "confidence should store");
    require(row.suggested_quantity == 3, "quantity should store");
    require(row.suggested_entry_price && *row.suggested_entry_price == 100.5, "entry should store");
    require(row.suggested_target_price && *row.suggested_target_price == 110.0, "target should store");
    require(row.suggested_stop_loss && *row.suggested_stop_loss == 95.0, "stop loss should store");
    require(row.strategy_name == "manual_buy", "strategy name should store");
    require(row.reason == "manual buy triggered", "reason should store");
    require(row.created_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
}

void maps_stored_row_to_signal() {
    const auto result = tradingbot::persistence::map_stored_strategy_signal_row(stored_row());

    require(result.ok, "valid stored signal should map");
    require(result.signal.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(result.signal.action == tradingbot::core::TradeAction::Buy, "buy action should map");
    require(result.signal.confidence == 0.82, "confidence should map");
    require(result.signal.suggested_quantity == 3, "quantity should map");
    require(result.signal.suggested_entry_price && *result.signal.suggested_entry_price == 100.5,
            "entry should map");
    require(result.signal.suggested_target_price && *result.signal.suggested_target_price == 110.0,
            "target should map");
    require(result.signal.suggested_stop_loss && *result.signal.suggested_stop_loss == 95.0,
            "stop loss should map");
    require(result.signal.reason == "manual buy triggered", "reason should map");
    require(result.signal.strategy_name == "manual_buy", "strategy name should map");
    require(result.signal.timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should map");
}

void maps_sell_signal_without_optional_prices() {
    auto row = stored_row();
    row.action = "sell";
    row.suggested_entry_price = std::nullopt;
    row.suggested_target_price = std::nullopt;
    row.suggested_stop_loss = std::nullopt;
    row.strategy_name = "target_profit_sell";

    const auto result = tradingbot::persistence::map_stored_strategy_signal_row(row);

    require(result.ok, "valid sell signal should map");
    require(result.signal.action == tradingbot::core::TradeAction::Sell, "sell action should map");
    require(!result.signal.suggested_entry_price, "missing entry should map");
    require(!result.signal.suggested_target_price, "missing target should map");
    require(!result.signal.suggested_stop_loss, "missing stop loss should map");
    require(result.signal.strategy_name == "target_profit_sell", "sell strategy name should map");
}

void round_trips_signal_through_stored_row() {
    const auto row = tradingbot::persistence::map_strategy_signal_to_stored_row(signal(), "run-1");
    const auto result = tradingbot::persistence::map_stored_strategy_signal_row(row);

    require(result.ok, "signal should round trip");
    require(result.signal.instrument_key.value == signal().instrument_key.value, "instrument should round trip");
    require(result.signal.action == signal().action, "action should round trip");
    require(result.signal.confidence == signal().confidence, "confidence should round trip");
    require(result.signal.suggested_quantity == signal().suggested_quantity, "quantity should round trip");
    require(result.signal.suggested_entry_price == signal().suggested_entry_price, "entry should round trip");
    require(result.signal.suggested_target_price == signal().suggested_target_price, "target should round trip");
    require(result.signal.suggested_stop_loss == signal().suggested_stop_loss, "stop loss should round trip");
    require(result.signal.reason == signal().reason, "reason should round trip");
    require(result.signal.strategy_name == signal().strategy_name, "strategy name should round trip");
    require(result.signal.timestamp == signal().timestamp, "timestamp should round trip");
}

void parses_all_trade_actions() {
    require(tradingbot::persistence::stored_trade_action_name(tradingbot::core::TradeAction::Buy) == "buy",
            "buy action should serialize");
    require(tradingbot::persistence::stored_trade_action_name(tradingbot::core::TradeAction::Sell) == "sell",
            "sell action should serialize");
    require(tradingbot::persistence::parse_stored_trade_action("buy") == tradingbot::core::TradeAction::Buy,
            "buy action should parse");
    require(tradingbot::persistence::parse_stored_trade_action("sell") == tradingbot::core::TradeAction::Sell,
            "sell action should parse");
}

void invalid_action_fails_closed() {
    auto row = stored_row();
    row.action = "watch";

    const auto result = tradingbot::persistence::map_stored_strategy_signal_row(row);

    require(!result.ok, "invalid action should fail");
    require(result.error.find("invalid action") != std::string::npos, "invalid action error should be clear");
}

}  // namespace

int main() {
    maps_signal_to_stored_row();
    maps_stored_row_to_signal();
    maps_sell_signal_without_optional_prices();
    round_trips_signal_through_stored_row();
    parses_all_trade_actions();
    invalid_action_fails_closed();
    return 0;
}
