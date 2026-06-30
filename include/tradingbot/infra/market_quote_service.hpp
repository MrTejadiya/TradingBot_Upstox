#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"

#include <memory>
#include <string>

namespace tradingbot::infra {

struct QuoteResult {
    bool ok{false};
    core::QuoteSnapshot quote;
    ApiEvent api_event;
    std::string error;
};

class MarketQuoteService {
public:
    explicit MarketQuoteService(std::shared_ptr<UpstoxApiClient> api_client);

    QuoteResult fetch_ltp(const core::InstrumentKey& instrument_key);

private:
    std::shared_ptr<UpstoxApiClient> api_client_;
};

QuoteResult parse_ltp_response(const core::InstrumentKey& instrument_key, const ApiResult& api_result);
std::string ltp_quote_path(const core::InstrumentKey& instrument_key);

}  // namespace tradingbot::infra

