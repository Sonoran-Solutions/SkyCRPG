// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <array>
#include <charconv>
#include <optional>
#include <string_view>

namespace skycrpg::camera::thin {
constexpr unsigned maxForms = 16, maxReferences = 8, maxParts = 16, maxPool = 256;
constexpr unsigned maxCasts = 24, maxGaps = 16, maxSteps = 8, maxContacts = 256;
constexpr unsigned maxTraceLines = 12000;
inline float validateAlpha(float alpha) { return bounded(alpha,.35f,.1f,.8f); }
constexpr bool recycle(bool active,bool conflict,bool sameMaterial,bool sameFlags,bool sameAlpha,
    bool reserved,std::uint64_t retired,std::uint64_t frame) {
    return !active && !conflict && sameMaterial && sameFlags && sameAlpha && !reserved &&
        retired && frame >= retired && frame-retired >= 2;
}
struct FormEntry { std::string_view plugin; std::uint32_t local; };
constexpr std::array defaults{FormEntry{"Skyrim.esm",0x602A6},
    FormEntry{"Skyrim.esm",0x602AF},FormEntry{"Skyrim.esm",0x616EA}};
inline std::optional<FormEntry> parseForm(std::string_view text) {
    const auto split = text.find('|');
    if (split == std::string_view::npos || split == 0 || split > 100) return {};
    const auto plugin = text.substr(0,split), id = text.substr(split+1);
    if (plugin.find_first_of("/\\:\r\n") != std::string_view::npos || id.empty() || id.size() > 6) return {};
    std::uint32_t local{};
    const auto result = std::from_chars(id.data(),id.data()+id.size(),local,16);
    if (result.ec != std::errc{} || result.ptr != id.data()+id.size() || !local || local > 0xffffff) return {};
    return FormEntry{plugin,local};
}
inline bool matches(std::uint32_t base,const std::array<std::uint32_t,maxForms>& forms) {
    return base && std::find(forms.begin(),forms.end(),base) != forms.end();
}
constexpr bool eligible(State state) { return state == State::normal || state == State::targetLock || state == State::obstruction; }
constexpr bool install(bool enabled,bool hashes,bool addresses,bool prologues,bool forms) {
    return enabled && hashes && addresses && prologues && forms;
}
struct Budget {
    unsigned casts{}, gaps{}, fades{}, scans{}, refused{}, discarded{}, kept{}, unknown{}, overlap{}, cap{};
    bool cast() { if (casts == maxCasts) return false; ++casts; return true; }
    bool gap() { if (gaps == maxGaps) return false; ++gaps; return true; }
};
struct Point { float x{},y{},z{}; };
inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline float distance(Point a,Point b) { return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z); }
inline bool intersects(Point from,Point to,Point center,float radius) {
    if (!finite(from) || !finite(to) || !finite(center) || !std::isfinite(radius) || radius <= 0 || radius > 2048) return false;
    const Point d{to.x-from.x,to.y-from.y,to.z-from.z};
    const float length2 = d.x*d.x+d.y*d.y+d.z*d.z;
    if (length2 <= .001f) return false;
    const float t = std::clamp(((center.x-from.x)*d.x+(center.y-from.y)*d.y+(center.z-from.z)*d.z)/length2,0.0f,1.0f);
    return distance({from.x+t*d.x,from.y+t*d.y,from.z+t*d.z},center) <= radius;
}
// Segment versus a local, axis-aligned OBND box. Caller applies the inverse
// reference transform; a whole-model sphere is only a preparation broad phase.
inline bool intersectsBox(Point from,Point to,Point low,Point high,float margin = 2) {
    if (!finite(from) || !finite(to) || !finite(low) || !finite(high) ||
        !std::isfinite(margin) || margin < 0) return false;
    const std::array a{from.x,from.y,from.z}, b{to.x,to.y,to.z};
    const std::array lo{low.x,low.y,low.z}, hi{high.x,high.y,high.z};
    float enter = 0, leave = 1;
    for (unsigned i = 0;i < 3;++i) {
        if (lo[i] > hi[i]) return false;
        const float delta = b[i]-a[i];
        if (std::abs(delta) < .0001f) {
            if (a[i] < lo[i]-margin || a[i] > hi[i]+margin) return false;
        } else {
            float first = (lo[i]-margin-a[i])/delta, last = (hi[i]+margin-a[i])/delta;
            if (first > last) std::swap(first,last);
            enter = (std::max)(enter,first); leave = (std::min)(leave,last);
            if (enter > leave) return false;
        }
    }
    return distance(from,to) > .001f;
}
struct HeightWindow {
    bool valid{};
    float previous{},minimum{},maximum{},maxDelta{};
    void observe(float z) {
        if (!std::isfinite(z)) return;
        if (!valid) { previous = minimum = maximum = z; valid = true; }
        minimum = (std::min)(minimum,z); maximum = (std::max)(maximum,z);
        maxDelta = (std::max)(maxDelta,std::abs(z-previous)); previous = z;
    }
    void sampled() { minimum = maximum = previous; maxDelta = 0; }
};
constexpr bool actorLayer(unsigned layer) {
    return layer == 5 || layer == 8 || layer == 19 || layer == 20 ||
        layer == 25 || layer == 30 || layer == 32 || layer == 33 || layer == 35;
}
constexpr bool skipCharacter(unsigned layer,bool character,bool player) {
    return actorLayer(layer) && (player || (character && layer != 32));
}
constexpr bool countsAsKept(bool hit,bool listedContact,bool nonzero) {
    return hit && !listedContact && nonzero;
}
inline std::optional<Point> advance(Point start,Point goal,Point contact,float hull,unsigned steps) {
    if (!finite(start) || !finite(goal) || !finite(contact) || !std::isfinite(hull) || hull <= 0 || hull > 64 || steps >= maxSteps) return {};
    const float length = distance(start,goal);
    if (length <= .001f) return {};
    const Point direction{(goal.x-start.x)/length,(goal.y-start.y)/length,(goal.z-start.z)/length};
    const float projected = (contact.x-start.x)*direction.x+(contact.y-start.y)*direction.y+(contact.z-start.z)*direction.z;
    const float step = (std::max)(0.0f,projected)+(std::max)(30.0f,2*hull);
    if (!std::isfinite(step) || step >= length) return {};
    return Point{start.x+direction.x*step,start.y+direction.y*step,start.z+direction.z*step};
}
enum class GapResult { clear, obstruction, refuse };
// Cast fractions and start penetration distances are different units. Unknown
// or unlisted START overlaps cannot be converted into a forward safe endpoint.
struct GapDecision {
    bool complete{true}, originalSeen{}, unknown{}, overlap{};
    float nearest{2};
    unsigned index{};
    void contact(bool known,bool listed,bool start,float fraction,unsigned i) {
        if (!known || !std::isfinite(fraction) || (!start && (fraction < 0 || fraction > 1))) { unknown = true; return; }
        if (listed) return;
        if (start) { overlap = true; return; }
        if (fraction < nearest) { nearest = fraction; index = i; }
    }
    GapResult result() const {
        if (!complete || !originalSeen || unknown || overlap) return GapResult::refuse;
        return nearest <= 1 ? GapResult::obstruction : GapResult::clear;
    }
};
constexpr bool nativeIgnored(unsigned layer,bool missingOrCulled,bool player) {
    if (layer != 17 && missingOrCulled) return true;
    return player && actorLayer(layer);
}
constexpr std::uint16_t translucentFlags(std::uint16_t original) {
    // Blend SRC_ALPHA / INV_SRC_ALPHA; disable alpha test, retain other flags.
    return static_cast<std::uint16_t>((original & ~0x03ffu) | 1u | (6u << 1) | (7u << 5));
}
struct FadeState {
    double first{-1}, last{-1};
    float alpha{1};
    bool admitted{};
    void touch(double now) { if (first < 0 || now-last > .4) first = now; last = now; }
    void reset() { *this = {}; }
    // Already admitted before a ticking menu restored it; skip the enter dwell.
    void resume(double now) { first = now-.10; last = now; admitted = true; }
    bool step(double now,double dt,float target,bool valid,bool opacityOnly = false) {
        if (!valid || !std::isfinite(now) || !std::isfinite(dt) || dt < 0) { reset(); return false; }
        const bool recent = last >= 0 && now-last <= .4;
        if (recent && now-first >= .10) admitted = true;
        if (!recent) admitted = false;
        const float goal = recent && !opacityOnly ? target : 1.0f;
        const float increment = static_cast<float>((std::min)(dt,.05)) * 4.0f;
        alpha += std::clamp(goal-alpha,-increment,increment);
        return recent || alpha < 1;
    }
};
enum class Exit { disable, f8, menu, firstPerson, special, load, cell, unload, shutdown, conflict };
constexpr bool immediateRestore(Exit) { return true; }
// A menu leaves the camera where it was, so a faded beam is still valid on close.
constexpr bool carriesAdmission(Exit cause,bool faded) { return cause == Exit::menu && faded; }
// Special frames can trail a menu close; a stale carry only skips the enter dwell.
constexpr bool keepsCarried(Exit cause) { return cause == Exit::menu || cause == Exit::special; }
struct RestoreCheck {
    bool geometryShader{}, installedMaterial{}, installedProperty{}, alphaUnchanged{}, flagsUnchanged{};
    constexpr bool material() const { return geometryShader && installedMaterial && alphaUnchanged; }
    constexpr bool property() const { return geometryShader && installedProperty && flagsUnchanged; }
};
constexpr bool compressionAllowed(bool featureInstalled,bool nativeKept) { return !featureInstalled || nativeKept; }
} // namespace skycrpg::camera::thin
