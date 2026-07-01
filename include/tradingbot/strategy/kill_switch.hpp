#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::strategy {

struct KillSwitchState {
    bool manual_triggered{false};
    bool config_disabled{false};
    bool external_triggered{false};
    std::string reason;
};

struct KillSwitchResult {
    bool active{false};
    std::optional<core::RiskEvent> risk_event;
    std::string diagnostic;
};

class KillSwitch {
public:
    KillSwitchResult evaluate(const core::InstrumentKey& instrument_key, const KillSwitchState& state,
                              core::TimePoint evaluated_at) const;
};

std::string kill_switch_reason_code(const KillSwitchState& state);

}  // namespace tradingbot::strategy

