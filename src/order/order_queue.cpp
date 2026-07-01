#include "tradingbot/order/order_queue.hpp"

#include <algorithm>
#include <utility>

namespace tradingbot::order {

std::string OrderRequestPriorityQueue::push(core::OrderRequest request, OrderPriority priority) {
    const auto sequence = next_sequence_++;
    const auto id = "order-" + std::to_string(sequence + 1);
    entries_.push_back({
        .item = {.id = id, .request = std::move(request), .priority = priority},
        .sequence = sequence,
    });
    return id;
}

std::optional<QueuedOrderRequest> OrderRequestPriorityQueue::peek() const {
    const auto higher_priority = [](const Entry& left, const Entry& right) {
        if (left.item.priority != right.item.priority) {
            return static_cast<int>(left.item.priority) > static_cast<int>(right.item.priority);
        }
        return left.sequence < right.sequence;
    };

    auto best = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->cancelled) {
            continue;
        }
        if (best == entries_.end() || higher_priority(*it, *best)) {
            best = it;
        }
    }
    if (best == entries_.end()) {
        return std::nullopt;
    }
    return best->item;
}

std::optional<QueuedOrderRequest> OrderRequestPriorityQueue::pop() {
    const auto higher_priority = [](const Entry& left, const Entry& right) {
        if (left.item.priority != right.item.priority) {
            return static_cast<int>(left.item.priority) > static_cast<int>(right.item.priority);
        }
        return left.sequence < right.sequence;
    };

    auto best = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->cancelled) {
            continue;
        }
        if (best == entries_.end() || higher_priority(*it, *best)) {
            best = it;
        }
    }
    if (best == entries_.end()) {
        prune_cancelled_front();
        return std::nullopt;
    }

    auto item = best->item;
    entries_.erase(best);
    prune_cancelled_front();
    return item;
}

bool OrderRequestPriorityQueue::cancel(const std::string& id) {
    for (auto& entry : entries_) {
        if (!entry.cancelled && entry.item.id == id) {
            entry.cancelled = true;
            prune_cancelled_front();
            return true;
        }
    }
    return false;
}

bool OrderRequestPriorityQueue::empty() const {
    return size() == 0;
}

std::size_t OrderRequestPriorityQueue::size() const {
    return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(), [](const auto& entry) {
        return !entry.cancelled;
    }));
}

void OrderRequestPriorityQueue::prune_cancelled_front() {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const auto& entry) {
                       return entry.cancelled;
                   }),
                   entries_.end());
}

std::string order_priority_name(OrderPriority priority) {
    switch (priority) {
        case OrderPriority::Low:
            return "low";
        case OrderPriority::Normal:
            return "normal";
        case OrderPriority::High:
            return "high";
        case OrderPriority::Emergency:
            return "emergency";
    }
    return "unknown";
}

}  // namespace tradingbot::order
