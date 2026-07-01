#include "tradingbot/strategy/kill_switch.hpp"

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

void inactive_state_allows_trading() {
    const auto result = tradingbot::strategy::KillSwitch{}.evaluate({"NSE_EQ|INE002A01018"}, {},
                                                                    tradingbot::core::Clock::now());

    require(!result.active, "inactive kill switch should not be active");
    require(!result.risk_event.has_value(), "inactive kill switch should not emit risk event");
    require(result.diagnostic == "kill switch inactive", "inactive diagnostic should be stable");
}

void manual_trigger_emits_rejected_risk_event() {
    const auto result = tradingbot::strategy::KillSwitch{}.evaluate(
        {"NSE_EQ|INE002A01018"}, {.manual_triggered = true, .reason = "operator stop"},
        tradingbot::core::Clock::now());

    require(result.active, "manual kill switch should be active");
    require(result.risk_event.has_value(), "manual kill switch should emit risk event");
    require(result.risk_event->decision == tradingbot::core::RiskDecision::Rejected,
            "kill switch event should reject");
    require(result.risk_event->reason_code == "KILL_SWITCH_MANUAL", "manual reason code should win");
    require(result.risk_event->detail == "operator stop", "manual reason detail should be preserved");
}

void config_disabled_trigger_has_stable_reason_code() {
    const auto code = tradingbot::strategy::kill_switch_reason_code({.config_disabled = true});

    require(code == "KILL_SWITCH_CONFIG_DISABLED", "config disabled reason code should be stable");
}

void external_trigger_is_reported_when_no_higher_priority_trigger_exists() {
    const auto result = tradingbot::strategy::KillSwitch{}.evaluate(
        {"NSE_EQ|INE002A01018"}, {.external_triggered = true}, tradingbot::core::Clock::now());

    require(result.active, "external kill switch should be active");
    require(result.risk_event->reason_code == "KILL_SWITCH_EXTERNAL", "external reason code should be reported");
    require(result.risk_event->detail == "kill switch active", "empty reason should use default detail");
}

}  // namespace

int main() {
    inactive_state_allows_trading();
    manual_trigger_emits_rejected_risk_event();
    config_disabled_trigger_has_stable_reason_code();
    external_trigger_is_reported_when_no_higher_priority_trigger_exists();
    return 0;
}

