#include "tradingbot/order/rate_limited_executor.hpp"

#include <algorithm>

namespace tradingbot::order {

SlidingWindowRateLimiter::SlidingWindowRateLimiter(RateLimitConfig config) : config_(config) {
    if (config_.max_executions <= 0) {
        config_.max_executions = 1;
    }
    if (config_.window <= std::chrono::milliseconds{0}) {
        config_.window = std::chrono::milliseconds{1000};
    }
}

RateLimitDecision SlidingWindowRateLimiter::check(RateLimitTimePoint now) const {
    auto active = executions_;
    active.erase(std::remove_if(active.begin(), active.end(), [this, now](const auto execution) {
                     return now - execution >= config_.window;
                 }),
                 active.end());

    if (active.size() < static_cast<std::size_t>(config_.max_executions)) {
        return {.allowed = true, .reason_code = "RATE_LIMIT_ALLOWED"};
    }

    const auto oldest = *std::min_element(active.begin(), active.end());
    return {
        .allowed = false,
        .retry_after = std::chrono::duration_cast<std::chrono::milliseconds>(config_.window - (now - oldest)),
        .reason_code = "RATE_LIMITED",
    };
}

RateLimitDecision SlidingWindowRateLimiter::consume(RateLimitTimePoint now) {
    prune(now);
    const auto decision = check(now);
    if (decision.allowed) {
        executions_.push_back(now);
    }
    return decision;
}

std::size_t SlidingWindowRateLimiter::used() const {
    return executions_.size();
}

void SlidingWindowRateLimiter::prune(RateLimitTimePoint now) {
    executions_.erase(std::remove_if(executions_.begin(), executions_.end(), [this, now](const auto execution) {
                          return now - execution >= config_.window;
                      }),
                      executions_.end());
}

RateLimitedApiExecutor::RateLimitedApiExecutor(RateLimitConfig config) : limiter_(config) {}

ApiExecutionResult RateLimitedApiExecutor::try_execute(RateLimitTimePoint now,
                                                       const std::function<std::string()>& operation) {
    const auto decision = limiter_.consume(now);
    if (!decision.allowed) {
        return {.executed = false, .rate_limit = decision};
    }
    return {.executed = true, .rate_limit = decision, .value = operation()};
}

std::size_t RateLimitedApiExecutor::used() const {
    return limiter_.used();
}

}  // namespace tradingbot::order

