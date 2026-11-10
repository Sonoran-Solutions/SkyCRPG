// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "light" (DYL-217). Disposable spike policy: no engine
// headers, no engine pointers. The native adapter is src/fixed_spike_light.h.
// With iLightFillMode=0 and iLightTrace=0 nothing here is reached.
#include "fixed_spike_policy.h"

namespace skycrpg::camera::fixed::light {
// ---- [FixedCameraSpike] light keys -----------------------------------------
struct Config {
    // 0 off; 1 = a fill light that follows the player in interior cells while
    // the rig is engaged. A PRESENTATION AID: it adds light the cell does not
    // have, so it changes the look instead of restoring it.
    unsigned fillMode{};
    float fillHeight{192.0f};  // above the feet
    float fillRadius{640.0f};  // NiLight radius
    float fillFade{.6f};       // NiLight fade (dimmer)
    float fillRed{1.0f},fillGreen{.86f},fillBlue{.70f};
    bool trace{};              // FixedLight census lines
};
// ID 106342 (150E444-150E453) switches a light off when its NiLight fade is
// below 0.05; the bound keeps the fill light well clear of that.
constexpr float engineFadeFloor = .05f;
inline Config validate(Config c,bool* adjusted = nullptr) {
    const Config in = c;
    if (c.fillMode > 1) c.fillMode = 0;
    c.fillHeight = bounded(c.fillHeight,192.0f,0.0f,1024.0f);
    c.fillRadius = bounded(c.fillRadius,640.0f,64.0f,4096.0f);
    c.fillFade = bounded(c.fillFade,.6f,.1f,4.0f);
    c.fillRed = bounded(c.fillRed,1.0f,0.0f,1.0f);
    c.fillGreen = bounded(c.fillGreen,.86f,0.0f,1.0f);
    c.fillBlue = bounded(c.fillBlue,.70f,0.0f,1.0f);
    if (adjusted) *adjusted = c.fillMode != in.fillMode || c.fillHeight != in.fillHeight || c.fillRadius != in.fillRadius ||
        c.fillFade != in.fillFade || c.fillRed != in.fillRed || c.fillGreen != in.fillGreen || c.fillBlue != in.fillBlue;
    return c;
}
// Nothing is allocated, read or installed for the light package unless this holds.
constexpr bool configured(const Config& c) { return c.fillMode != 0 || c.trace; }
constexpr unsigned lightTraceLines = 6000;

// ---- fill light --------------------------------------------------------------
// Interior cells only: outdoors the sun lights the scene and a light that follows
// the player would read as a lantern.
constexpr bool fillWanted(unsigned mode,bool eligible,bool interior,bool ready) {
    return mode == 1 && eligible && interior && ready;
}
inline Vec fillPoint(Vec feet,float height) { return {feet.x,feet.y,feet.z+height}; }
// Where the engine holds our light, read under the ShadowSceneNode light lock
// (ID 106335 takes the same lock, +1A8, before it moves queued lights). A
// pending removal outranks a pending add outranks the active list.
enum class Presence { absent, queuedAdd, active, queuedRemove };
constexpr Presence presenceOf(bool active,bool queuedAdd,bool queuedRemove) {
    return queuedRemove ? Presence::queuedRemove : queuedAdd ? Presence::queuedAdd : active ? Presence::active : Presence::absent;
}
constexpr const char* presenceName(Presence p) {
    return p == Presence::active ? "active" : p == Presence::queuedAdd ? "queued-add" : p == Presence::queuedRemove ? "queued-remove" : "absent";
}
enum class FillAction { none, add, remove, dropLimit, removeLimit };
constexpr unsigned maxDropsPerCell = 3;   // the engine removed our light while wanted
constexpr unsigned maxRemoveRequests = 8; // RemoveLight is a no-op while +300 is false
struct FillTrack {
    bool requested{};    // AddLight called and no removal asked for since
    bool gaveUp{};       // drop limit reached: no more adds in this cell
    bool stuck{};        // removal limit reached: the light stays hidden
    unsigned drops{},removeRequests{};
    void newCell() { drops = 0; gaveUp = false; }
};
// One decision per presence check. Wanted: add only when the engine holds no
// copy at all; a queued removal finishes before a new add. Unwanted: ask for
// removal only while the light is active; RemoveLight (ID 106331) searches the
// active arrays only, so a queued add is waited out.
inline FillAction fillStep(bool wanted,Presence p,FillTrack& t) {
    if (wanted) {
        t.removeRequests = 0;
        if (p != Presence::absent || t.gaveUp) return FillAction::none;
        if (t.requested && ++t.drops > maxDropsPerCell) { t.gaveUp = true; return FillAction::dropLimit; }
        t.requested = true;
        return FillAction::add;
    }
    t.requested = false;
    if (p != Presence::active) {
        if (p == Presence::absent) { t.removeRequests = 0; t.stuck = false; }
        return FillAction::none;
    }
    if (t.stuck) return FillAction::none;
    if (++t.removeRequests > maxRemoveRequests) { t.stuck = true; return FillAction::removeLimit; }
    return FillAction::remove;
}

// ---- census ------------------------------------------------------------------
// BSLight +5C after the per-frame camera cull (IDs 108278, 108618, 108625):
// 0 visible, 0xFF culled. BSShadowLight +520 after the shadow pass (ID 107132
// clears every channel to 0xFF, ID 107137 assigns at most four): channel or 0xFF.
constexpr std::uint32_t culled = 0xff, noChannel = 0xff;
// Distance from the light's sphere to the point; <= 0 means the point is inside.
inline float reach(Vec light,float radius,Vec point) {
    const float d = span(light,point)-radius;
    return std::isfinite(d) ? d : std::numeric_limits<float>::infinity();
}
struct LightView {
    Vec position{};
    float radius{},dimmer{};
    std::uint32_t cull{};
    bool hidden{};
    constexpr bool live() const { return cull != culled && !hidden && dimmer > 0.0f; }
};
struct Census {
    unsigned point{},pointCulled{},pointDimmed{},pointReach{},pointReachLive{};
    unsigned shadow{},shadowChannel{},shadowReach{},shadowReachChannel{};
    float pointNearest{std::numeric_limits<float>::infinity()},shadowNearest{std::numeric_limits<float>::infinity()};
    void addPoint(const LightView& l,Vec at) {
        ++point;
        if (l.cull == culled) ++pointCulled;
        else if (l.dimmer < 1.0f) ++pointDimmed;
        const float r = reach(l.position,l.radius,at);
        if (r < pointNearest) pointNearest = r;
        if (r <= 0.0f) { ++pointReach; if (l.live()) ++pointReachLive; }
    }
    void addShadow(const LightView& l,std::uint32_t channel,Vec at) {
        ++shadow;
        const bool lit = channel != noChannel && !l.hidden;
        if (lit) ++shadowChannel;
        const float r = reach(l.position,l.radius,at);
        if (r < shadowNearest) shadowNearest = r;
        if (r <= 0.0f) { ++shadowReach; if (lit) ++shadowReachChannel; }
    }
    // In reach of the point, but without a shadow-mask channel last frame: such
    // a light contributes nothing (ID 107784 draws shadow lights by channel only).
    constexpr unsigned starved() const { return shadowReach-shadowReachChannel; }
};
struct FillReading {
    bool created{},hidden{},found{};
    Presence presence{Presence::absent};
    std::uint32_t cull{culled};
    float dimmer{};
    Vec at{};
    unsigned adds{},removes{},drops{};
};
struct LightSample {
    double seconds{};
    unsigned long long frame{};
    unsigned cell{};
    bool interior{},eligible{},wanted{};
    Vec eye{},camera{};
    Census census{};
    FillReading fill{};
};
inline bool formatLight(char* out,std::size_t size,const LightSample& s) {
    const auto& c = s.census;
    const auto& f = s.fill;
    const int written = std::snprintf(out,size,
        "FixedLight t=%.3f frame=%llu cell=%08X interior=%u eligible=%u eye=%.1f,%.1f,%.1f cam=%.1f,%.1f,%.1f "
        "point=%u pointCulled=%u pointDimmed=%u pointReach=%u pointReachLive=%u pointNearest=%.1f "
        "shadow=%u shadowChannel=%u shadowReach=%u shadowReachChannel=%u shadowStarved=%u shadowNearest=%.1f "
        "fillWanted=%u fill=%s fillCreated=%u fillHidden=%u fillCull=%u fillDimmer=%.3f fillAt=%.1f,%.1f,%.1f "
        "adds=%u removes=%u drops=%u sample=state",
        s.seconds,s.frame,s.cell,static_cast<unsigned>(s.interior),static_cast<unsigned>(s.eligible),
        static_cast<double>(s.eye.x),static_cast<double>(s.eye.y),static_cast<double>(s.eye.z),
        static_cast<double>(s.camera.x),static_cast<double>(s.camera.y),static_cast<double>(s.camera.z),
        c.point,c.pointCulled,c.pointDimmed,c.pointReach,c.pointReachLive,static_cast<double>(c.pointNearest),
        c.shadow,c.shadowChannel,c.shadowReach,c.shadowReachChannel,c.starved(),static_cast<double>(c.shadowNearest),
        static_cast<unsigned>(s.wanted),presenceName(f.presence),static_cast<unsigned>(f.created),static_cast<unsigned>(f.hidden),
        f.found ? f.cull : 999u,static_cast<double>(f.found ? f.dimmer : std::numeric_limits<float>::quiet_NaN()),
        static_cast<double>(f.at.x),static_cast<double>(f.at.y),static_cast<double>(f.at.z),f.adds,f.removes,f.drops);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
// Fill transitions are traced as events, eligible or not, so a lifecycle exit
// shows when the light was hidden and when the engine let it go.
inline bool formatFillEvent(char* out,std::size_t size,double seconds,unsigned long long frame,const char* event,
    const char* cause,Presence presence,unsigned adds,unsigned removes,unsigned drops) {
    const int written = std::snprintf(out,size,"FixedLight t=%.3f frame=%llu event=%s cause=%s presence=%s adds=%u removes=%u drops=%u",
        seconds,frame,event,cause,presenceName(presence),adds,removes,drops);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::light
