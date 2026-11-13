// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader_frame_policy.h. P5.5-02 wave 2 package
// "shader": the door void's glow cards (iDoorVoidGlow, off by default; only
// with iDoorVoid=1, bFramePass=1, iCutStyle=1 and bShaderCutEffect=1). The
// door void blacks out what lies behind a chosen load door's plane, by depth.
// The level designers fake daylight at a cave or mine mouth with additive
// Effect-shader cards (FXGlowFillRound*, 516-unit glow spheres, and light-shaft
// beams) placed in the stub behind the door. Such a card writes no depth, so
// where it is drawn over the floor in front of the plane the frame pass leaves
// it: from the two headings with the camera behind the plane a card in the stub
// lies between the camera and the dungeon floor, and the floor in front of the
// door gets a flat blue haze (Embershard Mine: 0008F94E 117 and 0008F94D 507
// behind the plane of 000B6CCD). Putting out the placed light behind the door
// (iDoorVoidLights) leaves that haze; the cards themselves must not be drawn.
//
// Which cards. A census reference (Static or MovableStatic, initially enabled,
// not deleted) whose model is under effects\ and whose file name starts with
// one of glowPrefixes. Source (Skyrim.esm without Update.esm and the vanilla
// mesh archives, survey 2026-11-12, session-p55/logs/scratch-doorglow): 116
// models match (FXGlowFillRound*, FXGlowFlatRnd*, FXAmbBeam*,
// FX_RainAndMistBeam*); every one of their 298 shapes is BSEffectShaderProperty
// without the Cast_Shadows flag (no Lighting, Utility or shadow-map pass draws
// them); 2,720 are placed in interiors, at most 72 in one cell (Sovngarde01).
// Mist, fog, smoke and cobweb cards are left out: they are the dungeon's own
// atmosphere, not fake light.
//
// Which of them for a chosen door (glowPlace). The card's reference position is
// inside the door's lane as the void draws it (across within the lane's half
// width, fDoorVoidSide included; its height within the lane's heights widened
// by the card's reach) and
//   - iDoorVoidGlow=1: on or behind the door's plane, no deeper than
//     fDoorVoidDepth: the stub's own cards. Vanilla: 18 cards at 16 of the 388
//     load doors the void can draw (16 glows, most at cave and mine mouths, and
//     2 beams),
//     both Embershard cards among them; none initially disabled;
//   - iDoorVoidGlow=2: also in front of the plane by at most glowFrontReach
//     (an A/B setting): 44 more cards at 30 more doors in vanilla, mostly the
//     glow of lamps and braziers beside doors inside Nordic and Dwemer ruins
//     (Nchuand-Zel, Bthardamz, Kilkreath).
// The chosen doors are the frame pass's own (selectDoors over the same census,
// feet and radius), so a card is hidden exactly while its door's void is
// drawn, and drawn again as soon as that door is not chosen. Cards elsewhere in
// the dungeon are never touched.
//
// The adapter (src/fixed_spike_shader_door_glow.h) draws an Effect pass of the
// published (main) view whose owning reference is in this frame's set with the
// third pass's wholeConstant (nothing rasterised); no engine object or state is
// written. No engine headers, no engine pointers here.
#include "fixed_spike_shader_door_policy.h"

namespace skycrpg::camera::fixed::shader {
// ---- which models ------------------------------------------------------------------------------------
constexpr std::array<const char*,4> glowPrefixes{"fxglowfill","fxglowflat","fxambbeam","fx_rainandmistbeam"};
constexpr std::size_t maxCellGlows = 128, maxHiddenGlows = 16, maxTracedGlows = 6, glowNameSize = 40;
constexpr unsigned glowOff = 0, glowBehind = 1, glowFront = 2; // iDoorVoidGlow
constexpr float glowFrontReach = 512.0f; // iDoorVoidGlow=2: in front of the plane by at most this
// The file name of a model path (after the last separator of either kind).
inline const char* modelFile(const char* path) {
    if (!path) return nullptr;
    const char* name = path;
    for (const char* at = path;*at;++at) if (*at == '\\' || *at == '/') name = at+1;
    return name;
}
inline bool glowModel(const char* path) {
    if (modelFamily(path) != Family::effects) return false;
    const char* name = modelFile(path);
    for (const char* prefix : glowPrefixes) {
        std::size_t i = 0;
        while (prefix[i] && name[i] && lowered(name[i]) == prefix[i]) ++i;
        if (!prefix[i]) return true;
    }
    return false;
}
// The card's reach about its reference position: the largest OBND coordinate times the scale (the glow spheres'
// OBND is +-258 on every axis). 0 when unreadable.
inline float glowReach(Vec low,Vec high,float scale) {
    if (!finite(low) || !finite(high) || !std::isfinite(scale) || scale <= 0.0f) return 0.0f;
    const float largest = (std::max)({std::abs(low.x),std::abs(low.y),std::abs(low.z),std::abs(high.x),std::abs(high.y),std::abs(high.z)});
    const float reach = largest*scale;
    return std::isfinite(reach) ? reach : 0.0f;
}

// ---- the census: glow cards of the cell --------------------------------------------------------------
struct GlowCard {
    std::uint32_t id{};
    Vec at{};
    float reach{};
    std::array<char,glowNameSize> name{}; // the model's file name, lower case, cut to fit (the trace's)
};
struct GlowCensus {
    std::array<GlowCard,maxCellGlows> cards{};
    std::size_t count{};
    unsigned refs{},off{},full{}; // glow-model references seen; initially disabled, deleted or unplaced; past the table
};
enum class GlowNote : unsigned { added, notModel, off, full };
// One Static or MovableStatic reference of the cell: `path` its model, `off` initially disabled or deleted.
inline GlowNote noteGlowCard(GlowCensus& c,std::uint32_t id,const char* path,bool off,Vec at,float reach) {
    if (!glowModel(path)) return GlowNote::notModel;
    ++c.refs;
    if (off || !finite(at) || !std::isfinite(reach) || reach < 0.0f) { ++c.off; return GlowNote::off; }
    if (c.count >= maxCellGlows) { ++c.full; return GlowNote::full; }
    GlowCard& card = c.cards[c.count++];
    card = {id,at,reach,{}};
    const char* name = modelFile(path);
    for (std::size_t i = 0;i+1 < glowNameSize && name[i];++i) card.name[i] = lowered(name[i]);
    return GlowNote::added;
}

// ---- the place of a card at one door -------------------------------------------------------------------
enum class GlowPlace : unsigned { none, behind, front };
constexpr const char* glowPlaceName(GlowPlace p) { return p == GlowPlace::behind ? "behind" : p == GlowPlace::front ? "front" : "none"; }
// `depth` fDoorVoidDepth, `mode` iDoorVoidGlow. Across and heights as the void's lane (fixed_spike_shader_door_policy.h
// doorPlane); behind: from the plane itself (a card in the doorway is the stub's too), not from the frame's start.
inline GlowPlace glowPlace(const DoorPlane& d,float depth,unsigned mode,Vec at,float reach) {
    if (mode == glowOff || !d.valid || !finite(at) || !std::isfinite(depth) || !std::isfinite(reach) || reach < 0.0f) return GlowPlace::none;
    if (!(doorAcross(d,at) <= d.half) || at.z < d.low-reach || at.z > d.high+reach) return GlowPlace::none;
    const float back = -doorSide(d,at);
    if (back >= 0.0f && back <= depth) return GlowPlace::behind;
    if (mode >= glowFront && back < 0.0f && -back <= glowFrontReach) return GlowPlace::front;
    return GlowPlace::none;
}

// ---- the cards not drawn this frame --------------------------------------------------------------------
struct GlowHidden { std::uint32_t ref{},door{}; std::size_t card{}; GlowPlace place{}; };
struct GlowSet {
    std::uint32_t cell{};
    std::size_t doors{};   // doors chosen
    std::array<GlowHidden,maxHiddenGlows> hidden{};
    std::size_t count{};
    unsigned full{};       // cards that qualified past maxHiddenGlows (drawn)
    bool hides(std::uint32_t id) const {
        for (std::size_t i = 0;i < count && i < maxHiddenGlows;++i) if (hidden[i].ref == id) return true;
        return false;
    }
};
// Every card of the census at a chosen door (the nearest chosen door that places it), in census order.
inline GlowSet selectGlows(const GlowCensus& c,const DoorSelection& s,float depth,unsigned mode) {
    GlowSet out;
    out.cell = s.cell;
    out.doors = (std::min)(s.count,maxDoorVoids);
    if (mode == glowOff) return out;
    for (std::size_t i = 0;i < c.count && i < maxCellGlows;++i) {
        const GlowCard& card = c.cards[i];
        for (std::size_t j = 0;j < out.doors;++j) {
            const GlowPlace place = glowPlace(s.chosen[j].door.plane,depth,mode,card.at,card.reach);
            if (place == GlowPlace::none) continue;
            if (out.count < maxHiddenGlows) out.hidden[out.count++] = {card.id,s.chosen[j].door.id,i,place};
            else ++out.full;
            break;
        }
    }
    return out;
}
// Same cards for the same doors in the same order (the trace's change test).
inline bool sameGlows(const GlowSet& a,const GlowSet& b) {
    if (a.cell != b.cell || a.count != b.count || a.full != b.full) return false;
    for (std::size_t i = 0;i < a.count && i < maxHiddenGlows;++i) if (a.hidden[i].ref != b.hidden[i].ref || a.hidden[i].door != b.hidden[i].door) return false;
    return true;
}

// ---- trace ---------------------------------------------------------------------------------------------
// Effect passes of hidden cards since the last row: removed (drawn with nothing rasterised) or left native (no
// audited patched shader for them, or the Effect budget spent).
struct GlowCounts {
    unsigned long long removed{},native{};
};
// FixedDoorGlow, through the frame row's emitter and budget, only when the set changes. Up to maxTracedGlows cards are
// listed as door:reference:model/place (more=N for the rest).
struct GlowTrace {
    double seconds{};
    unsigned long long frame{};
    unsigned mode{};
    const GlowSet* set{};
    const GlowCensus* census{};
    GlowCounts counts{};
};
inline bool formatDoorGlow(char* out,std::size_t size,const GlowTrace& t) {
    if (!t.set) return false;
    const GlowSet& s = *t.set;
    int written = std::snprintf(out,size,"FixedDoorGlow t=%.3f frame=%llu cell=%08X mode=%u doors=%zu hidden=%zu full=%u removed=%llu native=%llu list=",
        t.seconds,t.frame,s.cell,t.mode,s.doors,s.count,s.full,t.counts.removed,t.counts.native);
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written);
    if (!s.count) {
        written = std::snprintf(out+used,size-used,"none");
        return written > 0 && used+static_cast<std::size_t>(written) < size;
    }
    const std::size_t listed = (std::min)((std::min)(s.count,maxHiddenGlows),maxTracedGlows);
    for (std::size_t i = 0;i < listed;++i) {
        const GlowHidden& h = s.hidden[i];
        const bool named = t.census && h.card < t.census->count && h.card < maxCellGlows && t.census->cards[h.card].name[0];
        written = std::snprintf(out+used,size-used,"%s%08X:%08X:%s/%s",i ? ";" : "",h.door,h.ref,named ? t.census->cards[h.card].name.data() : "none",
            glowPlaceName(h.place));
        if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
        used += static_cast<std::size_t>(written);
    }
    if (s.count > listed) {
        written = std::snprintf(out+used,size-used," more=%zu",s.count-listed);
        if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
    }
    return true;
}
// The plugin log, once per cell after the census: the glow cards of the cell, and those a census door places (as
// glowPlace with that door chosen), by door, with their distance behind the plane (negative: in front); at most
// maxHiddenGlows listed.
inline bool formatDoorGlowCensus(char* out,std::size_t size,std::uint32_t cell,unsigned mode,const DoorCensus& doors,const GlowCensus& glows,
    float depth) {
    std::size_t placed = 0;
    for (std::size_t i = 0;i < glows.count && i < maxCellGlows;++i)
        for (std::size_t d = 0;d < doors.count && d < maxCellDoors;++d)
            if (glowPlace(doors.doors[d].plane,depth,mode,glows.cards[i].at,glows.cards[i].reach) != GlowPlace::none) { ++placed; break; }
    int written = std::snprintf(out,size,"FixedCameraSpike shader door glow cell=%08X mode=%u cards=%zu refs=%u off=%u full=%u doors=%zu placed=%zu list=",
        cell,mode,glows.count,glows.refs,glows.off,glows.full,doors.count,placed);
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written),listed = 0;
    for (std::size_t i = 0;i < glows.count && i < maxCellGlows && listed < maxHiddenGlows;++i)
        for (std::size_t d = 0;d < doors.count && d < maxCellDoors;++d) {
            const DoorPlane& p = doors.doors[d].plane;
            const GlowCard& g = glows.cards[i];
            if (glowPlace(p,depth,mode,g.at,g.reach) == GlowPlace::none) continue;
            written = std::snprintf(out+used,size-used,"%s%08X@%08X/%.1f:%s",listed ? "," : "",g.id,doors.doors[d].id,
                static_cast<double>(-doorSide(p,g.at)),g.name[0] ? g.name.data() : "none");
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
