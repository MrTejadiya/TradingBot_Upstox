#include "tradingbot/infra/market_feed.hpp"

#include <cctype>
#include <chrono>
#include <sstream>
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
    quote.stale = false;
    return true;
}

}  // namespace tradingbot::infra
