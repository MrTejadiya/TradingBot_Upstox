#include "tradingbot/infra/market_quote_service.hpp"

#include <cctype>
#include <chrono>
#include <sstream>
#include <string_view>
#include <utility>

namespace tradingbot::infra {
namespace {

std::string url_encode_instrument_key(std::string_view value) {
    std::ostringstream out;
    for (const auto ch : value) {
        if (ch == '|') {
            out << "%7C";
        } else if (ch == ' ') {
            out << "%20";
        } else {
            out << ch;
        }
    }
    return out.str();
}

bool contains_success_status(const std::string& body) {
    const auto status_pos = body.find("\"status\"");
    if (status_pos == std::string::npos) {
        return false;
    }
    const auto success_pos = body.find("\"success\"", status_pos);
    return success_pos != std::string::npos;
}

bool parse_number_after_key(const std::string& body, const std::string& key, double& value) {
    const auto key_pos = body.find("\"" + key + "\"");
    if (key_pos == std::string::npos) {
        return false;
    }
    const auto colon_pos = body.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return false;
    }
    auto begin = colon_pos + 1;
    while (begin < body.size() && std::isspace(static_cast<unsigned char>(body[begin])) != 0) {
        ++begin;
    }
    auto end = begin;
    while (end < body.size() &&
           (std::isdigit(static_cast<unsigned char>(body[end])) != 0 || body[end] == '.' || body[end] == '-')) {
        ++end;
    }
    if (begin == end) {
        return false;
    }
    try {
        value = std::stod(body.substr(begin, end - begin));
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace

MarketQuoteService::MarketQuoteService(std::shared_ptr<UpstoxApiClient> api_client)
    : api_client_(std::move(api_client)) {}

QuoteResult MarketQuoteService::fetch_ltp(const core::InstrumentKey& instrument_key) {
    if (!api_client_) {
        return {.ok = false, .error = "Upstox API client is required"};
    }
    return parse_ltp_response(instrument_key, api_client_->get(ltp_quote_path(instrument_key)));
}

QuoteResult parse_ltp_response(const core::InstrumentKey& instrument_key, const ApiResult& api_result) {
    QuoteResult result;
    result.api_event = api_result.event;
    result.quote.instrument_key = instrument_key;
    result.quote.timestamp = core::Clock::now();

    if (!api_result.ok) {
        result.error = api_result.error.empty() ? "LTP request failed" : api_result.error;
        return result;
    }
    if (!contains_success_status(api_result.response.body)) {
        result.error = "LTP response status is not success";
        return result;
    }

    double last_price = 0.0;
    if (!parse_number_after_key(api_result.response.body, "last_price", last_price) || last_price <= 0.0) {
        result.error = "LTP response is missing a positive last_price";
        return result;
    }

    result.ok = true;
    result.quote.ltp = last_price;
    result.quote.stale = false;
    return result;
}

std::string ltp_quote_path(const core::InstrumentKey& instrument_key) {
    return "/v3/market-quote/ltp?instrument_key=" + url_encode_instrument_key(instrument_key.value);
}

}  // namespace tradingbot::infra
