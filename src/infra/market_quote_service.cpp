#include "tradingbot/infra/market_quote_service.hpp"

#include <cctype>
#include <chrono>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>
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

std::optional<std::string> parse_string_after_key(const std::string& body, const std::string& key) {
    const auto key_pos = body.find("\"" + key + "\"");
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon_pos = body.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto quote_start = body.find('"', colon_pos + 1);
    if (quote_start == std::string::npos) {
        return std::nullopt;
    }
    const auto quote_end = body.find('"', quote_start + 1);
    if (quote_end == std::string::npos || quote_end == quote_start + 1) {
        return std::nullopt;
    }
    return body.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::optional<int> parse_two_digits(std::string_view value) {
    if (value.size() != 2 || std::isdigit(static_cast<unsigned char>(value[0])) == 0 ||
        std::isdigit(static_cast<unsigned char>(value[1])) == 0) {
        return std::nullopt;
    }
    return (value[0] - '0') * 10 + (value[1] - '0');
}

std::optional<int> parse_four_digits(std::string_view value) {
    if (value.size() != 4) {
        return std::nullopt;
    }
    auto result = 0;
    for (const auto ch : value) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return std::nullopt;
        }
        result = result * 10 + (ch - '0');
    }
    return result;
}

std::optional<core::TimePoint> parse_offset_timestamp(std::string_view value) {
    if (value.size() != 25 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || (value[19] != '+' && value[19] != '-') || value[22] != ':') {
        return std::nullopt;
    }

    const auto year = parse_four_digits(value.substr(0, 4));
    const auto month = parse_two_digits(value.substr(5, 2));
    const auto day = parse_two_digits(value.substr(8, 2));
    const auto hour = parse_two_digits(value.substr(11, 2));
    const auto minute = parse_two_digits(value.substr(14, 2));
    const auto second = parse_two_digits(value.substr(17, 2));
    const auto offset_hour = parse_two_digits(value.substr(20, 2));
    const auto offset_minute = parse_two_digits(value.substr(23, 2));
    if (!year || !month || !day || !hour || !minute || !second || !offset_hour || !offset_minute ||
        *month < 1 || *month > 12 || *day < 1 || *day > 31 || *hour > 23 || *minute > 59 || *second > 59 ||
        *offset_hour > 23 || *offset_minute > 59) {
        return std::nullopt;
    }

    std::tm utc{};
    utc.tm_year = *year - 1900;
    utc.tm_mon = *month - 1;
    utc.tm_mday = *day;
    utc.tm_hour = *hour;
    utc.tm_min = *minute;
    utc.tm_sec = *second;
#ifdef _WIN32
    auto epoch = _mkgmtime(&utc);
#else
    auto epoch = timegm(&utc);
#endif
    if (epoch == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    const auto offset_seconds = (*offset_hour * 60 + *offset_minute) * 60;
    epoch += value[19] == '+' ? -offset_seconds : offset_seconds;
    return core::Clock::from_time_t(epoch);
}

std::optional<std::string> quote_timestamp_text(const std::string& body) {
    for (const auto* key : {"timestamp", "last_trade_time", "last_traded_time"}) {
        const auto value = parse_string_after_key(body, key);
        if (value) {
            return value;
        }
    }
    return std::nullopt;
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
    const auto timestamp_text = quote_timestamp_text(api_result.response.body);
    if (timestamp_text) {
        const auto timestamp = parse_offset_timestamp(*timestamp_text);
        if (!timestamp) {
            result.error = "LTP response has malformed timestamp";
            return result;
        }
        result.quote.timestamp = *timestamp;
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
