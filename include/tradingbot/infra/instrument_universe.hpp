#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <vector>

namespace tradingbot::infra {

core::Exchange exchange_from_instrument_key(const std::string& key);
std::vector<core::Instrument> prefer_nse_duplicate_listings(std::vector<core::Instrument> instruments);

}  // namespace tradingbot::infra
