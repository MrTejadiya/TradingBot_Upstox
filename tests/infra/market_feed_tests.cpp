#include "tradingbot/infra/market_feed.hpp"

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
        R"json({"instrument_key":"NSE_EQ|INE002A01018","ltp":303.9})json", quote);

    require(ok, "quote message should parse");
    require(quote.instrument_key.value == "NSE_EQ|INE002A01018", "instrument key should parse");
    require(quote.ltp == 303.9, "ltp should parse");
    require(!quote.stale, "parsed quote should not be stale");
}

void rejects_malformed_quote_message() {
    tradingbot::core::QuoteSnapshot quote;
    const auto ok = tradingbot::infra::parse_market_feed_quote(R"json({"instrument_key":"NSE_EQ|INE002A01018"})json", quote);

    require(!ok, "missing ltp should fail");
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
    rejects_malformed_quote_message();
    invokes_quote_handler_for_valid_message();
    invokes_status_handler_on_disconnect();
    return 0;
}

