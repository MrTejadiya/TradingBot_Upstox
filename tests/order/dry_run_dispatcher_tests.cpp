#include "tradingbot/order/dry_run_dispatcher.hpp"

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

tradingbot::core::OrderRequest valid_request() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .side = tradingbot::core::OrderSide::Buy,
        .quantity = 3,
        .price = 100.0,
        .tag = "dry-run",
        .source_strategy = "manual_buy",
        .run_id = "run-1",
    };
}

void accepts_valid_order_without_fill() {
    tradingbot::order::DryRunOrderDispatcher dispatcher;

    const auto result = dispatcher.dispatch(valid_request(), tradingbot::core::Clock::now());

    require(result.accepted, "valid dry-run order should be accepted");
    require(result.record.status == tradingbot::core::OrderStatus::Accepted, "dry-run status should be accepted");
    require(result.record.broker_order_id == "dry-run-1", "dry-run id should be deterministic");
    require(result.record.filled_quantity == 0, "dry-run should not mark fills");
    require(result.record.redacted_response_metadata == "dry_run=true", "dry-run metadata should be explicit");
}

void stores_dispatched_records() {
    tradingbot::order::DryRunOrderDispatcher dispatcher;
    dispatcher.dispatch(valid_request(), tradingbot::core::Clock::now());
    dispatcher.dispatch(valid_request(), tradingbot::core::Clock::now());

    require(dispatcher.records().size() == 2, "dispatcher should retain dry-run records");
    require(dispatcher.records().back().broker_order_id == "dry-run-2", "second id should increment");
}

void rejects_invalid_instrument() {
    tradingbot::order::DryRunOrderDispatcher dispatcher;
    auto request = valid_request();
    request.instrument_key = {"bad-key"};

    const auto result = dispatcher.dispatch(request, tradingbot::core::Clock::now());

    require(!result.accepted, "invalid instrument should reject");
    require(result.record.status == tradingbot::core::OrderStatus::Rejected, "invalid instrument status should reject");
    require(result.record.rejection_reason.find("instrument") != std::string::npos,
            "rejection should explain instrument issue");
}

void rejects_non_positive_quantity() {
    tradingbot::order::DryRunOrderDispatcher dispatcher;
    auto request = valid_request();
    request.quantity = 0;

    const auto result = dispatcher.dispatch(request, tradingbot::core::Clock::now());

    require(!result.accepted, "zero quantity should reject");
    require(result.record.rejection_reason.find("quantity") != std::string::npos,
            "rejection should explain quantity issue");
}

void rejects_limit_order_without_positive_price() {
    tradingbot::order::DryRunOrderDispatcher dispatcher;
    auto request = valid_request();
    request.price = 0.0;

    const auto result = dispatcher.dispatch(request, tradingbot::core::Clock::now());

    require(!result.accepted, "zero limit price should reject");
    require(result.record.rejection_reason.find("price") != std::string::npos, "rejection should explain price issue");
}

}  // namespace

int main() {
    accepts_valid_order_without_fill();
    stores_dispatched_records();
    rejects_invalid_instrument();
    rejects_non_positive_quantity();
    rejects_limit_order_without_positive_price();
    return 0;
}

