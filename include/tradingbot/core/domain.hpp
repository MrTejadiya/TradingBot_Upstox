#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::core {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using Quantity = std::int64_t;
using Money = double;
using Percent = double;

enum class Exchange {
    Unknown,
    NseEq,
    BseEq,
};

enum class TradeAction {
    Buy,
    Sell,
};

enum class OrderSide {
    Buy,
    Sell,
};

enum class ProductType {
    Delivery,
};

enum class OrderValidity {
    Day,
};

enum class OrderType {
    Limit,
    Market,
};

enum class OrderStatus {
    Pending,
    Accepted,
    Rejected,
    PartiallyFilled,
    Filled,
    Cancelled,
    TimedOut,
};

enum class DecisionType {
    Buy,
    Sell,
    Hold,
};

enum class RiskDecision {
    Approved,
    Rejected,
};

enum class ExitReason {
    EmergencyRisk,
    StopLoss,
    ManualTarget,
    FixedProfitTarget,
    StrategyTarget,
    StrategySignal,
    TrailingStop,
    MaximumHoldingDuration,
};

struct InstrumentKey {
    std::string value;
};

struct Instrument {
    InstrumentKey key;
    std::string symbol;
    Exchange exchange{Exchange::Unknown};
    bool enabled{true};
    Quantity quantity{0};
    Quantity max_position_quantity{0};
    std::optional<Money> manual_buy_price;
    std::optional<Money> manual_target_price;
    std::optional<Percent> stop_loss_pct;
    Percent target_profit_pct{10.0};
    std::optional<Percent> trailing_stop_pct;
    std::string strategy_profile;
    std::string notes;
};

struct Candle {
    InstrumentKey instrument_key;
    TimePoint timestamp{};
    Money open{0.0};
    Money high{0.0};
    Money low{0.0};
    Money close{0.0};
    Quantity volume{0};
    std::string interval;
};

struct QuoteSnapshot {
    InstrumentKey instrument_key;
    TimePoint timestamp{};
    Money ltp{0.0};
    bool stale{false};
};

struct StrategySignal {
    InstrumentKey instrument_key;
    TradeAction action{TradeAction::Buy};
    double confidence{0.0};
    Quantity suggested_quantity{0};
    std::optional<Money> suggested_entry_price;
    std::optional<Money> suggested_target_price;
    std::optional<Money> suggested_stop_loss;
    std::string reason;
    std::string strategy_name;
    TimePoint timestamp{};
};

struct Decision {
    InstrumentKey instrument_key;
    DecisionType type{DecisionType::Hold};
    double confidence{0.0};
    Quantity quantity{0};
    std::optional<Money> price;
    std::string reason;
    std::string source;
    TimePoint timestamp{};
};

struct RiskEvent {
    InstrumentKey instrument_key;
    RiskDecision decision{RiskDecision::Rejected};
    std::string reason_code;
    std::string detail;
    TimePoint timestamp{};
};

struct OrderRequest {
    InstrumentKey instrument_key;
    OrderSide side{OrderSide::Buy};
    Quantity quantity{0};
    Money price{0.0};
    ProductType product{ProductType::Delivery};
    OrderValidity validity{OrderValidity::Day};
    OrderType order_type{OrderType::Limit};
    std::string tag;
    std::string source_strategy;
    std::string run_id;
};

struct OrderRecord {
    OrderRequest request;
    std::string broker_order_id;
    OrderStatus status{OrderStatus::Pending};
    std::string rejection_reason;
    Quantity filled_quantity{0};
    std::optional<Money> average_fill_price;
    std::string redacted_response_metadata;
    TimePoint updated_at{};
};

struct Holding {
    InstrumentKey instrument_key;
    Quantity quantity{0};
    Money average_buy_price{0.0};
    TimePoint acquired_at{};
};

struct PortfolioState {
    Money available_funds{0.0};
    std::vector<Holding> holdings;
    std::vector<OrderRecord> open_orders;
    TimePoint updated_at{};
};

struct BotRun {
    std::string run_id;
    TimePoint started_at{};
    std::optional<TimePoint> ended_at;
    std::string mode;
    std::string config_hash;
};

bool is_valid_instrument_key(const InstrumentKey& key);
bool has_positive_order_quantity(const OrderRequest& request);
bool is_delivery_day_order(const OrderRequest& request);

}  // namespace tradingbot::core

