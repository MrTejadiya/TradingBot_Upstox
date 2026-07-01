#pragma once

#include <string>

namespace tradingbot::strategy {

enum class Weekday {
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
};

struct LocalMarketTime {
    Weekday weekday{Weekday::Monday};
    int hour{0};
    int minute{0};
};

struct MarketSessionConfig {
    int open_minute_of_day{9 * 60 + 15};
    int close_minute_of_day{15 * 60 + 30};
    int last_order_minute_of_day{15 * 60 + 20};
    bool allow_weekend_trading{false};
};

struct MarketSessionResult {
    bool market_open{false};
    bool order_allowed{false};
    std::string reason_code;
    std::string detail;
};

class MarketSessionChecker {
public:
    explicit MarketSessionChecker(MarketSessionConfig config = {});

    MarketSessionResult evaluate(const LocalMarketTime& local_time) const;

private:
    MarketSessionConfig config_;
};

int minute_of_day(const LocalMarketTime& local_time);
bool is_weekend(Weekday weekday);

}  // namespace tradingbot::strategy

