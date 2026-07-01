#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::order {

using RateLimitClock = std::chrono::steady_clock;
using RateLimitTimePoint = RateLimitClock::time_point;

struct RateLimitConfig {
    int max_executions{1};
    std::chrono::milliseconds window{std::chrono::milliseconds{1000}};
};

struct RateLimitDecision {
    bool allowed{false};
    std::chrono::milliseconds retry_after{std::chrono::milliseconds{0}};
    std::string reason_code;
};

class SlidingWindowRateLimiter {
public:
    explicit SlidingWindowRateLimiter(RateLimitConfig config = {});

    RateLimitDecision check(RateLimitTimePoint now) const;
    RateLimitDecision consume(RateLimitTimePoint now);
    std::size_t used() const;

private:
    void prune(RateLimitTimePoint now);

    RateLimitConfig config_;
    std::vector<RateLimitTimePoint> executions_;
};

struct ApiExecutionResult {
    bool executed{false};
    RateLimitDecision rate_limit;
    std::optional<std::string> value;
};

class RateLimitedApiExecutor {
public:
    explicit RateLimitedApiExecutor(RateLimitConfig config = {});

    ApiExecutionResult try_execute(RateLimitTimePoint now, const std::function<std::string()>& operation);
    std::size_t used() const;

private:
    SlidingWindowRateLimiter limiter_;
};

}  // namespace tradingbot::order

