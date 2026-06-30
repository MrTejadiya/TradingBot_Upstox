#include "tradingbot/core/domain.hpp"

namespace tradingbot::core {

bool is_valid_instrument_key(const InstrumentKey& key) {
    return !key.value.empty() && key.value.find('|') != std::string::npos;
}

bool has_positive_order_quantity(const OrderRequest& request) {
    return request.quantity > 0;
}

bool is_delivery_day_order(const OrderRequest& request) {
    return request.product == ProductType::Delivery && request.validity == OrderValidity::Day;
}

}  // namespace tradingbot::core

