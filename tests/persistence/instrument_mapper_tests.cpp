#include "tradingbot/persistence/instrument_mapper.hpp"

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

tradingbot::core::Instrument instrument() {
    return {
        .key = {"NSE_EQ|INE002A01018"},
        .symbol = "RELIANCE",
        .exchange = tradingbot::core::Exchange::NseEq,
        .enabled = true,
        .quantity = 2,
        .max_position_quantity = 10,
        .manual_buy_price = 2400.5,
        .manual_target_price = 2700.0,
        .stop_loss_pct = 3.0,
        .target_profit_pct = 10.0,
        .trailing_stop_pct = 2.5,
        .strategy_profile = "delivery",
        .notes = "quoted, note",
    };
}

tradingbot::persistence::StoredInstrumentRow stored_row() {
    return {
        .instrument_key = "NSE_EQ|INE002A01018",
        .symbol = "RELIANCE",
        .exchange = "NSE_EQ",
        .enabled = 1,
        .quantity = 2,
        .max_position_quantity = 10,
        .manual_buy_price = 2400.5,
        .manual_target_price = 2700.0,
        .stop_loss_pct = 3.0,
        .target_profit_pct = 10.0,
        .trailing_stop_pct = 2.5,
        .strategy_profile = "delivery",
        .notes = "quoted, note",
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
    };
}

void maps_instrument_to_stored_row() {
    const auto row = tradingbot::persistence::map_instrument_to_stored_row(
        instrument(), tradingbot::core::TimePoint{std::chrono::seconds{7}});

    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument key should store");
    require(row.symbol == "RELIANCE", "symbol should store");
    require(row.exchange == "NSE_EQ", "exchange should store");
    require(row.enabled == 1, "enabled should store as one");
    require(row.quantity == 2, "quantity should store");
    require(row.max_position_quantity == 10, "max position should store");
    require(row.manual_buy_price && *row.manual_buy_price == 2400.5, "manual buy should store");
    require(row.manual_target_price && *row.manual_target_price == 2700.0, "manual target should store");
    require(row.stop_loss_pct && *row.stop_loss_pct == 3.0, "stop loss should store");
    require(row.target_profit_pct == 10.0, "target profit should store");
    require(row.trailing_stop_pct && *row.trailing_stop_pct == 2.5, "trailing stop should store");
    require(row.strategy_profile == "delivery", "strategy profile should store");
    require(row.notes == "quoted, note", "notes should store");
    require(row.updated_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "updated time should store");
}

void maps_stored_row_to_instrument() {
    const auto result = tradingbot::persistence::map_stored_instrument_row(stored_row());

    require(result.ok, "valid stored instrument should map");
    require(result.instrument.key.value == "NSE_EQ|INE002A01018", "instrument key should map");
    require(result.instrument.symbol == "RELIANCE", "symbol should map");
    require(result.instrument.exchange == tradingbot::core::Exchange::NseEq, "exchange should map");
    require(result.instrument.enabled, "enabled should map");
    require(result.instrument.quantity == 2, "quantity should map");
    require(result.instrument.max_position_quantity == 10, "max position should map");
    require(result.instrument.manual_buy_price && *result.instrument.manual_buy_price == 2400.5, "manual buy should map");
    require(result.instrument.manual_target_price && *result.instrument.manual_target_price == 2700.0,
            "manual target should map");
    require(result.instrument.stop_loss_pct && *result.instrument.stop_loss_pct == 3.0, "stop loss should map");
    require(result.instrument.target_profit_pct == 10.0, "target profit should map");
    require(result.instrument.trailing_stop_pct && *result.instrument.trailing_stop_pct == 2.5,
            "trailing stop should map");
    require(result.instrument.strategy_profile == "delivery", "strategy profile should map");
    require(result.instrument.notes == "quoted, note", "notes should map");
}

void maps_empty_optional_values() {
    auto row = stored_row();
    row.exchange = "UNKNOWN";
    row.enabled = 0;
    row.manual_buy_price.reset();
    row.manual_target_price.reset();
    row.stop_loss_pct.reset();
    row.trailing_stop_pct.reset();

    const auto result = tradingbot::persistence::map_stored_instrument_row(row);

    require(result.ok, "instrument with empty optional values should map");
    require(result.instrument.exchange == tradingbot::core::Exchange::Unknown, "unknown exchange should map");
    require(!result.instrument.enabled, "disabled instrument should map");
    require(!result.instrument.manual_buy_price, "empty manual buy should map");
    require(!result.instrument.manual_target_price, "empty manual target should map");
    require(!result.instrument.stop_loss_pct, "empty stop loss should map");
    require(!result.instrument.trailing_stop_pct, "empty trailing stop should map");
}

void round_trips_instrument_through_stored_row() {
    auto original = instrument();
    original.exchange = tradingbot::core::Exchange::BseEq;
    original.enabled = false;
    const auto updated_at = tradingbot::core::TimePoint{std::chrono::seconds{11}};

    const auto row = tradingbot::persistence::map_instrument_to_stored_row(original, updated_at);
    const auto result = tradingbot::persistence::map_stored_instrument_row(row);

    require(result.ok, "instrument should round trip");
    require(row.updated_at == updated_at, "updated time should remain on stored row");
    require(result.instrument.key.value == original.key.value, "instrument key should round trip");
    require(result.instrument.symbol == original.symbol, "symbol should round trip");
    require(result.instrument.exchange == original.exchange, "exchange should round trip");
    require(result.instrument.enabled == original.enabled, "enabled should round trip");
    require(result.instrument.quantity == original.quantity, "quantity should round trip");
    require(result.instrument.max_position_quantity == original.max_position_quantity, "max position should round trip");
    require(result.instrument.manual_buy_price == original.manual_buy_price, "manual buy should round trip");
    require(result.instrument.manual_target_price == original.manual_target_price, "manual target should round trip");
    require(result.instrument.stop_loss_pct == original.stop_loss_pct, "stop loss should round trip");
    require(result.instrument.target_profit_pct == original.target_profit_pct, "target profit should round trip");
    require(result.instrument.trailing_stop_pct == original.trailing_stop_pct, "trailing stop should round trip");
    require(result.instrument.strategy_profile == original.strategy_profile, "strategy profile should round trip");
    require(result.instrument.notes == original.notes, "notes should round trip");
}

void parses_stored_values() {
    require(tradingbot::persistence::stored_exchange_value(tradingbot::core::Exchange::Unknown) == "UNKNOWN",
            "unknown exchange should store");
    require(tradingbot::persistence::stored_exchange_value(tradingbot::core::Exchange::NseEq) == "NSE_EQ",
            "NSE exchange should store");
    require(tradingbot::persistence::stored_exchange_value(tradingbot::core::Exchange::BseEq) == "BSE_EQ",
            "BSE exchange should store");
    require(tradingbot::persistence::parse_stored_exchange("UNKNOWN") == tradingbot::core::Exchange::Unknown,
            "UNKNOWN should parse");
    require(tradingbot::persistence::parse_stored_exchange("NSE_EQ") == tradingbot::core::Exchange::NseEq,
            "NSE_EQ should parse");
    require(tradingbot::persistence::parse_stored_exchange("BSE_EQ") == tradingbot::core::Exchange::BseEq,
            "BSE_EQ should parse");
    require(tradingbot::persistence::stored_instrument_enabled_value(false) == 0, "false should store as zero");
    require(tradingbot::persistence::stored_instrument_enabled_value(true) == 1, "true should store as one");
    require(tradingbot::persistence::parse_stored_instrument_enabled(0) == false, "zero should parse false");
    require(tradingbot::persistence::parse_stored_instrument_enabled(1) == true, "one should parse true");
}

void invalid_exchange_fails_closed() {
    auto row = stored_row();
    row.exchange = "NFO";

    const auto result = tradingbot::persistence::map_stored_instrument_row(row);

    require(!result.ok, "invalid exchange should fail");
    require(result.error.find("invalid exchange") != std::string::npos, "invalid exchange error should be clear");
}

void invalid_enabled_flag_fails_closed() {
    auto row = stored_row();
    row.enabled = 2;

    const auto result = tradingbot::persistence::map_stored_instrument_row(row);

    require(!result.ok, "invalid enabled flag should fail");
    require(result.error.find("invalid enabled flag") != std::string::npos, "invalid enabled error should be clear");
}

}  // namespace

int main() {
    maps_instrument_to_stored_row();
    maps_stored_row_to_instrument();
    maps_empty_optional_values();
    round_trips_instrument_through_stored_row();
    parses_stored_values();
    invalid_exchange_fails_closed();
    invalid_enabled_flag_fails_closed();
    return 0;
}
