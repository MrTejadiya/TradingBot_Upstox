#include "tradingbot/strategy/kill_switch.hpp"

namespace tradingbot::strategy {

KillSwitchResult KillSwitch::evaluate(const core::InstrumentKey& instrument_key, const KillSwitchState& state,
                                      core::TimePoint evaluated_at) const {
    if (!state.manual_triggered && !state.config_disabled && !state.external_triggered) {
        return {.active = false, .diagnostic = "kill switch inactive"};
    }

    const auto reason_code = kill_switch_reason_code(state);
    const auto detail = state.reason.empty() ? "kill switch active" : state.reason;
    return {
        .active = true,
        .risk_event = core::RiskEvent{
            .instrument_key = instrument_key,
            .decision = core::RiskDecision::Rejected,
            .reason_code = reason_code,
            .detail = detail,
            .timestamp = evaluated_at,
        },
        .diagnostic = detail,
    };
}

std::string kill_switch_reason_code(const KillSwitchState& state) {
    if (state.manual_triggered) {
        return "KILL_SWITCH_MANUAL";
    }
    if (state.config_disabled) {
        return "KILL_SWITCH_CONFIG_DISABLED";
    }
    if (state.external_triggered) {
        return "KILL_SWITCH_EXTERNAL";
    }
    return "KILL_SWITCH_INACTIVE";
}

}  // namespace tradingbot::strategy

