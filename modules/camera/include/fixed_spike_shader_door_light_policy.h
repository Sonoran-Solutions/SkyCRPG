// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader_frame_policy.h. P5.5-02 wave 2 package
// "shader": the door void's lights (iDoorVoidLights=1, off by default; only
// with iDoorVoid=1, bFramePass=1 and iCutStyle=1). The door void blacks out
// the stub behind a chosen load door in the frame pass, but a light placed in
// that stub still lights the dungeon side in front of the door (Embershard
// Mine: the placed light 0008F94C, 652 behind the entrance door's plane,
// throws a blue patch on the floor in front of it). While a door is chosen,
// every placed point light that sits inside the volume its void covers is put
// out the way the engine puts out a light: the app-culled bit of its NiLight,
// which the per-geometry light list skips (ID 107784) and, for a dynamic
// light, the per-frame light update detaches (ID 106335 -> 108285); the
// light's geometries are marked for a light-list rebuild exactly as the
// engine's own camera cull marks them (ID 108278). The bit is cleared again,
// with the same marking, as soon as the door is no longer chosen or the rig
// leaves (src/fixed_spike_shader_door_light.h has the engine side).
//
// Which lights. A light is put out only when all of these hold, read under the
// ShadowSceneNode's light lock on the main thread:
//   - it is an entry of the scene's active point lights of exactly the BSLight
//     class (shadow lights are never touched: their channel is admitted
//     elsewhere; they are counted),
//   - its NiLight's world position is inside a chosen door's lane (behind the
//     plane by at least that frame's start, no deeper than fDoorVoidDepth,
//     within the lane's half width and heights: what the void covers),
//   - that position is within lightMatch of a placed LIGH reference the cell
//     census noted (initially enabled, not deleted), so a light the census did
//     not see (the player's torch or magic light, an actor's torch, the fill
//     light of another package, anything spawned later) is never matched,
//   - its app-culled bit is clear (one already culled is someone else's: left
//     alone and counted).
// No engine headers, no engine pointers: entries and lights are opaque
// pointers the policy only compares.
#include "fixed_spike_shader_door_policy.h"

namespace skycrpg::camera::fixed::shader {
// ---- the census: placed LIGH references of the cell ------------------------------------------------
// Report (survey of Skyrim.esm, 2026-11-12): 16 placed lights sit in the 388 vanilla lanes at fDoorVoidSide=320,
// none with a model (the NiLight is the reference's own position), one an omni shadow light (Bthardamz).
constexpr std::size_t maxCellLights = 256, maxDoorLights = 16, maxSeenLights = 64, maxTracedLights = 6;
constexpr float lightMatch = 8.0f; // a light's NiLight within this of a placed reference's position is that reference's
struct PlacedLight { std::uint32_t id{}; Vec at{}; };
struct LightCensus {
    std::array<PlacedLight,maxCellLights> lights{};
    std::size_t count{};
    unsigned refs{},off{},full{}; // LIGH references seen; initially disabled or deleted; past the table
};
inline void notePlacedLight(LightCensus& c,std::uint32_t id,bool off,Vec at) {
    ++c.refs;
    if (off || !finite(at)) { ++c.off; return; }
    if (c.count >= maxCellLights) { ++c.full; return; }
    c.lights[c.count++] = {id,at};
}
inline const PlacedLight* placedAt(const LightCensus& c,Vec at) {
    if (!finite(at)) return nullptr;
    for (std::size_t i = 0;i < c.count && i < maxCellLights;++i) if (span(c.lights[i].at,at) <= lightMatch) return &c.lights[i];
    return nullptr;
}

// ---- the lane: what the void covers ---------------------------------------------------------------
// The hard volume of doorFade (fixed_spike_shader_frame_policy.h) without its soft edges: behind the plane by at
// least `start`, no deeper than `depth`, within the half width and between the heights.
inline bool inLane(const DoorPlane& d,float start,float depth,Vec p) {
    if (!d.valid || !finite(p) || !std::isfinite(start) || !std::isfinite(depth)) return false;
    const float back = -doorSide(d,p);
    return back >= start && back <= depth && doorAcross(d,p) <= d.half && p.z >= d.low && p.z <= d.high;
}
// The chosen door (slot) whose lane this frame holds `p`, the nearest door first; -1 none.
inline int laneOf(const DoorSelection& s,float depth,Vec p) {
    for (std::size_t i = 0;i < s.count && i < maxDoorVoids;++i)
        if (inLane(s.chosen[i].door.plane,s.chosen[i].start,depth,p)) return static_cast<int>(i);
    return -1;
}

// ---- the book: what we put out, and the plan for one frame --------------------------------------
// One entry of the scene's active point lights as read under the light lock this frame (only the entries the
// book holds and those inside a chosen lane are read into this list).
struct LightSeen {
    const void* entry{}; // the BSLight (compared, never dereferenced here)
    const void* light{}; // its NiLight
    Vec at{};            // NiLight world position
    float radius{};
    bool culled{};       // the NiLight's app-culled bit as read
    bool plain{};        // the entry is of exactly the BSLight class
};
struct HeldLight {
    const void* entry{};
    const void* light{};
    std::uint32_t door{},ref{}; // the chosen door and the placed reference it matched when it was put out
    Vec at{};
    float radius{};
};
// Per frame and per window. dimmed/restored: our writes; lost: a held entry no longer in the active list (or holding
// another NiLight): dropped, nothing written; conflict: our bit was already clear at restore, or cleared by another
// writer while held (dropped, never written again in this stretch); foreign: in a lane and placed, but culled by
// someone else; unplaced: in a lane, no placed reference there; shadow: a shadow light in a lane (never touched);
// other: an active point entry of another class in a lane; full: no room in the book or the read list.
struct LightCounts {
    unsigned dimmed{},restored{},lost{},conflict{},foreign{},unplaced{},shadow{},other{},full{};
    void add(const LightCounts& o) {
        dimmed += o.dimmed; restored += o.restored; lost += o.lost; conflict += o.conflict; foreign += o.foreign;
        unplaced += o.unplaced; shadow += o.shadow; other += o.other; full += o.full;
    }
    bool any() const { return dimmed || restored || lost || conflict; }
};
struct LightBook {
    std::array<HeldLight,maxDoorLights> held{};
    std::size_t count{};
    // Entries we restored after a conflict while their door stayed chosen: never put out again until the book is
    // emptied (no fight with another writer of the bit). A recycled address is at worst left lit.
    std::array<const void*,maxDoorLights> left{};
    std::size_t leftCount{};
    bool holds(const void* entry) const {
        for (std::size_t i = 0;i < count;++i) if (held[i].entry == entry) return true;
        return false;
    }
    bool leaves(const void* entry) const {
        for (std::size_t i = 0;i < leftCount;++i) if (left[i] == entry) return true;
        return false;
    }
};
enum class LightAct : unsigned { dim, restore };
struct LightAction { LightAct act{}; std::size_t seen{}; };
struct LightPlan {
    std::array<LightAction,2*maxDoorLights> actions{};
    std::size_t count{};
    LightBook book{};
    LightCounts counts{};
};
inline const LightSeen* findSeen(const LightSeen* seen,std::size_t n,const void* entry,std::size_t& at) {
    for (std::size_t i = 0;i < n;++i) if (seen[i].entry == entry) { at = i; return &seen[i]; }
    return nullptr;
}
// One frame. `want`: a door may hold lights this frame (an eligible frame with the void drawn); false empties the
// book. Every held entry is restored (bit set: clear it), lost (not in the list, or another NiLight: nothing
// written) or kept; then, only when wanted, every unheld entry in a lane, placed, of the BSLight class and lit is
// put out while the book has room. The policy never asks for a write on an entry it did not see this frame.
inline LightPlan planLights(const LightBook& book,const LightSeen* seen,std::size_t n,bool want,const DoorSelection& s,float depth,
    const LightCensus& census) {
    LightPlan p;
    if (!seen) n = 0;
    n = (std::min)(n,maxSeenLights);
    const auto wanted = [&](const LightSeen& l,int& slot,const PlacedLight*& placed) {
        slot = want ? laneOf(s,depth,l.at) : -1;
        placed = slot >= 0 ? placedAt(census,l.at) : nullptr;
        return slot >= 0 && placed && l.plain;
    };
    if (want) { p.book.left = book.left; p.book.leftCount = book.leftCount; }
    for (std::size_t i = 0;i < book.count && i < maxDoorLights;++i) {
        const HeldLight& h = book.held[i];
        std::size_t at = 0;
        const LightSeen* l = findSeen(seen,n,h.entry,at);
        if (!l || l->light != h.light) { ++p.counts.lost; continue; }
        int slot = -1;
        const PlacedLight* placed = nullptr;
        const bool keep = wanted(*l,slot,placed);
        if (keep && l->culled) { p.book.held[p.book.count++] = h; continue; }
        if (!l->culled) {
            // Someone else cleared our bit: never written again while this door stays chosen.
            ++p.counts.conflict;
            if (keep && p.book.leftCount < maxDoorLights) p.book.left[p.book.leftCount++] = h.entry;
            continue;
        }
        p.actions[p.count++] = {LightAct::restore,at};
        ++p.counts.restored;
    }
    if (!want) return p;
    for (std::size_t i = 0;i < n;++i) {
        const LightSeen& l = seen[i];
        std::size_t first = 0;
        if (!l.entry || !l.light || (findSeen(seen,n,l.entry,first) && first != i) || book.holds(l.entry)) continue;
        int slot = -1;
        const PlacedLight* placed = nullptr;
        if (!wanted(l,slot,placed)) {
            if (slot >= 0 && !l.plain) ++p.counts.other;
            else if (slot >= 0 && !placed) ++p.counts.unplaced;
            continue;
        }
        if (l.culled) { ++p.counts.foreign; continue; }
        if (p.book.leaves(l.entry)) continue;
        if (p.book.count >= maxDoorLights || p.count >= p.actions.size()) { ++p.counts.full; continue; }
        p.book.held[p.book.count++] = {l.entry,l.light,s.chosen[static_cast<std::size_t>(slot)].door.id,placed->id,l.at,l.radius};
        p.actions[p.count++] = {LightAct::dim,i};
        ++p.counts.dimmed;
    }
    return p;
}

// ---- when the lights may stay out ---------------------------------------------------------------
// The fill light's menu policy (combined build, fixed_spike_light_policy.h fillDuty / SilenceHold / settle), for
// lights that are the engine's: keep = this frame serves the chosen doors; hold = left exactly as they are (no
// read, no write); off = every held light restored now. An eligible frame keeps. A menu (State::menu, the console)
// holds, and so does a hook-silent frame inside the grace or one a pausing menu explains (LightSilence); the
// Loading Menu never holds (a load door, fast travel or coc detaches the cell under it). Everything else is off:
// first person, Camera off, F8, rig disabled, death, load, cell, the special states (the void is not drawn there),
// a hook lost past the grace. A hold never survives a cell change or a game load.
enum class LightDuty : unsigned { off, keep, hold };
constexpr const char* lightDutyName(LightDuty d) { return d == LightDuty::keep ? "keep" : d == LightDuty::hold ? "hold" : "off"; }
constexpr LightDuty lightDuty(bool eligible,bool exiting,Exit cause,bool silenceHeld,bool loadingMenu) {
    if (eligible) return LightDuty::keep;
    if (loadingMenu) return LightDuty::off;
    if (!exiting) return LightDuty::hold;
    if (cause == Exit::menu) return LightDuty::hold;
    if (cause == Exit::hookSilent && silenceHeld) return LightDuty::hold;
    return LightDuty::off;
}
constexpr LightDuty settleLightDuty(LightDuty d,bool cellChanged,bool loaded) {
    return d == LightDuty::hold && (cellChanged || loaded) ? LightDuty::off : d;
}
// The fill light's SilenceHold, verbatim (a pausing menu open, or the Tween camera state still easing back within
// lightTweenSeconds of the last paused frame, then the wave 1 grace; never without a pausing menu first).
constexpr double lightTweenSeconds = .5;
struct LightSilence {
    double pausedAt{-1.0};
    unsigned sinceHeld{silentGraceFrames};
    void reset() { pausedAt = -1.0; sinceHeld = silentGraceFrames; }
    bool step(bool paused,bool tween,double now) {
        if (paused) {
            if (std::isfinite(now)) pausedAt = now;
            sinceHeld = 0;
            return true;
        }
        const bool recent = pausedAt >= 0.0 && std::isfinite(now) && now >= pausedAt && now-pausedAt <= lightTweenSeconds;
        if (tween && recent) { sinceHeld = 0; return true; }
        if (sinceHeld < silentGraceFrames) ++sinceHeld;
        return sinceHeld < silentGraceFrames;
    }
};

// ---- trace ----------------------------------------------------------------------------------------
// FixedDoorLight, through the frame row's emitter and budget, only when the held set changed or something was
// restored, lost or in conflict since the last row. Up to maxTracedLights held lights are listed as
// door:reference@x,y,z/radius, whole units (more=N for the rest).
struct LightTrace {
    double seconds{};
    unsigned long long frame{};
    std::uint32_t cell{};
    const char* duty{};
    const char* cause{};
    std::size_t doors{};     // doors chosen in the last served frame
    const LightBook* book{};
    LightCounts counts{};    // since the last row
    unsigned long long dims{},restores{},lost{}; // session totals
};
inline bool formatDoorLight(char* out,std::size_t size,const LightTrace& t) {
    if (!t.book) return false;
    const LightBook& b = *t.book;
    const LightCounts& c = t.counts;
    int written = std::snprintf(out,size,"FixedDoorLight t=%.3f frame=%llu cell=%08X duty=%s cause=%s doors=%zu held=%zu dimmed=%u restored=%u lost=%u "
        "conflict=%u foreign=%u unplaced=%u shadow=%u other=%u full=%u total=%llu,%llu,%llu lights=",t.seconds,t.frame,t.cell,t.duty ? t.duty : "none",
        t.cause ? t.cause : "none",t.doors,b.count,c.dimmed,c.restored,c.lost,c.conflict,c.foreign,c.unplaced,c.shadow,c.other,c.full,t.dims,t.restores,t.lost);
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written);
    if (!b.count) {
        written = std::snprintf(out+used,size-used,"none");
        return written > 0 && used+static_cast<std::size_t>(written) < size;
    }
    const std::size_t listed = (std::min)((std::min)(b.count,maxDoorLights),maxTracedLights);
    for (std::size_t i = 0;i < listed;++i) {
        const HeldLight& h = b.held[i];
        written = std::snprintf(out+used,size-used,"%s%08X:%08X@%.0f,%.0f,%.0f/%.0f",i ? ";" : "",h.door,h.ref,static_cast<double>(h.at.x),
            static_cast<double>(h.at.y),static_cast<double>(h.at.z),static_cast<double>(h.radius));
        if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
        used += static_cast<std::size_t>(written);
    }
    if (b.count > listed) {
        written = std::snprintf(out+used,size-used," more=%zu",b.count-listed);
        if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
    }
    return true;
}
// Same held entries in the same order (the trace's change test).
inline bool sameHeld(const LightBook& a,const LightBook& b) {
    if (a.count != b.count) return false;
    for (std::size_t i = 0;i < a.count && i < maxDoorLights;++i) if (a.held[i].entry != b.held[i].entry || a.held[i].door != b.held[i].door) return false;
    return true;
}
// The plugin log, once per cell after the census: the placed lights that sit in a door's lane as the census places
// it (the plane's own start; the frame's start can only lie deeper), by door; at most maxDoorLights listed.
inline bool formatDoorLightCensus(char* out,std::size_t size,std::uint32_t cell,const DoorCensus& doors,const LightCensus& lights,float depth) {
    std::size_t lane = 0;
    for (std::size_t i = 0;i < lights.count && i < maxCellLights;++i)
        for (std::size_t d = 0;d < doors.count && d < maxCellDoors;++d)
            if (inLane(doors.doors[d].plane,doors.doors[d].plane.start,depth,lights.lights[i].at)) { ++lane; break; }
    int written = std::snprintf(out,size,"FixedCameraSpike shader door lights cell=%08X placed=%zu refs=%u off=%u full=%u doors=%zu lane=%zu list=",cell,
        lights.count,lights.refs,lights.off,lights.full,doors.count,lane);
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written),listed = 0;
    for (std::size_t i = 0;i < lights.count && i < maxCellLights && listed < maxDoorLights;++i)
        for (std::size_t d = 0;d < doors.count && d < maxCellDoors;++d) {
            const DoorPlane& p = doors.doors[d].plane;
            if (!inLane(p,p.start,depth,lights.lights[i].at)) continue;
            written = std::snprintf(out+used,size-used,"%s%08X@%08X/%.1f",listed ? "," : "",lights.lights[i].id,doors.doors[d].id,
                static_cast<double>(-doorSide(p,lights.lights[i].at)));
            if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
            used += static_cast<std::size_t>(written);
            ++listed;
            break;
        }
    if (!listed) {
        written = std::snprintf(out+used,size-used,"none");
        return written > 0 && used+static_cast<std::size_t>(written) < size;
    }
    return true;
}
} // namespace skycrpg::camera::fixed::shader
