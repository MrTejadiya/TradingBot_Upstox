#include "tradingbot/infra/instrument_universe.hpp"

#include <string>
#include <unordered_map>
#include <utility>

namespace tradingbot::infra {
namespace {

std::string listing_identity(const std::string& key) {
    const auto separator = key.find('|');
    if (separator == std::string::npos || separator + 1 >= key.size()) {
        return key;
    }
    return key.substr(separator + 1);
}

bool preferred_over(const core::Instrument& candidate, const core::Instrument& current) {
    const auto candidate_exchange = exchange_from_instrument_key(candidate.key.value);
    const auto current_exchange = exchange_from_instrument_key(current.key.value);
    return candidate_exchange == core::Exchange::NseEq && current_exchange == core::Exchange::BseEq;
}

}  // namespace

core::Exchange exchange_from_instrument_key(const std::string& key) {
    if (key.rfind("NSE_EQ|", 0) == 0) {
        return core::Exchange::NseEq;
    }
    if (key.rfind("BSE_EQ|", 0) == 0) {
        return core::Exchange::BseEq;
    }
    return core::Exchange::Unknown;
}

std::vector<core::Instrument> prefer_nse_duplicate_listings(std::vector<core::Instrument> instruments) {
    std::vector<core::Instrument> selected;
    std::unordered_map<std::string, std::size_t> index_by_identity;
    selected.reserve(instruments.size());

    for (auto& instrument : instruments) {
        const auto exchange = exchange_from_instrument_key(instrument.key.value);
        if (exchange != core::Exchange::NseEq && exchange != core::Exchange::BseEq) {
            selected.push_back(std::move(instrument));
            continue;
        }

        const auto identity = listing_identity(instrument.key.value);
        const auto found = index_by_identity.find(identity);
        if (found == index_by_identity.end()) {
            index_by_identity.emplace(identity, selected.size());
            selected.push_back(std::move(instrument));
            continue;
        }

        auto& current = selected[found->second];
        if (preferred_over(instrument, current)) {
            current = std::move(instrument);
        }
    }

    return selected;
}

}  // namespace tradingbot::infra
