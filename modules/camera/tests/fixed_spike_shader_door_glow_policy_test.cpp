// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader": portable checks of the door void's glow cards (iDoorVoidGlow). Which models are glow
// cards, the census, the place of a card at a door (Embershard Mine's entrance door and the two glow spheres behind it,
// Skyrim.esm placements), the cards hidden for this frame's doors (the frame pass's selectDoors), the change test, the
// settings and the two lines.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <cstring>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::fixed::Vec;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool close(float a,float b,float tolerance) { return std::abs(a-b) <= tolerance; }
bool has(const char* line,const char* text) { return std::string(line).find(text) != std::string::npos; }
// Skyrim.esm: Embershard Mine 000B6BE6, entrance MineDoor01Load 000B6CCD; behind its plane the two FXGlowFillRoundBrt
// 0008F94E and 0008F94D (effects\ambient\fxglowfillrndlitbrt.nif, OBND +-258, scale 1); at the forest end the glow
// 0008FB0D beside the AutoLoadDoor marker (no void); far off the beam 000B6EFB (fxambbeamxbdust_dim04.nif, scale 1.5).
const Vec embershardDoor{2472.6f,1303.8f,9245.1f},embershardAngle{0.0f,0.0f,3.393f};
const Vec glowNear{2448.6201f,1197.0934f,9392.4922f},glowDeep{2196.2837f,858.9473f,9422.5508f};
const Vec glowForest{-6089.0713f,1423.9038f,9021.7178f},beamFar{-3552.3066f,2226.6355f,9391.332f};
const Vec recordedArrival{2487.3f,1485.5f,9241.0f};
const char* const glowBrt = "effects\\ambient\\fxglowfillrndlitbrt.nif";
const DoorModel& mine() {
    const DoorModel* m = doorModel("dungeons\\mines\\minedoorload01.nif");
    assert(m);
    return *m;
}
DoorPlane embershard(float side = 320.0f) { return doorPlane(mine(),embershardDoor,embershardAngle,1.0f,side); }
DoorCensus doorsHere(float side = 320.0f) {
    DoorCensus c;
    assert(noteDoor(c,0x000b6ccdu,&mine(),Teleport::present,false,embershardDoor,embershardAngle,1.0f,side) == DoorNote::added);
    return c;
}
DoorSelection chosenAt(Vec feet,float side = 320.0f) {
    DoorSelection s = selectDoors(doorsHere(side),feet,1500.0f);
    s.cell = 0x000b6be6u;
    return s;
}
GlowCensus cardsHere() {
    GlowCensus c;
    const float reach = glowReach({-258.0f,-258.0f,-258.0f},{258.0f,258.0f,258.0f},1.0f);
    assert(noteGlowCard(c,0x0008f94eu,glowBrt,false,glowNear,reach) == GlowNote::added);
    assert(noteGlowCard(c,0x0008f94du,glowBrt,false,glowDeep,reach) == GlowNote::added);
    assert(noteGlowCard(c,0x0008fb0du,glowBrt,false,glowForest,reach) == GlowNote::added);
    assert(noteGlowCard(c,0x000b6efbu,"effects\\ambient\\fxambbeamxbdust_dim04.nif",false,beamFar,
        glowReach({-421.0f,-421.0f,-641.0f},{421.0f,421.0f,200.0f},1.5f)) == GlowNote::added);
    // Mist is the dungeon's atmosphere, not a card: never noted (not even counted).
    assert(noteGlowCard(c,0x0008fb47u,"effects\\ambient\\fxmistlow01.nif",false,{2837.0f,2372.7f,9152.7f},464.0f) == GlowNote::notModel);
    return c;
}
void models() {
    for (const char* path : {glowBrt,"effects\\ambient\\fxglowfillrndlitdim.nif","effects\\ambient\\fxglowfillrndlitmid.nif",
             "effects\\ambient\\fxglowfillrndlitxbrt.nif","effects\\ambient\\fxglowfillrndlitxdim.nif","effects\\ambient\\fxglowflatrndbrt.nif",
             "effects\\ambient\\fxambbeamdust02.nif","effects\\ambient\\fxambbeamslowfogbig_dim02.nif","effects\\ambient\\fxambbeamxbdustbig01.nif",
             "effects\\fx_rainandmistbeam.nif","effects\\fx_rainandmistbeam_fast.nif","effects\\fx_rainandmistbeamnoglow.nif",
             "Meshes\\Effects\\Ambient\\FXGlowFillRndLitBrt.nif","effects/ambient/FXAmbBeamDust00.nif"})
        assert(glowModel(path));
    for (const char* path : {"effects\\ambient\\fxmistlow01.nif","effects\\ambient\\fxmistlow01adjust.nif","effects\\fxfog01.nif",
             "effects\\ambient\\fxsmokewispslgvol01.nif","effects\\fxcobwebcorner01.nif","effects\\fxfirewithembers01.nif",
             "effects\\fxda09meridiaswordglow.nif","effects\\ambient\\fxambgentlyfallingsnow00.nif",
             "dungeons\\caves\\fxglowfillrndlitbrt.nif",            // not under effects\: never a card
             "architecture\\windhelm\\effects\\whfxwindowglow01.nif", // effects is not the first folder
             "effects\\ambient\\fxglowfil.nif","effects\\","","fxglowfillrndlitbrt.nif"})
        assert(!glowModel(path));
    assert(!glowModel(nullptr) && modelFile(nullptr) == nullptr && std::strcmp(modelFile("a\\b/c.nif"),"c.nif") == 0);
    // Reach: the largest OBND coordinate times the scale.
    assert(close(glowReach({-258.0f,-258.0f,-258.0f},{258.0f,258.0f,258.0f},1.0f),258.0f,.01f));
    assert(close(glowReach({-421.0f,-421.0f,-641.0f},{421.0f,421.0f,200.0f},1.5f),961.5f,.01f));
    assert(glowReach({nan,0.0f,0.0f},{1.0f,1.0f,1.0f},1.0f) == 0.0f && glowReach({0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f},0.0f) == 0.0f &&
        glowReach({0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f},nan) == 0.0f);
}
void census() {
    GlowCensus c = cardsHere();
    assert(c.count == 4 && c.refs == 4 && c.off == 0 && c.full == 0);
    assert(std::strcmp(c.cards[0].name.data(),"fxglowfillrndlitbrt.nif") == 0 && c.cards[0].id == 0x0008f94eu && close(c.cards[0].reach,258.0f,.01f));
    assert(std::strcmp(c.cards[3].name.data(),"fxambbeamxbdust_dim04.nif") == 0);
    // Initially disabled or deleted, an unplaced position or a bad reach: counted, never noted.
    assert(noteGlowCard(c,1u,glowBrt,true,glowNear,258.0f) == GlowNote::off);
    assert(noteGlowCard(c,2u,glowBrt,false,{nan,0.0f,0.0f},258.0f) == GlowNote::off);
    assert(noteGlowCard(c,3u,glowBrt,false,glowNear,nan) == GlowNote::off && noteGlowCard(c,4u,glowBrt,false,glowNear,-1.0f) == GlowNote::off);
    assert(c.count == 4 && c.refs == 8 && c.off == 4);
    // The table is never overrun; the rest is counted.
    GlowCensus full;
    for (std::uint32_t i = 0;i < maxCellGlows+3;++i) static_cast<void>(noteGlowCard(full,i+1u,glowBrt,false,glowNear,258.0f));
    assert(full.count == maxCellGlows && full.full == 3 && full.refs == maxCellGlows+3);
    // A name longer than the field is cut, still terminated, lower case.
    GlowCensus longName;
    const std::string path = "effects\\FXGlowFill" + std::string(80,'X') + ".nif";
    assert(noteGlowCard(longName,9u,path.c_str(),false,glowNear,258.0f) == GlowNote::added);
    assert(std::strlen(longName.cards[0].name.data()) == glowNameSize-1 && longName.cards[0].name[0] == 'f' && longName.cards[0].name[10] == 'x');
}
void place() {
    const DoorPlane d = embershard();
    assert(d.valid && close(d.half,487.8f,.1f));
    // Both Embershard spheres lie behind the plane, in the lane: 117 and 507 behind, 3 and 163 across, 151 and 181 over its floor.
    assert(close(-doorSide(d,glowNear),117.0f,1.0f) && close(doorAcross(d,glowNear),3.0f,1.0f) && close(glowNear.z-d.point.z,151.0f,1.0f));
    assert(close(-doorSide(d,glowDeep),507.0f,1.0f) && close(doorAcross(d,glowDeep),163.0f,1.0f) && close(glowDeep.z-d.point.z,181.0f,1.0f));
    for (unsigned mode : {glowBehind,glowFront}) {
        assert(glowPlace(d,900.0f,mode,glowNear,258.0f) == GlowPlace::behind && glowPlace(d,900.0f,mode,glowDeep,258.0f) == GlowPlace::behind);
        assert(glowPlace(d,900.0f,mode,glowForest,258.0f) == GlowPlace::none && glowPlace(d,900.0f,mode,beamFar,961.5f) == GlowPlace::none);
    }
    assert(glowPlace(d,900.0f,glowOff,glowNear,258.0f) == GlowPlace::none);
    // Depth: fDoorVoidDepth bounds it (507 behind is out with a 400 void).
    assert(glowPlace(d,400.0f,glowBehind,glowDeep,258.0f) == GlowPlace::none && glowPlace(d,400.0f,glowBehind,glowNear,258.0f) == GlowPlace::behind);
    const auto at = [&](float back,float across,float up) {
        return Vec{d.point.x-d.nx*back+d.ny*across,d.point.y-d.ny*back-d.nx*across,d.point.z+up};
    };
    // From the plane itself (a card in the doorway is the stub's), not from the void's start.
    assert(glowPlace(d,900.0f,glowBehind,at(0.0f,0.0f,100.0f),258.0f) == GlowPlace::behind);
    assert(glowPlace(d,900.0f,glowBehind,at(900.0f,0.0f,100.0f),258.0f) == GlowPlace::behind);
    assert(glowPlace(d,900.0f,glowBehind,at(900.5f,0.0f,100.0f),258.0f) == GlowPlace::none);
    // In front: only with mode 2, up to glowFrontReach.
    assert(glowPlace(d,900.0f,glowBehind,at(-1.0f,0.0f,100.0f),258.0f) == GlowPlace::none);
    assert(glowPlace(d,900.0f,glowFront,at(-1.0f,0.0f,100.0f),258.0f) == GlowPlace::front);
    assert(glowPlace(d,900.0f,glowFront,at(-glowFrontReach,0.0f,100.0f),258.0f) == GlowPlace::front);
    assert(glowPlace(d,900.0f,glowFront,at(-glowFrontReach-.5f,0.0f,100.0f),258.0f) == GlowPlace::none);
    // Across: the lane's own half width (fDoorVoidSide included), not widened by the card.
    assert(glowPlace(d,900.0f,glowBehind,at(200.0f,d.half-.5f,100.0f),258.0f) == GlowPlace::behind);
    assert(glowPlace(d,900.0f,glowBehind,at(200.0f,-(d.half+.5f),100.0f),258.0f) == GlowPlace::none);
    assert(glowPlace(embershard(0.0f),900.0f,glowBehind,glowDeep,258.0f) == GlowPlace::behind); // 163 across, half 168 with no side margin
    assert(glowPlace(embershard(0.0f),900.0f,glowBehind,at(200.0f,200.0f,100.0f),258.0f) == GlowPlace::none);
    // Heights: the lane's, widened by the card's reach.
    assert(glowPlace(d,900.0f,glowBehind,{glowNear.x,glowNear.y,d.high+258.0f},258.0f) == GlowPlace::behind);
    assert(glowPlace(d,900.0f,glowBehind,{glowNear.x,glowNear.y,d.high+258.5f},258.0f) == GlowPlace::none);
    assert(glowPlace(d,900.0f,glowBehind,{glowNear.x,glowNear.y,d.low-258.0f},258.0f) == GlowPlace::behind);
    assert(glowPlace(d,900.0f,glowBehind,{glowNear.x,glowNear.y,d.low-258.5f},258.0f) == GlowPlace::none);
    assert(glowPlace(d,900.0f,glowBehind,{glowNear.x,glowNear.y,d.low-1.0f},0.0f) == GlowPlace::none);
    // Anything unreadable places nothing.
    assert(glowPlace(DoorPlane{},900.0f,glowBehind,glowNear,258.0f) == GlowPlace::none);
    assert(glowPlace(d,nan,glowBehind,glowNear,258.0f) == GlowPlace::none && glowPlace(d,900.0f,glowBehind,{nan,0.0f,0.0f},258.0f) == GlowPlace::none);
    assert(glowPlace(d,900.0f,glowBehind,glowNear,nan) == GlowPlace::none && glowPlace(d,900.0f,glowBehind,glowNear,-1.0f) == GlowPlace::none);
    assert(std::strcmp(glowPlaceName(GlowPlace::behind),"behind") == 0 && std::strcmp(glowPlaceName(GlowPlace::front),"front") == 0 &&
        std::strcmp(glowPlaceName(GlowPlace::none),"none") == 0);
}
void chosen() {
    const GlowCensus c = cardsHere();
    // The recorded arrival, 175 in front of the plane: the door is chosen, both spheres are hidden, nothing else.
    const DoorSelection s = chosenAt(recordedArrival);
    assert(s.count == 1 && s.chosen[0].door.id == 0x000b6ccdu);
    const GlowSet g = selectGlows(c,s,900.0f,glowBehind);
    assert(g.cell == 0x000b6be6u && g.doors == 1 && g.count == 2 && g.full == 0);
    assert(g.hidden[0].ref == 0x0008f94eu && g.hidden[0].door == 0x000b6ccdu && g.hidden[0].card == 0 && g.hidden[0].place == GlowPlace::behind);
    assert(g.hidden[1].ref == 0x0008f94du && g.hidden[1].card == 1);
    assert(g.hides(0x0008f94eu) && g.hides(0x0008f94du) && !g.hides(0x0008fb0du) && !g.hides(0x000b6efbu) && !g.hides(0u));
    // Off: nothing, whatever the doors.
    const GlowSet off = selectGlows(c,s,900.0f,glowOff);
    assert(off.count == 0 && off.doors == 1 && !off.hides(0x0008f94eu));
    // The door not chosen (the player behind its plane, or out of the radius): drawn again.
    const DoorSelection behindIt = chosenAt({2448.6f,1197.1f,9241.0f});
    assert(behindIt.count == 0 && selectGlows(c,behindIt,900.0f,glowBehind).count == 0);
    const DoorSelection distant = chosenAt({embershardDoor.x+1600.0f*0.249f,embershardDoor.y+1600.0f*0.969f,9241.0f});
    assert(distant.count == 0 && distant.distant == 1 && selectGlows(c,distant,900.0f,glowBehind).count == 0);
    assert(selectGlows(c,DoorSelection{},900.0f,glowBehind).count == 0);
    // A card in front of the door: only with mode 2, and only up to glowFrontReach.
    const DoorPlane d = embershard();
    GlowCensus front = c;
    assert(noteGlowCard(front,0x00ab0001u,glowBrt,false,{d.point.x+d.nx*300.0f,d.point.y+d.ny*300.0f,d.point.z+150.0f},258.0f) == GlowNote::added);
    assert(noteGlowCard(front,0x00ab0002u,glowBrt,false,{d.point.x+d.nx*700.0f,d.point.y+d.ny*700.0f,d.point.z+150.0f},258.0f) == GlowNote::added);
    assert(selectGlows(front,s,900.0f,glowBehind).count == 2 && !selectGlows(front,s,900.0f,glowBehind).hides(0x00ab0001u));
    const GlowSet both = selectGlows(front,s,900.0f,glowFront);
    assert(both.count == 3 && both.hides(0x00ab0001u) && !both.hides(0x00ab0002u) && both.hidden[2].place == GlowPlace::front);
    // Two chosen doors: a card both place goes to the nearer one (the selection's order), once.
    DoorCensus two = doorsHere();
    assert(noteDoor(two,0x00ab0010u,&mine(),Teleport::present,false,{embershardDoor.x+40.0f,embershardDoor.y,embershardDoor.z},embershardAngle,1.0f,320.0f) ==
        DoorNote::added);
    DoorSelection pair = selectDoors(two,recordedArrival,1500.0f);
    assert(pair.count == 2);
    const GlowSet shared = selectGlows(c,pair,900.0f,glowBehind);
    assert(shared.count == 2 && shared.doors == 2 && shared.hidden[0].door == pair.chosen[0].door.id && shared.hidden[1].door == pair.chosen[0].door.id);
    // More qualifying cards than the set holds: the rest are drawn and counted.
    GlowCensus crowd;
    for (std::uint32_t i = 0;i < maxHiddenGlows+5;++i) static_cast<void>(noteGlowCard(crowd,0x00cc0000u+i,glowBrt,false,glowNear,258.0f));
    const GlowSet many = selectGlows(crowd,s,900.0f,glowBehind);
    assert(many.count == maxHiddenGlows && many.full == 5 && many.hides(0x00cc0000u) && !many.hides(0x00cc0000u+maxHiddenGlows));
    // The change test: the same cards for the same doors only.
    assert(sameGlows(g,selectGlows(c,chosenAt({recordedArrival.x+10.0f,recordedArrival.y,recordedArrival.z}),900.0f,glowBehind)));
    assert(!sameGlows(g,off) && !sameGlows(g,both) && sameGlows(many,selectGlows(crowd,s,900.0f,glowFront)));
    GlowSet otherCell = g;
    otherCell.cell = 1u;
    assert(!sameGlows(g,otherCell) && sameGlows(GlowSet{},GlowSet{}));
    GlowSet otherDoor = g;
    otherDoor.hidden[1].door = 2u;
    assert(!sameGlows(g,otherDoor));
}
void settings() {
    bool adjusted = true;
    Config c = validate(Config{},&adjusted);
    assert(!adjusted && c.frame.doorGlow == 0u && c.effect);
    for (unsigned mode : {1u,2u}) {
        c = {}; c.frame.doorVoid = 1u; c.frame.doorGlow = mode; c = validate(c,&adjusted);
        assert(!adjusted && c.frame.doorGlow == mode);
    }
    c = {}; c.frame.doorVoid = 1u; c.frame.doorGlow = 3u; c = validate(c,&adjusted);
    assert(adjusted && c.frame.doorGlow == 0u && c.frame.doorVoid == 1u);
    adjusted = false; c = {}; c.frame.doorGlow = 1u; c = validate(c,&adjusted); // without the door void
    assert(adjusted && c.frame.doorGlow == 0u);
    adjusted = false; c = {}; c.style = flatStyle; c.frame.doorVoid = 1u; c.frame.doorGlow = 1u; c = validate(c,&adjusted); // without Stage 1
    assert(adjusted && c.frame.doorVoid == 0u && c.frame.doorGlow == 0u);
    adjusted = false; c = {}; c.effect = false; c.frame.doorVoid = 1u; c.frame.doorGlow = 1u; c = validate(c,&adjusted); // without the Effect class
    assert(adjusted && c.frame.doorVoid == 1u && c.frame.doorGlow == 0u);
    adjusted = false; c = {}; c.frame.enabled = false; c.frame.doorGlow = 1u; c = validate(c,&adjusted); // bFramePass=0: not validated (no runtime)
    assert(!adjusted);
    // The lights and the glow are independent.
    adjusted = false; c = {}; c.frame.doorVoid = 1u; c.frame.doorLights = 1u; c.frame.doorGlow = 2u; c = validate(c,&adjusted);
    assert(!adjusted && c.frame.doorLights == 1u && c.frame.doorGlow == 2u);
}
void trace() {
    const GlowCensus c = cardsHere();
    const DoorSelection s = chosenAt(recordedArrival);
    const GlowSet g = selectGlows(c,s,900.0f,glowBehind);
    char line[768]{};
    GlowTrace t;
    t.seconds = 12.5; t.frame = 99u; t.mode = 1u; t.set = &g; t.census = &c; t.counts = {7u,1u};
    assert(formatDoorGlow(line,sizeof(line),t));
    assert(std::string(line) == "FixedDoorGlow t=12.500 frame=99 cell=000B6BE6 mode=1 doors=1 hidden=2 full=0 removed=7 native=1 "
        "list=000B6CCD:0008F94E:fxglowfillrndlitbrt.nif/behind;000B6CCD:0008F94D:fxglowfillrndlitbrt.nif/behind");
    // The last door leaves: one row with nothing listed.
    const GlowSet none = selectGlows(c,DoorSelection{},900.0f,glowBehind);
    t.set = &none; t.counts = {};
    assert(formatDoorGlow(line,sizeof(line),t) && has(line," doors=0 hidden=0 ") && has(line,"list=none"));
    // No census (latched): names read none. More cards than listed: more=N.
    GlowCensus crowd;
    for (std::uint32_t i = 0;i < maxHiddenGlows;++i) static_cast<void>(noteGlowCard(crowd,0x00cc0000u+i,glowBrt,false,glowNear,258.0f));
    const GlowSet many = selectGlows(crowd,s,900.0f,glowBehind);
    t.set = &many; t.census = nullptr;
    assert(formatDoorGlow(line,sizeof(line),t) && has(line,":none/behind;") && has(line," more=10") && std::strlen(line) < sizeof(line));
    t.census = &crowd;
    assert(formatDoorGlow(line,sizeof(line),t) && has(line," more=10") && std::strlen(line) < 600);
    char small[64]{};
    assert(!formatDoorGlow(small,sizeof(small),t));
    t.set = nullptr;
    assert(!formatDoorGlow(line,sizeof(line),t));
    // The census line, once per cell.
    char log[1280]{};
    const DoorCensus doors = doorsHere();
    assert(formatDoorGlowCensus(log,sizeof(log),0x000b6be6u,1u,doors,c,900.0f));
    assert(std::string(log) == "FixedCameraSpike shader door glow cell=000B6BE6 mode=1 cards=4 refs=4 off=0 full=0 doors=1 placed=2 "
        "list=0008F94E@000B6CCD/116.9:fxglowfillrndlitbrt.nif,0008F94D@000B6CCD/507.2:fxglowfillrndlitbrt.nif");
    assert(formatDoorGlowCensus(log,sizeof(log),0x000b6be6u,1u,doors,c,400.0f) && has(log," placed=1 ") && !has(log,"0008F94D@"));
    assert(formatDoorGlowCensus(log,sizeof(log),0x000b6be6u,1u,DoorCensus{},c,900.0f) && has(log," doors=0 placed=0 list=none"));
    // The worst case (every listed entry with the longest name) fits the adapter's buffer.
    GlowCensus longest;
    const std::string path = "effects\\fxglowfill" + std::string(60,'x') + ".nif";
    for (std::uint32_t i = 0;i < maxCellGlows;++i) static_cast<void>(noteGlowCard(longest,0xfffff000u+i,path.c_str(),false,glowNear,258.0f));
    assert(formatDoorGlowCensus(log,sizeof(log),~0u,2u,doors,longest,900.0f) && has(log," placed=128 ") && std::strlen(log) < sizeof(log));
    assert(!formatDoorGlowCensus(small,sizeof(small),~0u,2u,doors,longest,900.0f));
}
} // namespace
int main() {
    models();
    census();
    place();
    chosen();
    settings();
    trace();
    std::puts("fixed_spike_shader_door_glow_policy_test: PASS");
}
