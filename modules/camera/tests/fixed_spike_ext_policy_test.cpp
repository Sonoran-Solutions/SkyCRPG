// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "ext": the exterior see-through rule. Geometry is
// taken from Skyrim.esm (OBND of the bases, DATA of the Whiterun stable refs)
// so the stable rows below are the fixture the route plan uses.
#include "fixed_spike_ext_policy.h"
#include <cassert>
#include <limits>
#include <string>
using namespace skycrpg::camera;
using namespace skycrpg::camera::fixed;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool near(float a,float b,float tolerance = 1e-3f) { return std::abs(a-b) <= tolerance; }
// Rotation about z as the stable refs are placed: local +y maps to (sin, cos).
// Source: refs 0627E2 / 0627E3 / 0627E4 (WRStableStr01, rot z 0.0872667) sit
// 191.27 apart along (16.73, 191.27), i.e. along local y under this matrix.
std::array<float,9> rotZ(float angle) {
    const float c = std::cos(angle),s = std::sin(angle);
    return {c,s,0,-s,c,0,0,0,1};
}
const std::array<float,9> identity{1,0,0,0,1,0,0,0,1};
Obb box(std::array<int,6> obnd,Vec origin,const std::array<float,9>& rotate = identity,float scale = 1.0f) {
    return makeObb({static_cast<float>(obnd[0]),static_cast<float>(obnd[1]),static_cast<float>(obnd[2])},
        {static_cast<float>(obnd[3]),static_cast<float>(obnd[4]),static_cast<float>(obnd[5])},rotate,origin,scale);
}
// Skyrim.esm OBND (Source: scratch TES4 scan, session-p55/fixtures DB).
constexpr std::array<int,6> stable{-215,-98,-9,215,98,422};          // WRStableStr01 0627E1
constexpr std::array<int,6> stableWall{-198,-5,-6,191,5,100};        // WRStableWallShortStr01 0627E5
constexpr std::array<int,6> hayBale{-77,-41,-54,78,40,141};          // HayBale01 05B198
constexpr std::array<int,6> stableHouse{-383,-208,-224,446,643,352}; // WRHouseStables01 0627AA
constexpr std::array<int,6> cityWall{-512,-96,-777,512,115,245};     // WRWallStr01 089F28
constexpr std::array<int,6> cliff{-371,-798,-349,367,798,345};       // RockCliff01 01CF9F
constexpr std::array<int,6> dragonBridge{-2577,-529,-1576,2577,529,1434}; // DragonBridge01 0E37A7
constexpr std::array<int,6> dock{-167,-256,-438,161,256,69};          // DockStrSol01 0AE62E
constexpr std::array<int,6> gateRoof{-349,-386,-143,526,481,291};     // MrkFrontWALLintGateRoof01 0BACE2
constexpr std::array<int,6> gateHouse{-139,-228,-91,138,252,523};     // WRWallMainGateHouse01 069821
constexpr std::array<int,6> farmhouse{-452,-512,-128,451,343,808};    // Farmhouse05 02A745
constexpr std::array<int,6> millRoof{-202,-237,136,508,278,422};      // MrkMillRoof01 0B3A37
constexpr std::array<int,6> stallRoof{-67,-76,77,68,79,215};          // MrkMarketStallRoof01 04739F
constexpr std::array<int,6> pine{-459,-490,-66,489,486,2498};         // TreePineForest01 01306D
constexpr std::array<int,6> smallPine{-200,-216,-88,194,215,860};     // TreePineForest03 04B016
// Whiterun stable pose (session-p55 routes, run p55-a-open-1 probe sample a-h0):
// feet and the rendered camera at heading 45.
const Vec feet{18824.982f,-11005.162f,-4591.266f};
const Vec eye{18587.717f,-11242.428f,-3907.432f};
const Vec e3{18825.0996f,-11005.1621f,-4584.3374f},e2{18841.834f,-10813.8926f,-4584.3374f},e4{18808.365f,-11196.4316f,-4584.3374f};
const float stableAngle = 0.0872667f;

void settings() {
    bool adjusted = true;
    const ExtConfig defaults = validateExterior(ExtConfig{},&adjusted);
    assert(!adjusted && defaults.mode == 0 && defaults.maxSpan == 1024.0f && defaults.maxHidden == 16);
    ExtConfig c;
    c.mode = 3; c.maxSpan = nan; c.maxHidden = 0;
    c = validateExterior(c,&adjusted);
    assert(adjusted && c.mode == 0 && c.maxSpan == 1024.0f && c.maxHidden == 1);
    c = {}; c.maxSpan = 1e9f; c.maxHidden = 1000; c.mode = 2;
    c = validateExterior(c,&adjusted);
    assert(adjusted && c.mode == 2 && c.maxSpan == 2048.0f && c.maxHidden == extMaxHiddenLimit);
    c = {}; c.maxSpan = 1; assert(validateExterior(c).maxSpan == 512.0f);
    c = {}; c.mode = 1; c.maxSpan = 700; c.maxHidden = 32;
    assert(validateExterior(c,&adjusted).maxSpan == 700.0f && !adjusted);
    // Mode 0 never reaches the exterior scan, indoors or out; the wave 1 rules are untouched.
    for (const bool interior : {false,true}) {
        assert(!exteriorScanned(0,interior));
        assert(exteriorScanned(1,interior) == !interior && exteriorScanned(2,interior) == !interior);
        assert(hideScanned(1,interior) == interior && !hideScanned(0,interior));
    }
    assert(std::string(reasonName(Reason::extCover)) == "ext-cover" && std::string(reasonName(Reason::extOverhang)) == "ext-overhang");
    assert(std::string(reasonName(Reason::extCanopy)) == "ext-canopy" && std::string(reasonName(Reason::onRay)) == "on-ray");
    assert(extRefusalBit(ExtRefusal::settings) == 4u && extRefusalBit(ExtRefusal::pickSlot) == 16u);
    static_assert(static_cast<unsigned>(ExtRefusal::settings) >= 62 && static_cast<unsigned>(ExtRefusal::pickSlot) <= 69);
}

void boxes() {
    // Invalid inputs never make a box.
    assert(!box({0,0,0,0,0,0},{0,0,0}).valid);
    assert(!box({1,0,0,0,1,1},{0,0,0}).valid);
    assert(!box(stable,{0,0,0},identity,0).valid && !box(stable,{nan,0,0}).valid);
    assert(!box(stable,{0,0,0},{nan,0,0,0,1,0,0,0,1}).valid && box(stable,{0,0,0}).valid);
    // World <-> local: the stable's local +y points along the segment row.
    const Obb s3 = box(stable,e3,rotZ(stableAngle));
    const Vec l2 = toLocal(s3,e2);
    assert(near(l2.x,0,.05f) && near(l2.y,192.0f,.5f) && near(l2.z,0));
    const Obb scaled = box(stable,{100,0,0},identity,2.0f);
    const Vec l = toLocal(scaled,{100,50,10});
    assert(near(l.x,0) && near(l.y,25) && near(l.z,5));
    // Extent: world z from the base box, spans scaled, rotation about z keeps both.
    const Extent e = obbExtent(s3);
    assert(e.valid && near(e.minZ,e3.z-9) && near(e.maxZ,e3.z+422) && near(e.spanX,430) && near(e.spanY,196));
    assert(near(obbExtent(scaled).spanX,860) && !obbExtent(Obb{}).valid);
    // Segment against box: through, along a face, parallel inside / outside, grow.
    float a{},b{};
    const Obb unit = box({-10,-10,-10,10,10,10},{0,0,0});
    assert(segmentBox(unit,{-20,0,0},{20,0,0},0,a,b) && near(a,.25f) && near(b,.75f));
    assert(!segmentBox(unit,{-20,15,0},{20,15,0},0,a,b));
    assert(segmentBox(unit,{-20,15,0},{20,15,0},6,a,b));   // grown in local x/y
    assert(!segmentBox(unit,{0,0,15},{0,0,40},6,a,b));      // never grown in z
    assert(segmentBox(unit,{0,0,-40},{0,0,40},0,a,b) && near(a,.375f) && near(b,.625f));
    assert(segmentBox(unit,{0,0,0},{1,1,1},0,a,b) && a == 0 && b == 1); // wholly inside
    assert(!segmentBox(unit,{-20,0,0},{-15,0,0},0,a,b));    // stops short
    assert(!segmentBox(unit,{nan,0,0},{20,0,0},0,a,b) && !segmentBox(unit,{-20,0,0},{20,0,0},-1,a,b));
    const Obb turned = box({-10,-1,-1,10,1,1},{0,0,0},rotZ(pi/2)); // long axis now along world -y..+y
    assert(segmentBox(turned,{0,-20,0},{0,20,0},0,a,b) && segmentBox(turned,{-20,5,0},{20,5,0},0,a,b));
    assert(!segmentBox(turned,{-20,15,0},{20,15,0},0,a,b) && !segmentBox(turned,{5,-20,0},{5,20,0},0,a,b));
    assert(near(groundDistance({0,5,99},{-10,0,0},{10,0,0}),5) && near(groundDistance({20,0,0},{-10,0,0},{10,0,0}),10));
}

void cover() {
    const auto r = rotZ(stableAngle);
    const Obb s2 = box(stable,e2,r),s3 = box(stable,e3,r),s4 = box(stable,e4,r);
    // The stable pose stands under segment 0627E3 and under neither neighbour.
    assert(covers(s3,feet,eye.z,false) && !covers(s2,feet,eye.z,false) && !covers(s4,feet,eye.z,false));
    assert(!covers(s2,feet,eye.z,true) && !covers(s4,feet,eye.z,true)); // 94 units out: beyond the exit grow too
    // Stepping out across the long side: no cover past the footprint, kept 48 more while hidden.
    const Vec out30{feet.x+(215+30)*std::cos(stableAngle),feet.y-(215+30)*std::sin(stableAngle),feet.z};
    assert(!covers(s3,out30,eye.z,false) && covers(s3,out30,eye.z,true));
    const Vec out60{feet.x+(215+60)*std::cos(stableAngle),feet.y-(215+60)*std::sin(stableAngle),feet.z};
    assert(!covers(s3,out60,eye.z,true));
    // Low pieces are never over the head: the stable's short wall, a hay bale, even standing in them.
    assert(!covers(box(stableWall,{feet.x,feet.y,e3.z}),feet,eye.z,true));
    assert(!covers(box(hayBale,{feet.x,feet.y,e3.z}),feet,eye.z,true));
    // A wall beside the player, however tall, is not crossed by the column (no entry margin).
    const Obb wall = box(cityWall,{feet.x,feet.y+120,feet.z+777});
    assert(!covers(wall,feet,eye.z,false));
    // Something above the camera height is not between camera and player.
    assert(!covers(box(stallRoof,{feet.x,feet.y,eye.z+10}),feet,eye.z,false));
    assert(covers(box(stallRoof,{feet.x,feet.y,feet.z}),feet,eye.z,false)); // a stall roof 77..215 over the feet
    assert(!covers(s3,feet,feet.z+100,false) && !covers(s3,{nan,0,0},eye.z,false));
}

void large() {
    const auto structure = [](std::array<int,6> obnd,float scale = 1.0f) {
        return largeStructure(ExtKind::structure,obbExtent(box(obnd,{0,0,0},identity,scale)),1024.0f);
    };
    // Kept as candidates: the target pieces.
    assert(!structure(stable) && !structure(dock) && !structure(gateRoof) && !structure(gateHouse) && !structure(millRoof));
    assert(!structure(stallRoof) && !structure(stableWall) && !structure(hayBale));
    // One large reference: house shells, a city wall, a cliff, a whole bridge.
    assert(structure(stableHouse) && structure(farmhouse) && structure(cityWall) && structure(cliff) && structure(dragonBridge));
    assert(structure(stable,2.5f)); // 1075 wide when scaled
    // The threshold is the configured span; trees are large only from 1536.
    assert(!largeStructure(ExtKind::structure,obbExtent(box(cityWall,{0,0,0})),2048.0f)); // 1024 wide: large only at the default
    assert(largeStructure(ExtKind::structure,obbExtent(box({-300,-50,0,300,50,200},{0,0,0})),512.0f));
    assert(!largeStructure(ExtKind::tree,obbExtent(box(pine,{0,0,0})),1024.0f));
    assert(largeStructure(ExtKind::tree,obbExtent(box(pine,{0,0,0},identity,1.6f)),1024.0f));
    assert(largeStructure(ExtKind::structure,Extent{},1024.0f)); // no extent: never hidden
    assert(largeStructure(ExtKind::structure,obbExtent(box({-10,-10,0,10,10,1600},{0,0,0})),1024.0f)); // 1600 tall
}

void overhang() {
    const auto r = rotZ(stableAngle);
    const Obb s4 = box(stable,e4,r);
    // Heading 45 at the stable pose: the chest ray enters segment 0627E4 (the
    // probe's a-h0 sample reported 0627E4 on the head and chest rays).
    const Vec chest{feet.x,feet.y,feet.z+80};
    float a{},b{};
    assert(segmentBox(s4,eye,chest,0,a,b));
    const Vec entry = mix(eye,chest,a);
    assert(entry.z-feet.z >= overhangClear && overhangHit(entry,feet,false));
    // Too low (bales, walls), too far, not finite.
    assert(!overhangHit({feet.x+100,feet.y,feet.z+150},feet,false) && overhangHit({feet.x+100,feet.y,feet.z+170},feet,true));
    assert(!overhangHit({feet.x+400,feet.y,feet.z+300},feet,false) && overhangHit({feet.x+400,feet.y,feet.z+300},feet,true));
    assert(!overhangHit({feet.x+500,feet.y,feet.z+300},feet,true) && !overhangHit({nan,0,0},feet,true));
    // Probes: three, at 144 over the feet, ending 48 short of the ground under the hit.
    const auto probes = openProbes(feet,{feet.x+200,feet.y,feet.z+300});
    for (const auto& p : probes) {
        assert(p.valid && near(p.from.z,feet.z+probeHeight) && near(p.to.z,feet.z+probeHeight));
        assert(near(p.to.x-p.from.x,200-probeAllowance));
    }
    assert(near(probes[0].from.y,feet.y) && near(probes[1].from.y-feet.y,probeSpread,.01f) && near(probes[2].from.y-feet.y,-probeSpread,.01f));
    // Open when any valid probe is clear; a hit over the player has no probe and is open.
    assert(openBelow(probes,{false,true,false}) && !openBelow(probes,{false,false,false}));
    const auto over = openProbes(feet,{feet.x+10,feet.y,feet.z+300});
    assert(!over[0].valid && openBelow(over,{false,false,false}));
    assert(!openProbes(feet,{nan,0,0})[0].valid);
}

void classify() {
    const auto r = rotZ(stableAngle);
    ExtConfig c; c.mode = 1;
    ExtCandidate s3; s3.kind = ExtKind::structure; s3.box = box(stable,e3,r); s3.rayHit = true;
    // Under the stable roof: cover.
    assert(classifyExterior(c,s3,feet,eye.z,false).reason == Reason::extCover);
    // The neighbour segment the chest ray crosses: overhang only when open below.
    ExtCandidate s4 = s3; s4.box = box(stable,e4,r);
    float a{},b{};
    assert(segmentBox(s4.box,eye,{feet.x,feet.y,feet.z+80},0,a,b));
    s4.hit = mix(eye,{feet.x,feet.y,feet.z+80},a);
    assert(classifyExterior(c,s4,feet,eye.z,false).skip == ExtSkip::beside); // probes not run or blocked
    s4.openBelow = true;
    assert(classifyExterior(c,s4,feet,eye.z,false).reason == Reason::extOverhang);
    // Not hit by a ray: never a structure candidate, and nothing to trace.
    ExtCandidate silent = s3; silent.rayHit = false;
    const auto none = classifyExterior(c,silent,feet,eye.z,false);
    assert(none.reason == Reason::none && none.skip == ExtSkip::none);
    // Floor first, then size, both traced.
    ExtCandidate floor = s3; floor.floorContact = true;
    assert(classifyExterior(c,floor,feet,eye.z,false).skip == ExtSkip::floorContact);
    ExtCandidate shell = s3; shell.box = box(stableHouse,{feet.x,feet.y,feet.z});
    assert(classifyExterior(c,shell,feet,eye.z,false).skip == ExtSkip::large); // the player under a shell's box: still never hidden
    ExtCandidate bridge = s4; bridge.box = box(dragonBridge,{feet.x,feet.y,feet.z+800});
    assert(classifyExterior(c,bridge,feet,eye.z,false).skip == ExtSkip::large);
    // A roof-only piece of a building the player stands behind: hit above the head but walls block every probe.
    ExtCandidate roof; roof.kind = ExtKind::structure; roof.rayHit = true; roof.box = box(millRoof,{feet.x-300,feet.y-300,feet.z});
    roof.hit = {feet.x-200,feet.y-200,feet.z+300};
    assert(classifyExterior(c,roof,feet,eye.z,false).skip == ExtSkip::beside);
    // Without bounds a ray hit is traced and kept.
    ExtCandidate bare = s3; bare.box = {};
    assert(classifyExterior(c,bare,feet,eye.z,false).skip == ExtSkip::noBounds);
    // Excluded kinds and trees with trees off: traced only when a ray hit them.
    ExtCandidate door; door.kind = ExtKind::excluded; door.rayHit = true;
    assert(classifyExterior(c,door,feet,eye.z,false).skip == ExtSkip::kind);
    door.rayHit = false; assert(classifyExterior(c,door,feet,eye.z,false).skip == ExtSkip::none);
    ExtCandidate tree; tree.kind = ExtKind::tree; tree.box = box(pine,{feet.x+100,feet.y,feet.z}); tree.rayHit = true;
    assert(classifyExterior(c,tree,feet,eye.z,false).skip == ExtSkip::treesOff);
    // Mode 2: a tree over the player (canopy box over the column) or pierced above the head.
    c.mode = 2;
    assert(classifyExterior(c,tree,feet,eye.z,false).reason == Reason::extCover);
    ExtCandidate aside; aside.kind = ExtKind::tree; aside.box = box(smallPine,{feet.x-250,feet.y-250,feet.z}); // round the camera, not over the feet
    assert(classifyExterior(c,aside,feet,eye.z,false).skip == ExtSkip::none); // not on a ray, not over, not pierced: silent
    aside.pierced = piercedByFan(aside.box,eye,exteriorTargets(feet),feet.z);
    assert(aside.pierced && classifyExterior(c,aside,feet,eye.z,false).reason == Reason::extCanopy);
    ExtCandidate behind = aside; behind.box = box(smallPine,{feet.x+400,feet.y+400,feet.z});
    behind.pierced = piercedByFan(behind.box,eye,exteriorTargets(feet),feet.z);
    assert(!behind.pierced && classifyExterior(c,behind,feet,eye.z,false).skip == ExtSkip::none); // beyond the player
    ExtCandidate trunk = behind; trunk.rayHit = true;
    assert(classifyExterior(c,trunk,feet,eye.z,false).skip == ExtSkip::beside);
    ExtCandidate giant = tree; giant.box = box(pine,{feet.x,feet.y,feet.z},identity,1.6f);
    assert(classifyExterior(c,giant,feet,eye.z,false).skip == ExtSkip::large);
    // Exit hysteresis: a hidden cover stays while the player is within the exit grow.
    const Vec edge{feet.x+(215+30)*std::cos(stableAngle),feet.y-(215+30)*std::sin(stableAngle),feet.z};
    c.mode = 1;
    assert(classifyExterior(c,s3,edge,eye.z,true).reason == Reason::extCover && classifyExterior(c,s3,edge,eye.z,false).skip == ExtSkip::beside);
}

void fan() {
    const auto targets = exteriorTargets(feet);
    assert(targets.size() == 19);
    assert(near(targets[0].z-feet.z,120) && near(targets[1].z-feet.z,80) && near(targets[2].z-feet.z,40));
    for (unsigned i = 0;i < 8;++i) {
        assert(near(std::hypot(targets[3+i].x-feet.x,targets[3+i].y-feet.y),extInnerRing,.01f) && near(targets[3+i].z-feet.z,80));
        assert(near(std::hypot(targets[11+i].x-feet.x,targets[11+i].y-feet.y),extOuterRing,.01f) && near(targets[11+i].z-feet.z,24));
    }
    // Every target stays within the outer ring: the fan is bounded by the camera and a 128-unit disc.
    for (const auto t : targets) assert(std::hypot(t.x-feet.x,t.y-feet.y) <= extOuterRing+.01f);
    // Broad phase for trees.
    assert(nearFan({feet.x+300,feet.y,0},200,feet,eye) && !nearFan({feet.x+800,feet.y+800,0},200,feet,eye));
    assert(!nearFan({nan,0,0},10,feet,eye) && !nearFan(feet,-1,feet,eye));
    // Bound by construction: pieces may only qualify through rays (structures) or
    // the fan geometry (trees); a full camera-to-target pass is at most 19 x 4 picks.
    static_assert(extRayTargets*extRaySegments+maxFloorProbes+maxOverhangProbed*openProbeCount <= 117);
}

void cells() {
    // Position to cell, as TES::GetCell (ID 13322): floor, then >> 12.
    assert(*cellIndex(0) == 0 && *cellIndex(4095.9f) == 0 && *cellIndex(4096) == 1 && *cellIndex(-0.5f) == -1);
    assert(*cellIndex(-4096) == -1 && *cellIndex(-4096.5f) == -2 && *cellIndex(feet.x) == 4 && *cellIndex(feet.y) == -3);
    assert(!cellIndex(nan) && !cellIndex(2e9f));
    // The stable pose is WhiterunExterior01, grid [4, -3] in the ESM (Source: CELL 0000961B XCLC).
    const auto range = cellsAround(feet,eye);
    assert(range.valid && range.x0 <= 4 && range.x1 >= 4 && range.y0 <= -3 && range.y1 >= -3);
    assert(range.x1-range.x0 <= 1 && range.y1-range.y0 <= 1); // the fan plus margin spans at most 2 x 2 here
    assert(!cellsAround({nan,0,0},eye).valid);
    assert(!cellsAround(feet,{feet.x+9000,feet.y,feet.z}).valid); // a camera far out is refused, not walked
    // Grid slots: ID 13319 maps cell - centre + length/2; outside is refused.
    assert(gridSlot(4,-3,4,-3,5).x == 2 && gridSlot(4,-3,4,-3,5).y == 2);
    assert(gridSlot(2,-5,4,-3,5).x == 0 && gridSlot(6,-1,4,-3,5).y == 4);
    assert(!gridSlot(7,-3,4,-3,5).valid() && !gridSlot(4,-6,4,-3,5).valid() && !gridSlot(1,-3,4,-3,5).valid());
    assert(!gridSlot(0,0,0,0,0).valid() && !gridSlot(0,0,0,0,maxGridLength+1).valid() && gridSlot(0,0,0,0,1).valid());
    assert(!gridSlot((std::numeric_limits<int>::max)(),0,(std::numeric_limits<int>::min)(),0,5).valid()); // no overflow
}

void summary() {
    ExtSummary s;
    s.seconds = 123456.789; s.frame = 9999999999ull; s.cell = 0x0000961B; s.mode = 2; s.hidden = 32; s.tracked = 32; s.maxHidden = 32;
    s.feetZ = -12345.67f; s.eyeZ = -11661.83f; s.restores = 9999999999ull;
    s.c.scans = s.c.cells = s.c.visited = s.c.rays = s.c.rayHits = s.c.rayNoRef = 9999999999ull;
    for (auto& v : s.c.skips) v = 9999999999ull;
    for (auto& v : s.exits) v = 9999999999ull;
    char line[768]{};
    assert(formatExtSummary(line,sizeof(line),s)); // fits a trace row even with ten-digit counters
    const std::string text(line);
    assert(text.find("event=ext-summary cell=0000961B mode=2 hidden=32") != std::string::npos);
    assert(text.find("skipBeside=9999999999 feetZ=-12345.67") != std::string::npos && text.find("sample=cumulative") != std::string::npos);
    char tiny[32]{};
    assert(!formatExtSummary(tiny,sizeof(tiny),s));
    assert(std::string(extSkipNames[static_cast<std::size_t>(ExtSkip::large)]) == "large");
}
} // namespace
int main() {
    settings();
    boxes();
    cover();
    large();
    overhang();
    classify();
    fan();
    cells();
    summary();
    return 0;
}
