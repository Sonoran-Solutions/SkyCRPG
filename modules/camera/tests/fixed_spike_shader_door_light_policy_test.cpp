// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader": portable checks of the door void's lights (iDoorVoidLights). Which placed lights
// are in a chosen door's lane (Embershard Mine's entrance door and the blue light behind it, Skyrim.esm placements),
// the census match, the save/restore book as a pure state machine (dim, keep, restore, lost, conflict, foreign,
// unplaced, other, full; never a write on an entry not seen this frame), a simulated engine over several frames
// (every exit puts the flags back exactly), the hold/off duty (the fill light's menu rule), the settings and the
// two lines.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <cstring>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::fixed::Vec;
using skycrpg::camera::fixed::Exit;
using skycrpg::camera::fixed::silentGraceFrames;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool close(float a,float b,float tolerance) { return std::abs(a-b) <= tolerance; }
// Skyrim.esm: Embershard Mine 000B6BE6, entrance MineDoor01Load 000B6CCD; behind it the placed light 0008F94C
// (DefaultSunlightBounceAmb01NS, LIGH flags 0x2001, no model); in front of it the torch light 000B6CBE.
const Vec embershardDoor{2472.6f,1303.8f,9245.1f},embershardAngle{0.0f,0.0f,3.393f};
const Vec blueLight{2251.1f,695.9f,9331.5f},frontTorch{2672.2f,1681.2f,9317.0f};
const Vec recordedArrival{2487.3f,1485.5f,9241.0f};
const DoorModel& mine() {
    const DoorModel* m = doorModel("dungeons\\mines\\minedoorload01.nif");
    assert(m);
    return *m;
}
DoorPlane embershard(float side = 320.0f) { return doorPlane(mine(),embershardDoor,embershardAngle,1.0f,side); }
DoorSelection chosenAt(Vec feet,float side = 320.0f) {
    DoorCensus c;
    assert(noteDoor(c,0x000b6ccdu,&mine(),Teleport::present,false,embershardDoor,embershardAngle,1.0f,side) == DoorNote::added);
    DoorSelection s = selectDoors(c,feet,1500.0f);
    s.cell = 0x000b6be6u;
    return s;
}
LightCensus placed() {
    LightCensus c;
    notePlacedLight(c,0x0008f94cu,false,blueLight);
    notePlacedLight(c,0x000b6cbeu,false,frontTorch);
    return c;
}
// Opaque entries and lights: addresses of these objects (never dereferenced by the policy).
std::array<int,64> entries{},lights{};
LightSeen seen(std::size_t i,Vec at,bool culled = false,bool plain = true) { return {&entries[i],&lights[i],at,904.0f,culled,plain}; }

void lane() {
    const DoorPlane d = embershard();
    assert(d.valid && close(d.nx,0.249f,.001f) && close(d.ny,0.969f,.001f) && close(d.half,487.8f,.1f));
    // The blue light: 652 behind the plane, 70 across, 90 over the door's floor.
    assert(close(-doorSide(d,blueLight),652.0f,1.0f) && close(doorAcross(d,blueLight),70.0f,1.0f) && close(blueLight.z-d.point.z,90.1f,.5f));
    assert(inLane(d,d.start,900.0f,blueLight));
    assert(!inLane(d,d.start,900.0f,frontTorch));                      // in front of the plane
    assert(!inLane(d,d.start,600.0f,blueLight));                       // deeper than fDoorVoidDepth
    assert(!inLane(d,700.0f,900.0f,blueLight));                        // the frame's start lies beyond it
    assert(!inLane(embershard(0.0f),d.start,900.0f,{blueLight.x-180.0f*0.969f,blueLight.y+180.0f*0.249f,blueLight.z})); // 250 across, half 168
    assert(!inLane(d,d.start,900.0f,{blueLight.x,blueLight.y,d.high+1.0f}) && !inLane(d,d.start,900.0f,{blueLight.x,blueLight.y,d.low-1.0f}));
    assert(!inLane(d,d.start,900.0f,{nan,0.0f,0.0f}) && !inLane(d,nan,900.0f,blueLight) && !inLane(DoorPlane{},0.0f,900.0f,blueLight));
    // Just past the start and just short of the depth are in; just short of the start and past the depth are not.
    const auto behindBy = [&](float back) { return Vec{d.point.x-d.nx*back,d.point.y-d.ny*back,d.point.z}; };
    assert(inLane(d,d.start,900.0f,behindBy(d.start+.5f)) && !inLane(d,d.start,900.0f,behindBy(d.start-.5f)));
    assert(inLane(d,d.start,900.0f,behindBy(899.5f)) && !inLane(d,d.start,900.0f,behindBy(900.5f)));
    // The frame's choice from the recorded arrival: the door is chosen, the light is in its lane.
    const DoorSelection s = chosenAt(recordedArrival);
    assert(s.count == 1 && s.chosen[0].door.id == 0x000b6ccdu && laneOf(s,900.0f,blueLight) == 0 && laneOf(s,900.0f,frontTorch) == -1);
    // Standing behind the plane: no door chosen, nothing in a lane.
    const DoorSelection behind = chosenAt({2400.0f,1000.0f,9245.0f});
    assert(behind.count == 0 && laneOf(behind,900.0f,blueLight) == -1);
}

void census() {
    LightCensus c;
    notePlacedLight(c,1u,false,blueLight);
    notePlacedLight(c,2u,true,frontTorch);                 // initially disabled or deleted: not placed
    notePlacedLight(c,3u,false,{nan,0.0f,0.0f});
    assert(c.count == 1 && c.refs == 3 && c.off == 2 && c.full == 0);
    assert(placedAt(c,blueLight) && placedAt(c,blueLight)->id == 1u);
    assert(placedAt(c,{blueLight.x+7.9f,blueLight.y,blueLight.z}) && !placedAt(c,{blueLight.x+8.1f,blueLight.y,blueLight.z}));
    assert(!placedAt(c,frontTorch) && !placedAt(c,{nan,nan,nan}));
    LightCensus full;
    for (std::uint32_t i = 0;i < maxCellLights+3;++i) notePlacedLight(full,i,false,{static_cast<float>(i)*100.0f,0.0f,0.0f});
    assert(full.count == maxCellLights && full.full == 3 && full.refs == maxCellLights+3);
}

void plan() {
    const DoorSelection s = chosenAt(recordedArrival);
    const LightCensus c = placed();
    LightBook book;
    // 1. Dim: the blue light, lit, plain, placed, in the lane. The torch in front is not read into the list natively,
    //    but even if it were it is outside every lane.
    std::array<LightSeen,4> one{seen(0,blueLight),seen(1,frontTorch)};
    LightPlan p = planLights(book,one.data(),2,true,s,900.0f,c);
    assert(p.count == 1 && p.actions[0].act == LightAct::dim && p.actions[0].seen == 0 && p.counts.dimmed == 1);
    assert(p.book.count == 1 && p.book.held[0].entry == &entries[0] && p.book.held[0].light == &lights[0] && p.book.held[0].door == 0x000b6ccdu &&
        p.book.held[0].ref == 0x0008f94cu && close(p.book.held[0].radius,904.0f,0.0f));
    book = p.book;
    // 2. Keep: the next frame reads our bit set; nothing is written.
    one[0].culled = true;
    p = planLights(book,one.data(),2,true,s,900.0f,c);
    assert(p.count == 0 && p.book.count == 1 && !p.counts.any());
    // 3. The door is no longer chosen (the player went behind its plane): restore.
    const DoorSelection none = chosenAt({2400.0f,1000.0f,9245.0f});
    p = planLights(book,one.data(),2,true,none,900.0f,c);
    assert(p.count == 1 && p.actions[0].act == LightAct::restore && p.actions[0].seen == 0 && p.counts.restored == 1 && p.book.count == 0);
    // 4. Off (any exit): restore, never a new dim.
    std::array<LightSeen,2> fresh{seen(0,blueLight,true),seen(2,blueLight)};
    p = planLights(book,fresh.data(),2,false,s,900.0f,c);
    assert(p.count == 1 && p.actions[0].act == LightAct::restore && p.book.count == 0 && p.book.leftCount == 0);
    // 5. Lost: the held entry is gone from the list (or holds another NiLight): dropped, nothing written.
    p = planLights(book,nullptr,0,true,s,900.0f,c);
    assert(p.count == 0 && p.counts.lost == 1 && p.book.count == 0);
    std::array<LightSeen,1> other{LightSeen{&entries[0],&lights[9],blueLight,904.0f,true,true}};
    p = planLights(book,other.data(),1,false,s,900.0f,c);
    assert(p.count == 0 && p.counts.lost == 1 && p.book.count == 0);
    // ... and a lost entry that is still listed is not put out again in the same frame.
    p = planLights(book,other.data(),1,true,s,900.0f,c);
    assert(p.count == 0 && p.counts.lost == 1 && p.book.count == 0);
    // 6. Conflict while wanted: another writer cleared our bit. Left lit, never put out again while wanted.
    one[0].culled = false;
    p = planLights(book,one.data(),2,true,s,900.0f,c);
    assert(p.count == 0 && p.counts.conflict == 1 && p.book.count == 0 && p.book.leftCount == 1 && p.book.left[0] == &entries[0]);
    LightBook left = p.book;
    p = planLights(left,one.data(),2,true,s,900.0f,c);
    assert(p.count == 0 && p.book.count == 0 && p.book.leftCount == 1);
    // ... until the book is emptied by an off frame; then it is put out again.
    p = planLights(left,one.data(),2,false,s,900.0f,c);
    assert(p.count == 0 && p.book.leftCount == 0);
    p = planLights(p.book,one.data(),2,true,s,900.0f,c);
    assert(p.count == 1 && p.actions[0].act == LightAct::dim);
    // 7. Conflict at restore: the bit is already clear; nothing written.
    p = planLights(book,one.data(),2,false,s,900.0f,c);
    assert(p.count == 0 && p.counts.conflict == 1 && p.book.count == 0);
    // 8. Foreign: culled by someone else; unplaced: no placed reference there (an actor's torch, a magic light);
    //    other: an entry of another class; none is written.
    const Vec carried{blueLight.x+9.0f,blueLight.y,blueLight.z};
    std::array<LightSeen,3> mixed{seen(3,blueLight,true),seen(4,carried),seen(5,blueLight,false,false)};
    p = planLights({},mixed.data(),3,true,s,900.0f,c);
    assert(p.count == 0 && p.counts.foreign == 1 && p.counts.unplaced == 1 && p.counts.other == 1 && p.book.count == 0);
    // 9. Not wanted: nothing is ever put out.
    std::array<LightSeen,1> lit{seen(0,blueLight)};
    p = planLights({},lit.data(),1,false,s,900.0f,c);
    assert(p.count == 0 && p.book.count == 0);
    // 10. Duplicates in the list are read once; null entries are skipped.
    std::array<LightSeen,3> twice{seen(0,blueLight),seen(0,blueLight),LightSeen{}};
    p = planLights({},twice.data(),3,true,s,900.0f,c);
    assert(p.count == 1 && p.book.count == 1);
    // 11. Full: at most maxDoorLights held.
    LightCensus many;
    std::array<LightSeen,maxSeenLights> crowd{};
    const DoorPlane d = s.chosen[0].door.plane;
    for (std::size_t i = 0;i < maxDoorLights+3;++i) {
        const Vec at{d.point.x-d.nx*(100.0f+20.0f*static_cast<float>(i)),d.point.y-d.ny*(100.0f+20.0f*static_cast<float>(i)),d.point.z+50.0f};
        notePlacedLight(many,0x100u+static_cast<std::uint32_t>(i),false,at);
        crowd[i] = seen(i,at);
    }
    p = planLights({},crowd.data(),maxDoorLights+3,true,s,900.0f,many);
    assert(p.count == maxDoorLights && p.book.count == maxDoorLights && p.counts.full == 3);
    for (std::size_t i = 0;i < p.count;++i) assert(p.actions[i].seen < maxDoorLights+3);
    // 12. A list longer than maxSeenLights is cut there.
    p = planLights({},crowd.data(),maxSeenLights+100,true,s,900.0f,many);
    assert(p.count == maxDoorLights);
}

// The engine side as the native code applies a plan: bit 0 of each light's flags set (dim) or cleared (restore).
struct FakeScene {
    std::array<std::uint32_t,8> flags{};
    std::array<Vec,8> at{};
    std::array<bool,8> listed{};
    std::size_t read(std::array<LightSeen,8>& out) const {
        std::size_t n = 0;
        for (std::size_t i = 0;i < flags.size();++i)
            if (listed[i]) out[n++] = {&entries[i],&lights[i],at[i],300.0f,(flags[i] & 1u) != 0u,true};
        return n;
    }
    void apply(const LightPlan& p,const std::array<LightSeen,8>& seenList) {
        for (std::size_t i = 0;i < p.count;++i) {
            const auto* light = static_cast<const int*>(seenList[p.actions[i].seen].light);
            const auto k = static_cast<std::size_t>(light-lights.data());
            flags[k] = p.actions[i].act == LightAct::dim ? (flags[k] | 1u) : (flags[k] & ~1u);
        }
    }
};
LightBook frame(FakeScene& scene,const LightBook& book,bool want,const DoorSelection& s,const LightCensus& c) {
    std::array<LightSeen,8> list{};
    const std::size_t n = scene.read(list);
    const LightPlan p = planLights(book,list.data(),n,want,s,900.0f,c);
    scene.apply(p,list);
    return p.book;
}
void simulate() {
    const DoorSelection s = chosenAt(recordedArrival);
    const LightCensus c = placed();
    FakeScene scene;
    scene.at = {blueLight,frontTorch,blueLight,{blueLight.x+40.0f,blueLight.y,blueLight.z}};
    scene.flags = {0x00200000u,0x0u,0x1u,0x0u}; // 2: already culled by someone else; 3: in the lane, not placed
    scene.listed = {true,true,true,true};
    const auto original = scene.flags;
    // Exits: off frame, door unchosen, cell (off), load (off), first person (off). Every one puts the flags back.
    for (int exit = 0;exit < 3;++exit) {
        LightBook book = frame(scene,{},true,s,c);
        assert(book.count == 1 && scene.flags[0] == (original[0] | 1u) && scene.flags[1] == original[1] && scene.flags[2] == original[2] &&
            scene.flags[3] == original[3]);
        book = frame(scene,book,true,s,c);             // kept
        assert(book.count == 1 && scene.flags[0] == (original[0] | 1u));
        // (a hold frame: nothing is read or written)
        if (exit == 0) book = frame(scene,book,false,s,c);                                   // off
        else if (exit == 1) book = frame(scene,book,true,chosenAt({2400.0f,1000.0f,9245.0f}),c); // unchosen
        else book = frame(scene,book,true,DoorSelection{},c);                                // no door in a non-flat cell
        assert(book.count == 0 && scene.flags == original);
    }
    // The light leaves the active list while held: dropped, the flags of what is still listed are untouched.
    LightBook book = frame(scene,{},true,s,c);
    scene.listed[0] = false;
    book = frame(scene,book,false,s,c);
    assert(book.count == 0 && scene.flags[0] == (original[0] | 1u));
}

void duty() {
    using enum Exit;
    assert(lightDuty(true,false,special,false,false) == LightDuty::keep);
    assert(lightDuty(true,true,load,false,true) == LightDuty::keep);
    // The Loading Menu never holds.
    assert(lightDuty(false,true,menu,true,true) == LightDuty::off && lightDuty(false,false,hookSilent,true,true) == LightDuty::off);
    // Menus, the grace and a paused hook-silent stretch hold.
    assert(lightDuty(false,true,menu,false,false) == LightDuty::hold);
    assert(lightDuty(false,false,hookSilent,false,false) == LightDuty::hold);
    assert(lightDuty(false,true,hookSilent,true,false) == LightDuty::hold);
    assert(lightDuty(false,true,hookSilent,false,false) == LightDuty::off);
    for (Exit e : {disable,f8,firstPerson,load,cell,unload,death,special,expired,underfoot,evicted})
        assert(lightDuty(false,true,e,true,false) == LightDuty::off);
    // A hold never survives a cell change or a load.
    assert(settleLightDuty(LightDuty::hold,true,false) == LightDuty::off && settleLightDuty(LightDuty::hold,false,true) == LightDuty::off);
    assert(settleLightDuty(LightDuty::hold,false,false) == LightDuty::hold && settleLightDuty(LightDuty::keep,true,true) == LightDuty::keep);
    assert(std::string(lightDutyName(LightDuty::hold)) == "hold" && std::string(lightDutyName(LightDuty::off)) == "off");
    // The fill light's SilenceHold, step by step.
    LightSilence s;
    assert(!s.step(false,false,1.0));                 // no pausing menu first: not held
    assert(s.step(true,false,2.0));                   // paused
    assert(s.step(false,true,2.4));                   // the Tween state easing back within 0.5 s
    assert(s.step(false,false,2.45));                 // then the grace
    for (unsigned i = 1;i < silentGraceFrames;++i) s.step(false,false,2.5);
    assert(!s.step(false,false,2.6));
    LightSilence t;
    assert(t.step(true,false,1.0));
    for (unsigned i = 1;i < silentGraceFrames;++i) assert(t.step(false,true,1.6)); // the Tween state past 0.5 s: only the grace
    assert(!t.step(false,true,1.6));
    LightSilence u;
    assert(u.step(true,false,static_cast<double>(nan)) && u.pausedAt < 0.0); // a NaN clock: no Tween rule, only the grace
    for (unsigned i = 1;i < silentGraceFrames;++i) assert(u.step(false,true,1.0));
    assert(!u.step(false,true,1.0));
    s.reset();
    assert(s.pausedAt < 0.0 && s.sinceHeld == silentGraceFrames);
}

void settings() {
    bool adjusted = true;
    Config c = validate(Config{},&adjusted);
    assert(!adjusted && c.frame.doorLights == 0u);
    c = {}; c.frame.doorVoid = 1u; c.frame.doorLights = 1u; c = validate(c,&adjusted);
    assert(!adjusted && c.frame.doorLights == 1u);
    c = {}; c.frame.doorVoid = 1u; c.frame.doorLights = 2u; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorLights == 0u);
    adjusted = false; c = {}; c.frame.doorLights = 1u; c = validate(c,&adjusted); // without the door void
    assert(adjusted && c.frame.doorLights == 0u);
    adjusted = false; c = {}; c.style = flatStyle; c.frame.doorVoid = 1u; c.frame.doorLights = 1u; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorVoid == 0u && c.frame.doorLights == 0u);
    adjusted = false; c = {}; c.frame.enabled = false; c.frame.doorLights = 1u; c = validate(c,&adjusted); // bFramePass=0: not validated
    assert(!adjusted);
    static_assert(lastRefusal == 109 && static_cast<unsigned>(Refusal::doorLightSite) == 106 && static_cast<unsigned>(Refusal::doorLightConflict) == 109);
}

void trace() {
    LightBook b;
    b.held[0] = {&entries[0],&lights[0],0x000b6ccdu,0x0008f94cu,blueLight,904.0f};
    b.count = 1;
    LightTrace t;
    t.seconds = 12.5; t.frame = 77; t.cell = 0x000b6be6u; t.duty = "keep"; t.cause = "eligible"; t.doors = 1; t.book = &b;
    t.counts.dimmed = 1; t.counts.unplaced = 2; t.dims = 3; t.restores = 2; t.lost = 0;
    char line[sizeof(skycrpg::camera::fixed::TraceRow)]{};
    assert(formatDoorLight(line,sizeof(line),t));
    assert(std::string(line) == "FixedDoorLight t=12.500 frame=77 cell=000B6BE6 duty=keep cause=eligible doors=1 held=1 dimmed=1 restored=0 lost=0 "
        "conflict=0 foreign=0 unplaced=2 shadow=0 other=0 full=0 total=3,2,0 lights=000B6CCD:0008F94C@2251,696,9332/904");
    LightBook empty;
    t.book = &empty; t.duty = "off"; t.cause = "first-person"; t.doors = 0; t.counts = {}; t.counts.restored = 1;
    assert(formatDoorLight(line,sizeof(line),t));
    assert(std::string(line) == "FixedDoorLight t=12.500 frame=77 cell=000B6BE6 duty=off cause=first-person doors=0 held=0 dimmed=0 restored=1 lost=0 "
        "conflict=0 foreign=0 unplaced=0 shadow=0 other=0 full=0 total=3,2,0 lights=none");
    assert(!formatDoorLight(line,sizeof(line),LightTrace{}));
    // The longest row fits the trace row: every count at its widest, every held light listed, the rest as more=N.
    LightBook wide;
    for (auto& h : wide.held) h = {&entries[0],&lights[0],~0u,~0u,{-1e9f,-1e9f,-1e9f},-1e9f};
    wide.count = maxDoorLights;
    LightTrace w;
    w.seconds = 1e9; w.frame = ~0ull; w.cell = ~0u; w.duty = "keep"; w.cause = "loading-menu"; w.doors = maxDoorVoids; w.book = &wide;
    w.counts = {~0u,~0u,~0u,~0u,~0u,~0u,~0u,~0u,~0u}; w.dims = w.restores = w.lost = ~0ull;
    assert(formatDoorLight(line,sizeof(line),w));
    assert(std::string(line).find(" more=10") != std::string::npos);
    std::printf("longest FixedDoorLight row: %zu of %zu bytes\n",std::strlen(line),sizeof(line));
    char small[64]{};
    assert(!formatDoorLight(small,sizeof(small),w));
    // The change test.
    LightBook a = b,c = b;
    assert(sameHeld(a,c));
    c.held[0].door = 1u;
    assert(!sameHeld(a,c) && !sameHeld(a,empty));
    // The census line, once per cell: Embershard's door and the blue light 651.5 behind its plane (the report: 651).
    DoorCensus doors;
    noteDoor(doors,0x000b6ccdu,&mine(),Teleport::present,false,embershardDoor,embershardAngle,1.0f,320.0f);
    const LightCensus lightsHere = placed();
    char log[768]{};
    assert(formatDoorLightCensus(log,sizeof(log),0x000b6be6u,doors,lightsHere,900.0f));
    assert(std::string(log) == "FixedCameraSpike shader door lights cell=000B6BE6 placed=2 refs=2 off=0 full=0 doors=1 lane=1 list=0008F94C@000B6CCD/651.5");
    assert(formatDoorLightCensus(log,sizeof(log),0x000b6be6u,doors,lightsHere,600.0f) && std::string(log).find(" lane=0 list=none") != std::string::npos);
    // The widest census line fits.
    DoorCensus manyDoors;
    for (std::uint32_t i = 0;i < maxCellDoors;++i)
        noteDoor(manyDoors,0xfff00000u+i,&mine(),Teleport::present,false,{embershardDoor.x,embershardDoor.y+2000.0f*static_cast<float>(i),embershardDoor.z},
            embershardAngle,1.0f,320.0f);
    LightCensus crowd;
    for (std::uint32_t i = 0;i < maxCellLights;++i) {
        const DoorPlane& d = manyDoors.doors[i % maxCellDoors].plane;
        notePlacedLight(crowd,0xfff00000u+i,false,{d.point.x-d.nx*(100.0f+static_cast<float>(i)),d.point.y-d.ny*(100.0f+static_cast<float>(i)),d.point.z});
    }
    crowd.refs = crowd.off = crowd.full = ~0u;
    assert(formatDoorLightCensus(log,sizeof(log),~0u,manyDoors,crowd,900.0f));
    std::printf("longest door light census line: %zu of %zu bytes\n",std::strlen(log),sizeof(log));
    assert(!formatDoorLightCensus(small,sizeof(small),~0u,manyDoors,crowd,900.0f));
}
} // namespace

int main() {
    lane();
    census();
    plan();
    simulate();
    duty();
    settings();
    trace();
    std::puts("fixed_spike_shader_door_light_policy_test PASS");
}
