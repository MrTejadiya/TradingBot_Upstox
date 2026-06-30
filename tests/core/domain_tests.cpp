#include "tradingbot/core/domain.hpp"

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

void instrument_key_is_canonical_identifier() {
    const tradingbot::core::Instrument instrument{
        .key = {"NSE_EQ|INE002A01018"},
        .symbol = "RELIANCE",
        .exchange = tradingbot::core::Exchange::NseEq,
        .enabled = true,
        .quantity = 1,
        .max_position_quantity = 10,
    };

    require(tradingbot::core::is_valid_instrument_key(instrument.key), "instrument key should be valid");
    require(instrument.key.value == "NSE_EQ|INE002A01018", "instrument_key should remain canonical");
    require(instrument.symbol == "RELIANCE", "symbol should be display metadata");
}

void strategy_signal_contains_required_srs_fields() {
    const tradingbot::core::StrategySignal signal{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .action = tradingbot::core::TradeAction::Buy,
        .confidence = 0.82,
        .suggested_quantity = 5,
        .suggested_entry_price = 2500.0,
        .suggested_target_price = 2750.0,
        .suggested_stop_loss = 2400.0,
        .reason = "RSI oversold",
        .strategy_name = "rsi_oversold",
        .timestamp = tradingbot::core::Clock::now(),
    };

    require(tradingbot::core::is_valid_instrument_key(signal.instrument_key), "signal key should be valid");
    require(signal.action == tradingbot::core::TradeAction::Buy, "signal action should be retained");
    require(signal.confidence > 0.8, "signal confidence should be retained");
    require(signal.suggested_quantity == 5, "suggested quantity should be retained");
    require(signal.suggested_entry_price.has_value(), "entry price should be optional but present");
    require(signal.suggested_target_price.has_value(), "target price should be optional but present");
    require(signal.suggested_stop_loss.has_value(), "stop loss should be optional but present");
    require(signal.reason == "RSI oversold", "signal reason should be retained");
    require(signal.strategy_name == "rsi_oversold", "strategy name should be retained");
}

void order_request_defaults_to_delivery_day_limit_order() {
    const tradingbot::core::OrderRequest request{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .side = tradingbot::core::OrderSide::Buy,
        .quantity = 3,
        .price = 100.50,
        .tag = "dry-run",
        .source_strategy = "manual_buy",
        .run_id = "run-1",
    };

    require(tradingbot::core::has_positive_order_quantity(request), "quantity should be positive");
    require(tradingbot::core::is_delivery_day_order(request), "orders should default to delivery DAY");
    require(request.order_type == tradingbot::core::OrderType::Limit, "orders should default to LIMIT");
    require(request.source_strategy == "manual_buy", "source strategy should be retained");
    require(request.run_id == "run-1", "run identifier should be retained");
}

void risk_event_records_rejection_reason_code() {
    const tradingbot::core::RiskEvent event{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .decision = tradingbot::core::RiskDecision::Rejected,
        .reason_code = "DUPLICATE_OPEN_ORDER",
        .detail = "Open buy order exists for instrument",
        .timestamp = tradingbot::core::Clock::now(),
    };

    require(event.decision == tradingbot::core::RiskDecision::Rejected, "risk event should retain decision");
    require(event.reason_code == "DUPLICATE_OPEN_ORDER", "risk event should retain reason code");
}

}  // namespace

int main() {
    instrument_key_is_canonical_identifier();
    strategy_signal_contains_required_srs_fields();
    order_request_defaults_to_delivery_day_limit_order();
    risk_event_records_rejection_reason_code();
    return 0;
}

