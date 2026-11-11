// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 1 (DYL-217). Disposable spike policy: no engine headers, no
// engine pointers. The native adapter is src/fixed_spike.h; this branch is
// never merged into the shipped Camera DLL.
#include "policy.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <thread>

namespace skycrpg::camera::fixed {
struct Vec { float x{},y{},z{}; };
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline float span(Vec a,Vec b) { return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z); }
inline Vec mix(Vec a,Vec b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t}; }
constexpr float degrees(float value) { return value * 180.0f / pi; }

// ---- [FixedCameraSpike] ----------------------------------------------------
constexpr unsigned maxHidden = 256, maxRayTargets = 11, maxRaySegments = 6, maxRayHits = 48;
constexpr unsigned maxFloorProbes = 5, maxVisited = 8192, maxRooms = 512;
constexpr unsigned rigTraceLines = 12000, roomTraceLines = 12000, hideTraceLines = 8000;
struct Config {
    bool enabled{};
    float heading{45.0f};      // compass degrees the camera looks toward: 0 north (+Y), 90 east (+X)
    float snapSeconds{.25f};
    float zoom{.65f};          // native requested zoom scalar
    int snapLeftKey{51};       // DIK_COMMA
    int snapRightKey{52};      // DIK_PERIOD
    bool castProbe{true};      // also run the native cast and count what visible geometry it would have hit
    bool rigTrace{};
    unsigned roomSeedMode{};   // 0 vanilla, 1 player's room, 2 sightline, 3 radius
    float roomSeedRadius{1024.0f};
    bool roomTrace{};
    unsigned seeThroughMode{}; // 0 none, 1 hide, 3 shader cut (wave 2: fixed_spike_shader_policy.h)
    float cutHeight{160.0f};
    float hideRadius{384.0f};  // above-cut hides only within this ground-plane reach of the feet
    unsigned hidePerFrame{16};
    unsigned hideTotal{192};
    bool hideTrace{};
    bool holdFade{true};       // bHoldPlayerFade: the engine does not fade the player for its own camera point while the rig holds
};
// Keys that already have an owner in this stack: F8 (66) SmoothCam, K/L (37/38)
// map, U (22) target lock, [ ] (26/27) Selection cycle, plus the live Camera and
// cursor-mode bindings passed by the caller.
inline bool snapKeyAllowed(int key,const Settings& base,int cursorKey) {
    if (key == -1) return true;
    if (key <= 0 || key > 255) return false;
    for (const int taken : {66,37,38,22,26,27,base.toggleKey,base.recenterKey,cursorKey})
        if (key == taken) return false;
    return true;
}
// Returns the usable configuration; *adjusted reports any value that was not
// accepted as written (the adapter logs one coded line).
inline Config validate(Config c,const Settings& base,int cursorKey,bool* adjusted = nullptr) {
    const Config in = c;
    c.heading = std::isfinite(c.heading) && std::abs(c.heading) <= 3600.0f ? c.heading : 45.0f;
    c.heading = std::fmod(c.heading,360.0f);
    if (c.heading < 0) c.heading += 360.0f;
    c.snapSeconds = bounded(c.snapSeconds,.25f,.05f,2.0f);
    c.zoom = bounded(c.zoom,.65f,.05f,1.0f);
    if (!snapKeyAllowed(c.snapLeftKey,base,cursorKey)) c.snapLeftKey = -1;
    if (!snapKeyAllowed(c.snapRightKey,base,cursorKey) || (c.snapRightKey != -1 && c.snapRightKey == c.snapLeftKey)) c.snapRightKey = -1;
    if (c.roomSeedMode > 3) c.roomSeedMode = 0;
    c.roomSeedRadius = bounded(c.roomSeedRadius,1024.0f,0.0f,8192.0f);
    if (c.seeThroughMode > 1 && c.seeThroughMode != 3) c.seeThroughMode = 0; // 2 belongs to another wave 2 package
    c.cutHeight = bounded(c.cutHeight,160.0f,64.0f,1024.0f);
    c.hideRadius = bounded(c.hideRadius,384.0f,128.0f,2048.0f);
    c.hidePerFrame = std::clamp(c.hidePerFrame,1u,64u);
    c.hideTotal = std::clamp(c.hideTotal,1u,maxHidden);
    if (adjusted) *adjusted = c.heading != in.heading || c.snapSeconds != in.snapSeconds || c.zoom != in.zoom ||
        c.snapLeftKey != in.snapLeftKey || c.snapRightKey != in.snapRightKey || c.roomSeedMode != in.roomSeedMode ||
        c.roomSeedRadius != in.roomSeedRadius || c.seeThroughMode != in.seeThroughMode || c.cutHeight != in.cutHeight ||
        c.hideRadius != in.hideRadius || c.hidePerFrame != in.hidePerFrame || c.hideTotal != in.hideTotal;
    return c;
}
// -1 left (counter-clockwise seen from above), +1 right, 0 not a snap key.
constexpr int snapDirection(const Config& c,int key) {
    return key < 0 ? 0 : key == c.snapLeftKey ? -1 : key == c.snapRightKey ? 1 : 0;
}

// ---- rig -------------------------------------------------------------------
// The spike holds the view only where the shipped policy already holds pitch.
constexpr bool rigEligible(bool active,State state) {
    return active && (state == State::normal || state == State::targetLock);
}
// One definition of the four headings, so "lands on a heading" is an equality.
inline float headingRadians(float baseDegrees,unsigned index) {
    return wrap(radians(baseDegrees+90.0f*static_cast<float>(index%4u)));
}
inline float ease(float t) {
    t = std::clamp(t,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
struct SnapTurn {
    unsigned index{};   // heading the rig is on, or turning toward
    bool turning{};
    float remaining{};  // signed radians still to travel when the turn started
    double start{};
    void reset() { *this = {}; }
    // Heading to hold at `now`. The final value is headingRadians() itself.
    float target(float baseDegrees,double now,float seconds) {
        const float goal = headingRadians(baseDegrees,index);
        if (!turning) return goal;
        const double elapsed = now-start;
        if (!std::isfinite(elapsed) || !(seconds > 0) || elapsed < 0 || elapsed >= static_cast<double>(seconds)) {
            turning = false; return goal;
        }
        return wrap(goal-remaining*(1.0f-ease(static_cast<float>(elapsed/static_cast<double>(seconds)))));
    }
    // A press during a turn retargets from the current eased heading, keeping
    // the direction of the press; it can only ever end on one of the four.
    void snap(int direction,float baseDegrees,double now,float seconds) {
        if (!direction) return;
        const float current = target(baseDegrees,now,seconds);
        index = (index+(direction > 0 ? 1u : 3u))%4u;
        float travel = wrap(headingRadians(baseDegrees,index)-current);
        if (direction > 0 && travel < -.0001f) travel += 2.0f*pi;
        if (direction < 0 && travel > .0001f) travel -= 2.0f*pi;
        remaining = travel; start = now; turning = true;
    }
    // F7 in the spike: shortest way back to the configured heading.
    void home(float baseDegrees,double now,float seconds) {
        const float current = target(baseDegrees,now,seconds);
        index = 0;
        remaining = wrap(headingRadians(baseDegrees,0)-current); start = now;
        turning = std::abs(remaining) > .0001f;
    }
};
// World heading of the third-person camera = player heading + freeRotation.x
// (native UpdateRotation, RVA 8FE7B1..8FE7CB). Native HandleLookInput adds
// input.x * gain * dt to freeRotation.x, so this request lands on `target`.
inline float holdYawInput(float target,float playerHeading,float freeRotationX,float gain,float dt) {
    const float error = wrap(target-(playerHeading+freeRotationX));
    if (!std::isfinite(error) || !std::isfinite(gain) || !std::isfinite(dt) || gain <= 0.0f || dt <= 0.0f || dt > .1f) return 0.0f;
    return error/(gain*dt);
}
// In a native POV slide the engine subtracts the raw look x again after
// HandleLookInput (RVA 8FD6B9..8FD6D9); keep it so the two cancel.
constexpr float rigYawInput(bool slide,float rawX,float hold) { return (slide ? rawX : 0.0f)+hold; }
constexpr float heldZoom(bool rig,float fixedZoom,float baseRequest) { return rig ? fixedZoom : baseRequest; }
// DYL-157 compression is measured from pull-in, which the spike removes.
constexpr bool obstructionSampled(bool spikeConfigured) { return !spikeConfigured; }
enum class Cast { native, suppress };
constexpr Cast castDecision(bool castReady,bool frameEligible,bool mainThread,bool smoothCamCaller) {
    return castReady && frameEligible && mainThread && smoothCamCaller ? Cast::suppress : Cast::native;
}
constexpr float pullInTolerance = .5f; // game units between rendered and pre-collision position
// SmoothCam publishes a frame's pre-collision position only at the start of
// its NEXT update (thirdperson.cpp:296), so pull-in pairs the previous frame's
// rendered position with the value read now. Camera travel between frames
// never enters the number.
constexpr bool pullInPair(bool previousEligible,unsigned long long previousFrame,unsigned long long frame) {
    return previousEligible && previousFrame+1 == frame;
}
// Every eligible frame between two trace lines, so a single pulled-in or
// off-heading frame cannot fall between 10 Hz samples.
struct RigWindow {
    unsigned frames{},pairs{},pullInFrames{};
    float maxPullIn{},maxHeadingError{}; // game units, degrees
    void pull(float distance) {
        ++pairs;
        if (!(distance <= pullInTolerance)) ++pullInFrames; // nan counts as a failure
        if (distance > maxPullIn) maxPullIn = distance;
    }
    void heading(float errorDegrees) {
        ++frames;
        const float size = std::abs(errorDegrees);
        if (size > maxHeadingError || !std::isfinite(size)) maxHeadingError = size;
    }
};
struct RigSample {
    double seconds{};
    unsigned long long frame{};
    bool eligible{},slide{},turning{},pair{};
    unsigned state{},index{};
    Vec camera{},player{},previousCamera{},preCollision{},goal{};
    float heading{},target{},pitch{},zoom{},heldZoom{}; // degrees, native zoom scalar
    unsigned suppressed{},wouldHit{},passed{},passedHit{};
    RigWindow window{};
};
inline float headingError(float heading,float target) { return degrees(wrap(radians(heading-target))); }
inline bool formatRig(char* out,std::size_t size,const RigSample& s) {
    const int written = std::snprintf(out,size,
        "FixedRig t=%.3f frame=%llu eligible=%u state=%u slide=%u cam=%.2f,%.2f,%.2f player=%.2f,%.2f,%.2f offset=%.2f,%.2f,%.2f "
        "prevCam=%.2f,%.2f,%.2f prevPre=%.2f,%.2f,%.2f goal=%.2f,%.2f,%.2f pair=%u pullIn=%.3f heading=%.3f target=%.3f headingError=%.3f index=%u turning=%u "
        "pitch=%.3f zoom=%.4f heldZoom=%.4f suppressed=%u visibleWouldHit=%u passedNative=%u passedHit=%u "
        "frames=%u pairs=%u pullInFrames=%u maxPullIn=%.3f maxHeadingError=%.3f sample=window",
        s.seconds,s.frame,static_cast<unsigned>(s.eligible),s.state,static_cast<unsigned>(s.slide),
        static_cast<double>(s.camera.x),static_cast<double>(s.camera.y),static_cast<double>(s.camera.z),
        static_cast<double>(s.player.x),static_cast<double>(s.player.y),static_cast<double>(s.player.z),
        static_cast<double>(s.camera.x-s.player.x),static_cast<double>(s.camera.y-s.player.y),static_cast<double>(s.camera.z-s.player.z),
        static_cast<double>(s.previousCamera.x),static_cast<double>(s.previousCamera.y),static_cast<double>(s.previousCamera.z),
        static_cast<double>(s.preCollision.x),static_cast<double>(s.preCollision.y),static_cast<double>(s.preCollision.z),
        static_cast<double>(s.goal.x),static_cast<double>(s.goal.y),static_cast<double>(s.goal.z),static_cast<unsigned>(s.pair),
        s.pair ? static_cast<double>(span(s.previousCamera,s.preCollision)) : static_cast<double>(std::numeric_limits<float>::quiet_NaN()),
        static_cast<double>(s.heading),static_cast<double>(s.target),static_cast<double>(headingError(s.heading,s.target)),
        s.index,static_cast<unsigned>(s.turning),static_cast<double>(s.pitch),static_cast<double>(s.zoom),static_cast<double>(s.heldZoom),
        s.suppressed,s.wouldHit,s.passed,s.passedHit,s.window.frames,s.window.pairs,s.window.pullInFrames,
        static_cast<double>(s.window.maxPullIn),static_cast<double>(s.window.maxHeadingError));
    return written > 0 && static_cast<std::size_t>(written) < size;
}

// ---- room seeding ----------------------------------------------------------
// Immutable copy published by the main thread; the hook thread never sees an
// engine pointer through it.
struct RoomView {
    unsigned mode{};
    Vec viewer{};      // rendered camera position
    Vec eye{};         // player position + eyeLift
    float radius{};    // mode 3
    float tolerance{}; // how far a queried point may be from `viewer`
};
constexpr float eyeLift = 64.0f;        // Hypothesis for eye height, as in the DYL-199 spike
constexpr float viewerTolerance = 96.0f; // near plane (15) plus one frame of camera travel
// QPointWithin (RVA FCE0C0) is reachable only through vtable slot 0x3F, from
// five functions. Three are the culling passes, which ask about camera + near
// * forward. Reference-to-room attachment and the room-bounded water lookup
// ask about arbitrary points and must always keep the vanilla answer, even for
// a point next to the camera. Extents are the .pdata entries of 1.7.104.
struct CodeRange {
    std::uintptr_t begin{},end{};
    constexpr bool holds(std::uintptr_t address) const { return address >= begin && address < end; }
};
enum class RoomCaller { culling, attach, water, other };
constexpr std::array<CodeRange,3> cullingRvas{{{0x1a97b0,0x1a9de7},{0x1512540,0x151328c},{0x1513290,0x1513fbe}}}; // IDs 13408, 106400, 106401
constexpr std::array<std::uint64_t,3> cullingIds{13408,106400,106401};
// Return addresses of the six `call [rax+1F8]` sites inside those functions.
constexpr std::array<std::uintptr_t,6> cullingReturnRvas{0x1a9981,0x1a9a0e,0x151293c,0x15129ee,0x15136a1,0x151375b};
constexpr CodeRange attachRva{0x2c9b90,0x2ca195}; // ID 19163, call returns to 2C9E4E
constexpr CodeRange waterRva{0x2bee50,0x2bf14f};  // ID 19002, call returns to 2BF079
struct RoomCallers {
    std::array<CodeRange,3> culling{};
    CodeRange attach{},water{};
};
constexpr CodeRange rebased(CodeRange rva,std::uintptr_t base) { return {base+rva.begin,base+rva.end}; }
constexpr RoomCallers roomCallersAt(std::uintptr_t base,bool attachKnown = true,bool waterKnown = true) {
    RoomCallers callers;
    for (std::size_t i = 0;i < callers.culling.size();++i) callers.culling[i] = rebased(cullingRvas[i],base);
    if (attachKnown) callers.attach = rebased(attachRva,base);
    if (waterKnown) callers.water = rebased(waterRva,base);
    return callers;
}
constexpr RoomCaller roomCaller(const RoomCallers& callers,std::uintptr_t returnAddress) {
    for (const auto range : callers.culling) if (range.holds(returnAddress)) return RoomCaller::culling;
    if (callers.attach.holds(returnAddress)) return RoomCaller::attach;
    if (callers.water.holds(returnAddress)) return RoomCaller::water;
    return RoomCaller::other;
}
// Second condition: the query must also be made at the rendered camera, so the
// culling of another camera (reflection, local map) keeps the vanilla answer.
inline bool viewerQuery(const RoomView& view,Vec point) {
    return finite(point) && finite(view.viewer) && std::isfinite(view.tolerance) && span(point,view.viewer) <= view.tolerance;
}
// Called only when the vanilla answer was false. `contains` asks the same room
// about another point through the original function.
template<class Contains>
bool seedClaim(RoomCaller caller,const RoomView& view,Vec point,Vec roomCentre,float roomRadius,Contains contains) {
    if (caller != RoomCaller::culling) return false;
    if (view.mode == 0 || view.mode > 3 || !viewerQuery(view,point) || !finite(view.eye)) return false;
    if (view.mode == 1) return contains(view.eye);
    if (view.mode == 2) {
        // Player end first, then toward the camera, as the DYL-199 spike sampled.
        for (unsigned step = 4;step > 0;--step)
            if (contains(mix(point,view.eye,static_cast<float>(step)/4.0f))) return true;
        return false;
    }
    if (!finite(roomCentre) || !std::isfinite(roomRadius) || roomRadius < 0 || !std::isfinite(view.radius) || view.radius < 0) return false;
    return span(roomCentre,view.eye) < view.radius+roomRadius;
}
// calls = every hook call, split by caller class. viewerCalls = culling calls
// at the camera while seeding is active; gatedNear = non-culling calls at the
// camera while seeding is active, i.e. what the caller gate kept vanilla.
struct RoomCounters {
    std::atomic<unsigned long long> calls{0},culling{0},attach{0},water{0},other{0},viewerCalls{0},gatedNear{0},vanillaTrue{0},claims{0};
    void count(RoomCaller caller) {
        calls.fetch_add(1ull,std::memory_order_relaxed);
        (caller == RoomCaller::culling ? culling : caller == RoomCaller::attach ? attach : caller == RoomCaller::water ? water : other)
            .fetch_add(1ull,std::memory_order_relaxed);
    }
};
struct RoomSample {
    double seconds{};
    unsigned long long frame{};
    unsigned mode{},cell{},rooms{},eyeRooms{},cameraRooms{};
    bool hook{},active{},graph{},capped{};
    int playerRoom{-1},cameraRoom{-1}; // index of the first containing room, -1 none
    unsigned long long calls{},culling{},attach{},water{},other{},viewerCalls{},gatedNear{},vanillaTrue{},claims{};
};
inline bool formatRoom(char* out,std::size_t size,const RoomSample& s) {
    const int written = std::snprintf(out,size,
        "FixedRoom t=%.3f frame=%llu mode=%u hook=%u active=%u cell=%08X graph=%u rooms=%u capped=%u playerRoom=%d cameraRoom=%d "
        "eyeRooms=%u cameraRooms=%u calls=%llu cullCalls=%llu attachCalls=%llu waterCalls=%llu otherCalls=%llu viewerCalls=%llu "
        "gatedNear=%llu vanillaTrue=%llu claims=%llu sample=window",
        s.seconds,s.frame,s.mode,static_cast<unsigned>(s.hook),static_cast<unsigned>(s.active),s.cell,
        static_cast<unsigned>(s.graph),s.rooms,static_cast<unsigned>(s.capped),s.playerRoom,s.cameraRoom,s.eyeRooms,s.cameraRooms,
        s.calls,s.culling,s.attach,s.water,s.other,s.viewerCalls,s.gatedNear,s.vanillaTrue,s.claims);
    return written > 0 && static_cast<std::size_t>(written) < size;
}

// ---- rule-based hide -------------------------------------------------------
enum class Kind { excluded, architecture, object };
enum class Reason { none, aboveCut, onRay };
enum class Skip { none, kind, noBounds, floorContact, floorSpan };
constexpr const char* reasonName(Reason r) { return r == Reason::aboveCut ? "above-cut" : r == Reason::onRay ? "on-ray" : "none"; }
constexpr const char* skipName(Skip s) {
    return s == Skip::kind ? "kind" : s == Skip::noBounds ? "no-bounds" : s == Skip::floorContact ? "floor-contact" :
        s == Skip::floorSpan ? "floor-span" : "none";
}
// World z range and local horizontal footprint of a reference's base bounds.
struct Extent { bool valid{}; float minZ{},maxZ{},spanX{},spanY{}; };
// zRow is the third row of the world rotation; low/high are the base OBND box.
// A zero box (no OBND) is invalid: the caller then has only a sphere.
inline Extent worldExtent(Vec low,Vec high,Vec zRow,float scale,float originZ) {
    Extent e;
    if (!finite(low) || !finite(high) || !finite(zRow) || !std::isfinite(scale) || !std::isfinite(originZ) || scale <= .001f ||
        low.x > high.x || low.y > high.y || low.z > high.z || (low.x == high.x && low.y == high.y && low.z == high.z)) return e;
    const std::array lo{low.x,low.y,low.z},hi{high.x,high.y,high.z},row{zRow.x,zRow.y,zRow.z};
    float bottom = 0,top = 0;
    for (unsigned i = 0;i < 3;++i) {
        bottom += (std::min)(row[i]*lo[i],row[i]*hi[i]);
        top += (std::max)(row[i]*lo[i],row[i]*hi[i]);
    }
    e.minZ = originZ+scale*bottom; e.maxZ = originZ+scale*top;
    e.spanX = scale*(high.x-low.x); e.spanY = scale*(high.y-low.y);
    e.valid = true;
    return e;
}
// Sphere fallback: only the "wholly above" question can be answered.
inline Extent sphereExtent(float centreZ,float radius) {
    Extent e;
    if (!std::isfinite(centreZ) || !std::isfinite(radius) || radius <= 0) return e;
    e.minZ = centreZ-radius; e.maxZ = centreZ+radius; e.valid = true;
    return e;
}
constexpr float cutExitMargin = 24.0f; // a hidden reference stays hidden this far below the cut
constexpr float radiusExitMargin = 64.0f; // and this far outside the hide radius
// This wave's hide is an interior mechanism: an exterior cell is neither
// scanned nor ray-cast (4096-unit cells, uneven ground; wave 2).
constexpr bool hideScanned(unsigned seeThroughMode,bool interiorCell) { return seeThroughMode == 1 && interiorCell; }
// Ground-plane distance from the feet to the nearest point a bound can reach.
// Not finite means "not within any radius".
inline float groundReach(Vec feet,Vec boundCentre,float boundRadius) {
    const float distance = std::hypot(boundCentre.x-feet.x,boundCentre.y-feet.y);
    if (!std::isfinite(distance) || !std::isfinite(boundRadius)) return std::numeric_limits<float>::infinity();
    return (std::max)(0.0f,distance-(std::max)(0.0f,boundRadius));
}
constexpr float floorReach = 16.0f;    // "reaches the floor" = bottom within this of the player's feet or below
constexpr float floorFootprint = 200.0f; // both horizontal spans at least this: a room or cave piece, not a wall
struct Verdict { Reason reason{Reason::none}; Skip skip{Skip::none}; };
// Above the cut: the WHOLE base box is at or above cutZ, so a piece that also
// carries floor (a one-piece cave or room mesh) never qualifies, AND its bound
// reaches within `radius` of the feet in the ground plane, so a ceiling at the
// far end of a hall is left alone. On a ray: architecture only, never the
// piece the player stands on, and never a piece that reaches the floor with a
// room-sized footprint; rays are bounded by the camera-player segment already.
inline Verdict classify(Kind kind,const Extent& box,bool boxIsSphere,bool rayHit,bool floorContact,float cutZ,float feetZ,
    float reach,float radius,bool hiddenNow) {
    if (kind == Kind::excluded) return {Reason::none,Skip::kind};
    const float threshold = hiddenNow ? cutZ-cutExitMargin : cutZ;
    const float within = hiddenNow ? radius+radiusExitMargin : radius;
    if (box.valid && !floorContact && box.minZ >= threshold && reach <= within) return {Reason::aboveCut,Skip::none};
    if (!rayHit || kind != Kind::architecture) return {};
    if (floorContact) return {Reason::none,Skip::floorContact};
    if (!box.valid || boxIsSphere) return {Reason::none,Skip::noBounds};
    if (box.minZ <= feetZ+floorReach && (std::min)(box.spanX,box.spanY) >= floorFootprint) return {Reason::none,Skip::floorSpan};
    return {Reason::onRay,Skip::none};
}
// A hidden piece that the player now stands on, or an above-cut hide whose
// bottom is at or below the feet (after a climb, a teleport or a load), is
// restored at once: the exit dwell must never leave the player on an island.
// On-ray hides stand on the player's floor, so only floor contact restores them
// early; otherwise a wall missed by one scan at the fan's edge would flicker.
inline bool restoreAtOnce(const Extent& box,bool floorContact,float feetZ,bool aboveCut) {
    return floorContact || (aboveCut && box.valid && box.minZ <= feetZ);
}
constexpr double hideEnterSeconds = .2, hideExitSeconds = .6, hideScanSeconds = .1;
constexpr double dwellSlack = 1e-6; // scan times are sums of doubles
// Per-reference dwell: qualify for hideEnterSeconds before hiding, fail for
// hideExitSeconds before restoring. A gap longer than the exit dwell restarts.
struct Track {
    double first{-1},last{-1};
    bool hidden{};
    void seen(double now) { if (first < 0 || now-last > hideExitSeconds) first = now; last = now; }
    bool shouldHide(double now) const { return !hidden && last == now && now-first >= hideEnterSeconds-dwellSlack; }
    bool shouldRestore(double now) const { return hidden && now-last >= hideExitSeconds-dwellSlack; }
    bool expired(double now) const { return !hidden && now-last > hideExitSeconds; }
};
// Caps are nearest-first: hides are applied in order of ground reach, and a
// full table gives its farthest entry up to a strictly nearer candidate.
struct Near { float reach{}; unsigned slot{}; };
template<std::size_t Size>
void nearestFirst(std::array<Near,Size>& items,unsigned count) {
    std::sort(items.begin(),items.begin()+(std::min)(static_cast<std::size_t>(count),Size),[](const Near& a,const Near& b) {
        return a.reach < b.reach || (a.reach == b.reach && a.slot < b.slot);
    });
}
// Index of the farthest tracked entry, -1 when nothing is tracked.
template<class Slots,class Tracked,class Reach>
int farthestSlot(const Slots& slots,Tracked tracked,Reach reach) {
    int best = -1,index = 0;
    float most = -1.0f;
    for (const auto& slot : slots) {
        if (tracked(slot) && reach(slot) > most) { most = reach(slot); best = index; }
        ++index;
    }
    return best;
}
constexpr bool evicts(float candidateReach,float farthestReach) { return candidateReach < farthestReach; }
struct HideBudget {
    unsigned perFrame{},total{},frameUsed{},hidden{},frameCap{},totalCap{};
    void frame() { frameUsed = 0; }
    bool take() {
        if (hidden >= total) { ++totalCap; return false; }
        if (frameUsed >= perFrame) { ++frameCap; return false; }
        ++frameUsed; ++hidden; return true;
    }
    void release() { if (hidden) --hidden; }
};
// Cut reference height. Inside the band it does not move; between the band and
// `snap` it follows the feet after they have stayed there for `settle` seconds,
// so a jump (apex about 76 units) or one stair step does not move it; beyond
// `snap`, up or down, it follows at once (a teleport, a fall, a fast climb).
struct CutPlane {
    constexpr static float band = 24.0f;
    constexpr static float snap = 96.0f;
    constexpr static double settle = .6;
    bool valid{};
    float base{};
    double outside{-1};
    void reset() { *this = {}; }
    float update(float feetZ,double now) {
        if (!std::isfinite(feetZ)) return base;
        if (!valid) { valid = true; base = feetZ; outside = -1; return base; }
        if (std::abs(feetZ-base) <= band) { outside = -1; return base; }
        if (std::abs(feetZ-base) > snap) { base = feetZ; outside = -1; return base; }
        if (outside < 0) outside = now;
        if (now-outside >= settle) { base = feetZ; outside = -1; }
        return base;
    }
};
// Head, chest and a ring at chest height; the floor itself is never a target.
inline std::array<Vec,maxRayTargets> rayTargets(Vec feet,float ring = 48.0f) {
    std::array<Vec,maxRayTargets> out{};
    out[0] = {feet.x,feet.y,feet.z+120.0f};
    out[1] = {feet.x,feet.y,feet.z+80.0f};
    out[2] = {feet.x,feet.y,feet.z+40.0f};
    for (unsigned i = 0;i < 8;++i) {
        const float angle = static_cast<float>(i)*pi/4.0f;
        out[3+i] = {feet.x+ring*std::cos(angle),feet.y+ring*std::sin(angle),feet.z+80.0f};
    }
    return out;
}
inline std::array<Vec,maxFloorProbes> floorProbes(Vec feet,float ring = 96.0f) {
    return {Vec{feet.x,feet.y,feet.z},{feet.x+ring,feet.y,feet.z},{feet.x-ring,feet.y,feet.z},
        {feet.x,feet.y+ring,feet.z},{feet.x,feet.y-ring,feet.z}};
}
constexpr float rayStep = 8.0f; // advance past a hit before the next segment
// Next start point after a hit at `fraction` of from->to; nullopt when the
// remaining segment is too short or the inputs are not usable.
inline std::optional<Vec> rayContinue(Vec from,Vec to,float fraction) {
    const float length = span(from,to);
    if (!finite(from) || !finite(to) || !std::isfinite(fraction) || fraction < 0 || fraction > 1 || length <= rayStep) return {};
    const float next = fraction+rayStep/length;
    if (!(next < 1.0f)) return {};
    return mix(from,to,next);
}

// ---- restore table ---------------------------------------------------------
enum class Exit { disable, f8, firstPerson, menu, load, cell, unload, death, special, hookSilent, expired, underfoot, evicted, count };
constexpr std::array exitNames{"disable","f8","first-person","menu","load","cell","unload","death","special","hook-silent","expired",
    "underfoot","evicted"};
static_assert(exitNames.size() == static_cast<std::size_t>(Exit::count));
constexpr unsigned exitBit(Exit e) { return 1u << static_cast<unsigned>(e); }
// Lowest set request wins; the order above is the reporting priority.
constexpr Exit firstExit(unsigned requests) {
    for (unsigned i = 0;i < static_cast<unsigned>(Exit::count);++i)
        if (requests & (1u << i)) return static_cast<Exit>(i);
    return Exit::special;
}
// A hook that did not run this frame for no identified reason gets a short
// grace so one skipped camera update does not flash every hidden reference.
constexpr unsigned silentGraceFrames = 3;
constexpr bool restoreNow(Exit cause,unsigned silentFrames) {
    return cause != Exit::hookSilent || silentFrames >= silentGraceFrames;
}
// Only undo our own write: the root was visible before, and is still hidden.
constexpr bool restoreWrite(bool hiddenNow,bool hiddenBefore) { return hiddenNow && !hiddenBefore; }
struct HideCounters {
    unsigned long long scans{},visited{},rays{},rayHits{},rayFailed{},hides{},restores{},conflicts{},foreign{},visitCap{},rayCap{};
    std::array<unsigned long long,5> skips{};
    std::array<unsigned long long,static_cast<std::size_t>(Exit::count)> exits{};
};

// ---- trace -----------------------------------------------------------------
struct LineBudget {
    unsigned limit{},used{};
    bool take() { if (used >= limit) return false; ++used; return true; }
};
using TraceRow = std::array<char,768>;
// Single producer (the main thread); rows are immutable once published. Only
// the worker touches the file, so the frame never opens, writes or flushes.
class TraceWriter {
public:
    ~TraceWriter() { stop(); }
    bool start(const std::filesystem::path& path,unsigned rowCapacity) {
        if (worker.joinable() || !rowCapacity) return false;
        rows = std::make_unique<TraceRow[]>(rowCapacity);
        capacity = rowCapacity;
        worker = std::thread([this,path] {
            std::ofstream file(path,std::ios::trunc);
            unsigned written = 0;
            for (;;) {
                const bool done = stopping.load(std::memory_order_acquire);
                const auto available = published.load(std::memory_order_acquire);
                for (;written < available;++written) file << rows[written].data() << '\n';
                file.flush();
                if (done || written == capacity) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        return true;
    }
    bool append(const char* text) {
        const auto index = published.load(std::memory_order_relaxed);
        if (!rows || index >= capacity || !text) return false;
        std::snprintf(rows[index].data(),rows[index].size(),"%s",text);
        published.store(index+1,std::memory_order_release);
        return true;
    }
    void stop() {
        stopping.store(true,std::memory_order_release);
        if (worker.joinable()) worker.join();
    }
private:
    std::unique_ptr<TraceRow[]> rows;
    unsigned capacity{};
    std::atomic<unsigned> published{0};
    std::atomic<bool> stopping{false};
    std::thread worker;
};
} // namespace skycrpg::camera::fixed
