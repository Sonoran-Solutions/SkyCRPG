// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skycrpg::camera {
constexpr float pi = 3.14159265358979323846f;
constexpr float radians(float degrees) { return degrees * pi / 180.0f; }
struct Settings {
    int schema = 1;
    bool enabled = true;
    float pitch = 55.0f;
    float pitchSpeed = 90.0f;
    float recenterSpeed = 180.0f;
    float zoom = 0.65f;
    float minZoom = 0.25f;
    float maxZoom = 0.90f;
    int toggleKey = 64; // DIK_F6
    int recenterKey = 65; // DIK_F7
    int toggleGamepad = -1;
    int recenterGamepad = -1;
};
inline float bounded(float value, float fallback, float low, float high) {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
inline Settings validate(Settings s) {
    if (s.schema != 1) { s = {}; s.enabled = false; return s; }
    s.pitch = bounded(s.pitch, 55.0f, 15.0f, 75.0f);
    s.pitchSpeed = bounded(s.pitchSpeed, 90.0f, 15.0f, 180.0f);
    s.recenterSpeed = bounded(s.recenterSpeed, 180.0f, 30.0f, 360.0f);
    s.minZoom = bounded(s.minZoom, 0.25f, 0.05f, 0.95f);
    s.maxZoom = bounded(s.maxZoom, 0.90f, s.minZoom, 1.0f);
    s.zoom = bounded(s.zoom, 0.65f, s.minZoom, s.maxZoom);
    s.zoom = std::clamp(s.zoom, s.minZoom, s.maxZoom);
    // F8/provider emergency and K/L/map are never valid custom bindings.
    auto key = [](int k) { return k == -1 || (k > 0 && k <= 255 && k != 66 && k != 37 && k != 38); };
    if (!key(s.toggleKey)) s.toggleKey = -1;
    if (!key(s.recenterKey) || s.recenterKey == s.toggleKey) s.recenterKey = -1;
    auto pad = [](int k) { return k == -1 || (k > 0 && k <= 65535 && (k & (k - 1)) == 0); };
    if (!pad(s.toggleGamepad)) s.toggleGamepad = -1;
    if (!pad(s.recenterGamepad) || s.recenterGamepad == s.toggleGamepad) s.recenterGamepad = -1;
    return s;
}
inline float zoomRequest(float nativeRequest, const Settings& s, bool applyPreference) {
    return applyPreference ? s.zoom : bounded(nativeRequest,s.zoom,s.minZoom,s.maxZoom);
}
enum class State { disabled, invalidWorld, special, menu, firstPerson, precision, targeting, obstruction, normal };
struct Context {
    bool enabled = false, prerequisites = false, world = false, special = false, menu = false;
    bool firstPerson = false, precision = false, targeting = false, obstruction = false;
};
constexpr State select(Context c) {
    if (!c.enabled || !c.prerequisites) return State::disabled;
    if (!c.world) return State::invalidWorld;
    if (c.special) return State::special;
    if (c.menu) return State::menu;
    if (c.firstPerson) return State::firstPerson;
    if (c.precision) return State::precision;
    if (c.targeting) return State::targeting;
    if (c.obstruction) return State::obstruction;
    return State::normal;
}
enum class ToggleRequest { none, firstPerson, thirdPerson, enableOnly };
constexpr ToggleRequest toggleRequest(Context c, bool ownFirstPerson) {
    if (!c.prerequisites || !c.world || c.special || c.menu) return ToggleRequest::none;
    if (c.firstPerson) return ownFirstPerson ? ToggleRequest::thirdPerson : ToggleRequest::none;
    return c.enabled ? ToggleRequest::firstPerson : ToggleRequest::enableOnly;
}
inline float wrap(float angle) { return std::remainder(angle, 2.0f * pi); }
// Native HandleLookInput increments native input angles by input * gain * dt.
// This is an INPUT request, never a camera transform/rotation assignment.
inline float correction(float error, float speed, float gain, float dt) {
    if (!std::isfinite(error) || !std::isfinite(gain) || !std::isfinite(dt) || gain <= 0.0f || dt <= 0.0f || dt > 0.1f) return 0.0f;
    return std::clamp(error, -speed * dt, speed * dt) / (gain * dt);
}
struct FreshPress {
    bool armed = true;
    bool down = false;
    void suspend() { if (down) armed = false; }
    void observeSuppressed(bool pressed) { down = pressed; armed = !pressed; }
    bool update(bool pressed, bool edge) {
        if (!pressed) { armed = true; down = false; return false; }
        const bool fire = armed && !down && edge;
        down = true;
        return fire;
    }
};
} // namespace skycrpg::camera
