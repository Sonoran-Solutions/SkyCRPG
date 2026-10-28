// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "precision_policy.h"
#include <array>

namespace skycrpg::camera {
struct Hazards {
    bool mount = false, killmove = false, swimming = false, furniture = false;
    bool bleedout = false, beast = false, scripted = false;
};
constexpr bool specialHazard(Hazards h) {
    return h.mount || h.killmove || h.swimming || h.furniture || h.bleedout || h.beast || h.scripted;
}
constexpr bool leaseWanted(bool startEnabled, bool runtimeEnableRequested, bool haveLease) {
    return (startEnabled || runtimeEnableRequested) && !haveLease;
}
constexpr bool enableAfterLease(bool haveLease) { return haveLease; }
struct FailureDebounce {
    bool pending = false;
    double since = 0;
    bool observe(bool eligible, double now) {
        if (!eligible) { pending = false; return false; }
        if (!pending) { pending = true; since = now; return false; }
        return now - since >= 1.0;
    }
};
// Slides pause correction without changing the selected lifecycle state.
constexpr bool pauseCorrection(Context c) { return c.slide; }
struct LifecycleTraceDebounce {
    static constexpr unsigned stableFrames = 10;
    State candidate = State::disabled, logged = State::disabled;
    std::uint32_t candidateId = 0, loggedId = 0;
    unsigned frames = 0;
    bool hasCandidate = false, hasLogged = false;
    bool observe(State state, std::uint32_t id) {
        if (!hasCandidate || candidate != state || candidateId != id) {
            candidate = state; candidateId = id; frames = 1; hasCandidate = true;
        } else if (frames < stableFrames) ++frames;
        if (frames < stableFrames || (hasLogged && logged == state && loggedId == id)) return false;
        logged = state; loggedId = id; hasLogged = true; return true;
    }
    void resetCandidate() { hasCandidate = false; frames = 0; }
};
inline void resetTransient(bool& owned, bool& recenter, bool& applyZoom, std::array<FreshPress,4>& keys) {
    owned = recenter = false;
    applyZoom = true;
    // A key held across the reset must be released first; an idle key fires
    // on its next press (no release event arrives for a key not held).
    for (auto& key : keys) key.suspend();
}
constexpr bool applyDeferredPreferences(Context c, bool pending, bool currentState) {
    return pending && currentState && select(c) == State::normal;
}
} // namespace skycrpg::camera
