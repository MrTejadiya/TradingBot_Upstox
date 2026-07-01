#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tradingbot::order {

enum class OrderPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Emergency = 3,
};

struct QueuedOrderRequest {
    std::string id;
    core::OrderRequest request;
    OrderPriority priority{OrderPriority::Normal};
};

class OrderRequestPriorityQueue {
public:
    std::string push(core::OrderRequest request, OrderPriority priority = OrderPriority::Normal);
    std::optional<QueuedOrderRequest> peek() const;
    std::optional<QueuedOrderRequest> pop();
    bool cancel(const std::string& id);
    bool empty() const;
    std::size_t size() const;

private:
    struct Entry {
        QueuedOrderRequest item;
        std::size_t sequence{0};
        bool cancelled{false};
    };

    void prune_cancelled_front();

    std::vector<Entry> entries_;
    std::size_t next_sequence_{0};
};

std::string order_priority_name(OrderPriority priority);

}  // namespace tradingbot::order

