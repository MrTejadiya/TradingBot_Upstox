#include "tradingbot/order/order_queue.hpp"

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

tradingbot::core::OrderRequest request(const std::string& tag) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .side = tradingbot::core::OrderSide::Buy,
        .quantity = 1,
        .price = 100.0,
        .tag = tag,
        .source_strategy = "test",
        .run_id = "run-1",
    };
}

void pops_highest_priority_first() {
    tradingbot::order::OrderRequestPriorityQueue queue;
    queue.push(request("normal"), tradingbot::order::OrderPriority::Normal);
    queue.push(request("emergency"), tradingbot::order::OrderPriority::Emergency);
    queue.push(request("high"), tradingbot::order::OrderPriority::High);

    const auto item = queue.pop();

    require(item.has_value(), "queue should pop an item");
    require(item->request.tag == "emergency", "emergency priority should pop first");
}

void preserves_fifo_order_within_same_priority() {
    tradingbot::order::OrderRequestPriorityQueue queue;
    queue.push(request("first"), tradingbot::order::OrderPriority::High);
    queue.push(request("second"), tradingbot::order::OrderPriority::High);

    require(queue.pop()->request.tag == "first", "first same-priority request should pop first");
    require(queue.pop()->request.tag == "second", "second same-priority request should pop second");
}

void peek_does_not_remove_item() {
    tradingbot::order::OrderRequestPriorityQueue queue;
    queue.push(request("one"), tradingbot::order::OrderPriority::Normal);

    const auto peeked = queue.peek();

    require(peeked.has_value(), "peek should return item");
    require(queue.size() == 1, "peek should not remove item");
    require(queue.pop()->id == peeked->id, "pop should return peeked item");
}

void cancels_queued_request() {
    tradingbot::order::OrderRequestPriorityQueue queue;
    const auto low_id = queue.push(request("low"), tradingbot::order::OrderPriority::Low);
    queue.push(request("normal"), tradingbot::order::OrderPriority::Normal);

    require(queue.cancel(low_id), "cancel should return true for queued id");
    require(queue.size() == 1, "cancelled item should not count toward size");
    require(queue.pop()->request.tag == "normal", "cancelled low item should never pop");
    require(!queue.cancel(low_id), "cancel should return false for already removed id");
}

void reports_empty_queue() {
    tradingbot::order::OrderRequestPriorityQueue queue;

    require(queue.empty(), "new queue should be empty");
    require(!queue.pop().has_value(), "empty pop should return nullopt");
}

void priority_names_are_stable() {
    require(tradingbot::order::order_priority_name(tradingbot::order::OrderPriority::Emergency) == "emergency",
            "emergency priority name should be stable");
}

}  // namespace

int main() {
    pops_highest_priority_first();
    preserves_fifo_order_within_same_priority();
    peek_does_not_remove_item();
    cancels_queued_request();
    reports_empty_queue();
    priority_names_are_stable();
    return 0;
}

