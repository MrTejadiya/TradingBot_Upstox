#include "tradingbot/infra/market_feed.hpp"

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

std::time_t utc_epoch(int year, int month, int day, int hour, int minute, int second) {
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
#ifdef _WIN32
    return _mkgmtime(&utc);
#else
    return timegm(&utc);
#endif
}

void builds_subscription_payload() {
    const auto payload = tradingbot::infra::build_market_feed_subscription_payload({
        .instruments = {{"NSE_EQ|INE002A01018"}, {"NSE_EQ|INE467B01029"}},
    });

    require(payload.find("\"method\":\"sub\"") != std::string::npos, "subscription method should be present");
    require(payload.find("\"mode\":\"ltpc\"") != std::string::npos, "ltpc mode should be present");
    require(payload.find("NSE_EQ|INE002A01018") != std::string::npos, "first key should be present");
    require(payload.find("NSE_EQ|INE467B01029") != std::string::npos, "second key should be present");
}

void parses_quote_message() {
    tradingbot::core::QuoteSnapshot quote;
    const auto ok = tradingbot::infra::parse_market_feed_quote(
        R"json({"instrument_key":"NSE_EQ|INE002A01018","ltp":303.9,"timestamp":"2026-06-30T15:29:30+05:30"})json",
        quote);

    require(ok, "quote message should parse");
    require(quote.instrument_key.value == "NSE_EQ|INE002A01018", "instrument key should parse");
    require(quote.ltp == 303.9, "ltp should parse");
    require(tradingbot::core::Clock::to_time_t(quote.timestamp) == utc_epoch(2026, 6, 30, 9, 59, 30),
            "timestamp should convert from +05:30 to UTC");
    require(!quote.stale, "parsed quote should not be stale");
}

void parses_quote_without_timestamp_using_parse_time() {
    tradingbot::core::QuoteSnapshot quote;
    const auto ok = tradingbot::infra::parse_market_feed_quote(
        R"json({"instrument_key":"NSE_EQ|INE002A01018","ltp":303.9})json", quote);

    require(ok, "quote without timestamp should parse");
    require(quote.ltp == 303.9, "ltp should parse without timestamp");
}

void rejects_malformed_quote_message() {
    tradingbot::core::QuoteSnapshot quote;
    const auto ok = tradingbot::infra::parse_market_feed_quote(R"json({"instrument_key":"NSE_EQ|INE002A01018"})json", quote);

    require(!ok, "missing ltp should fail");
}

void rejects_malformed_quote_timestamp() {
    tradingbot::core::QuoteSnapshot quote;
    const auto ok = tradingbot::infra::parse_market_feed_quote(
        R"json({"instrument_key":"NSE_EQ|INE002A01018","ltp":303.9,"timestamp":"bad-time"})json", quote);

    require(!ok, "malformed timestamp should fail");
}

void invokes_quote_handler_for_valid_message() {
    tradingbot::infra::UpstoxMarketFeed feed;
    std::vector<tradingbot::core::QuoteSnapshot> quotes;
    feed.set_quote_handler([&quotes](const tradingbot::core::QuoteSnapshot& quote) {
        quotes.push_back(quote);
    });

    feed.on_message(R"json({"instrumentKey":"NSE_EQ|INE002A01018","lastPrice":303.9})json");

    require(quotes.size() == 1, "quote handler should be invoked");
    require(quotes.front().ltp == 303.9, "handler should receive parsed quote");
}

void invokes_status_handler_on_disconnect() {
    tradingbot::infra::UpstoxMarketFeed feed;
    std::string status;
    feed.set_status_handler([&status](const std::string& value) {
        status = value;
    });

    feed.on_disconnect("network reset");

    require(status.find("disconnected") != std::string::npos, "disconnect should notify status handler");
    require(status.find("network reset") != std::string::npos, "disconnect reason should be included");
}

}  // namespace

int main() {
    builds_subscription_payload();
    parses_quote_message();
    parses_quote_without_timestamp_using_parse_time();
    rejects_malformed_quote_message();
    rejects_malformed_quote_timestamp();
    invokes_quote_handler_for_valid_message();
    invokes_status_handler_on_disconnect();
    return 0;
}
