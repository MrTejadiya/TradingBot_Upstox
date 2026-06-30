#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <set>
#include <vector>

namespace tradingbot::infra {

struct InstrumentCsvLoadOptions {
    bool skip_invalid_rows{false};
    std::set<std::string> strategy_profiles;
};

struct InstrumentCsvLoadResult {
    bool ok{false};
    std::vector<core::Instrument> instruments;
    std::vector<std::string> errors;
};

InstrumentCsvLoadResult load_instruments_csv_file(const std::string& path);
InstrumentCsvLoadResult load_instruments_csv_file(const std::string& path, const InstrumentCsvLoadOptions& options);
InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text);
InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text, const InstrumentCsvLoadOptions& options);

}  // namespace tradingbot::infra
