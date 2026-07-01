#include "tradingbot/strategy/scanner_signal_ranker.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void require_near(double actual, double expected, double tolerance, const std::string& message) {
    require(std::fabs(actual - expected) <= tolerance, message);
}

tradingbot::core::StrategySignal signal(const std::string& key, tradingbot::core::TradeAction action,
                                        double confidence, const std::string& strategy_name,
                                        double price = 100.0) {
    return {
        .instrument_key = {key},
        .action = action,
        .confidence = confidence,
        .suggested_quantity = 1,
        .suggested_entry_price = price,
        .reason = strategy_name + " signal",
        .strategy_name = strategy_name,
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void ranks_buy_candidates_by_weighted_scanner_score() {
    const auto ranked = tradingbot::strategy::rank_scanner_signals(
        {
            signal("NSE_EQ|AAA", tradingbot::core::TradeAction::Buy, 0.80, "rsi_divergence", 101.0),
            signal("NSE_EQ|BBB", tradingbot::core::TradeAction::Buy, 0.70, "macd_bullish_cross", 202.0),
        },
        {
            .strategy_weights = {{"rsi_divergence", 1.0}, {"macd_bullish_cross", 1.3}},
        },
        tradingbot::core::TimePoint{std::chrono::seconds{7}});

    require(ranked.size() == 2, "two buy candidates should rank");
    require(ranked[0].instrument_key.value == "NSE_EQ|BBB", "weighted MACD score should outrank RSI");
    require_near(ranked[0].score, 0.91, 0.000001, "rank score should apply scanner weight");
    require(ranked[0].suggested_entry_price && *ranked[0].suggested_entry_price == 202.0,
            "rank should retain strongest signal price");
    require(ranked[0].suggested_quantity == 1, "rank should retain suggested quantity");
    require(ranked[0].ranked_at == tradingbot::core::TimePoint{std::chrono::seconds{7}},
            "rank timestamp should be retained");
}

void combines_multiple_scanner_signals_for_same_stock() {
    const auto ranked = tradingbot::strategy::rank_scanner_signals(
        {
            signal("NSE_EQ|AAA", tradingbot::core::TradeAction::Buy, 0.60, "rsi_divergence"),
            signal("NSE_EQ|AAA", tradingbot::core::TradeAction::Buy, 0.70, "macd_bullish_cross"),
            signal("NSE_EQ|BBB", tradingbot::core::TradeAction::Buy, 0.95, "rsi_divergence"),
        },
        {.strategy_weights = {{"macd_bullish_cross", 1.2}}},
        tradingbot::core::TimePoint{});

    require(ranked.size() == 2, "signals should group by instrument");
    require(ranked[0].instrument_key.value == "NSE_EQ|AAA", "combined scanner score should rank first");
    require(ranked[0].signal_count == 2, "candidate should count both scanner signals");
    require(ranked[0].strategy_names.size() == 2, "candidate should retain unique scanner names");
    require(ranked[0].strongest_confidence == 0.70, "candidate should retain strongest raw confidence");
}

void filters_to_requested_action_and_actionable_signals() {
    auto invalid = signal("NSE_EQ|BAD", tradingbot::core::TradeAction::Buy, 0.80, "rsi_divergence");
    invalid.suggested_quantity = 0;

    const auto ranked = tradingbot::strategy::rank_scanner_signals({
        signal("NSE_EQ|BUY", tradingbot::core::TradeAction::Buy, 0.70, "rsi_divergence"),
        signal("NSE_EQ|SELL", tradingbot::core::TradeAction::Sell, 0.99, "macd_bearish_cross"),
        invalid,
    });

    require(ranked.size() == 1, "default ranker should include only actionable buy signals");
    require(ranked[0].instrument_key.value == "NSE_EQ|BUY", "buy candidate should remain");
}

void supports_sell_candidate_ranking() {
    const auto ranked = tradingbot::strategy::rank_scanner_signals(
        {
            signal("NSE_EQ|BUY", tradingbot::core::TradeAction::Buy, 0.99, "rsi_divergence"),
            signal("NSE_EQ|SELL", tradingbot::core::TradeAction::Sell, 0.80, "macd_bearish_cross"),
        },
        {.action = tradingbot::core::TradeAction::Sell});

    require(ranked.size() == 1, "sell ranking should include only sell signals");
    require(ranked[0].instrument_key.value == "NSE_EQ|SELL", "sell candidate should remain");
    require(ranked[0].action == tradingbot::core::TradeAction::Sell, "rank action should be sell");
}

void applies_top_n_limit_and_deterministic_ties() {
    const auto ranked = tradingbot::strategy::rank_scanner_signals(
        {
            signal("NSE_EQ|CCC", tradingbot::core::TradeAction::Buy, 0.80, "rsi_divergence"),
            signal("NSE_EQ|AAA", tradingbot::core::TradeAction::Buy, 0.80, "rsi_divergence"),
            signal("NSE_EQ|BBB", tradingbot::core::TradeAction::Buy, 0.80, "rsi_divergence"),
        },
        {.limit = 2});

    require(ranked.size() == 2, "ranker should apply top-N limit");
    require(ranked[0].instrument_key.value == "NSE_EQ|AAA", "ties should sort by instrument key");
    require(ranked[1].instrument_key.value == "NSE_EQ|BBB", "second tie should be deterministic");
}

}  // namespace

int main() {
    ranks_buy_candidates_by_weighted_scanner_score();
    combines_multiple_scanner_signals_for_same_stock();
    filters_to_requested_action_and_actionable_signals();
    supports_sell_candidate_ranking();
    applies_top_n_limit_and_deterministic_ties();
    return 0;
}
