#include "tradingbot/strategy/exit_engine.hpp"

#include <chrono>
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

tradingbot::strategy::ExitEngineRequest base_request() {
    return {
        .instrument = {
            .key = {"NSE_EQ|INE002A01018"},
            .symbol = "RELIANCE",
            .enabled = true,
            .manual_target_price = 115.0,
            .stop_loss_pct = 5.0,
            .target_profit_pct = 10.0,
        },
        .holding = {
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .quantity = 8,
            .average_buy_price = 100.0,
            .acquired_at = tradingbot::core::Clock::now() - std::chrono::hours{24},
        },
        .quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 100.0},
        .candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .high = 120.0, .close = 110.0}},
        .evaluated_at = tradingbot::core::Clock::now(),
    };
}

tradingbot::core::StrategySignal sell_signal(double confidence, const std::string& name) {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .action = tradingbot::core::TradeAction::Sell,
        .confidence = confidence,
        .suggested_quantity = 1,
        .suggested_entry_price = 104.0,
        .reason = name + " says sell",
        .strategy_name = name,
        .timestamp = tradingbot::core::Clock::now(),
    };
}

void emergency_risk_has_highest_priority() {
    auto request = base_request();
    request.emergency_exit = true;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 94.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.decision.has_value(), "emergency risk should exit");
    require(result.exit_reason == tradingbot::core::ExitReason::EmergencyRisk, "emergency risk should win priority");
    require(result.decision->quantity == 8, "exit quantity should use full holding");
    require(result.decision->confidence == 1.0, "emergency risk should be highest confidence");
}

void stop_loss_beats_manual_target_when_both_match() {
    auto request = base_request();
    request.instrument.manual_target_price = 90.0;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 94.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::StopLoss, "stop loss should beat target exits");
    require(result.decision->source == "exit_engine:stop_loss", "source should name stop loss");
}

void manual_target_beats_fixed_target() {
    auto request = base_request();
    request.instrument.manual_target_price = 110.0;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 116.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::ManualTarget, "manual target should beat fixed target");
    require(result.decision->price == 116.0, "decision price should use fresh quote");
}

void fixed_target_matches_when_manual_target_is_absent() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 111.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::FixedProfitTarget, "fixed target should exit");
}

void strategy_sell_signal_is_used_after_price_rules() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 20.0;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 104.0};
    request.strategy_signals = {sell_signal(0.6, "weak_sell"), sell_signal(0.9, "strong_sell")};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::StrategySignal, "strategy sell should exit after price rules");
    require(result.decision->source == "exit_engine:strategy_signal:strong_sell",
            "strategy exit should pick strongest sell signal");
    require(result.decision->quantity == 8, "strategy exit should still use full holding quantity");
}

void strategy_sell_signal_beats_trailing_stop() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.instrument.trailing_stop_pct = 4.0;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 115.0};
    request.strategy_signals = {sell_signal(0.8, "strategy_exit")};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::StrategySignal,
            "strategy sell should beat trailing stop per SRS priority");
}

void trailing_stop_matches_after_strategy_signals() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.instrument.trailing_stop_pct = 4.0;
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 115.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::TrailingStop, "trailing stop should exit after reversal");
    require(result.decision->source == "exit_engine:trailing_stop", "source should name trailing stop");
}

void trailing_stop_skips_without_favorable_move() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.instrument.trailing_stop_pct = 4.0;
    request.candles = {{.instrument_key = {"NSE_EQ|INE002A01018"}, .high = 100.0, .close = 99.0}};
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 99.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(!result.decision.has_value(), "trailing stop should not exit before favorable move");
}

void maximum_holding_duration_matches_after_trailing_stop() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.instrument.trailing_stop_pct = std::nullopt;
    request.max_holding_duration = std::chrono::hours{12};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::MaximumHoldingDuration,
            "max holding duration should exit when age exceeds limit");
    require(result.decision->source == "exit_engine:maximum_holding_duration",
            "source should name maximum holding duration");
}

void trailing_stop_beats_maximum_holding_duration() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.instrument.trailing_stop_pct = 4.0;
    request.max_holding_duration = std::chrono::hours{12};
    request.quote = tradingbot::core::QuoteSnapshot{.instrument_key = {"NSE_EQ|INE002A01018"}, .ltp = 115.0};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::TrailingStop,
            "trailing stop should beat maximum holding duration per SRS priority");
}

void maximum_holding_duration_skips_before_limit() {
    auto request = base_request();
    request.instrument.manual_target_price = std::nullopt;
    request.instrument.target_profit_pct = 50.0;
    request.max_holding_duration = std::chrono::hours{48};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(!result.decision.has_value(), "max holding duration should skip before configured limit");
}

void no_exit_returns_diagnostic() {
    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(base_request());

    require(!result.decision.has_value(), "no matching exit should not decide");
    require(!result.diagnostics.empty(), "no matching exit should include diagnostic");
}

void rejected_risk_event_triggers_emergency_exit() {
    auto request = base_request();
    request.risk_events = {{
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .decision = tradingbot::core::RiskDecision::Rejected,
        .reason_code = "KILL_SWITCH",
        .detail = "manual kill switch",
        .timestamp = tradingbot::core::Clock::now(),
    }};

    const auto result = tradingbot::strategy::ExitEngine{}.evaluate(request);

    require(result.exit_reason == tradingbot::core::ExitReason::EmergencyRisk,
            "rejected risk event should force emergency exit");
}

}  // namespace

int main() {
    emergency_risk_has_highest_priority();
    stop_loss_beats_manual_target_when_both_match();
    manual_target_beats_fixed_target();
    fixed_target_matches_when_manual_target_is_absent();
    strategy_sell_signal_is_used_after_price_rules();
    strategy_sell_signal_beats_trailing_stop();
    trailing_stop_matches_after_strategy_signals();
    trailing_stop_skips_without_favorable_move();
    maximum_holding_duration_matches_after_trailing_stop();
    trailing_stop_beats_maximum_holding_duration();
    maximum_holding_duration_skips_before_limit();
    no_exit_returns_diagnostic();
    rejected_risk_event_triggers_emergency_exit();
    return 0;
}
