#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <vector>

namespace tradingbot::order {

struct DispatchResult {
    bool accepted{false};
    core::OrderRecord record;
};

class DryRunOrderDispatcher {
public:
    DispatchResult dispatch(const core::OrderRequest& request, core::TimePoint dispatched_at);
    const std::vector<core::OrderRecord>& records() const;

private:
    std::string next_order_id();

    std::vector<core::OrderRecord> records_;
    int next_id_{1};
};

}  // namespace tradingbot::order

