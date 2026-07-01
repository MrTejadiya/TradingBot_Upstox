#include "tradingbot/infra/market_feed.hpp"

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

bool extract_string_after_key(const std::string& body, const std::string& key, std::string& value) {
    const auto key_pos = body.find("\"" + key + "\"");
    if (key_pos == std::string::npos) {
        return false;
    }
    const auto colon_pos = body.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return false;
    }
    const auto first_quote = body.find('"', colon_pos + 1);
    if (first_quote == std::string::npos) {
        return false;
    }
    const auto second_quote = body.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return false;
    }
    value = body.substr(first_quote + 1, second_quote - first_quote - 1);
    return true;
}

bool extract_number_after_key(const std::string& body, const std::string& key, double& value) {
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
    std::string value;
    for (const auto* key : {"timestamp", "last_trade_time", "last_traded_time"}) {
        if (extract_string_after_key(body, key, value)) {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace

void UpstoxMarketFeed::set_quote_handler(QuoteHandler handler) {
    quote_handler_ = std::move(handler);
}

void UpstoxMarketFeed::set_status_handler(FeedStatusHandler handler) {
    status_handler_ = std::move(handler);
}

MarketFeedSubscription UpstoxMarketFeed::subscribe(const MarketFeedConfig& config) {
    return {.command_payload = build_market_feed_subscription_payload(config)};
}

void UpstoxMarketFeed::on_message(const std::string& message) {
    core::QuoteSnapshot quote;
    if (quote_handler_ && parse_market_feed_quote(message, quote)) {
        quote_handler_(quote);
    }
}

void UpstoxMarketFeed::on_disconnect(const std::string& reason) {
    if (status_handler_) {
        status_handler_("disconnected: " + reason);
    }
}

std::string build_market_feed_subscription_payload(const MarketFeedConfig& config) {
    std::ostringstream out;
    out << "{\"method\":\"sub\",\"data\":{\"mode\":\"ltpc\",\"instrumentKeys\":[";
    for (std::size_t index = 0; index < config.instruments.size(); ++index) {
        if (index != 0) {
            out << ",";
        }
        out << "\"" << config.instruments[index].value << "\"";
    }
    out << "]}}";
    return out.str();
}

bool parse_market_feed_quote(const std::string& message, core::QuoteSnapshot& quote) {
    std::string instrument_key;
    double ltp = 0.0;
    if (!extract_string_after_key(message, "instrument_key", instrument_key) &&
        !extract_string_after_key(message, "instrumentKey", instrument_key) &&
        !extract_string_after_key(message, "instrument_token", instrument_key)) {
        return false;
    }
    if (!extract_number_after_key(message, "ltp", ltp) && !extract_number_after_key(message, "last_price", ltp) &&
        !extract_number_after_key(message, "lastPrice", ltp)) {
        return false;
    }
    if (ltp <= 0.0) {
        return false;
    }

    quote.instrument_key.value = instrument_key;
    quote.ltp = ltp;
    quote.timestamp = core::Clock::now();
    const auto timestamp_text = quote_timestamp_text(message);
    if (timestamp_text) {
        const auto timestamp = parse_offset_timestamp(*timestamp_text);
        if (!timestamp) {
            return false;
        }
        quote.timestamp = *timestamp;
    }
    quote.stale = false;
    return true;
}

}  // namespace tradingbot::infra
