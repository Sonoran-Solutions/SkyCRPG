// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader": portable checks of the door void (iDoorVoid). The generated table and its
// lookup, a door's plane and lane from its reference data (Embershard's and Bleak Falls Barrow's entrance doors,
// Skyrim.esm placements and recorded arrivals), the census notes, the per-frame choice (player side, radius,
// order, limit, the start pushed past the player's body), the constant rows and the C++ copy of the shader's
// door term, the settings, and the two trace lines.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <cstring>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::fixed::Vec;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool close(float a,float b,float tolerance) { return std::abs(a-b) <= tolerance; }
const DoorModel& model(const char* path) {
    const DoorModel* m = doorModel(path);
    assert(m);
    return *m;
}
// Skyrim.esm: Embershard Mine's entrance MineDoor01Load 000B6CCD and Bleak Falls Barrow's NorLargeDoor 00037603.
const Vec embershardDoor{2472.6f,1303.8f,9245.1f},embershardAngle{0.0f,0.0f,3.393f};
const Vec bfbDoor{-7537.2f,1018.5f,512.0f},bfbAngle{0.0f,0.0f,1.571f};

void table() {
    static_assert(doorTableSorted());
    assert(loadDoorModels.size() == 13 && wrongFacingDoors.size() == 1);
    const DoorModel& mine = model("dungeons\\mines\\minedoorload01.nif");
    assert(mine.thin == 1 && mine.front == -1 && close(mine.centre,-7.56f,.01f) && close(mine.keep,44.25f,.01f) && close(mine.halfWidth,167.78f,.01f));
    // The path as the engine may hand it: "meshes\" prefix, forward slashes, any case.
    assert(doorModel("Meshes\\Dungeons\\Mines\\MineDoorLoad01.nif") == &mine && doorModel("meshes/dungeons/mines/minedoorload01.nif") == &mine);
    const DoorModel& large = model("dungeons\\nordic\\doors\\animated\\largedoor\\ruins_largedoor01.nif");
    assert(large.thin == 1 && large.front == 1 && close(large.height,497.18f,.01f) && close(large.floor,61.36f,.01f));
    const DoorModel& nordic = model("dungeons\\nordic\\doors\\animated\\smdoor01\\nordoorsmload01.nif");
    assert(nordic.thin == 0 && nordic.front == -1);
    assert(close(model("dungeons\\imperial\\door\\impwooddoordoubleload01.nif").centre,265.64f,.01f)); // the plane 266 off the origin
    // Not load doors in a doorway, two-sided, or unknown: no entry.
    for (const char* path : {"dungeons\\riften\\ratwayhall\\riftenrwdoorload01.nif","dungeons\\ship\\shipdoor02.nif",
                             "dungeons\\imperial\\door\\trapdoorladder01.nif","markers\\autoloadmarker01.nif","architecture\\whiterun\\wrdoor01.nif",""})
        assert(!doorModel(path));
    assert(!doorModel(nullptr));
    assert(wrongFacing(0x00020320u) && !wrongFacing(0x000b6ccdu));
}

void plane() {
    const DoorModel& mine = model("dungeons\\mines\\minedoorload01.nif");
    const DoorPlane d = doorPlane(mine,embershardDoor,embershardAngle,1.0f);
    assert(d.valid);
    // Normal: local y turned by z (sin z, cos z), times the front sign -1.
    assert(close(d.nx,-std::sin(3.393f),1e-5f) && close(d.ny,-std::cos(3.393f),1e-5f) && d.ny > .96f && close(std::hypot(d.nx,d.ny),1.0f,1e-5f));
    assert(close(d.point.x,2480.55f,.05f) && close(d.point.y,1309.56f,.05f) && close(d.point.z,9245.1f-3.72f,.01f));
    assert(close(d.half,167.78f+32.0f,.01f) && close(d.start,44.25f+8.0f,.01f) && close(d.low,d.point.z-120.0f,.01f) && close(d.high,d.point.z+500.0f,.01f));
    // The recorded coc arrival and the XTEL arrival are in front of the plane (report: 175 and 55); the cliff stub behind it.
    assert(close(doorSide(d,{2487.3f,1485.5f,9241.0f}),172.1f,1.0f) && close(doorSide(d,{2478.7f,1366.0f,9253.7f}),54.2f,1.0f));
    assert(doorSide(d,{2400.0f,1000.0f,9245.0f}) < -200.0f);
    assert(close(doorAcross(d,d.point),0.0f,1e-3f) && close(doorAcross(d,{d.point.x+100.0f*d.ny,d.point.y-100.0f*d.nx,0.0f}),100.0f,.01f));
    // Bleak Falls Barrow's entrance: front +1 on local y, so the normal points +x at z = pi/2; the leaf is 497 tall.
    const DoorModel& large = model("dungeons\\nordic\\doors\\animated\\largedoor\\ruins_largedoor01.nif");
    const DoorPlane b = doorPlane(large,bfbDoor,bfbAngle,1.0f);
    assert(b.valid && close(b.nx,1.0f,1e-3f) && close(b.ny,0.0f,1e-3f));
    assert(close(b.point.z,573.36f,.01f) && close(b.high-b.point.z,497.18f+64.0f,.01f) && close(b.half,258.91f+32.0f,.01f));
    assert(close(doorSide(b,{-7411.0f,1082.0f,573.1f}),117.9f,1.0f) && doorSide(b,{-7329.4f,978.9f,573.1f}) > 190.0f);
    // The imperial double door's plane is 266 off its origin, scaled with the reference.
    const DoorModel& twin = model("dungeons\\imperial\\door\\impwooddoordoubleload01.nif");
    const DoorPlane t = doorPlane(twin,{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f},2.0f);
    assert(t.valid && close(t.point.y,2.0f*265.64f,.05f) && close(t.ny,-1.0f,1e-6f) && close(t.half,2.0f*142.65f+32.0f,.05f) && close(t.start,2.0f*26.12f+8.0f,.05f));
    // Tilted, non-finite or unscaled references have no plane.
    assert(!doorPlane(mine,embershardDoor,{.2f,0.0f,1.0f},1.0f).valid && !doorPlane(mine,embershardDoor,{0.0f,-.05f,1.0f},1.0f).valid);
    assert(!doorPlane(mine,{nan,0.0f,0.0f},embershardAngle,1.0f).valid && !doorPlane(mine,embershardDoor,embershardAngle,0.0f).valid);
    assert(doorPlane(mine,embershardDoor,{.005f,-.005f,3.393f},1.0f).valid);
}

void census() {
    const DoorModel& mine = model("dungeons\\mines\\minedoorload01.nif");
    DoorCensus c;
    assert(noteDoor(c,0x000b6ccdu,&mine,Teleport::present,false,embershardDoor,embershardAngle,1.0f) == DoorNote::added);
    assert(noteDoor(c,0x1u,nullptr,Teleport::present,false,embershardDoor,embershardAngle,1.0f) == DoorNote::notTable);
    assert(noteDoor(c,0x2u,&mine,Teleport::absent,false,embershardDoor,embershardAngle,1.0f) == DoorNote::noTeleport); // a door of the model that is no load door
    assert(noteDoor(c,0x00020320u,&mine,Teleport::present,false,embershardDoor,embershardAngle,1.0f) == DoorNote::wrongFacing);
    assert(noteDoor(c,0x3u,&mine,Teleport::present,false,embershardDoor,{0.3f,0.0f,0.0f},1.0f) == DoorNote::tilted);
    assert(noteDoor(c,0x5u,&mine,Teleport::busy,false,embershardDoor,embershardAngle,1.0f) == DoorNote::busy); // a writer held the list
    assert(noteDoor(c,0x4u,&mine,Teleport::present,true,embershardDoor,embershardAngle,1.0f) == DoorNote::disabled);
    assert(c.count == 1 && c.refs == 7 && c.doors[0].id == 0x000b6ccdu && c.doors[0].model == &mine);
    for (std::size_t i = 1;i < maxCellDoors;++i) assert(noteDoor(c,0x100u+static_cast<std::uint32_t>(i),&mine,Teleport::present,false,embershardDoor,embershardAngle,1.0f) == DoorNote::added);
    assert(noteDoor(c,0x999u,&mine,Teleport::present,false,embershardDoor,embershardAngle,1.0f) == DoorNote::full && c.count == maxCellDoors);
    const auto note = [&](DoorNote n) { return c.notes[static_cast<std::size_t>(n)]; };
    assert(note(DoorNote::added) == maxCellDoors && note(DoorNote::notTable) == 1 && note(DoorNote::noTeleport) == 1 && note(DoorNote::wrongFacing) == 1 &&
        note(DoorNote::tilted) == 1 && note(DoorNote::disabled) == 1 && note(DoorNote::full) == 1 && note(DoorNote::busy) == 1);
}

// A door at the origin facing +y (the dungeon is +y; behind is -y), lane half 100, start 20, floor 0.
CellDoor door(std::uint32_t id,float x,float y,float nx = 0.0f,float ny = 1.0f) {
    CellDoor d;
    d.id = id;
    d.plane = {{x,y,0.0f},nx,ny,100.0f,20.0f,-120.0f,500.0f,true};
    return d;
}
void selection() {
    DoorCensus c;
    c.doors[0] = door(10,0.0f,0.0f);
    c.doors[1] = door(11,600.0f,0.0f);
    c.doors[2] = door(12,-300.0f,0.0f);
    c.doors[3] = door(13,0.0f,-2000.0f);               // too far
    c.doors[4] = door(14,0.0f,-400.0f,0.0f,-1.0f);     // its front faces -y: the feet (y = 100) are behind it
    c.doors[5] = door(15,0.0f,50.0f,1.0f,0.0f);        // the feet on its plane: skipped
    c.doors[6] = door(16,900.0f,0.0f);
    c.doors[7] = door(17,-900.0f,0.0f);
    c.count = 8;
    const Vec feet{0.0f,100.0f,0.0f};
    DoorSelection s = selectDoors(c,feet,1500.0f);
    assert(s.count == 4 && s.doors == 8 && s.behind == 2 && s.distant == 1);
    assert(s.chosen[0].door.id == 10 && s.chosen[1].door.id == 12 && s.chosen[2].door.id == 11 && s.chosen[3].door.id == 16);
    assert(close(s.chosen[0].distance,100.0f,1e-3f) && close(s.chosen[0].side,100.0f,1e-3f) && close(s.chosen[0].start,20.0f,1e-6f));
    // A limit of 2 keeps the two nearest; ties go to the lower form ID.
    s = selectDoors(c,feet,1500.0f,2);
    assert(s.count == 2 && s.chosen[0].door.id == 10 && s.chosen[1].door.id == 12);
    DoorCensus tie;
    tie.doors[0] = door(31,-100.0f,0.0f); tie.doors[1] = door(30,100.0f,0.0f); tie.count = 2;
    s = selectDoors(tie,{0.0f,50.0f,0.0f},1500.0f,1);
    assert(s.count == 1 && s.chosen[0].door.id == 30);
    // The player close in front: the void starts at least 48 behind the feet, so the body never enters it.
    s = selectDoors(c,{0.0f,12.0f,0.0f},200.0f);
    assert(s.count == 1 && s.chosen[0].door.id == 10 && close(s.chosen[0].start,36.0f,1e-4f) && s.chosen[0].side-doorBodyClear >= -s.chosen[0].start-1e-3f);
    // The player behind the plane (just past it, or through an open doorway): no void for that door.
    s = selectDoors(c,{0.0f,-5.0f,0.0f},200.0f);
    assert(s.count == 0 && s.behind >= 1);
    s = selectDoors(c,{nan,0.0f,0.0f},1500.0f);
    assert(s.count == 0);
    // Radius: a smaller one drops the farther doors.
    s = selectDoors(c,feet,320.0f);
    assert(s.count == 2 && s.chosen[1].door.id == 12);
    // Change test: same IDs in the same order, same cell.
    DoorSelection a = selectDoors(c,feet,1500.0f),b = selectDoors(c,{0.0f,110.0f,0.0f},1500.0f);
    a.cell = b.cell = 7;
    assert(sameDoors(a,b));
    b.cell = 8; assert(!sameDoors(a,b));
    b = selectDoors(c,feet,320.0f); b.cell = 7; assert(!sameDoors(a,b));
    assert(sameDoors(DoorSelection{},DoorSelection{}));
}

// clip (ndc x, ndc y, depth, 1) -> world (x0 + 1000 ndc x, y0 + 1000 ndc y, depth * 1000 - 200), on a 200 x 200 view.
FrameConstants orthographic(Vec origin) {
    FrameConstants k;
    k.inverseColumn[0] = {1000.0f,0.0f,0.0f,origin.x};
    k.inverseColumn[1] = {0.0f,1000.0f,0.0f,origin.y};
    k.inverseColumn[2] = {0.0f,0.0f,1000.0f,-200.0f};
    k.inverseColumn[3] = {0.0f,0.0f,0.0f,1.0f};
    k.viewport = {0.0f,0.0f,2.0f/200.0f,2.0f/200.0f};
    k.depthRange = {0.0f,1.0f,voidDepth,0.0f};
    k.cut = {origin.x,origin.y,10000.0f,10000.0f};
    k.shape = {0.0f,500.0f,900.0f,0.0f};
    k.guard = {guardInner,guardOuter,0.0f,vignetteStart};
    k.voidColor = {0.0f,0.0f,0.0f,1.0f};
    return k;
}
// The pixel whose world position is (x, y, z) in the orthographic view above.
FramePixel at(const FrameConstants& k,Vec origin,float x,float y,float z) {
    const float px = ((x-origin.x)/1000.0f+1.0f)*100.0f,py = (1.0f-(y-origin.y)/1000.0f)*100.0f;
    return framePixel(k,px,py,(z+200.0f)/1000.0f);
}
void shading() {
    // One door: plane at (1000, 2000), front +y, at floor 0 (world); the position adjustment is (1000, 2000, 50).
    const Vec adjust{1000.0f,2000.0f,50.0f};
    DoorSelection s;
    s.count = 1;
    s.chosen[0].door = door(1,1000.0f,2000.0f);
    s.chosen[0].start = 20.0f;
    FrameConstants k = orthographic({0.0f,0.0f,0.0f});
    const FrameConstants before = k;
    applyDoors(k,s,adjust,48.0f,1024.0f);
    assert(k.doorShape[0] == 1.0f && k.doorShape[1] == 48.0f && k.doorShape[2] == 1024.0f && k.doorShape[3] == 0.0f);
    assert(k.door[0][0] == 0.0f && k.door[0][1] == 0.0f && k.door[0][2] == 0.0f && k.door[0][3] == 1.0f);
    assert(k.door[1][0] == -170.0f && k.door[1][1] == 450.0f && k.door[1][2] == 100.0f && k.door[1][3] == 20.0f);
    for (std::size_t i = 2;i < k.door.size();++i) assert((k.door[i] == std::array<float,4>{}));
    assert(std::memcmp(&k,&before,160) == 0); // the first ten rows are the frame pass's, untouched
    // World positions here are relative to the adjustment, as the shader sees them: the plane is at y = 0.
    const Vec o{0.0f,0.0f,0.0f};
    assert(close(at(k,o,0.0f,-200.0f,0.0f).amount,1.0f,1e-5f));   // deep behind, mid-lane, at the floor
    assert(close(at(k,o,0.0f,150.0f,0.0f).amount,0.0f,1e-6f));    // in front (the dungeon side)
    assert(close(at(k,o,0.0f,-10.0f,0.0f).amount,0.0f,1e-6f));    // behind but short of the start (the leaf)
    assert(close(at(k,o,0.0f,-44.0f,0.0f).amount,.5f,1e-3f));     // half way through the soft start
    assert(close(at(k,o,0.0f,-68.0f,0.0f).amount,1.0f,1e-5f));    // past start + edge
    assert(close(at(k,o,124.0f,-200.0f,0.0f).amount,.5f,1e-3f));  // half way out of the lane's side
    assert(close(at(k,o,160.0f,-200.0f,0.0f).amount,0.0f,1e-6f)); // beside the lane
    assert(close(at(k,o,0.0f,-200.0f,-250.0f).amount,0.0f,1e-6f)); // under the lane (a storey below)
    assert(close(at(k,o,0.0f,-200.0f,474.0f).amount,.5f,1e-3f));   // half way out of the top
    assert(close(at(k,o,0.0f,-1100.0f,0.0f).amount,0.0f,1e-6f));   // past the depth
    // The void itself is unchanged, and with no door drawn the pass is exactly as before.
    assert(at(k,o,0.0f,-200.0f,800.0f).voidPixel && at(k,o,0.0f,-200.0f,800.0f).amount == 1.0f);
    FrameConstants off = before;
    off.door = k.door; // rows left over, but no door drawn: unread
    for (float y = -1000.0f;y <= 1000.0f;y += 37.0f)
        for (float z = -150.0f;z <= 600.0f;z += 41.0f) {
            const FramePixel a = at(before,o,10.0f,y,z),b = at(off,o,10.0f,y,z);
            assert(a.amount == b.amount && a.voidPixel == b.voidPixel);
        }
    // The door term is a max with the band and falloff: never brighter than without it.
    FrameConstants fall = k;
    fall.shape = {0.0f,100.0f,300.0f,1.0f};
    FrameConstants fallOnly = before;
    fallOnly.shape = fall.shape;
    for (float y = -600.0f;y <= 600.0f;y += 50.0f) {
        const FramePixel pixel = at(fall,o,0.0f,y,0.0f);
        const float with = pixel.amount,without = at(fallOnly,o,0.0f,y,0.0f).amount;
        assert(with >= without && with >= doorFade(fall,pixel.world) && (y > -20.0f || y < -1072.0f || with > without || without == 1.0f));
    }
    // Two doors: the strongest fade wins.
    DoorSelection two = s;
    two.count = 2;
    two.chosen[1].door = door(2,1500.0f,2000.0f,0.0f,-1.0f); // facing -y: behind it is +y
    two.chosen[1].start = 20.0f;
    FrameConstants k2 = orthographic(o);
    applyDoors(k2,two,adjust,48.0f,1024.0f);
    assert(k2.doorShape[0] == 2.0f && close(doorFade(k2,{500.0f,200.0f,0.0f}),1.0f,1e-5f) && close(doorFade(k2,{0.0f,-200.0f,0.0f}),1.0f,1e-5f) &&
        close(doorFade(k2,{0.0f,200.0f,0.0f}),0.0f,1e-6f));
}

void settings() {
    bool adjusted = true;
    Config c = validate(Config{},&adjusted);
    assert(!adjusted && c.frame.doorVoid == 0u && c.frame.doorDepth == 900.0f && c.frame.doorEdge == 48.0f && c.frame.doorRadius == 1500.0f);
    c = {}; c.frame.doorVoid = 1u; c = validate(c,&adjusted);
    assert(!adjusted && c.frame.doorVoid == 1u);
    c = {}; c.frame.doorVoid = 1u; c.frame.doorDepth = 10.0f; c.frame.doorEdge = nan; c.frame.doorRadius = 1e9f; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorDepth == 128.0f && c.frame.doorEdge == 48.0f && c.frame.doorRadius == 8192.0f);
    c = {}; c.frame.doorVoid = 2u; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorVoid == 0u);
    // Off: the door keys are not read, so nothing is reported for them.
    c = {}; c.frame.doorDepth = nan; c.frame.doorEdge = -1.0f; c = validate(c,&adjusted);
    assert(!adjusted && std::isnan(c.frame.doorDepth) && c.frame.doorEdge == -1.0f);
    // It needs Stage 1's census (iCutStyle=1).
    c = {}; c.style = flatStyle; c.frame.doorVoid = 1u; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorVoid == 0u);
    // bFramePass=0: nothing of the frame pass is validated (the door key with it).
    c = {}; c.frame.enabled = false; c.frame.doorVoid = 5u; c = validate(c,&adjusted);
    assert(!adjusted && c.frame.doorVoid == 5u);
}

void trace() {
    DoorSelection s;
    s.cell = 0x000b6c0au; s.count = 2; s.doors = 3; s.behind = 1; s.distant = 0;
    const DoorModel& mine = model("dungeons\\mines\\minedoorload01.nif");
    s.chosen[0].door.id = 0x000b6ccdu; s.chosen[0].door.model = &mine;
    s.chosen[0].door.plane = doorPlane(mine,embershardDoor,embershardAngle,1.0f);
    s.chosen[0].distance = 175.25f; s.chosen[0].side = 172.5f; s.chosen[0].start = 52.25f;
    s.chosen[1].door.id = 0x000b6f24u;
    char line[sizeof(skycrpg::camera::fixed::TraceRow)]{};
    assert(formatDoorVoid(line,sizeof(line),{12.5,77,&s,0}));
    assert(std::string(line) == "FixedDoorVoid t=12.500 frame=77 cell=000B6C0A active=2 slot=0 door=000B6CCD model=dungeons\\mines\\minedoorload01.nif "
        "plane=2480.6,1309.6,9241.4 normal=0.249,0.969 half=199.8 start=52.2 z=9121.4,9741.4 dist=175.2 side=172.5 doors=3 behind=1 distant=0");
    // No door drawn (the last one left): one row with active=0.
    DoorSelection none;
    none.cell = 0x000b6c0au; none.doors = 3; none.behind = 3;
    assert(formatDoorVoid(line,sizeof(line),{13.0,80,&none,0}));
    assert(std::string(line) == "FixedDoorVoid t=13.000 frame=80 cell=000B6C0A active=0 slot=0 door=none model=none plane=nan,nan,nan normal=nan,nan "
        "half=nan start=nan z=nan,nan dist=nan side=nan doors=3 behind=3 distant=0");
    assert(!formatDoorVoid(line,sizeof(line),{0.0,0,nullptr,0}));
    // The longest row fits the trace row.
    DoorSelection wide = s;
    wide.cell = ~0u; wide.doors = ~std::size_t{0}; wide.behind = wide.distant = ~0u; wide.count = maxDoorVoids;
    const DoorModel* longest = &loadDoorModels[0];
    for (const auto& m : loadDoorModels) if (std::strlen(m.path) > std::strlen(longest->path)) longest = &m;
    for (auto& c : wide.chosen) {
        c.door.id = ~0u; c.door.model = longest;
        c.door.plane = {{-1e9f,-1e9f,-1e9f},-1.0f,-1.0f,-1e9f,-1e9f,-1e9f,-1e9f,true}; c.distance = c.side = c.start = -1e9f;
    }
    assert(formatDoorVoid(line,sizeof(line),{1e9,~0ull,&wide,maxDoorVoids-1}));
    std::printf("longest FixedDoorVoid row: %zu of %zu bytes\n",std::strlen(line),sizeof(line));
    // The census, once per cell.
    DoorCensus c;
    c.doors[0].id = 0x000b6ccdu; c.doors[1].id = 0x000b6f24u; c.count = 2; c.refs = 5;
    c.notes[static_cast<std::size_t>(DoorNote::added)] = 2; c.notes[static_cast<std::size_t>(DoorNote::notTable)] = 2;
    c.notes[static_cast<std::size_t>(DoorNote::noTeleport)] = 1;
    char log[512]{};
    assert(formatDoorCensus(log,sizeof(log),0x000b6c0au,"flat",c));
    assert(std::string(log) == "FixedCameraSpike shader door census cell=000B6C0A style=flat doors=2 refs=5 notTable=2 noTeleport=1 busy=0 wrongFacing=0 "
        "tilted=0 disabled=0 full=0 list=000B6CCD,000B6F24");
    DoorCensus empty;
    assert(formatDoorCensus(log,sizeof(log),0x1u,"kit",empty) && std::string(log).find(" doors=0 refs=0 ") != std::string::npos &&
        std::string(log).find(" list=none") != std::string::npos);
    DoorCensus full;
    full.count = maxCellDoors;
    for (auto& d : full.doors) d.id = ~0u;
    for (auto& n : full.notes) n = ~0u;
    full.refs = ~0u;
    assert(formatDoorCensus(log,sizeof(log),~0u,"unknown",full));
    std::printf("longest door census line: %zu of %zu bytes\n",std::strlen(log),sizeof(log));
    char small[64]{};
    assert(!formatDoorCensus(small,sizeof(small),~0u,"unknown",full));
}
} // namespace

int main() {
    table();
    plane();
    census();
    selection();
    shading();
    settings();
    trace();
    std::puts("fixed_spike_shader_door_policy_test PASS");
}
