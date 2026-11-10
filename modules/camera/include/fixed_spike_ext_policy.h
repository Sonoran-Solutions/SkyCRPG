// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "ext" (DYL-217): see-through for roofs, overhangs and
// canopies in exterior cells. Disposable spike policy: no engine headers, no
// engine pointers. The native adapter is src/fixed_spike_ext.h.
//
// Bounded by construction: a structure is a candidate only when a camera-to-
// player sightline ray hits its collision, and it is hidden only when an upward
// pick over the player's head hits it too (cover; never a piece that stands on
// the player's ground with a building-sized footprint), or when that sightline
// hit is above the head, near the player, and the space under it is open (a
// 64-unit corridor at 144 over the feet reaches past the point under the hit,
// and a pick down from just below the hit does not meet the same reference), so
// the roof of a building the player stands behind (walls in between) stays.
// A tree (mode 2) is a candidate only when its box covers the player or a
// sightline segment crosses it above the head. There is no cut height and no
// scan of a radius outdoors; large one-piece structures are skipped and traced.
#include "fixed_spike_policy.h"
#include <cstdint>

namespace skycrpg::camera::fixed {
// ---- [FixedCameraSpike] exterior keys --------------------------------------
constexpr unsigned extMaxHiddenLimit = 32;
struct ExtConfig {
    unsigned mode{};          // 0 off (wave 1: nothing hidden outdoors), 1 structures over or just ahead of the player, 2 also trees
    float maxSpan{1024.0f};   // a structure this wide in either local horizontal span is one large structure: skipped
    unsigned maxHidden{16};   // exterior references hidden at once, nearest kept
    float coverFootprint{200.0f}; // cover never takes a piece reaching the feet's level with both spans at least this
};
inline ExtConfig validateExterior(ExtConfig c,bool* adjusted = nullptr) {
    const ExtConfig in = c;
    if (c.mode > 2) c.mode = 0;
    c.maxSpan = bounded(c.maxSpan,1024.0f,512.0f,2048.0f);
    c.maxHidden = std::clamp(c.maxHidden,1u,extMaxHiddenLimit);
    c.coverFootprint = bounded(c.coverFootprint,200.0f,128.0f,1024.0f);
    if (adjusted) *adjusted = c.mode != in.mode || c.maxSpan != in.maxSpan || c.maxHidden != in.maxHidden ||
        c.coverFootprint != in.coverFootprint;
    return c;
}
// Mode 0 never reaches the exterior scan: the wave 1 branch (FCS-31 notice) runs.
constexpr bool exteriorScanned(unsigned mode,bool interiorCell) { return mode != 0 && !interiorCell; }
// Walking from one exterior cell into the next keeps the exterior hides: the
// per-scan handle and 3D check (unload) and the 0.6 s expiry already restore
// whatever is left behind, so a roof does not flash back at a 4096-unit border.
// Any change that involves an interior, or with the exterior rule off, restores.
constexpr bool keepAcrossCells(unsigned mode,bool wasExterior,bool isExterior) { return mode != 0 && wasExterior && isExterior; }

// ---- oriented bounds -------------------------------------------------------
// Base OBND box (lo, hi) under the 3D root's world transform:
// world = rotate * (scale * local) + origin, rotate row-major as NiMatrix3.
struct Obb {
    bool valid{};
    Vec lo{},hi{},origin{};
    std::array<float,9> rotate{};
    float scale{1.0f};
};
inline Obb makeObb(Vec lo,Vec hi,const std::array<float,9>& rotate,Vec origin,float scale) {
    Obb b;
    for (const float value : rotate) if (!std::isfinite(value)) return b;
    if (!finite(lo) || !finite(hi) || !finite(origin) || !std::isfinite(scale) || scale <= .001f ||
        lo.x > hi.x || lo.y > hi.y || lo.z > hi.z || (lo.x == hi.x && lo.y == hi.y && lo.z == hi.z)) return b;
    b.valid = true; b.lo = lo; b.hi = hi; b.rotate = rotate; b.origin = origin; b.scale = scale;
    return b;
}
// The rotation is orthonormal, so its transpose undoes it.
inline Vec toLocal(const Obb& b,Vec p) {
    const Vec d{p.x-b.origin.x,p.y-b.origin.y,p.z-b.origin.z};
    const auto& m = b.rotate;
    return {(m[0]*d.x+m[3]*d.y+m[6]*d.z)/b.scale,(m[1]*d.x+m[4]*d.y+m[7]*d.z)/b.scale,(m[2]*d.x+m[5]*d.y+m[8]*d.z)/b.scale};
}
inline Extent obbExtent(const Obb& b) {
    if (!b.valid) return {};
    return worldExtent(b.lo,b.hi,{b.rotate[6],b.rotate[7],b.rotate[8]},b.scale,b.origin.z);
}
// Parametric overlap [first, last] of the segment a->b with the box grown by
// `grow` world units in its local x and y (slab test in local space).
inline bool segmentBox(const Obb& box,Vec a,Vec b,float grow,float& first,float& last) {
    if (!box.valid || !finite(a) || !finite(b) || !std::isfinite(grow) || grow < 0) return false;
    const Vec p = toLocal(box,a),q = toLocal(box,b);
    const float g = grow/box.scale;
    const std::array lo{box.lo.x-g,box.lo.y-g,box.lo.z},hi{box.hi.x+g,box.hi.y+g,box.hi.z};
    const std::array from{p.x,p.y,p.z},step{q.x-p.x,q.y-p.y,q.z-p.z};
    float t0 = 0.0f,t1 = 1.0f;
    for (std::size_t i = 0;i < 3;++i) {
        if (std::abs(step[i]) < 1e-9f) {
            if (from[i] < lo[i] || from[i] > hi[i]) return false;
            continue;
        }
        float u = (lo[i]-from[i])/step[i],v = (hi[i]-from[i])/step[i];
        if (u > v) std::swap(u,v);
        t0 = (std::max)(t0,u); t1 = (std::min)(t1,v);
        if (t0 > t1) return false;
    }
    first = t0; last = t1;
    return true;
}
// Ground-plane distance from p to the segment a->b (height ignored).
inline float groundDistance(Vec p,Vec a,Vec b) {
    const float dx = b.x-a.x,dy = b.y-a.y,length = dx*dx+dy*dy;
    float t = length > 0.0f ? ((p.x-a.x)*dx+(p.y-a.y)*dy)/length : 0.0f;
    t = std::clamp(t,0.0f,1.0f);
    return std::hypot(p.x-(a.x+dx*t),p.y-(a.y+dy*t));
}

// ---- the rule --------------------------------------------------------------
constexpr float headClear = 160.0f;      // above the head and clutter (a hay bale tops out 148 over the feet beside it): the cover column starts here, canopy crossings count from here
constexpr float coverExitGrow = 48.0f;   // a hidden cover's box test is this much looser (the pick still has to hit it)
constexpr float largeHeight = 1536.0f;   // any structure this tall (world z) is large
constexpr float largeFootprint = 640.0f, largeFootprintHeight = 512.0f; // both spans this wide and this tall: a building shell
constexpr float treeMaxSpan = 1536.0f;   // only the widest trees are skipped
// The vertical column from headClear above the feet up to the camera height
// crosses the box. This is only the box's agreement: cover itself is the
// collision fact that the upward pick along that column hit the reference
// (ExtCandidate::overhead), because an OBND box also spans the empty space
// beside a shell's walls (review M1: WRHouseShack01 at a door exit).
inline bool covers(const Obb& box,Vec feet,float topZ,bool hiddenNow) {
    if (!finite(feet) || !std::isfinite(topZ) || !(topZ > feet.z+headClear)) return false;
    float first{},last{};
    return segmentBox(box,{feet.x,feet.y,feet.z+headClear},{feet.x,feet.y,topZ},hiddenNow ? coverExitGrow : 0.0f,first,last);
}
// A piece that reaches the level of the feet (bottom within floorReach of them
// or lower) with both horizontal spans at least `footprint` stands on the
// player's ground: a shell, a wall segment, a rock. Never a cover. Wave 1's
// floor-span guard, with the footprint configurable (WRStableStr01 is 196).
inline bool groundStanding(const Extent& e,float feetZ,float footprint) {
    return e.valid && e.minZ <= feetZ+floorReach && !((std::min)(e.spanX,e.spanY) < footprint);
}
// A sightline segment (camera to a target) crosses the box somewhere at or
// above headClear over the feet. Heights along the segment are linear, so the
// highest crossing point is one of the two ends of the overlap.
inline bool pierces(const Obb& box,Vec from,Vec to,float feetZ) {
    float first{},last{};
    if (!segmentBox(box,from,to,0.0f,first,last)) return false;
    const float a = from.z+(to.z-from.z)*first,b = from.z+(to.z-from.z)*last;
    return (std::max)(a,b) >= feetZ+headClear;
}
enum class ExtKind { excluded, structure, tree };
// One large reference (house shell, city wall, cliff mesh, a whole bridge)
// carries walls and ground next to the player and is never hidden.
inline bool largeStructure(ExtKind kind,const Extent& e,float maxSpan) {
    if (!e.valid) return true;
    const float wide = (std::max)(e.spanX,e.spanY),narrow = (std::min)(e.spanX,e.spanY),tall = e.maxZ-e.minZ;
    if (kind == ExtKind::tree) return !(wide < treeMaxSpan);
    return !(wide < maxSpan) || !(tall < largeHeight) || (narrow >= largeFootprint && tall >= largeFootprintHeight);
}
// Overhang: the highest point where a sightline ray hit the structure is at
// least overhangClear above the feet (clear of barrels, bales, low walls and
// fences) and within overhangReach of them in the ground plane (the camera
// stands 335.6 units out at the wave 1 framing).
constexpr float overhangClear = 192.0f, overhangReach = 384.0f;
inline bool overhangHit(Vec hit,Vec feet,bool hiddenNow) {
    if (!finite(hit) || !finite(feet)) return false;
    const float clear = overhangClear-(hiddenNow ? cutExitMargin : 0.0f),reach = overhangReach+(hiddenNow ? radiusExitMargin : 0.0f);
    return hit.z >= feet.z+clear && std::hypot(hit.x-feet.x,hit.y-feet.y) <= reach;
}
// Open below, two collision facts (review m1: one point 48 units short of the
// hit let anything thinner than that count, e.g. the stable's end walls):
// 1. a corridor of three horizontal probes at probeHeight over the feet, from
//    the player to probeBeyond past the ground point under the hit (straight,
//    and shifted sideways by probeSpread), every one clear of any collision; a
//    wall, a post line, a building's walls under a roof, a rock face or a hill
//    in that corridor closes it;
// 2. a pick down from downStart below the hit to headClear over the feet that
//    does not meet the same reference (a wall or column continuing under the hit).
constexpr float probeHeight = 144.0f, probeBeyond = 32.0f, probeSpread = 32.0f, probeMinLength = 16.0f, downStart = 16.0f;
constexpr unsigned openProbeCount = 3, maxOverhangProbed = 12, downProbeHits = 3;
struct Probe { bool valid{}; Vec from{},to{}; };
inline std::array<Probe,openProbeCount> openProbes(Vec feet,Vec hit) {
    std::array<Probe,openProbeCount> out{};
    if (!finite(feet) || !finite(hit)) return out;
    const float dx = hit.x-feet.x,dy = hit.y-feet.y,length = std::hypot(dx,dy);
    if (!(length > probeMinLength)) return out; // straight over the player: only the down pick decides
    const float ux = dx/length,uy = dy/length,end = length+probeBeyond,z = feet.z+probeHeight;
    for (unsigned i = 0;i < openProbeCount;++i) {
        const float side = i == 0 ? 0.0f : i == 1 ? probeSpread : -probeSpread;
        const float sx = -uy*side,sy = ux*side;
        out[i] = {true,{feet.x+sx,feet.y+sy,z},{feet.x+ux*end+sx,feet.y+uy*end+sy,z}};
    }
    return out;
}
inline Probe downProbe(Vec feet,Vec hit) {
    if (!finite(feet) || !finite(hit) || !(hit.z-downStart > feet.z+headClear)) return {};
    return {true,{hit.x,hit.y,hit.z-downStart},{hit.x,hit.y,feet.z+headClear}};
}
// clear[i]: corridor probe i reached its end without any hit (a failed pick is
// not clear). downSame: the down pick met the same reference. Fail closed: with
// neither a valid corridor nor a valid down pick there is nothing open to show.
inline bool openBelow(const std::array<Probe,openProbeCount>& probes,const std::array<bool,openProbeCount>& clear,
    bool downValid,bool downSame) {
    bool any = downValid;
    if (downValid && downSame) return false;
    for (unsigned i = 0;i < openProbeCount;++i) {
        if (!probes[i].valid) continue;
        any = true;
        if (!clear[i]) return false;
    }
    return any;
}
enum class ExtSkip { none, kind, treesOff, noBounds, floorContact, large, beside, ground, count };
constexpr std::array extSkipNames{"none","kind","trees-off","no-bounds","floor-contact","large","beside","ground"};
static_assert(extSkipNames.size() == static_cast<std::size_t>(ExtSkip::count));
struct ExtCandidate {
    ExtKind kind{ExtKind::excluded};
    Obb box{};
    bool rayHit{};       // a sightline ray hit its collision this scan
    bool floorContact{}; // a floor probe hit it this scan
    bool pierced{};      // trees: a sightline segment crosses its box above the head
    bool overhead{};     // the upward pick from headClear over the feet to the camera height hit it
    Vec hit{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    bool openBelow{};    // openBelow() for `hit` (highest sightline hit) was true
};
struct ExtVerdict { Reason reason{Reason::none}; ExtSkip skip{ExtSkip::none}; };
// Order: what is not a candidate at all returns {} (no trace); then floor and
// size refusals; then cover, overhang, canopy. A structure on a ray that is
// neither over the player nor an open overhang is "beside" (a wall, a rock
// face, the roof of a building the player stands behind) and stays drawn; a
// structure the upward pick hit that stands on the player's ground with a
// building-sized footprint is "ground" (a shell or wall the player is next to).
inline ExtVerdict classifyExterior(const ExtConfig& c,const ExtCandidate& x,Vec feet,float topZ,bool hiddenNow) {
    if (x.kind == ExtKind::excluded) return {Reason::none,x.rayHit ? ExtSkip::kind : ExtSkip::none};
    if (x.kind == ExtKind::tree && c.mode < 2) return {Reason::none,x.rayHit ? ExtSkip::treesOff : ExtSkip::none};
    if (x.kind == ExtKind::structure && !x.rayHit) return {};
    if (!x.box.valid) return {Reason::none,x.rayHit ? ExtSkip::noBounds : ExtSkip::none};
    const bool cover = x.overhead && covers(x.box,feet,topZ,hiddenNow);
    const bool overhang = x.rayHit && x.openBelow && overhangHit(x.hit,feet,hiddenNow);
    const bool canopy = x.kind == ExtKind::tree && x.pierced;
    if (!x.rayHit && !cover && !canopy) return {};
    if (x.floorContact) return {Reason::none,ExtSkip::floorContact};
    const Extent e = obbExtent(x.box);
    if (largeStructure(x.kind,e,c.maxSpan)) return {Reason::none,ExtSkip::large};
    // Trees are never shells: a pine's box always reaches the ground 950 wide.
    if (cover && x.kind == ExtKind::structure && groundStanding(e,feet.z,c.coverFootprint)) return {Reason::none,ExtSkip::ground};
    if (cover) return {Reason::extCover,ExtSkip::none};
    if (overhang) return {Reason::extOverhang,ExtSkip::none};
    if (canopy) return {Reason::extCanopy,ExtSkip::none};
    return {Reason::none,ExtSkip::beside};
}

// ---- rays and cells --------------------------------------------------------
constexpr unsigned extRayTargets = 19, extRaySegments = 4, extMaxHits = 48, extMaxCells = 9;
constexpr float extInnerRing = 64.0f, extOuterRing = 128.0f;
// Head, chest, hip; 8 points 64 units out at chest height (the body); 8 points
// 128 units out at knee height (the ground the player is about to step on).
inline std::array<Vec,extRayTargets> exteriorTargets(Vec feet) {
    std::array<Vec,extRayTargets> out{};
    out[0] = {feet.x,feet.y,feet.z+120.0f};
    out[1] = {feet.x,feet.y,feet.z+80.0f};
    out[2] = {feet.x,feet.y,feet.z+40.0f};
    for (unsigned i = 0;i < 8;++i) {
        const float angle = static_cast<float>(i)*pi/4.0f;
        out[3+i] = {feet.x+extInnerRing*std::cos(angle),feet.y+extInnerRing*std::sin(angle),feet.z+80.0f};
        out[11+i] = {feet.x+extOuterRing*std::cos(angle),feet.y+extOuterRing*std::sin(angle),feet.z+24.0f};
    }
    return out;
}
// A tree is tested against the fan only when its bound sphere reaches the
// camera-to-feet segment widened by the outer ring (ground plane).
inline bool nearFan(Vec centre,float radius,Vec feet,Vec eye) {
    if (!finite(centre) || !std::isfinite(radius) || radius < 0) return false;
    return groundDistance(centre,eye,feet) <= radius+extOuterRing;
}
template<std::size_t Size>
bool piercedByFan(const Obb& box,Vec eye,const std::array<Vec,Size>& targets,float feetZ) {
    for (const auto target : targets) if (pierces(box,eye,target,feetZ)) return true;
    return false;
}
// Exterior cells are 4096 units square, cell (0,0) spanning [0,4096). The
// engine's own position-to-cell step (TES::GetCell, ID 13322 at 1A3C60)
// truncates, corrects toward negative infinity and shifts right by 12.
constexpr float cellLimit = 1073741824.0f; // 2^30: beyond this the index would not fit
inline std::optional<int> cellIndex(float coordinate) {
    if (!std::isfinite(coordinate) || std::abs(coordinate) >= cellLimit) return {};
    return static_cast<int>(std::floor(coordinate)) >> 12;
}
// Every qualifying reference is narrower than this, so its origin cell is
// within it of the fan: tree limit, which is the larger.
constexpr float cellMargin = treeMaxSpan;
struct CellRange { bool valid{}; int x0{},x1{},y0{},y1{}; };
// Cells whose square comes within cellMargin of the ground-plane box around the
// camera, the feet and the outer ring.
inline CellRange cellsAround(Vec feet,Vec eye) {
    CellRange r;
    if (!finite(feet) || !finite(eye)) return r;
    const float left = (std::min)(feet.x-extOuterRing,eye.x)-cellMargin,right = (std::max)(feet.x+extOuterRing,eye.x)+cellMargin;
    const float low = (std::min)(feet.y-extOuterRing,eye.y)-cellMargin,high = (std::max)(feet.y+extOuterRing,eye.y)+cellMargin;
    const auto x0 = cellIndex(left),x1 = cellIndex(right),y0 = cellIndex(low),y1 = cellIndex(high);
    if (!x0 || !x1 || !y0 || !y1) return r;
    r = {true,*x0,*x1,*y0,*y1};
    // At most 3 x 3 (camera up to about 4096 units from the feet); more is refused.
    if (r.x1-r.x0 > 2 || r.y1-r.y0 > 2) r.valid = false;
    return r;
}
// Slot of an exterior cell in the loaded grid. ID 13319 (1A3A00): half =
// length >> 1, slot = cell - centre (TES +B0/+B4) + half, inside [0, length);
// GridCellArray slot (x, y) is cells[x * length + y] (ID 15946 at 227120).
constexpr unsigned maxGridLength = 17; // as the Selection scan; uGridsToLoad is 5 in the pinned file
struct GridSlot {
    int x{-1},y{-1};
    constexpr bool valid() const { return x >= 0 && y >= 0; }
};
constexpr GridSlot gridSlot(int cellX,int cellY,int centreX,int centreY,unsigned length) {
    if (length == 0 || length > maxGridLength) return {};
    const int size = static_cast<int>(length),half = static_cast<int>(length >> 1);
    const long long x = static_cast<long long>(cellX)-centreX+half,y = static_cast<long long>(cellY)-centreY+half;
    if (x < 0 || y < 0 || x >= size || y >= size) return {};
    return {static_cast<int>(x),static_cast<int>(y)};
}

// ---- counters and trace ----------------------------------------------------
// cells: cells actually walked. unmatched: hit references not found in the
// walked cells (a persistent reference, an owner cell outside the walked set,
// or a walk cut by visitCap); they are never hidden. overhead: references the
// upward pick hit. downSame: overhang candidates closed by the down pick.
// floorFailed: scans in which a floor probe failed (nothing claimed or hidden).
struct ExtCounters {
    unsigned long long scans{},cells{},visited{},rays{},rayHits{},rayNoRef{},rayFailed{},rayCap{},matched{},unmatched{},
        overhead{},probed{},open{},downSame{},probeCap{},trees{},treeTests{},hides{},gridSkips{},visitCap{},cap{},noWorld{},
        floorFailed{};
    std::array<unsigned long long,static_cast<std::size_t>(ExtSkip::count)> skips{};
};
// FCS-62..69 are this package's codes. FCS-60 and FCS-61 are already the base
// restoreConflict and offMain codes, and the base refusal mask is one 64-bit
// word, so the exterior codes keep their own mask (bit = code - 60).
enum class ExtRefusal : unsigned { settings = 62, grid = 63, pickSlot = 64, world = 65 };
constexpr unsigned extRefusalBit(ExtRefusal code) { return 1u << (static_cast<unsigned>(code)-60u); }
// A grid that does not hold the player's cell for this many scans in a row is
// refused (FCS-63); a one-scan mismatch while crossing a cell border is only counted.
constexpr unsigned gridRefuseScans = 10;
// Own line budget for ext-summary (one per second: an hour), so skip lines on a
// long walk cannot end the summaries routes assert on (review m5).
constexpr unsigned extSummaryLines = 7200; // two lines a second (ext-summary, ext-exits): an hour
struct ExtState {
    ExtConfig config;
    ExtCounters counters;
    std::atomic_uint refusals{};
    double nextSummary{};
    float lastEyeZ{};
    unsigned gridMisses{};  // consecutive scans without a usable grid
    unsigned worldMisses{}; // consecutive scans without a world or character controller (FCS-65 after gridRefuseScans)
    LineBudget summaryLines{extSummaryLines};
};
static_assert(static_cast<std::size_t>(Exit::count) == 13, "formatExtSummary prints thirteen exit counters");
struct ExtSummary {
    double seconds{};
    unsigned long long frame{};
    unsigned cell{},mode{},hidden{},tracked{},maxHidden{};
    float feetZ{},eyeZ{};
    unsigned long long restores{};
    ExtCounters c{};
    std::array<unsigned long long,static_cast<std::size_t>(Exit::count)> exits{};
};
// ext-summary: the exterior counters; ext-exits (formatExtExits): restores and
// the thirteen restore causes in Exit order (exitNames). Both cumulative.
inline bool formatExtSummary(char* out,std::size_t size,const ExtSummary& s) {
    const auto& c = s.c;
    const auto skip = [&](ExtSkip k) { return c.skips[static_cast<std::size_t>(k)]; };
    const int written = std::snprintf(out,size,
        "FixedHide t=%.3f frame=%llu event=ext-summary cell=%08X mode=%u hidden=%u tracked=%u maxHidden=%u scans=%llu "
        "cells=%llu visited=%llu rays=%llu rayHits=%llu rayNoRef=%llu rayFailed=%llu rayCap=%llu matched=%llu unmatched=%llu "
        "overhead=%llu probed=%llu open=%llu downSame=%llu probeCap=%llu trees=%llu treeTests=%llu hides=%llu "
        "extCap=%llu gridSkips=%llu visitCap=%llu noWorld=%llu floorFailed=%llu skipKind=%llu skipTreesOff=%llu skipNoBounds=%llu "
        "skipFloorContact=%llu skipLarge=%llu skipBeside=%llu skipGround=%llu feetZ=%.2f eyeZ=%.2f sample=cumulative",
        s.seconds,s.frame,s.cell,s.mode,s.hidden,s.tracked,s.maxHidden,c.scans,
        c.cells,c.visited,c.rays,c.rayHits,c.rayNoRef,c.rayFailed,c.rayCap,c.matched,c.unmatched,
        c.overhead,c.probed,c.open,c.downSame,c.probeCap,c.trees,c.treeTests,c.hides,
        c.cap,c.gridSkips,c.visitCap,c.noWorld,c.floorFailed,
        skip(ExtSkip::kind),skip(ExtSkip::treesOff),skip(ExtSkip::noBounds),skip(ExtSkip::floorContact),skip(ExtSkip::large),
        skip(ExtSkip::beside),skip(ExtSkip::ground),static_cast<double>(s.feetZ),static_cast<double>(s.eyeZ));
    return written > 0 && static_cast<std::size_t>(written) < size;
}
// Second line of the same second: restores and the thirteen restore causes, cumulative.
inline bool formatExtExits(char* out,std::size_t size,const ExtSummary& s) {
    const int written = std::snprintf(out,size,
        "FixedHide t=%.3f frame=%llu event=ext-exits cell=%08X hidden=%u restores=%llu "
        "exits=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu sample=cumulative",
        s.seconds,s.frame,s.cell,s.hidden,s.restores,
        s.exits[0],s.exits[1],s.exits[2],s.exits[3],s.exits[4],s.exits[5],s.exits[6],s.exits[7],s.exits[8],s.exits[9],s.exits[10],
        s.exits[11],s.exits[12]);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed
