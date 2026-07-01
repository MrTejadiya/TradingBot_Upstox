#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::order {

struct OrderTrackingResult {
    bool ok{false};
    std::vector<core::OrderRecord> records;
    infra::ApiEvent api_event;
    std::string error;
};

class OrderMonitor {
public:
    explicit OrderMonitor(std::shared_ptr<infra::UpstoxApiClient> api_client);

    OrderTrackingResult fetch_order_book();

private:
    std::shared_ptr<infra::UpstoxApiClient> api_client_;
};

std::string order_book_path();
core::OrderStatus map_upstox_order_status(const std::string& status);
bool is_terminal_order_status(core::OrderStatus status);
OrderTrackingResult parse_order_book_response(const infra::ApiResult& api_result);

}  // namespace tradingbot::order

