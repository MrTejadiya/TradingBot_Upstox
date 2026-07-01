#include "tradingbot/paper/paper_portfolio_simulator.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <utility>

namespace tradingbot::paper {
namespace {

bool is_filled_status(core::OrderStatus status) {
    return status == core::OrderStatus::Filled || status == core::OrderStatus::PartiallyFilled;
}

std::optional<core::Money> fill_price(const core::OrderRecord& record) {
    if (record.average_fill_price) {
        return *record.average_fill_price;
    }
    if (record.request.price > 0.0) {
        return record.request.price;
    }
    return std::nullopt;
}

auto find_holding(std::vector<core::Holding>& holdings, const core::InstrumentKey& key) {
    return std::find_if(holdings.begin(), holdings.end(), [&key](const core::Holding& holding) {
        return holding.instrument_key.value == key.value;
    });
}

auto find_holding(const std::vector<core::Holding>& holdings, const core::InstrumentKey& key) {
    return std::find_if(holdings.begin(), holdings.end(), [&key](const core::Holding& holding) {
        return holding.instrument_key.value == key.value;
    });
}

}  // namespace

PaperPortfolioSimulator::PaperPortfolioSimulator(core::PortfolioState starting_portfolio)
    : portfolio_(std::move(starting_portfolio)) {}

PaperSimulationResult PaperPortfolioSimulator::apply_fill(const core::OrderRecord& record) {
    PaperSimulationResult result{.portfolio = portfolio_, .realized_pnl = realized_pnl_};
    if (!is_filled_status(record.status) || record.filled_quantity <= 0) {
        return result;
    }

    const auto price = fill_price(record);
    if (!price) {
        result.rejection_reason = "filled order requires a positive fill price";
        return result;
    }

    const auto value = *price * static_cast<double>(record.filled_quantity);
    if (record.request.side == core::OrderSide::Buy) {
        if (portfolio_.available_funds < value) {
            result.rejection_reason = "insufficient paper funds";
            return result;
        }

        portfolio_.available_funds -= value;
        auto holding = find_holding(portfolio_.holdings, record.request.instrument_key);
        if (holding == portfolio_.holdings.end()) {
            portfolio_.holdings.push_back({
                .instrument_key = record.request.instrument_key,
                .quantity = record.filled_quantity,
                .average_buy_price = *price,
                .acquired_at = record.updated_at,
            });
        } else {
            const auto existing_value = holding->average_buy_price * static_cast<double>(holding->quantity);
            const auto combined_quantity = holding->quantity + record.filled_quantity;
            holding->average_buy_price = (existing_value + value) / static_cast<double>(combined_quantity);
            holding->quantity = combined_quantity;
        }
    } else {
        auto holding = find_holding(portfolio_.holdings, record.request.instrument_key);
        if (holding == portfolio_.holdings.end() || holding->quantity < record.filled_quantity) {
            result.rejection_reason = "insufficient paper holding";
            return result;
        }

        portfolio_.available_funds += value;
        realized_pnl_ += (*price - holding->average_buy_price) * static_cast<double>(record.filled_quantity);
        holding->quantity -= record.filled_quantity;
        if (holding->quantity == 0) {
            portfolio_.holdings.erase(holding);
        }
    }

    portfolio_.updated_at = record.updated_at;
    result.applied = true;
    result.portfolio = portfolio_;
    result.realized_pnl = realized_pnl_;
    return result;
}

PaperPerformanceSnapshot PaperPortfolioSimulator::performance(const std::vector<core::QuoteSnapshot>& quotes) const {
    std::unordered_map<std::string, core::Money> latest_prices;
    for (const auto& quote : quotes) {
        if (!quote.instrument_key.value.empty() && quote.ltp > 0.0) {
            latest_prices[quote.instrument_key.value] = quote.ltp;
        }
    }

    PaperPerformanceSnapshot snapshot{
        .available_funds = portfolio_.available_funds,
        .realized_pnl = realized_pnl_,
    };

    for (const auto& holding : portfolio_.holdings) {
        const auto cost = holding.average_buy_price * static_cast<double>(holding.quantity);
        const auto quote = latest_prices.find(holding.instrument_key.value);
        const auto market_price = quote == latest_prices.end() ? holding.average_buy_price : quote->second;
        const auto market_value = market_price * static_cast<double>(holding.quantity);
        snapshot.holdings_cost += cost;
        snapshot.holdings_market_value += market_value;
        snapshot.unrealized_pnl += market_value - cost;
    }

    snapshot.total_equity = snapshot.available_funds + snapshot.holdings_market_value;
    return snapshot;
}

const core::PortfolioState& PaperPortfolioSimulator::portfolio() const {
    return portfolio_;
}

core::Money PaperPortfolioSimulator::realized_pnl() const {
    return realized_pnl_;
}

}  // namespace tradingbot::paper
