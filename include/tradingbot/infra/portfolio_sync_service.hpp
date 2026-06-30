#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tradingbot::infra {

struct PortfolioSyncResult {
    bool ok{false};
    core::PortfolioState portfolio;
    std::vector<ApiEvent> api_events;
    std::string error;
};

class PortfolioSyncService {
public:
    explicit PortfolioSyncService(std::shared_ptr<UpstoxApiClient> api_client);

    PortfolioSyncResult sync();

private:
    std::shared_ptr<UpstoxApiClient> api_client_;
};

std::string funds_and_margin_path();
std::string long_term_holdings_path();
PortfolioSyncResult parse_portfolio_sync_response(const ApiResult& funds_result, const ApiResult& holdings_result);

}  // namespace tradingbot::infra

