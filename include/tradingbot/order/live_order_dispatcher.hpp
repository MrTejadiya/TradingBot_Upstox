#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"
#include "tradingbot/order/dry_run_dispatcher.hpp"
#include "tradingbot/order/rate_limited_executor.hpp"
#include "tradingbot/strategy/market_session.hpp"

#include <memory>
#include <string>

namespace tradingbot::order {

struct LiveOrderSafetyGates {
    bool live_trading_enabled{false};
    core::RiskEvent risk_event;
    strategy::MarketSessionResult market_session;
    RateLimitTimePoint rate_limit_time{};
};

class LiveOrderDispatcher {
public:
    LiveOrderDispatcher(std::shared_ptr<infra::UpstoxApiClient> api_client, RateLimitConfig rate_limit_config = {});

    DispatchResult dispatch(const core::OrderRequest& request, const LiveOrderSafetyGates& gates,
                            core::TimePoint dispatched_at);

private:
    DispatchResult reject(const core::OrderRequest& request, const std::string& reason,
                          core::TimePoint dispatched_at) const;

    std::shared_ptr<infra::UpstoxApiClient> api_client_;
    RateLimitedApiExecutor executor_;
};

std::string upstox_place_order_v3_path();
std::string build_upstox_place_order_v3_payload(const core::OrderRequest& request);
std::string parse_upstox_order_id(const std::string& response_body);

}  // namespace tradingbot::order

