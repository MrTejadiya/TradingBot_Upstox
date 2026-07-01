#include "tradingbot/order/rate_limited_executor.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::order::RateLimitTimePoint t(int milliseconds) {
    return tradingbot::order::RateLimitTimePoint{std::chrono::milliseconds{milliseconds}};
}

void allows_configured_number_inside_window() {
    tradingbot::order::SlidingWindowRateLimiter limiter({
        .max_executions = 2,
        .window = std::chrono::milliseconds{1000},
    });

    require(limiter.consume(t(0)).allowed, "first execution should be allowed");
    require(limiter.consume(t(100)).allowed, "second execution should be allowed");
    const auto blocked = limiter.consume(t(200));
    require(!blocked.allowed, "third execution in window should be blocked");
    require(blocked.reason_code == "RATE_LIMITED", "blocked reason should be stable");
    require(blocked.retry_after == std::chrono::milliseconds{800}, "retry after should point to oldest expiry");
}

void allows_again_after_window_expires() {
    tradingbot::order::SlidingWindowRateLimiter limiter({
        .max_executions = 1,
        .window = std::chrono::milliseconds{1000},
    });

    require(limiter.consume(t(0)).allowed, "first execution should be allowed");
    require(!limiter.consume(t(999)).allowed, "execution before expiry should be blocked");
    require(limiter.consume(t(1000)).allowed, "execution at expiry should be allowed");
}

void check_does_not_consume_capacity() {
    tradingbot::order::SlidingWindowRateLimiter limiter({
        .max_executions = 1,
        .window = std::chrono::milliseconds{1000},
    });

    require(limiter.check(t(0)).allowed, "check should allow initial capacity");
    require(limiter.check(t(0)).allowed, "check should not consume capacity");
    require(limiter.used() == 0, "check should not record execution");
}

void executor_runs_callback_when_allowed() {
    tradingbot::order::RateLimitedApiExecutor executor({
        .max_executions = 1,
        .window = std::chrono::milliseconds{1000},
    });
    auto call_count = 0;

    const auto result = executor.try_execute(t(0), [&call_count] {
        ++call_count;
        return std::string{"ok"};
    });

    require(result.executed, "executor should run allowed callback");
    require(result.value == "ok", "executor should return callback value");
    require(call_count == 1, "callback should be called once");
}

void executor_skips_callback_when_rate_limited() {
    tradingbot::order::RateLimitedApiExecutor executor({
        .max_executions = 1,
        .window = std::chrono::milliseconds{1000},
    });
    auto call_count = 0;

    executor.try_execute(t(0), [&call_count] {
        ++call_count;
        return std::string{"first"};
    });
    const auto result = executor.try_execute(t(1), [&call_count] {
        ++call_count;
        return std::string{"second"};
    });

    require(!result.executed, "executor should skip rate-limited callback");
    require(!result.value.has_value(), "skipped callback should not have value");
    require(call_count == 1, "rate-limited callback should not be called");
}

}  // namespace

int main() {
    allows_configured_number_inside_window();
    allows_again_after_window_expires();
    check_does_not_consume_capacity();
    executor_runs_callback_when_allowed();
    executor_skips_callback_when_rate_limited();
    return 0;
}

