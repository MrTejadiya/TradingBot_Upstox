#include "tradingbot/infra/upstox_instrument_json.hpp"

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

void loads_supported_nse_equity_record() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "NSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "exchange": "NSE",
            "isin": "INE002A01018",
            "instrument_type": "EQ",
            "instrument_key": "NSE_EQ|INE002A01018",
            "lot_size": 1,
            "trading_symbol": "RELIANCE",
            "short_name": "Reliance"
        }
    ])json");

    require(result.ok, "NSE equity record should load");
    require(result.instruments.size() == 1, "one instrument should load");
    const auto& instrument = result.instruments.front();
    require(instrument.key.value == "NSE_EQ|INE002A01018", "instrument key should map");
    require(instrument.symbol == "RELIANCE", "trading symbol should be preferred");
    require(instrument.exchange == tradingbot::core::Exchange::NseEq, "exchange should map from segment");
    require(instrument.enabled, "default enabled should apply");
    require(instrument.quantity == 1, "default quantity should apply");
    require(instrument.max_position_quantity == 1, "default max position should apply");
    require(instrument.target_profit_pct == 10.0, "default target should apply");
}

void applies_default_options() {
    tradingbot::infra::UpstoxInstrumentJsonLoadOptions options;
    options.enabled = false;
    options.quantity = 2;
    options.max_position_quantity = 4;
    options.target_profit_pct = 7.5;
    options.strategy_profile = "scanner";
    options.notes = "full universe import";

    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "BSE_EQ",
            "name": "BSE ONLY LTD",
            "instrument_type": "EQ",
            "instrument_key": "BSE_EQ|INE999A01010",
            "short_name": "BSEONLY"
        }
    ])json", options);

    require(result.ok, "BSE equity record should load with options");
    const auto& instrument = result.instruments.front();
    require(instrument.symbol == "BSEONLY", "short_name should be fallback display symbol");
    require(!instrument.enabled, "enabled option should apply");
    require(instrument.quantity == 2, "quantity option should apply");
    require(instrument.max_position_quantity == 4, "max position option should apply");
    require(instrument.target_profit_pct == 7.5, "target option should apply");
    require(instrument.strategy_profile == "scanner", "strategy profile option should apply");
    require(instrument.notes == "full universe import", "notes option should apply");
}

void skips_unsupported_records() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "NSE_FO",
            "name": "RELIANCE FUT",
            "instrument_type": "FUT",
            "instrument_key": "NSE_FO|12345"
        },
        {
            "segment": "NSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "instrument_type": "EQ",
            "instrument_key": "NSE_EQ|INE002A01018"
        }
    ])json");

    require(result.ok, "valid records should load while unsupported records are skipped");
    require(result.skipped_records == 1, "unsupported record should be counted");
    require(result.instruments.size() == 1, "only equity record should load");
}

void prefers_nse_when_same_equity_exists_on_nse_and_bse() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "BSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "instrument_type": "EQ",
            "instrument_key": "BSE_EQ|INE002A01018",
            "trading_symbol": "RELIANCE_BSE"
        },
        {
            "segment": "NSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "instrument_type": "EQ",
            "instrument_key": "NSE_EQ|INE002A01018",
            "trading_symbol": "RELIANCE_NSE"
        }
    ])json");

    require(result.ok, "NSE/BSE duplicate listing JSON should load");
    require(result.instruments.size() == 1, "duplicate NSE/BSE listing should collapse");
    require(result.instruments.front().key.value == "NSE_EQ|INE002A01018", "NSE listing should be preferred");
    require(result.instruments.front().symbol == "RELIANCE_NSE", "NSE row metadata should be retained");
}

void keeps_bse_when_no_nse_listing_exists() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "BSE_EQ",
            "name": "BSE ONLY LTD",
            "instrument_type": "EQ",
            "instrument_key": "BSE_EQ|INE999A01010",
            "trading_symbol": "BSEONLY"
        }
    ])json");

    require(result.ok, "BSE-only equity should load");
    require(result.instruments.size() == 1, "BSE-only listing should remain");
    require(result.instruments.front().key.value == "BSE_EQ|INE999A01010", "BSE-only key should be retained");
}

void rejects_duplicate_instrument_keys() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text(R"json([
        {
            "segment": "NSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "instrument_type": "EQ",
            "instrument_key": "NSE_EQ|INE002A01018"
        },
        {
            "segment": "NSE_EQ",
            "name": "RELIANCE INDUSTRIES LTD",
            "instrument_type": "EQ",
            "instrument_key": "NSE_EQ|INE002A01018"
        }
    ])json");

    require(!result.ok, "exact duplicate instrument_key should fail");
    require(!result.errors.empty(), "duplicate key should report an error");
    require(result.errors.back().find("duplicate instrument_key") != std::string::npos,
            "duplicate error should be clear");
}

void reports_invalid_json() {
    const auto result = tradingbot::infra::load_upstox_instruments_json_text("{not json");

    require(!result.ok, "invalid JSON should fail");
    require(!result.errors.empty(), "invalid JSON should report an error");
    require(result.errors.front().find("invalid Upstox instrument JSON") != std::string::npos,
            "invalid JSON error should be clear");
}

void loads_from_file() {
    const auto path = std::string{"upstox_instrument_json_test_tmp.json"};
    {
        std::ofstream file(path);
        file << R"json([
            {
                "segment": "NSE_EQ",
                "name": "RELIANCE INDUSTRIES LTD",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE002A01018",
                "trading_symbol": "RELIANCE"
            }
        ])json";
    }

    const auto result = tradingbot::infra::load_upstox_instruments_json_file(path);
    std::remove(path.c_str());

    require(result.ok, "JSON file should load");
    require(result.instruments.size() == 1, "file-loaded JSON should contain record");
}

}  // namespace

int main() {
    loads_supported_nse_equity_record();
    applies_default_options();
    skips_unsupported_records();
    prefers_nse_when_same_equity_exists_on_nse_and_bse();
    keeps_bse_when_no_nse_listing_exists();
    rejects_duplicate_instrument_keys();
    reports_invalid_json();
    loads_from_file();
    return 0;
}
