// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader_frame_policy.h. P5.5-02 wave 2 package
// "shader": the door void (iDoorVoid=1, off by default; research
// session-p55/research/load-doors.md, option A). A vanilla load door is a
// single-sided leaf whose faces point to the dungeon side; the fixed camera
// stands behind its plane in two of the four headings, where the leaf and its
// frame wall are culled and the stub behind the door (cut ceiling, missing
// outer faces, lights faking daylight) fills the doorway. The frame pass moves
// every surface behind a nearby load door's plane, in a lane as wide as the
// doorway and bounded in height, to the void colour, with a soft edge: the
// doorway leads into darkness from every heading.
//
// Which doors. The cell census (fixed_spike_shader_style.h noteCell, Stage 1's
// one walk of a newly entered cell) notes every Door reference whose model is
// in the generated table (fixed_spike_shader_door_table.h, tools/load_doors.py)
// and which carries ExtraTeleport (a load door; a door of the same model that
// is not one never gets a void), placed from its reference data alone. The
// invisible AutoLoadDoor markers are left out: their side cannot be read from
// the marker (research: 87 of 121 arrivals on local -y, 34 on +y) and the
// statics-majority rule is wrong for 6 of 125. Each drawn frame the frame pass
// takes up to maxDoorVoids of them, nearest first, within fDoorVoidRadius of
// the feet, skipping every door the player stands behind.
//
// The player is never voided: a door whose plane has the feet on its far side
// is skipped, and for the rest the void starts at least doorBodyClear behind
// the feet. No engine headers, no engine pointers.
#include "fixed_spike_shader_door_table.h"

namespace skycrpg::camera::fixed::shader {
// ---- the lane ---------------------------------------------------------------------------------
// Report, Q2: the lane is the leaf's width plus 32 each side (fDoorVoidSide); it starts 8 beyond what the table keeps
// (the leaf's front faces and its reveal); it reaches from 120 under the door's floor to 500 over it
// (the BFB entrance leaf, 497 tall, gets its height + 64). The player's body is never within
// doorBodyClear of the feet on the far side of the plane (the frame pass's guard radius).
constexpr std::size_t maxDoorVoids = 4, maxCellDoors = 16;
constexpr float doorLaneMargin = 32.0f, doorStartMargin = 8.0f, doorBelow = 120.0f, doorAbove = 500.0f, doorOverLeaf = 64.0f;
constexpr float doorBodyClear = 48.0f, doorTilt = .01f;
// The table, sorted by hash (the generator writes it so).
constexpr bool doorTableSorted() {
    for (std::size_t i = 1;i < loadDoorModels.size();++i) if (!(loadDoorModels[i-1].hash < loadDoorModels[i].hash)) return false;
    for (std::size_t i = 1;i < wrongFacingDoors.size();++i) if (!(wrongFacingDoors[i-1] < wrongFacingDoors[i])) return false;
    return true;
}
static_assert(doorTableSorted());
inline const DoorModel* doorModel(const char* path) {
    if (!path) return nullptr;
    const std::uint64_t hash = modelHash(path);
    const auto at = std::lower_bound(loadDoorModels.begin(),loadDoorModels.end(),hash,[](const DoorModel& m,std::uint64_t value) { return m.hash < value; });
    return at != loadDoorModels.end() && at->hash == hash ? &*at : nullptr;
}
inline bool wrongFacing(std::uint32_t formId) { return std::binary_search(wrongFacingDoors.begin(),wrongFacingDoors.end(),formId); }

// One door's void, in world units. `point` is on the plane at the lane's middle, at the door's floor;
// (nx, ny) the plane's unit normal towards the front (the dungeon side); "behind" is against it.
struct DoorPlane {
    Vec point{};
    float nx{},ny{};
    float half{};      // lane half width
    float start{};     // the void starts this far behind the plane
    float low{},high{}; // the lane's world heights
    bool valid{};
};
// The engine's rotation about z (local x -> (cos z, -sin z), local y -> (sin z, cos z)): the convention
// tools/load_doors.py checks against every vanilla arrival. A tilted reference (x or y angle) has no plane.
inline DoorPlane doorPlane(const DoorModel& m,Vec position,Vec angle,float scale,float side = doorLaneMargin) {
    DoorPlane d;
    if (!finite(position) || !finite(angle) || !std::isfinite(scale) || scale <= .01f || std::abs(angle.x) > doorTilt ||
        std::abs(angle.y) > doorTilt || (m.front != 1 && m.front != -1) || m.thin > 1u) return d;
    const float c = std::cos(angle.z),s = std::sin(angle.z);
    const Vec localX{c,-s,0.0f},localY{s,c,0.0f};
    const Vec thin = m.thin == 0 ? localX : localY,across = m.thin == 0 ? localY : localX;
    d.point = {position.x+scale*(thin.x*m.centre+across.x*m.lateral),position.y+scale*(thin.y*m.centre+across.y*m.lateral),position.z+scale*m.floor};
    d.nx = thin.x*static_cast<float>(m.front); d.ny = thin.y*static_cast<float>(m.front);
    d.half = scale*m.halfWidth+side;
    d.start = scale*m.keep+doorStartMargin;
    d.low = d.point.z-doorBelow;
    d.high = d.point.z+(std::max)(doorAbove,scale*m.height+doorOverLeaf);
    d.valid = finite(d.point) && std::isfinite(d.nx) && std::isfinite(d.ny);
    return d;
}
// Signed ground distance in front of the plane (negative: behind it), and the distance across the lane.
inline float doorSide(const DoorPlane& d,Vec p) { return (p.x-d.point.x)*d.nx+(p.y-d.point.y)*d.ny; }
inline float doorAcross(const DoorPlane& d,Vec p) { return std::abs((p.x-d.point.x)*d.ny-(p.y-d.point.y)*d.nx); }

// ---- the census ---------------------------------------------------------------------------------
enum class DoorNote : unsigned { added, notTable, noTeleport, busy, wrongFacing, tilted, disabled, full, count };
constexpr std::array doorNoteNames{"added","notTable","noTeleport","busy","wrongFacing","tilted","disabled","full"};
static_assert(doorNoteNames.size() == static_cast<std::size_t>(DoorNote::count));
struct CellDoor { std::uint32_t id{}; const DoorModel* model{}; DoorPlane plane{}; };
// ExtraTeleport on the reference: present (a load door), absent, or not read because a writer held the
// extra list's lock (the census never waits for it; that door gets no void in this cell).
enum class Teleport : unsigned { absent, present, busy };
struct DoorCensus {
    std::array<CellDoor,maxCellDoors> doors{};
    std::size_t count{};
    std::array<unsigned,static_cast<std::size_t>(DoorNote::count)> notes{};
    unsigned refs{}; // Door references seen
};
// One Door reference of the cell. `model` is doorModel(its path) (null: not in the table); `teleport`
// whether it carries ExtraTeleport (read only for a table model); `off` initially disabled or deleted.
inline DoorNote noteDoor(DoorCensus& c,std::uint32_t id,const DoorModel* model,Teleport teleport,bool off,Vec position,Vec angle,float scale,
    float side = doorLaneMargin) {
    ++c.refs;
    const DoorPlane plane = model ? doorPlane(*model,position,angle,scale,side) : DoorPlane{};
    DoorNote why = DoorNote::added;
    if (!model) why = DoorNote::notTable;
    else if (off) why = DoorNote::disabled;
    else if (teleport == Teleport::busy) why = DoorNote::busy;
    else if (teleport != Teleport::present) why = DoorNote::noTeleport;
    else if (wrongFacing(id)) why = DoorNote::wrongFacing;
    else if (!plane.valid) why = DoorNote::tilted;
    else if (c.count >= maxCellDoors) why = DoorNote::full;
    if (why == DoorNote::added) c.doors[c.count++] = {id,model,plane};
    ++c.notes[static_cast<std::size_t>(why)];
    return why;
}

// ---- the doors drawn this frame -------------------------------------------------------------------
struct DoorChoice {
    CellDoor door{};
    float distance{}; // from the feet to the plane's point
    float side{};     // the feet in front of the plane
    float start{};    // the void's start this frame: the door's, or further back so the player's body stays out
};
struct DoorSelection {
    std::uint32_t cell{};
    std::array<DoorChoice,maxDoorVoids> chosen{};
    std::size_t count{};
    unsigned behind{},distant{}; // doors left out: the player behind the plane, farther than the radius
    std::size_t doors{};         // the cell's doors considered
};
// The nearest `limit` (at most maxDoorVoids) doors within `radius` of the feet that the player is in front
// of. A door the player stands behind (or on the plane of, or any non-finite measure) is skipped.
inline DoorSelection selectDoors(const DoorCensus& c,Vec feet,float radius,std::size_t limit = maxDoorVoids) {
    DoorSelection out;
    out.doors = (std::min)(c.count,maxCellDoors);
    limit = (std::min)(limit,maxDoorVoids);
    for (std::size_t i = 0;i < out.doors;++i) {
        const CellDoor& door = c.doors[i];
        const float side = doorSide(door.plane,feet);
        if (!door.plane.valid || !(side > 0.0f)) { ++out.behind; continue; }
        const float distance = span(feet,door.plane.point);
        if (!(distance <= radius)) { ++out.distant; continue; }
        const DoorChoice choice{door,distance,side,(std::max)(door.plane.start,doorBodyClear-side)};
        std::size_t at = out.count;
        while (at > 0 && (choice.distance < out.chosen[at-1].distance ||
            (choice.distance == out.chosen[at-1].distance && choice.door.id < out.chosen[at-1].door.id))) --at;
        if (at >= limit) continue;
        const std::size_t last = (std::min)(out.count,limit-1);
        for (std::size_t j = last;j > at;--j) out.chosen[j] = out.chosen[j-1];
        out.chosen[at] = choice;
        if (out.count < limit) ++out.count;
    }
    return out;
}
// Same doors in the same order (the trace's change test; the starts move with the feet every frame).
inline bool sameDoors(const DoorSelection& a,const DoorSelection& b) {
    if (a.cell != b.cell || a.count != b.count) return false;
    for (std::size_t i = 0;i < a.count;++i) if (a.chosen[i].door.id != b.chosen[i].door.id) return false;
    return true;
}

// ---- trace ------------------------------------------------------------------------------------------
// FixedDoorVoid, written on change only: one row per door drawn (slot 0..active-1), or one row with
// active=0 when the last door leaves.
struct DoorTrace {
    double seconds{};
    unsigned long long frame{};
    const DoorSelection* selection{};
    std::size_t slot{};
};
inline bool formatDoorVoid(char* out,std::size_t size,const DoorTrace& t) {
    if (!t.selection) return false;
    const DoorSelection& s = *t.selection;
    const bool have = t.slot < s.count;
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const DoorChoice empty{};
    const DoorChoice& c = have ? s.chosen[t.slot] : empty;
    const DoorPlane& p = c.door.plane;
    const auto of = [&](float value) { return have ? static_cast<double>(value) : none; };
    char door[16] = "none";
    if (have) std::snprintf(door,sizeof(door),"%08X",c.door.id);
    const int written = std::snprintf(out,size,
        "FixedDoorVoid t=%.3f frame=%llu cell=%08X active=%zu slot=%zu door=%s model=%s plane=%.1f,%.1f,%.1f normal=%.3f,%.3f half=%.1f "
        "start=%.1f z=%.1f,%.1f dist=%.1f side=%.1f doors=%zu behind=%u distant=%u",
        t.seconds,t.frame,s.cell,s.count,have ? t.slot : static_cast<std::size_t>(0),door,have && c.door.model && c.door.model->path ? c.door.model->path : "none",
        of(p.point.x),of(p.point.y),of(p.point.z),of(p.nx),of(p.ny),of(p.half),of(c.start),of(p.low),of(p.high),of(c.distance),of(c.side),
        s.doors,s.behind,s.distant);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
// The census, once per cell (the log): the doors that can get a void, then why the other Door references
// cannot. At most maxCellDoors IDs fit.
inline bool formatDoorCensus(char* out,std::size_t size,std::uint32_t cell,const char* style,const DoorCensus& c) {
    const auto tally = [&](DoorNote which) { return c.notes[static_cast<std::size_t>(which)]; };
    int written = std::snprintf(out,size,"FixedCameraSpike shader door census cell=%08X style=%s doors=%zu refs=%u notTable=%u noTeleport=%u "
        "busy=%u wrongFacing=%u tilted=%u disabled=%u full=%u list=",cell,style ? style : "none",c.count,c.refs,tally(DoorNote::notTable),
        tally(DoorNote::noTeleport),tally(DoorNote::busy),tally(DoorNote::wrongFacing),tally(DoorNote::tilted),tally(DoorNote::disabled),tally(DoorNote::full));
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    std::size_t used = static_cast<std::size_t>(written);
    if (!c.count) {
        written = std::snprintf(out+used,size-used,"none");
        return written > 0 && used+static_cast<std::size_t>(written) < size;
    }
    for (std::size_t i = 0;i < c.count && i < maxCellDoors;++i) {
        written = std::snprintf(out+used,size-used,"%s%08X",i ? "," : "",c.doors[i].id);
        if (written <= 0 || used+static_cast<std::size_t>(written) >= size) return false;
        used += static_cast<std::size_t>(written);
    }
    return true;
}
} // namespace skycrpg::camera::fixed::shader
