#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <vector>

namespace tradingbot::infra {

struct InstrumentCsvLoadResult {
    bool ok{false};
    std::vector<core::Instrument> instruments;
    std::vector<std::string> errors;
};

InstrumentCsvLoadResult load_instruments_csv_file(const std::string& path);
InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text);

}  // namespace tradingbot::infra

