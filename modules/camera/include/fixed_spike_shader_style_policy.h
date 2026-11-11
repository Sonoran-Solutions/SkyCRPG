// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike_shader_policy.h. P5.5-02 wave 2 package
// "shader", fourth pass: Stage 1 of the seamless cutaway (owner decision
// 2026-11-10, session-p55/research/seamless-cutaway.md), iCutStyle=1.
//
// The cut stops being one plane for every draw. Each draw's owning reference
// gets one decision per frame, from its world box (base OBND x world
// rotation), its kind and model family, and where it lies relative to the
// player as seen from the camera:
//   - back side (the camera cannot see the player through it): kept whole;
//   - objects and members (posts, pillars, beams) are never sliced: kept
//     whole, or removed whole when on the camera side and in the lane from
//     the camera to the player;
//   - the storey above (everything wholly above the storey line, and beams
//     above head height) is removed whole; bCutKeepFarGallery keeps its back
//     half;
//   - camera-side kit pieces are still sliced at the old plane (feet +
//     fShaderCutHeight), as now; pieces straddling the player at the storey
//     line, or at the plane where they cross the lane;
//   - cells whose static pieces are mostly dungeon kits (caves, ruins), and
//     every piece this policy cannot place, keep the flat cut exactly.
// Removal is the clip constant `removeConstant` for every pass of every
// geometry of the reference; with bCutStyleShadows the shadow passes of the
// geometry removed in the main view get it too. Nothing here reads engine
// memory; the adapter is src/fixed_spike_shader_style.h.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

namespace skycrpg::camera::fixed::shader {
// ---- whole-draw removal -------------------------------------------------------
// clip = relZ * -1 - 1e9: negative for every height a vertex of this world can
// have (relative to the position adjustment, well inside +-1e8), and, unlike
// the third pass's wholeConstant, a constant `usable` accepts.
constexpr Constant removeConstant{-1.0f,-1.0e9f,0.0f,0.0f};
static_assert(clipDistance(removeConstant,1.0e8f) < 0.0f && clipDistance(removeConstant,-1.0e8f) < 0.0f);

// ---- what a reference is --------------------------------------------------------
// The first folder of the base object's model path (relative to Data\Meshes,
// "meshes\" tolerated, either separator, any case). Source: the placed statics
// of WhiterunBanneredMare (architecture\whiterun 212, clutter 76, furniture 12),
// EmbershardMine01 (dungeons\mines 614, architecture\shackkit 9) and
// BleakFallsBarrow01 (dungeons\nordic 996, dungeons\caves 448) in Skyrim.esm.
enum class Family : unsigned { other, architecture, dungeons, clutter, furniture, landscape, effects, count };
constexpr std::array familyNames{"other","architecture","dungeons","clutter","furniture","landscape","effects"};
static_assert(familyNames.size() == static_cast<std::size_t>(Family::count));
constexpr char lowered(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c-'A'+'a') : c; }
// `text` (lower case, no separator) is the folder at `path`, followed by a separator.
inline bool folderIs(const char* path,const char* text) {
    std::size_t i = 0;
    for (;text[i];++i) if (!path[i] || lowered(path[i]) != text[i]) return false;
    return path[i] == '\\' || path[i] == '/';
}
inline Family modelFamily(const char* path) {
    if (!path) return Family::other;
    if (folderIs(path,"meshes")) path += 7;
    constexpr std::array<std::pair<const char*,Family>,6> folders{{{"architecture",Family::architecture},{"dungeons",Family::dungeons},
        {"clutter",Family::clutter},{"furniture",Family::furniture},{"landscape",Family::landscape},{"effects",Family::effects}}};
    for (const auto& [text,family] : folders) if (folderIs(path,text)) return family;
    return Family::other;
}
// flat: the policy cannot place it (a dungeon, landscape or effect static, an
// unknown model, no OBND), so it keeps the flat cut. kit: an architecture-kit
// wall, corner, floor, roof or ceiling piece. member: an architecture piece
// with two small spans and one long (a pillar, post, beam or rafter). object:
// a placed object of an object form type, or a clutter or furniture static.
enum class Piece : unsigned { flat, kit, member, object, count };
constexpr std::array pieceNames{"flat","kit","member","object"};
static_assert(pieceNames.size() == static_cast<std::size_t>(Piece::count));

// ---- the world box ------------------------------------------------------------------
struct Box { Vec low{},high{}; bool valid{}; };
// The axis-aligned world box of the base OBND [low, high] placed by the world
// transform: row i of the rotation is `rows[i]`, world = origin + scale * R * local.
inline Box worldBox(Vec low,Vec high,const std::array<Vec,3>& rows,float scale,Vec origin) {
    Box b;
    if (!finite(low) || !finite(high) || !finite(origin) || !std::isfinite(scale) || scale <= .001f || low.x > high.x || low.y > high.y ||
        low.z > high.z || (low.x == high.x && low.y == high.y && low.z == high.z)) return b;
    const Vec centre{(low.x+high.x)*.5f,(low.y+high.y)*.5f,(low.z+high.z)*.5f},half{(high.x-low.x)*.5f,(high.y-low.y)*.5f,(high.z-low.z)*.5f};
    const std::array<float,3> at{origin.x,origin.y,origin.z};
    std::array<float,3> lows{},highs{};
    for (std::size_t i = 0;i < 3;++i) {
        const Vec r = rows[i];
        if (!finite(r)) return b;
        const float middle = at[i]+scale*(r.x*centre.x+r.y*centre.y+r.z*centre.z);
        const float reachOut = scale*(std::abs(r.x)*half.x+std::abs(r.y)*half.y+std::abs(r.z)*half.z);
        lows[i] = middle-reachOut; highs[i] = middle+reachOut;
    }
    b.low = {lows[0],lows[1],lows[2]}; b.high = {highs[0],highs[1],highs[2]};
    b.valid = finite(b.low) && finite(b.high);
    return b;
}
// A cube around a bound sphere: used for objects without an OBND.
inline Box sphereBox(Vec centre,float radius) {
    Box b;
    if (!finite(centre) || !std::isfinite(radius) || radius <= 0.0f) return b;
    b.low = {centre.x-radius,centre.y-radius,centre.z-radius}; b.high = {centre.x+radius,centre.y+radius,centre.z+radius};
    b.valid = true;
    return b;
}
struct Spans { float x{},y{},z{}; };
constexpr Spans spansOf(const Box& b) { return {b.high.x-b.low.x,b.high.y-b.low.y,b.high.z-b.low.z}; }
// Member: the middle span at most 128 and the longest at least twice it. Mare
// numbers (OBND spans): WRIntFreeWallColum01 62 x 73 x 292, WRIntBeam512
// 22 x 512 x 116, WRIntBeam256 239 x 21 x 14 are members; WRIntWallStr01
// 256 x 39 x 288 and WRIntWallCor01 248 x 248 x 288 are not.
constexpr float memberSpan = 128.0f;
inline bool memberShape(const Box& b) {
    const Spans s = spansOf(b);
    std::array<float,3> v{s.x,s.y,s.z};
    std::sort(v.begin(),v.end());
    return v[1] <= memberSpan && v[2] >= 2.0f*v[1];
}
// A wall: one horizontal span at most 64 and the other at least 2.5 times it,
// at least 128 tall. Its thin axis says which side of the player it is on.
enum class Thin : unsigned { none, x, y };
constexpr float thinWall = 64.0f, thinRatio = 2.5f, wallHeight = 128.0f;
inline Thin thinAxis(const Box& b) {
    const Spans s = spansOf(b);
    if (s.z < wallHeight) return Thin::none;
    if (s.x <= thinWall && s.y >= thinRatio*s.x) return Thin::x;
    if (s.y <= thinWall && s.x >= thinRatio*s.y) return Thin::y;
    return Thin::none;
}
// `obnd`: the box came from the base OBND (otherwise from the bound sphere).
// An object without a usable box (no 3D, no OBND and a zero bound) cannot be
// placed either: flat (review of 30503e0, MINOR-1).
inline Piece pieceOf(Kind kind,Family family,const Box& box,bool obnd) {
    if (kind == Kind::object) return box.valid ? Piece::object : Piece::flat;
    if (kind != Kind::architecture) return Piece::flat;
    if (family == Family::clutter || family == Family::furniture) return box.valid ? Piece::object : Piece::flat;
    if (family != Family::architecture || !box.valid || !obnd) return Piece::flat;
    return memberShape(box) ? Piece::member : Piece::kit;
}

// ---- which side of the player ---------------------------------------------------
// front: between the player and the camera (or beyond the camera); back: the
// camera cannot see the player through it; across: neither can be said.
// Back is decided by a separating plane through the player: perpendicular to
// a wall's thin axis, or to the horizontal camera direction for any other
// shape. Nothing wholly beyond such a plane can lie on a sightline from the
// camera to the player, whatever its height, so keeping it whole never hides
// the player. With the four locked headings the camera offset is (+-237.3,
// +-237.3, 683.8) (wave 1), so for each heading each world axis has a front
// and a back side.
enum class Side : unsigned { across, front, back, count };
constexpr std::array sideNames{"across","front","back"};
static_assert(sideNames.size() == static_cast<std::size_t>(Side::count));
constexpr float sideEnter = 24.0f, sideExit = 0.0f; // clearance from the player to take a side; to keep it (hysteresis)
// One axis: the box [lo, hi], the player at p, the camera at p + offset.
constexpr Side alongAxis(float lo,float hi,float p,float offset,float clearance) {
    const bool above = lo >= p+clearance,below = hi <= p-clearance;
    if (above == below) return Side::across;
    // A camera level with the player on this axis keeps the sightline in the plane: both sides are back.
    if (offset > -1.0f && offset < 1.0f) return Side::back;
    return (above ? offset > 0.0f : offset < 0.0f) ? Side::front : Side::back;
}
inline Side diagonalSide(const Box& b,Vec feet,Vec eye,float clearance) {
    float dx = eye.x-feet.x,dy = eye.y-feet.y;
    const float length = std::hypot(dx,dy);
    if (!std::isfinite(length) || length < 1.0f) return Side::across;
    dx /= length; dy /= length;
    const float x0 = (b.low.x-feet.x)*dx,x1 = (b.high.x-feet.x)*dx,y0 = (b.low.y-feet.y)*dy,y1 = (b.high.y-feet.y)*dy;
    const float lowest = (std::min)(x0,x1)+(std::min)(y0,y1),highest = (std::max)(x0,x1)+(std::max)(y0,y1);
    if (lowest >= clearance) return Side::front;
    if (highest <= -clearance) return Side::back;
    return Side::across;
}
inline Side sideOf(const Box& b,Thin thin,Vec feet,Vec eye,float clearance) {
    if (!b.valid || !finite(feet) || !finite(eye)) return Side::across;
    // A wall in line with the player (its thin axis passes through the feet, the wall itself well
    // to one side) is placed by the diagonal like any other shape.
    const Side axis = thin == Thin::x ? alongAxis(b.low.x,b.high.x,feet.x,eye.x-feet.x,clearance) :
        thin == Thin::y ? alongAxis(b.low.y,b.high.y,feet.y,eye.y-feet.y,clearance) : Side::across;
    return axis != Side::across ? axis : diagonalSide(b,feet,eye,clearance);
}
// The quadrant of the camera around the player: one per locked heading; it
// changes only on a snap. -1: not known.
inline int quadrantOf(Vec feet,Vec eye) {
    const float dx = eye.x-feet.x,dy = eye.y-feet.y;
    if (!std::isfinite(dx) || !std::isfinite(dy)) return -1;
    return (dx >= 0.0f ? 1 : 0)+(dy >= 0.0f ? 2 : 0);
}
// A piece keeps its front or back class while the player stays outside its
// extent on that axis (clearance 0); it takes a new one only with 24 units of
// clearance. A heading snap (another quadrant) starts again without memory.
inline Side settledSide(const Box& b,Thin thin,Vec feet,Vec eye,Side previous,bool sameQuadrant) {
    if (sameQuadrant && previous != Side::across && sideOf(b,thin,feet,eye,sideExit) == previous) return previous;
    return sideOf(b,thin,feet,eye,sideEnter);
}

// ---- the lane from the camera to the player ----------------------------------------
// Two segments, at 20 and at 120 units above the feet (shins and head), each
// starting where the sightline leaves the player's body (24 units from the
// feet toward the camera in the ground plane) and ending at the eye. A box
// grown sideways only (x and y, never z) by `grow` that a segment enters is in
// the lane: enter at 24, stay at 48 (hysteresis while the player walks).
// Review of 30503e0, MAJOR-1: the earlier lane started in the player's own
// column and grew boxes by 48/112 on every axis, so pieces beside the player
// (a stool, a bench, a pillar walked past sideways) counted as in the lane.
constexpr float laneEnter = 24.0f, laneExit = 48.0f, laneLow = 20.0f, laneHigh = 120.0f, laneStart = 24.0f;
// Ground-plane distance from (x, y) to the box's footprint.
inline float footprintDistance(float x,float y,const Box& b) {
    return std::hypot((std::max)({b.low.x-x,0.0f,x-b.high.x}),(std::max)({b.low.y-y,0.0f,y-b.high.y}));
}
// Ground-plane distance from the point (px, py) to the segment (ax, ay)-(bx, by).
inline float pointSegment(float px,float py,float ax,float ay,float bx,float by) {
    const float dx = bx-ax,dy = by-ay,length = dx*dx+dy*dy;
    const float t = length > 0.0f ? std::clamp(((px-ax)*dx+(py-ay)*dy)/length,0.0f,1.0f) : 0.0f;
    return std::hypot(px-(ax+t*dx),py-(ay+t*dy));
}
// The part of the segment whose height is within the box's height range comes
// within `grow` of the box's footprint in the ground plane (a sideways margin
// only: never above or below the box).
inline bool segmentHitsBox(Vec from,Vec to,const Box& b,float grow) {
    if (!b.valid || !finite(from) || !finite(to) || !std::isfinite(grow)) return false;
    float t0 = 0.0f,t1 = 1.0f;
    const float dz = to.z-from.z;
    if (std::abs(dz) < 1.0e-6f) {
        if (from.z < b.low.z || from.z > b.high.z) return false;
    } else {
        float a = (b.low.z-from.z)/dz,c = (b.high.z-from.z)/dz;
        if (a > c) std::swap(a,c);
        t0 = (std::max)(t0,a); t1 = (std::min)(t1,c);
        if (t0 > t1) return false;
    }
    const float ax = from.x+(to.x-from.x)*t0,ay = from.y+(to.y-from.y)*t0,bx = from.x+(to.x-from.x)*t1,by = from.y+(to.y-from.y)*t1;
    // Crossing the footprint (2D slab test), else the nearest of endpoints and corners.
    float enter = 0.0f,leave = 1.0f;
    bool crosses = true;
    const std::array<float,2> start{ax,ay},delta{bx-ax,by-ay},lo{b.low.x,b.low.y},hi{b.high.x,b.high.y};
    for (std::size_t i = 0;i < 2 && crosses;++i) {
        if (std::abs(delta[i]) < 1.0e-6f) { crosses = start[i] >= lo[i] && start[i] <= hi[i]; continue; }
        float u = (lo[i]-start[i])/delta[i],v = (hi[i]-start[i])/delta[i];
        if (u > v) std::swap(u,v);
        enter = (std::max)(enter,u); leave = (std::min)(leave,v);
        crosses = enter <= leave;
    }
    if (crosses) return true;
    float nearest = (std::min)(footprintDistance(ax,ay,b),footprintDistance(bx,by,b));
    for (const float x : {b.low.x,b.high.x})
        for (const float y : {b.low.y,b.high.y}) nearest = (std::min)(nearest,pointSegment(x,y,ax,ay,bx,by));
    return nearest <= grow;
}
inline bool inLane(const Box& b,Vec feet,Vec eye,float grow) {
    float dx = eye.x-feet.x,dy = eye.y-feet.y;
    const float length = std::hypot(dx,dy);
    if (!std::isfinite(length) || length <= laneStart) return false;
    dx *= laneStart/length; dy *= laneStart/length;
    return segmentHitsBox({feet.x+dx,feet.y+dy,feet.z+laneLow},eye,b,grow) || segmentHitsBox({feet.x+dx,feet.y+dy,feet.z+laneHigh},eye,b,grow);
}
inline bool settledLane(const Box& b,Vec feet,Vec eye,bool previous) { return inLane(b,feet,eye,previous ? laneExit : laneEnter); }

// ---- the storey line ------------------------------------------------------------------
// The lowest bottom of a kit piece at least 128 wide in both horizontal spans,
// starting 128 to 768 units above the held feet height and reaching within 512
// units of the feet in the ground plane: the upper floor, the ceiling or the
// roof over the player's storey. In the Bannered Mare (ESM data) that is the
// loft floor WRIntLoftMid01 at 246 + its reference z. Beams and pillars never
// count (members); nor does anything at or below head height.
constexpr float storeyHeadroom = 128.0f, storeyReach = 512.0f, storeyHighest = 768.0f, storeySlab = 128.0f, aboveTolerance = 8.0f;
inline float boxReach(const Box& b,Vec feet) {
    const float dx = (std::max)({b.low.x-feet.x,0.0f,feet.x-b.high.x}),dy = (std::max)({b.low.y-feet.y,0.0f,feet.y-b.high.y});
    const float distance = std::hypot(dx,dy);
    return std::isfinite(distance) ? distance : std::numeric_limits<float>::infinity();
}
inline bool storeyCandidate(Piece piece,const Box& b,Vec feet,float base) {
    if (piece != Piece::kit || !b.valid || !std::isfinite(base)) return false;
    const Spans s = spansOf(b);
    return (std::min)(s.x,s.y) >= storeySlab && b.low.z >= base+storeyHeadroom && b.low.z <= base+storeyHighest && boxReach(b,feet) <= storeyReach;
}
// The held storey line. `found` is the lowest candidate of the last frame
// (infinity: none). The line is the storey height when one is known and the
// flat plane otherwise. The first one seen on a storey base is taken at once.
// A new storey base (the plane's held feet height moved:
// stairs, a teleport) takes the new value at once; within one storey a line
// that would come down takes effect at once (more is removed: the safe way),
// one that would go up, or no storey at all, only after it has persisted for
// `settle` seconds.
struct Storey {
    constexpr static double settle = .6;
    bool primed{},known{},seen{};
    float z{},base{};
    double since{-1};
    unsigned long long moves{};
    void reset() { *this = {}; }
    float line(float planeZ) const { return known ? z : planeZ; }
    void update(float found,float baseZ,float planeZ,double now) {
        const bool have = std::isfinite(found);
        if (!primed || baseZ != base) {
            if (primed) ++moves;
            primed = true; base = baseZ; known = seen = have; z = have ? found : 0.0f; since = -1; return;
        }
        // The first storey seen on this base (a cell or storey entered a moment ago) is taken at once.
        if (have && !seen) { known = seen = true; z = found; since = -1; ++moves; return; }
        const float before = line(planeZ),after = have ? found : planeZ;
        if (!(std::abs(after-before) > aboveTolerance) && have == known) { since = -1; return; }
        // Only a lower storey actually found is taken at once; losing every candidate (a frame that drew
        // none of them) holds the line like a higher one.
        if ((have && after < before-aboveTolerance) || (since >= 0 && now-since >= settle)) {
            known = have; z = have ? found : 0.0f; since = -1; ++moves; return;
        }
        if (since < 0) since = now;
    }
};

// ---- the decision ---------------------------------------------------------------------
enum class Treat : unsigned { flat, keep, slice, low, remove, count };
constexpr std::array treatNames{"flat","keep","slice","low","remove"};
static_assert(treatNames.size() == static_cast<std::size_t>(Treat::count));
// Trace counters by decision (counted per pass).
enum class Bucket : unsigned { flat, fallback, keptFar, keptObject, keptMember, keptGallery, slicedNear, slicedSpan, removedWhole, removedObject,
    interactCut, aroundCut, count };
struct Facts {
    Piece piece{Piece::flat};
    Side side{Side::across};
    bool lane{};      // in the lane (settledLane)
    bool above{};     // box bottom at or above the storey line (less aboveTolerance)
    bool overhead{};  // box bottom at or above the held feet height + storeyHeadroom
    bool around{};    // the box contains the feet in the ground plane (the player stands in or under it)
    bool low{};       // the box's top is at most feet + laneHigh (a hearth, a rug, a bench: not a cabinet)
    bool floor{};     // around, and the top within floorTop of the feet: the floor the player stands on
    bool interact{};  // an interactable reference (door, container, activator, furniture, flora, an item): never removed whole
    bool gallery{};   // bCutKeepFarGallery
    bool kitCell{};   // the cell's census says kit building
};
struct Decision { Treat treat{Treat::flat}; Bucket bucket{Bucket::flat}; };
// Rules 1-4 of the owner decision, in this order:
//   not a kit cell                       flat (caves and ruins exactly as the flat cut)
//   a piece this policy cannot place     flat, counted as a fallback
//   the storey above (above, or a member overhead): removed whole; with the
//     gallery switch its back-side pieces are kept whole
//   object or member around the player: removed whole when overhead (a lamp hanging over the
//     player); kept whole when its top is at most head height (a hearth, a rug, a bench the player
//     stands in); taller and not overhead (a hearth fire's flame and smoke bound): the flat cut at the
//     plane, as the flat cut drew it, never the lane rule, since such a box is mostly empty volume
//     around the player (orchestrator decision after the fix pass; counted with fallback on the row)
//   object or member: removed whole when not on the back side and in the lane, else kept whole
// and over all of these (review of 30503e0):
//   the floor under the player (around, top within 32 of the feet) is never removed (MINOR-4): it
//     keeps the flat cut, which does not reach it
//   an interactable reference is never removed whole (it would stay solid and clickable while
//     invisible): it gets the flat cut at the plane instead, counted as interactCut (MINOR-5)
//   kit piece on the back side: kept whole
//   kit piece on the front side: sliced at the plane, as the flat cut does (rule 3: near walls "as now";
//     single-sided kit walls are not drawn from outside anyway, and a double-sided partition on the camera
//     side stays as low as it was)
//   kit piece across: sliced, at the plane in the lane, at the storey line otherwise (the remaining slices)
constexpr float floorTop = 32.0f;
constexpr Decision rules(const Facts& f) {
    const bool object = f.piece == Piece::object;
    if (f.above || (f.piece == Piece::member && f.overhead)) {
        if (f.gallery && f.side == Side::back) return {Treat::keep,Bucket::keptGallery};
        return {Treat::remove,object ? Bucket::removedObject : Bucket::removedWhole};
    }
    if (object || f.piece == Piece::member) {
        if (f.around && f.overhead) return {Treat::remove,object ? Bucket::removedObject : Bucket::removedWhole};
        if (f.around && f.low) return {Treat::keep,object ? Bucket::keptObject : Bucket::keptMember};
        if (f.around) return {Treat::flat,Bucket::aroundCut};
        if (f.side != Side::back && f.lane) return {Treat::remove,object ? Bucket::removedObject : Bucket::removedWhole};
        return {Treat::keep,object ? Bucket::keptObject : Bucket::keptMember};
    }
    if (f.side == Side::back) return {Treat::keep,Bucket::keptFar};
    if (f.side == Side::front) return {Treat::low,Bucket::slicedNear};
    return {f.lane ? Treat::low : Treat::slice,Bucket::slicedSpan};
}
constexpr Decision decide(const Facts& f) {
    if (!f.kitCell) return {Treat::flat,Bucket::flat};
    if (f.piece == Piece::flat) return {Treat::flat,Bucket::fallback};
    const Decision d = rules(f);
    if (d.treat != Treat::remove) return d;
    if (f.floor) return {Treat::flat,Bucket::fallback};
    if (f.interact) return {Treat::flat,Bucket::interactCut};
    return d;
}
// The height a slicing treatment cuts at.
inline float sliceHeight(Treat treat,float planeZ,float lineZ) {
    return treat == Treat::slice ? lineZ : treat == Treat::low ? (std::min)(planeZ,lineZ) : planeZ;
}

// ---- the cell -------------------------------------------------------------------------
// A cell is a kit building when its placed statics with an architecture model
// outnumber those with a dungeon model and there are at least 16 of them. The
// census runs once per cell, on its first eligible frame: Bannered Mare 212 vs
// 0 (kit), Embershard Mine 9 vs 622 and Bleak Falls Barrow 0 vs 1444 (flat).
enum class CellStyle : unsigned { unknown, kit, flat, count };
constexpr std::array cellNames{"unknown","kit","flat"};
static_assert(cellNames.size() == static_cast<std::size_t>(CellStyle::count));
struct Census { unsigned kit{},dungeon{},statics{}; };
constexpr unsigned censusLeast = 16, censusRetries = 120;
constexpr CellStyle cellStyle(const Census& c) {
    if (!c.statics) return CellStyle::unknown;
    return c.kit >= censusLeast && c.kit > c.dungeon ? CellStyle::kit : CellStyle::flat;
}

// ---- trace: FixedShaderStyle -------------------------------------------------------
struct StyleWindow {
    std::array<unsigned long long,static_cast<std::size_t>(Bucket::count)> buckets{}; // passes by decision
    unsigned long long frames{},underLine{},slicedLow{},slicedFar{},slicedObjects{},tableFull{},storeyUploads{};
    unsigned long long shadowPasses{},shadowRemoved{},shadowSkips{};
};
struct StyleSample {
    double seconds{};
    unsigned long long frame{};
    unsigned style{};
    Feature state{};
    CellStyle cell{};
    Census census{};
    bool gallery{},shadows{},haveView{},storeyKnown{};
    float storeyZ{},base{},lineZ{};
    unsigned refs{};              // references refreshed in the last frame
    unsigned long long moves{};   // storey line changes (cumulative)
    StyleWindow window{};
    unsigned long long fxFloorKept{}; // Effect passes kept at the plane by the floor rule (third-pass wholeEffect, review MINOR-3)
    float fxWholeRadius{};            // largest bound radius of an Effect pass clipped whole
};
inline bool formatStyle(char* out,std::size_t size,const StyleSample& s) {
    const auto& w = s.window;
    const auto bucket = [&](Bucket which) { return w.buckets[static_cast<std::size_t>(which)]; };
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const bool storey = s.haveView && s.storeyKnown;
    const int written = std::snprintf(out,size,
        "FixedShaderStyle t=%.3f frame=%llu style=%u state=%s cell=%s kit=%u dungeon=%u gallery=%u shadows=%u frames=%llu "
        "storeyZ=%.2f relStorey=%.2f lineZ=%.2f keptFar=%llu keptObjects=%llu keptMembers=%llu keptGallery=%llu underLine=%llu "
        "slicedNear=%llu slicedSpan=%llu slicedLow=%llu slicedFar=%llu slicedObjects=%llu removedWhole=%llu removedObjects=%llu "
        "flat=%llu fallback=%llu refs=%u tableFull=%llu storeyMoves=%llu storeyMaps=%llu shadowPasses=%llu shadowRemoved=%llu "
        "shadowSkips=%llu fxFloorKept=%llu fxWholeRadius=%.1f interactCut=%llu sample=window",
        s.seconds,s.frame,s.style,featureNames[static_cast<std::size_t>(s.state)],cellNames[static_cast<std::size_t>(s.cell)],s.census.kit,
        s.census.dungeon,static_cast<unsigned>(s.gallery),static_cast<unsigned>(s.shadows),w.frames,storey ? static_cast<double>(s.storeyZ) : none,
        storey ? static_cast<double>(s.storeyZ-s.base) : none,s.haveView ? static_cast<double>(s.lineZ) : none,bucket(Bucket::keptFar),
        bucket(Bucket::keptObject),bucket(Bucket::keptMember),bucket(Bucket::keptGallery),w.underLine,bucket(Bucket::slicedNear),
        bucket(Bucket::slicedSpan),w.slicedLow,w.slicedFar,w.slicedObjects,bucket(Bucket::removedWhole),bucket(Bucket::removedObject),
        bucket(Bucket::flat),bucket(Bucket::fallback)+bucket(Bucket::aroundCut),s.refs,w.tableFull,s.moves,w.storeyUploads,w.shadowPasses,w.shadowRemoved,w.shadowSkips,s.fxFloorKept,
        static_cast<double>(s.fxWholeRadius),bucket(Bucket::interactCut));
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
