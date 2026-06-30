#include "tradingbot/infra/instrument_csv.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void loads_minimum_columns() {
    const auto result = tradingbot::infra::load_instruments_csv_text(
        "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct\n"
        "NSE_EQ|INE002A01018,RELIANCE,true,2,10,12.5\n");

    require(result.ok, "minimum CSV should load");
    require(result.instruments.size() == 1, "one instrument should load");
    const auto& instrument = result.instruments.front();
    require(instrument.key.value == "NSE_EQ|INE002A01018", "instrument_key should load");
    require(instrument.symbol == "RELIANCE", "symbol should load as display metadata");
    require(instrument.enabled, "enabled should parse");
    require(instrument.quantity == 2, "quantity should parse");
    require(instrument.max_position_quantity == 10, "max position should parse");
    require(instrument.target_profit_pct == 12.5, "target profit should parse");
}

void loads_recommended_columns_and_quoted_notes() {
    const auto result = tradingbot::infra::load_instruments_csv_text(
        "instrument_key,symbol,exchange,enabled,quantity,max_position_qty,manual_buy_price,manual_target_price,"
        "stop_loss_pct,target_profit_pct,trailing_stop_pct,strategy_profile,notes\n"
        "NSE_EQ|INE002A01018,RELIANCE,NSE_EQ,false,2,10,2400.5,2700,3,10,2.5,delivery,\"quoted, note\"\n");

    require(result.ok, "recommended CSV should load");
    const auto& instrument = result.instruments.front();
    require(instrument.exchange == tradingbot::core::Exchange::NseEq, "exchange should parse");
    require(!instrument.enabled, "enabled false should parse");
    require(instrument.manual_buy_price && *instrument.manual_buy_price == 2400.5, "manual buy should parse");
    require(instrument.manual_target_price && *instrument.manual_target_price == 2700.0, "manual target should parse");
    require(instrument.stop_loss_pct && *instrument.stop_loss_pct == 3.0, "stop loss should parse");
    require(instrument.trailing_stop_pct && *instrument.trailing_stop_pct == 2.5, "trailing stop should parse");
    require(instrument.strategy_profile == "delivery", "strategy profile should parse");
    require(instrument.notes == "quoted, note", "quoted notes should preserve comma");
}

void treats_symbol_as_metadata_only() {
    const auto result = tradingbot::infra::load_instruments_csv_text(
        "symbol,instrument_key,enabled,quantity,max_position_qty,target_profit_pct\n"
        "CHANGED_SYMBOL,NSE_EQ|INE002A01018,true,1,2,10\n");

    require(result.ok, "CSV should load");
    require(result.instruments.front().key.value == "NSE_EQ|INE002A01018", "key should remain canonical");
    require(result.instruments.front().symbol == "CHANGED_SYMBOL", "symbol should remain metadata");
}

void fails_without_instrument_key_header() {
    const auto result = tradingbot::infra::load_instruments_csv_text(
        "symbol,enabled,quantity,max_position_qty,target_profit_pct\n"
        "RELIANCE,true,1,2,10\n");

    require(!result.ok, "missing instrument_key header should fail");
    require(!result.errors.empty(), "missing header should report an error");
    require(result.errors.front().find("instrument_key") != std::string::npos, "error should name instrument_key");
}

void loads_from_file() {
    const auto path = std::string{"instrument_csv_test_tmp.csv"};
    {
        std::ofstream file(path);
        file << "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct\n";
        file << "NSE_EQ|INE002A01018,RELIANCE,true,2,10,12.5\n";
    }

    const auto result = tradingbot::infra::load_instruments_csv_file(path);
    std::remove(path.c_str());

    require(result.ok, "CSV file should load");
    require(result.instruments.size() == 1, "file-loaded CSV should contain row");
}

void reports_malformed_numeric_field_without_throwing() {
    const auto result = tradingbot::infra::load_instruments_csv_text(
        "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct\n"
        "NSE_EQ|INE002A01018,RELIANCE,true,not-a-number,10,12.5\n");

    require(!result.ok, "malformed numeric field should fail load");
    require(result.instruments.empty(), "malformed row should not be loaded");
    require(!result.errors.empty(), "malformed row should report error");
    require(result.errors.front().find("row 2") != std::string::npos, "error should include row number");
}

}  // namespace

int main() {
    loads_minimum_columns();
    loads_recommended_columns_and_quoted_notes();
    treats_symbol_as_metadata_only();
    fails_without_instrument_key_header();
    loads_from_file();
    reports_malformed_numeric_field_without_throwing();
    return 0;
}
