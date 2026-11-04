// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <array>
#include <span>
#include <limits>
#include <cassert>

namespace skycrpg::camera::cursor {
struct Settings {
    bool enabled = false;
    int toggleKey = 48, toggleGamepad = -1; // B; proposal only
    float sensitivity = 900, radius = 48, latchPixels = 12;
    unsigned trace = 0;
};
inline Settings validate(Settings s, const camera::Settings& camera) {
    s.sensitivity = bounded(s.sensitivity,900,1,10000);
    s.radius = bounded(s.radius,48,1,256);
    s.latchPixels = bounded(s.latchPixels,12,1,128);
    // Native map adds remapped bindings; these reserve sibling-module controls.
    constexpr std::array reserved{64,65,66,22,26,27,37,38,2,3,4,5,6,7,8,9,71,72,75,76,77,79,80,81};
    if (s.toggleKey <= 0 || s.toggleKey > 254 || s.toggleKey == camera.toggleKey ||
        s.toggleKey == camera.recenterKey || std::find(reserved.begin(),reserved.end(),s.toggleKey) != reserved.end()) s.toggleKey = -1;
    if (s.toggleGamepad <= 0 || s.toggleGamepad > 65535 || (s.toggleGamepad & (s.toggleGamepad-1)) ||
        s.toggleGamepad == 128 || s.toggleGamepad == camera.toggleGamepad || s.toggleGamepad == camera.recenterGamepad) s.toggleGamepad = -1;
    s.trace = (std::min)(s.trace,512u);
    return s;
}
enum class Exit { none, toggle, disabled, lifecycle, menu, load, death, cell, firstPerson, special, precision, lock, f6, f8, selection, overlay, thread, stale };
constexpr Exit interruptKey(bool pad, int id, bool nativePOV) {
    if (!pad && id == 64) return Exit::f6;
    if (!pad && id == 66) return Exit::f8;
    if ((!pad && id == 22) || (pad && id == 128)) return Exit::lock;
    return nativePOV ? Exit::firstPerson : Exit::none;
}
struct Eligibility {
    bool enabled = false, lifecycle = false, menu = false, loaded = false, dead = false;
    bool sameCell = true, firstPerson = false, special = false, precision = false, locked = false;
    bool selection = false, external = false, overlay = false;
};
constexpr Exit refusal(Eligibility c) {
    if (!c.enabled) return Exit::disabled;
    if (!c.loaded) return Exit::load;
    if (c.dead) return Exit::death;
    if (c.menu) return Exit::menu;
    if (!c.sameCell) return Exit::cell;
    if (c.firstPerson) return Exit::firstPerson;
    if (c.special) return Exit::special;
    if (c.precision) return Exit::precision;
    if (c.locked) return Exit::lock;
    if (!c.lifecycle) return Exit::lifecycle;
    if (!c.selection || !c.external) return Exit::selection;
    if (!c.overlay) return Exit::overlay;
    return Exit::none;
}
struct Point { float x = 0, y = 0; };
inline float distance2(Point a, Point b) { const auto x=a.x-b.x,y=a.y-b.y; return x*x+y*y; }
struct Pointer {
    Point point;
    void centre(float width, float height) { point = {width*.5f,height*.5f}; }
    void move(Point delta, float sensitivity, float width, float height) {
        if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || !std::isfinite(sensitivity) ||
            !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;
        point.x = std::clamp(point.x+delta.x*sensitivity,0.0f,width);
        point.y = std::clamp(point.y-delta.y*sensitivity,0.0f,height);
    }
};
struct Candidate { std::uint32_t id = 0; Point point; float worldDistance2 = 0; };
inline std::uint32_t choose(std::span<const Candidate> candidates, Point pointer, float radius) {
    float best = radius*radius, world = std::numeric_limits<float>::infinity();
    std::uint32_t id = 0;
    for (const auto& c : candidates) {
        const float d = distance2(c.point,pointer);
        if (!c.id || !std::isfinite(d) || !std::isfinite(c.worldDistance2)) continue;
        if (d < best || (d == best && (c.worldDistance2 < world || (c.worldDistance2 == world && c.id < id)))) {
            best = d; world = c.worldDistance2; id = c.id;
        }
    }
    return id;
}
inline bool within(std::span<const Candidate> candidates, std::uint32_t id, Point point, float radius) {
    return id && std::any_of(candidates.begin(),candidates.end(),[&](const auto& c) {
        return c.id == id && distance2(c.point,point) <= radius*radius;
    });
}
inline bool inFront(const float row[4], float x, float y, float z) {
    const float w = row[0]*x+row[1]*y+row[2]*z+row[3];
    return std::isfinite(w) && w > 1e-5f;
}
template<class Register> bool prepareOverlay(bool& registered, bool active, bool pending, Register attempt) {
    if (!registered) {
        assert(!active && !pending); // no session may precede main-thread registration
        if (active || pending) return false;
        registered = attempt();
    }
    return registered;
}
struct HoverAttempts {
    std::uint64_t refresh = 0;
    std::array<std::uint32_t,16> ids{};
    std::size_t count = 0;
    bool claim(std::uint32_t id, std::uint64_t value) {
        if (refresh != value) { refresh = value; count = 0; }
        if (!id || count == ids.size() || std::find(ids.begin(),ids.begin()+count,id) != ids.begin()+count) return false;
        ids[count++] = id;
        return true;
    }
};
struct Freshness {
    std::uint64_t refresh = 0;
    double at = 0, deadline = .5;
    bool seen = false;
    void observe(std::uint64_t value, double now, double interval = .1) {
        deadline = (std::max)(.5,3*(std::isfinite(interval) ? std::clamp(interval,.1,1.0) : .1));
        if (value && (!seen || refresh != value)) { refresh = value; at = now; seen = true; }
    }
    bool usable(double now, bool sessionLost) const { return !sessionLost && seen && now >= at && now-at < deadline; }
};
struct Latch {
    std::uint32_t id = 0;
    Point origin;
    void cycle(std::uint32_t target, Point point) { id = target; origin = point; }
    bool retain(Point point, float threshold, bool valid) {
        if (!valid || distance2(origin,point) > threshold*threshold) id = 0;
        return id != 0;
    }
};
struct Session {
    bool active = false;
    Pointer pointer;
    Latch latch;
    bool toggle(Eligibility c, float width, float height) {
        if (active) { exit(); return false; }
        if (refusal(c) != Exit::none) return false;
        active = true; pointer.centre(width,height); latch = {}; return true;
    }
    void exit() { active = false; latch = {}; }
};
} // namespace skycrpg::camera::cursor
