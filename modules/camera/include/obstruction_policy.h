// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <limits>
#include <optional>

namespace skycrpg::camera {
struct ObstructionSettings {
    bool enabled = false; // No per-profile settings overlay exists; opt in for testing.
    float enterRatio = .25f, exitRatio = .10f;
    float enterSeconds = .25f, exitSeconds = .8f, warmupSeconds = .5f;
    float tightPitch = 25.0f, tightZoom = .35f;
    float tightenRate = .8f, recoverRate = .15f; // framing blend / second
};
inline ObstructionSettings validateObstruction(ObstructionSettings s, const Settings& camera) {
    s.enterRatio = bounded(s.enterRatio,.25f,.05f,.9f);
    s.exitRatio = bounded(s.exitRatio,.10f,0.0f,s.enterRatio-.01f);
    s.enterSeconds = bounded(s.enterSeconds,.25f,.05f,5.0f);
    s.exitSeconds = bounded(s.exitSeconds,.8f,.1f,10.0f);
    s.warmupSeconds = bounded(s.warmupSeconds,.5f,.1f,5.0f);
    s.tightPitch = bounded(s.tightPitch,25.0f,15.0f,camera.pitch);
    s.tightZoom = bounded(s.tightZoom,.35f,camera.minZoom,camera.maxZoom);
    s.tightenRate = bounded(s.tightenRate,.8f,.05f,2.0f);
    s.recoverRate = bounded(s.recoverRate,.15f,.01f,.5f);
    return s;
}
constexpr bool obstructionFinite(double v) {
    return v >= -std::numeric_limits<double>::max() && v <= std::numeric_limits<double>::max();
}
struct ObstructionPoint { double x, y, z; };
constexpr bool obstructionFinite(ObstructionPoint p) {
    return obstructionFinite(p.x) && obstructionFinite(p.y) && obstructionFinite(p.z);
}
// All positions are copies. No engine objects, physics queries or transform writes.
// focus = GetGoalPosition.world - GetGoalPosition.local on the pinned provider.
inline std::optional<float> cameraCompression(ObstructionPoint actual, ObstructionPoint goal, ObstructionPoint focus) {
    if (!obstructionFinite(actual) || !obstructionFinite(goal) || !obstructionFinite(focus)) return std::nullopt;
    const auto distance = [focus](ObstructionPoint p) {
        return std::hypot(p.x-focus.x,p.y-focus.y,p.z-focus.z);
    };
    const double preferred = distance(goal), rendered = distance(actual);
    if (!obstructionFinite(preferred) || !obstructionFinite(rendered) || preferred < 1.0) return std::nullopt;
    return static_cast<float>(std::clamp(1.0-rendered/preferred,0.0,1.0));
}
struct ObstructionFrame {
    bool valid = false, obstructed = false;
    float blend = 0; // 0 configured preference; 1 tight framing
};
constexpr bool obstructionFramingAllowed(Context context) {
    context.obstruction = false;
    return select(context) == State::normal;
}
class ObstructionPolicy {
public:
    constexpr void reset() { *this = {}; }
    // Compare against the current goal while adjusted. A clear *tight* goal
    // never releases the latch: slowly probe all the way back to the saved
    // preferred framing, then require another full low-compression dwell.
    // Compression during a probe freezes its progress. It does not repeatedly
    // retract/extend the framing in a fixed doorway (a near/far feedback loop).
    [[nodiscard]] constexpr ObstructionFrame step(Context context, std::optional<float> compression,
                                                 double dt, const ObstructionSettings& s) {
        if (!s.enabled || !obstructionFramingAllowed(context) || !compression ||
            !obstructionFinite(*compression) || *compression < 0 || *compression > 1 ||
            !obstructionFinite(dt) || dt <= 0 || dt > .1) {
            reset(); return {};
        }
        if (warm_ < s.warmupSeconds) {
            warm_ += dt; return {true,false,0};
        }
        if (!latched_) {
            high_ = *compression >= s.enterRatio ? high_+dt : 0;
            if (high_ < s.enterSeconds) return {true,false,0};
            latched_ = true;
        }
        if (!probing_) {
            blend_ = std::min(1.0f,blend_+s.tightenRate*static_cast<float>(dt));
            if (blend_ < 1.0f) return {true,true,blend_};
            probing_ = true;
            low_ = 0;
            return {true,true,blend_};
        }
        low_ = *compression <= s.exitRatio ? low_+dt : 0;
        if (low_ >= s.exitSeconds) {
            if (blend_ == 0) {
                latched_ = probing_ = false; high_ = low_ = 0;
                return {true,false,0};
            }
            blend_ = std::max(0.0f,blend_-s.recoverRate*static_cast<float>(dt));
            if (blend_ == 0) low_ = 0; // dwell at preferred, not only at tight goal
        }
        return {true,true,blend_};
    }
private:
    double warm_ = 0, high_ = 0, low_ = 0;
    float blend_ = 0;
    bool latched_ = false, probing_ = false;
};
constexpr float obstructionPitch(float preferred, float blend, const ObstructionSettings& s) {
    return preferred + (std::min(preferred,s.tightPitch)-preferred)*blend;
}
constexpr float obstructionZoom(float preferred, float blend, const ObstructionSettings& s) {
    return preferred + (std::min(preferred,s.tightZoom)-preferred)*blend;
}
// Native zoom changes are deltas relative to the last adjusted input. Persist
// only the user's preferred zoom, never the obstruction controller's output.
inline float preferredZoomRequest(float native, float lastAdjusted, bool adjusted,
                                  const Settings& s, bool applyPreference) {
    if (applyPreference) return s.zoom;
    if (adjusted && std::isfinite(native)) return bounded(s.zoom+(native-lastAdjusted),s.zoom,s.minZoom,s.maxZoom);
    return zoomRequest(native,s,false);
}
} // namespace skycrpg::camera
