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
    // 1 (default): the fill light adds nothing to any actor's light level (the
    // stealth light level, IDs 39946 -> 106362 / 107776 -> 108292); 0: it counts
    // like any other light. With 1, the fill light is refused unless the two
    // light-level summers can be wrapped (FCS-58 / FCS-59).
    bool stealthNeutral{true};
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
constexpr unsigned lightLevelTraceLines = 1500; // FixedLightLevel, 1 Hz, iLightTrace=1 and iLightFillMode=1

// ---- fill light --------------------------------------------------------------
// Interior cells only: outdoors the sun lights the scene and a light that follows
// the player would read as a lantern.
constexpr bool fillWanted(unsigned mode,bool eligible,bool interior,bool ready) {
    return mode == 1 && eligible && interior && ready;
}
// What the fill light does in a frame, by the reason the rig is (in)eligible.
// keep: lit and following the player. hold: left exactly as it was (no hide,
// no move, no removal timer). off: hidden at once, handed back after the delay.
// Starting points for the owner, one row each in the evidence document:
// eligible -> keep; a hook-silent frame inside the grace -> hold; menu or
// console (State::menu) -> hold; precision aim, targeting, obstruction and the
// special states (mount, swim, beast, bleedout, furniture, killmove, scripted)
// -> keep; first person, Camera off, F8, load, death, invalid world and a
// hook-silent stretch past the grace -> off.
enum class FillDuty { off, keep, hold };
constexpr FillDuty fillDuty(bool eligible,bool exiting,Exit cause,bool stateSeen,State state) {
    if (eligible) return FillDuty::keep;
    if (!exiting) return FillDuty::hold;
    if (cause == Exit::menu) return FillDuty::hold;
    if (cause == Exit::special && stateSeen && (state == State::precision || state == State::targeting ||
        state == State::obstruction || state == State::special)) return FillDuty::keep;
    return FillDuty::off;
}
constexpr const char* dutyName(FillDuty d) { return d == FillDuty::keep ? "keep" : d == FillDuty::hold ? "hold" : "off"; }
// A hold never survives a cell change: the first serviced frame after a load
// door can be a menu frame (wave 1, p55-e-door-C frames 2536 -> 2537), and the
// light must not stay where it was in the previous cell.
constexpr FillDuty settledDuty(FillDuty duty,bool cellChanged) { return duty == FillDuty::hold && cellChanged ? FillDuty::off : duty; }
// Light level without the fill light: the engine's sum minus the fill light's
// own ID 108292 terms, as the wrapped summer saw them; never below zero, and
// untouched when the fill light contributed nothing or a value is not finite.
inline float withoutFill(float total,float fill) {
    if (!std::isfinite(total) || !std::isfinite(fill) || !(fill > 0.0f)) return total;
    return (std::max)(0.0f,total-fill);
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
// The light is app-culled on the first unwanted frame, but handed back only
// after it has been unwanted this long, so a menu tap or a short special state
// does not free and rebuild the engine's BSLight (and its 0x30230-byte culling
// process, allocated by ID 106401 at 1513344) for nothing.
constexpr double removeDelaySeconds = .5;
struct FillTrack {
    bool requested{};    // AddLight called and no removal asked for since
    bool gaveUp{};       // drop limit reached: no more adds in this cell
    bool stuck{};        // removal limit reached: the light stays hidden
    unsigned drops{},removeRequests{};
    double unwantedSince{-1.0};
    void newCell() { drops = 0; gaveUp = false; }
};
// One decision per presence check. Wanted: add only when the engine holds no
// copy at all; a queued removal finishes before a new add. Unwanted: ask for
// removal only while the light is active and has been unwanted for
// removeDelaySeconds; RemoveLight (ID 106331) searches the active arrays only,
// so a queued add is waited out.
inline FillAction fillStep(bool wanted,Presence p,FillTrack& t,double now) {
    if (wanted) {
        t.removeRequests = 0;
        t.unwantedSince = -1.0;
        if (p != Presence::absent || t.gaveUp) return FillAction::none;
        if (t.requested && ++t.drops > maxDropsPerCell) { t.gaveUp = true; return FillAction::dropLimit; }
        t.requested = true;
        return FillAction::add;
    }
    t.requested = false;
    if (t.unwantedSince < 0.0 || !std::isfinite(now) || now < t.unwantedSince) t.unwantedSince = now;
    if (p == Presence::active && std::isfinite(now) && now-t.unwantedSince < removeDelaySeconds) return FillAction::none;
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
// Can a light reach geometry at the point? Its sphere must contain the point
// and, for a portal-strict light in a cell with rooms, one of the rooms its own
// portal walk attached it to (BSLight+D8, appended by ID 108279 from ID
// 106400/106401) must contain the point too: ID 108302 lights a node under a
// room only if the room is in that light's walk (15775A0-15775DD). A cell
// without rooms takes the no-graph path (ID 106400 at 15127B0), where the
// sphere is all. Spot cones and occlusion are not modelled: this is an upper
// bound on "lights the player", never a proof that it does.
constexpr bool lightsPoint(bool contains,bool portalStrict,bool cellHasRooms,bool sharesRoom) {
    return contains && (!portalStrict || !cellHasRooms || sharesRoom);
}
template<class T>
bool anyShared(const T* a,std::size_t aCount,const T* b,std::size_t bCount) {
    if (!a || !b) return false;
    for (std::size_t i = 0;i < aCount;++i)
        for (std::size_t j = 0;j < bCount;++j)
            if (a[i] && a[i] == b[j]) return true;
    return false;
}
struct LightView {
    Vec position{};
    float radius{},dimmer{};
    std::uint32_t cull{};
    bool hidden{},portalStrict{},sharesRoom{};
    constexpr bool live() const { return cull != culled && !hidden && dimmer > 0.0f; }
};
// Census counters. *Reach = the bare sphere test (kept for comparison with
// wave 2 first-pass lines); *Lit = lightsPoint(), the room-aware test; only
// the room-aware counts say anything about the player. *Nearest = smallest
// (distance - radius) among lights that may light the player's room.
struct Census {
    bool cellHasRooms{};
    unsigned playerRooms{};
    unsigned point{},pointCulled{},pointDimmed{},pointReach{},pointLit{},pointLitLive{};
    unsigned shadow{},shadowChannel{},shadowReach{},shadowLit{},shadowLitChannel{};
    float pointNearest{std::numeric_limits<float>::infinity()},shadowNearest{std::numeric_limits<float>::infinity()};
    bool lights(const LightView& l,float r) const { return lightsPoint(r <= 0.0f,l.portalStrict,cellHasRooms,l.sharesRoom); }
    bool roomOk(const LightView& l) const { return lightsPoint(true,l.portalStrict,cellHasRooms,l.sharesRoom); }
    void addPoint(const LightView& l,Vec at) {
        ++point;
        if (l.cull == culled) ++pointCulled;
        else if (l.dimmer < 1.0f) ++pointDimmed;
        const float r = reach(l.position,l.radius,at);
        if (r <= 0.0f) ++pointReach;
        if (roomOk(l) && r < pointNearest) pointNearest = r;
        if (!lights(l,r)) return;
        ++pointLit;
        if (l.live()) ++pointLitLive;
    }
    // Returns the bare reach, for the per-light list.
    float addShadow(const LightView& l,std::uint32_t channel,Vec at) {
        ++shadow;
        const bool lit = channel != noChannel && !l.hidden;
        if (lit) ++shadowChannel;
        const float r = reach(l.position,l.radius,at);
        if (r <= 0.0f) ++shadowReach;
        if (roomOk(l) && r < shadowNearest) shadowNearest = r;
        if (lights(l,r)) {
            ++shadowLit;
            if (lit) ++shadowLitChannel;
        }
        return r;
    }
    // Could light the player (room-aware), but held no shadow-mask channel last
    // frame: such a light adds nothing to the picture (ID 107784 draws shadow lights by
    // channel only). Only this count can argue for a shadow-admission change.
    constexpr unsigned starved() const { return shadowLit-shadowLitChannel; }
};
// Every active shadow light, nearest first, reported by reference, so a route
// can follow one light (for example 000395F1 in the BFB corridor room) by ID.
struct ShadowEntry {
    std::uint32_t ref{};             // placed reference whose 3D holds the NiLight, 0 unknown
    float reach{},radius{};
    std::uint32_t channel{noChannel},cull{};
    bool portalStrict{},sharesRoom{},lit{},hidden{};
};
constexpr unsigned maxShadowEntries = 8; // the widest list still fits one 768-byte trace row
struct ShadowList {
    std::array<ShadowEntry,maxShadowEntries> items{};
    unsigned count{},total{};
    // Keeps the nearest entries; NaN reach sorts last.
    void offer(const ShadowEntry& e) {
        ++total;
        const auto before = [](const ShadowEntry& a,const ShadowEntry& b) {
            return a.reach < b.reach || (!std::isnan(a.reach) && std::isnan(b.reach));
        };
        unsigned at = count;
        while (at > 0 && before(e,items[at-1])) --at;
        if (at >= maxShadowEntries) return;
        const unsigned last = count < maxShadowEntries ? count : maxShadowEntries-1;
        for (unsigned i = last;i > at;--i) items[i] = items[i-1];
        items[at] = e;
        if (count < maxShadowEntries) ++count;
    }
};
inline bool formatShadows(char* out,std::size_t size,double seconds,unsigned long long frame,const ShadowList& list) {
    int written = std::snprintf(out,size,"FixedLightShadow t=%.3f frame=%llu total=%u listed=%u",seconds,frame,list.total,list.count);
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written);
    for (unsigned i = 0;i < list.count;++i) {
        const auto& e = list.items[i];
        written = std::snprintf(out+used,size-used," %08X:reach=%.0f,r=%.0f,ch=%u,cull=%u,strict=%u,room=%u,lit=%u,hidden=%u",
            e.ref,static_cast<double>(e.reach),static_cast<double>(e.radius),e.channel,e.cull,static_cast<unsigned>(e.portalStrict),
            static_cast<unsigned>(e.sharesRoom),static_cast<unsigned>(e.lit),static_cast<unsigned>(e.hidden));
        if (written <= 0 || static_cast<std::size_t>(written) >= size-used) return false;
        used += static_cast<std::size_t>(written);
    }
    return true;
}
struct FillReading {
    bool created{},hidden{},found{};
    Presence presence{Presence::absent};
    std::uint32_t cull{culled};
    float dimmer{};
    Vec at{};
    unsigned adds{},removes{},drops{};
};
// The player's light level as ID 106362 summed it (with the fill light) and as
// returned (without it when neutral); units are ID 106362's sum, which ID 39946
// adds to the sun and ambient terms and multiplies by 100 (726522-72653E).
struct LevelReading {
    bool hooked{},neutral{};
    float with{std::numeric_limits<float>::quiet_NaN()},without{std::numeric_limits<float>::quiet_NaN()};
    unsigned long long playerCalls{},npcCalls{},npcAdjusted{};
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
        "rooms=%u playerRooms=%u point=%u pointCulled=%u pointDimmed=%u pointReach=%u pointLit=%u pointLitLive=%u pointNearest=%.1f "
        "shadow=%u shadowChannel=%u shadowReach=%u shadowLit=%u shadowLitChannel=%u shadowStarved=%u shadowNearest=%.1f "
        "fillWanted=%u fill=%s fillCreated=%u fillHidden=%u fillCull=%u fillDimmer=%.3f fillAt=%.1f,%.1f,%.1f "
        "adds=%u removes=%u drops=%u sample=state",
        s.seconds,s.frame,s.cell,static_cast<unsigned>(s.interior),static_cast<unsigned>(s.eligible),
        static_cast<double>(s.eye.x),static_cast<double>(s.eye.y),static_cast<double>(s.eye.z),
        static_cast<double>(s.camera.x),static_cast<double>(s.camera.y),static_cast<double>(s.camera.z),
        static_cast<unsigned>(c.cellHasRooms),c.playerRooms,
        c.point,c.pointCulled,c.pointDimmed,c.pointReach,c.pointLit,c.pointLitLive,static_cast<double>(c.pointNearest),
        c.shadow,c.shadowChannel,c.shadowReach,c.shadowLit,c.shadowLitChannel,c.starved(),static_cast<double>(c.shadowNearest),
        static_cast<unsigned>(s.wanted),presenceName(f.presence),static_cast<unsigned>(f.created),static_cast<unsigned>(f.hidden),
        f.found ? f.cull : 999u,static_cast<double>(f.found ? f.dimmer : std::numeric_limits<float>::quiet_NaN()),
        static_cast<double>(f.at.x),static_cast<double>(f.at.y),static_cast<double>(f.at.z),f.adds,f.removes,f.drops);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
// The FixedLight field list is the a8639f3 one (the route checker parses an
// exact list); the light-level reading rides on its own line kind, 1 Hz.
inline bool formatLightLevel(char* out,std::size_t size,double seconds,unsigned long long frame,const LevelReading& l) {
    const int written = std::snprintf(out,size,
        "FixedLightLevel t=%.3f frame=%llu levelHook=%u neutral=%u level=%.4f levelNoFill=%.4f levelCalls=%llu npcCalls=%llu "
        "npcAdjusted=%llu sample=second",
        seconds,frame,static_cast<unsigned>(l.hooked),static_cast<unsigned>(l.neutral),static_cast<double>(l.with),
        static_cast<double>(l.without),l.playerCalls,l.npcCalls,l.npcAdjusted);
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
