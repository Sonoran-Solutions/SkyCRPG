// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace skycrpg::camera {
// Input facts are supplied from verified engine state. In particular,
// trustedTransition MUST remain false until a specific menu-related state has
// been observed and its classification audited for the pinned runtime.
struct ResumeObservation {
    bool owned{}, prerequisites{}, world{}, menu{}, firstPerson{};
    bool special{}, hazard{}, trustedTransition{};
    std::uint64_t milliseconds{};
};
enum class ResumeDecision { revoke, defer, retain };

// This state machine never requests a camera transition or reacquires an
// unowned session. A deferred result blocks F6 until a valid first-person
// state is observed. The event sink must feed menu focus changes and input
// observations in their actual order on one serialized owner thread, and call
// cancel() immediately on native POV, load, disable and provider loss.
// State alone cannot prove who caused a later first-person transition: even a
// bounded, otherwise valid transition may be caused by another actor. Do not
// wire this as an unconditional claim of causal ownership.
class PrecisionResume {
public:
    static constexpr std::uint64_t graceMs = 1000;
    void cancel() { phase_ = Phase::normal; start_ = 0; }
    ResumeDecision observe(const ResumeObservation& o) {
        if (!o.owned || !o.prerequisites || !o.world || o.hazard) return revoke();
        if (o.menu) { phase_ = Phase::focused; return ResumeDecision::defer; }
        if (phase_ == Phase::focused) { phase_ = Phase::awaiting; start_ = o.milliseconds; }
        if (phase_ == Phase::awaiting &&
            (o.milliseconds < start_ || o.milliseconds - start_ > graceMs)) return revoke();
        if (o.firstPerson && !o.special) { cancel(); return ResumeDecision::retain; }
        if (phase_ == Phase::awaiting && o.trustedTransition) return ResumeDecision::defer;
        return revoke();
    }
private:
    enum class Phase { normal, focused, awaiting };
    ResumeDecision revoke() { cancel(); return ResumeDecision::revoke; }
    Phase phase_{Phase::normal};
    std::uint64_t start_{};
};
} // namespace skycrpg::camera
