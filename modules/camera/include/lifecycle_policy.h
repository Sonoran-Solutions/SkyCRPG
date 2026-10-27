// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "precision_policy.h"
#include <array>

namespace skycrpg::camera {
struct Hazards {
    bool mount = false, killmove = false, swimming = false, furniture = false;
    bool bleedout = false, beast = false, scripted = false, slide = false;
};
constexpr bool specialHazard(Hazards h) {
    return h.mount || h.killmove || h.swimming || h.furniture || h.bleedout || h.beast || h.scripted || h.slide;
}
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
