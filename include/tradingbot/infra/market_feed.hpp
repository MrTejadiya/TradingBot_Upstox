#pragma once

#include "tradingbot/core/domain.hpp"

#include <functional>
#include <string>
#include <vector>

namespace tradingbot::infra {

using QuoteHandler = std::function<void(const core::QuoteSnapshot&)>;
using FeedStatusHandler = std::function<void(const std::string&)>;

struct MarketFeedConfig {
    std::vector<core::InstrumentKey> instruments;
};

struct MarketFeedSubscription {
    std::string command_payload;
};

class MarketFeed {
public:
    virtual ~MarketFeed() = default;
    virtual void set_quote_handler(QuoteHandler handler) = 0;
    virtual void set_status_handler(FeedStatusHandler handler) = 0;
    virtual MarketFeedSubscription subscribe(const MarketFeedConfig& config) = 0;
    virtual void on_message(const std::string& message) = 0;
    virtual void on_disconnect(const std::string& reason) = 0;
};

class UpstoxMarketFeed final : public MarketFeed {
public:
    void set_quote_handler(QuoteHandler handler) override;
    void set_status_handler(FeedStatusHandler handler) override;
    MarketFeedSubscription subscribe(const MarketFeedConfig& config) override;
    void on_message(const std::string& message) override;
    void on_disconnect(const std::string& reason) override;

private:
    QuoteHandler quote_handler_;
    FeedStatusHandler status_handler_;
};

std::string build_market_feed_subscription_payload(const MarketFeedConfig& config);
bool parse_market_feed_quote(const std::string& message, core::QuoteSnapshot& quote);

}  // namespace tradingbot::infra

