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
constexpr std::array<int,6> shack{-336,-226,0,398,251,503};           // WRHouseShack01 (review M1)
constexpr std::array<int,6> wallCurve{-512,-92,-777,475,446,244};     // WRWallCurve45 (review M1)
constexpr std::array<int,6> stableEnd{-242,68,-9,242,108,451};        // WRStableStr01End (review m1)
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
    assert(extRefusalBit(ExtRefusal::settings) == 4u && extRefusalBit(ExtRefusal::pickSlot) == 16u && extRefusalBit(ExtRefusal::world) == 32u);
    static_assert(static_cast<unsigned>(ExtRefusal::settings) >= 62 && static_cast<unsigned>(ExtRefusal::world) <= 69);
    // The cover footprint (review M1): default 256 (owner decision 2026-11-10), bounded, reported when adjusted.
    assert(defaults.coverFootprint == 256.0f);
    c = {}; c.coverFootprint = 5; assert(validateExterior(c,&adjusted).coverFootprint == 128.0f && adjusted);
    c = {}; c.coverFootprint = nan; assert(validateExterior(c).coverFootprint == 256.0f);
    c = {}; c.coverFootprint = 200; assert(validateExterior(c,&adjusted).coverFootprint == 200.0f && !adjusted);
    // fExteriorDownStart (re-review n1): default 16, 4..128.
    assert(defaults.downStart == 16.0f);
    c = {}; c.downStart = 1; assert(validateExterior(c,&adjusted).downStart == 4.0f && adjusted);
    c = {}; c.downStart = 500; assert(validateExterior(c).downStart == 128.0f);
    c = {}; c.downStart = nan; assert(validateExterior(c).downStart == 16.0f);
    c = {}; c.downStart = 48; assert(validateExterior(c,&adjusted).downStart == 48.0f && !adjusted);
    // Cell changes (review m4): only exterior to exterior with the rule on keeps hides.
    assert(keepAcrossCells(1,true,true) && keepAcrossCells(2,true,true));
    assert(!keepAcrossCells(0,true,true) && !keepAcrossCells(1,false,true) && !keepAcrossCells(1,true,false) && !keepAcrossCells(1,false,false));
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
    // Ground-standing (review M1): reaches the feet's level with both spans at the footprint or more.
    const Extent e3x = obbExtent(s3);
    assert(!groundStanding(e3x,feet.z,200) && groundStanding(e3x,feet.z,196)); // WRStableStr01 is 196 wide
    assert(!groundStanding(obbExtent(box(stable,{e3.x,e3.y,feet.z+200},r)),feet.z,128)); // lifted: not on the ground
    assert(!groundStanding(Extent{},feet.z,128));
}
// Review M1 counterexamples from the masters (refs, rotations and OBND from the
// reviewer's parse, /tmp/rev-ext): the box column crosses a shell or a wall the
// player stands beside, so the box alone would call it cover.
void groundPieces() {
    ExtConfig c; c.mode = 1;
    // WRHouseShack01 0x021E84 (WhiterunWorld) at the door-exit arrival of 0001A6FE.
    const Vec door{24757.9f,-9727.5f,-3363.3f},doorEye{door.x-237.3f,door.y-237.3f,door.z+683.8f};
    ExtCandidate shell; shell.kind = ExtKind::structure; shell.rayHit = true;
    shell.box = box(shack,{24864.0f,-9888.008f,-3376.0f},rotZ(2.4260077f));
    assert(covers(shell.box,door,doorEye.z,false));                     // the box test alone says cover (the defect)
    assert(!largeStructure(ExtKind::structure,obbExtent(shell.box),1024)); // and the size test passes it
    shell.overhead = true;                                                // even if the upward pick hit its roof:
    assert(classifyExterior(c,shell,door,doorEye.z,false).skip == ExtSkip::ground);
    assert(classifyExterior(c,shell,door,doorEye.z,true).skip == ExtSkip::ground);
    shell.overhead = false;                                               // the usual case: the pick goes up past the wall
    assert(classifyExterior(c,shell,door,doorEye.z,false).skip == ExtSkip::beside);
    // WRWallCurve45 0x089FD7 (Tamriel) / 0x089F6C (WhiterunWorld) at WallLeanMarker 000F7953.
    const Vec lean{29996.8f,-4234.9f,-2730.6f};
    ExtCandidate wall; wall.kind = ExtKind::structure; wall.rayHit = true; wall.overhead = true;
    wall.box = box(wallCurve,{30096.0f,-4496.0f,-2720.0f},rotZ(5.4977875f));
    assert(covers(wall.box,lean,lean.z+683.8f,false) && !largeStructure(ExtKind::structure,obbExtent(wall.box),1024));
    assert(classifyExterior(c,wall,lean,lean.z+683.8f,false).skip == ExtSkip::ground);
    // Raising the footprint past a piece's narrow span lets it through: the key is the owner's trade-off.
    c.coverFootprint = 600;
    assert(classifyExterior(c,wall,lean,lean.z+683.8f,false).reason == Reason::extCover);
    // The stable segment over the player still passes at the default (196 < 256), and only with the pick.
    c.coverFootprint = ExtConfig{}.coverFootprint;
    ExtCandidate s3; s3.kind = ExtKind::structure; s3.rayHit = true; s3.box = box(stable,e3,rotZ(stableAngle));
    assert(classifyExterior(c,s3,feet,eye.z,false).skip == ExtSkip::beside);
    s3.overhead = true;
    assert(classifyExterior(c,s3,feet,eye.z,false).reason == Reason::extCover);
    // What the 256 default admits and refuses (narrow spans as placed, Source: masters via /tmp/rev-ext):
    // ImperialTentSmall 218 and SFarmPorch03 241 pass; NorTentSmall 268, StockadeLeanTo01 267,
    // NorTentLarge 457 and every review counterexample (shack 477, wall curve 538) stay ground.
    const auto ground = [&](float narrow,float wide) {
        Extent e; e.valid = true; e.minZ = feet.z-8; e.maxZ = feet.z+400; e.spanX = wide; e.spanY = narrow;
        return groundStanding(e,feet.z,ExtConfig{}.coverFootprint);
    };
    assert(!ground(196,430) && !ground(218,328) && !ground(241,302) && !ground(248,567) && !ground(255,300));
    assert(ground(256,300) && ground(268,269) && ground(267,270) && ground(457,585) && ground(477,734) && ground(538,987));
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
    // Corridor: three probes at 144 over the feet, reaching 32 past the ground point under the hit.
    const auto probes = openProbes(feet,{feet.x+200,feet.y,feet.z+300});
    for (const auto& p : probes) {
        assert(p.valid && near(p.from.z,feet.z+probeHeight) && near(p.to.z,feet.z+probeHeight));
        assert(near(p.to.x-p.from.x,200+probeBeyond));
    }
    assert(near(probes[0].from.y,feet.y) && near(probes[1].from.y-feet.y,probeSpread,.01f) && near(probes[2].from.y-feet.y,-probeSpread,.01f));
    // Down pick: from 16 below the hit to 160 over the feet; none when the hit is not above that.
    const auto down = downProbe(feet,{feet.x+200,feet.y,feet.z+300},ExtConfig{}.downStart);
    assert(down.valid && near(down.from.z,feet.z+300-16) && near(down.to.z,feet.z+headClear) && near(down.from.x,down.to.x));
    assert(!downProbe(feet,{feet.x,feet.y,feet.z+170},16).valid && !downProbe(feet,{nan,0,0},16).valid);
    // fExteriorDownStart (re-review n1): a deeper start, e.g. 48, clears a roof slab 17-25 thick.
    const auto deep = downProbe(feet,{feet.x+200,feet.y,feet.z+300},48);
    assert(deep.valid && near(deep.from.z,feet.z+252) && !downProbe(feet,{feet.x,feet.y,feet.z+200},48).valid);
    assert(!downProbe(feet,{feet.x,feet.y,feet.z+300},nan).valid);
    // Open only when every corridor probe is clear and the down pick did not meet the same reference.
    assert(openBelow(probes,{true,true,true},true,false) && openBelow(probes,{true,true,true},false,false));
    assert(!openBelow(probes,{false,true,true},true,false) && !openBelow(probes,{true,true,false},true,false));
    assert(!openBelow(probes,{true,true,true},true,true));
    // Straight over the player: no corridor, the down pick alone decides; neither valid: closed (fail closed).
    const auto over = openProbes(feet,{feet.x+10,feet.y,feet.z+300});
    assert(!over[0].valid && openBelow(over,{false,false,false},true,false) && !openBelow(over,{false,false,false},true,true));
    assert(!openBelow(over,{true,true,true},false,false));
    assert(!openProbes(feet,{nan,0,0})[0].valid);
    // Review m1: the stable's end wall WRStableStr01End (484 x 40 x 460) between camera and player.
    // The old probes ended 48 short of the point under the hit and missed a 40-unit wall;
    // the corridor now crosses it (box model), so it is not an open overhang.
    const Vec standing{18858.568f-200.0f,-10622.623f,feet.z};      // 200 units west of end wall 000627F0
    const Obb endWall = box(stableEnd,{18858.568f,-10622.623f,-4584.337f},rotZ(3.2288594f));
    const Vec topHit{18858.568f-20.0f,-10622.623f-88.0f,-4584.337f+451.0f}; // on its top, camera side
    const auto across = openProbes(standing,topHit);
    float a2{},b2{};
    std::array<bool,openProbeCount> clearOfWall{};
    for (unsigned k = 0;k < openProbeCount;++k) clearOfWall[k] = !segmentBox(endWall,across[k].from,across[k].to,0,a2,b2);
    assert(!clearOfWall[0] && !openBelow(across,clearOfWall,true,false));
}

// Re-review M1 (overhang branch) and fix 2 (kit model rule). Sizes as placed
// from the reviewer's master parse; the boxes are placed relative to the feet.
void kitAndOverhang() {
    // Model-path rule.
    for (const char* kit : {"Dungeons\\Dwemer\\Facades\\DweFacadeLiftDown01.nif","Dungeons\\Nordic\\SmHalls\\NorHallSm1wayExSm01.nif",
             "DLC01\\Dungeons\\Castle\\CasSmHall1way01.nif","dlc02/landscape/rocks/x.nif","Landscape\\Rocks\\RockL04.nif",
             "meshes\\Dungeons\\Caves\\Green\\SmallHall\\CaveGHallDeadEnd01.nif","DUNGEONS\\x.nif"})
        assert(kitModel(kit));
    for (const char* other : {"Architecture\\WhiteRun\\WRBuildings\\WRStableStr01.nif","Architecture\\Solitude\\Farms\\SFarmPorch03.nif",
             "Clutter\\Stockade\\StockadeScaffoldBase0Sided02.nif","LandscapeX\\a.nif","DLC01\\Architecture\\a.nif","","dlc\\Dungeons\\a.nif","Dungeons"})
        assert(kitModel(other) == (std::string(other) == "Dungeons"));
    // Floor (third re-review): probe 0 refuses a claim on any hit; an outer probe only on a walkable face.
    const Vec up{0,0,1},downward{0,0,-1},canvas46{.8879f,0,.46f},canvas48{.8773f,0,.48f},slope70{.7141f,0,.7f};
    assert(walkableFace(up) && walkableFace(downward) && walkableFace(slope70)); // the sign is not relied on
    assert(!walkableFace(canvas46) && !walkableFace(canvas48) && !walkableFace({1,0,0}));
    assert(walkableFace({nan,0,1}) && walkableFace({0,0,0}) && walkableFace({0,0,2.0f}) && walkableFace({.5f,0,0})); // fail closed
    for (unsigned k = 0;k < maxFloorProbes;++k) {
        assert(floorClaimHit(k,false) == (k == 0));
        assert(floorClaimHit(k,true));
    }
    assert(floorNearHit(0,feet.z+40,feet.z) && floorNearHit(0,feet.z-150,feet.z));
    assert(floorNearHit(1,feet.z+7.8f,feet.z) && floorNearHit(2,feet.z+3.8f,feet.z) && floorNearHit(4,feet.z-24,feet.z));
    assert(!floorNearHit(2,feet.z+30,feet.z) && !floorNearHit(3,feet.z-40,feet.z) && !floorNearHit(1,feet.z+120,feet.z));
    // ImperialTentSmall 0x0D95CB (328x218x187), the player at its centre: the upward pick meets the canvas
    // (+164..+172 in the reviewer's collision model); the outer probes meet the canvas at normal z .46/.48,
    // not walkable, so no floor contact: cover at the 256 footprint.
    ExtCandidate tent; tent.kind = ExtKind::structure; tent.rayHit = true; tent.overhead = true;
    tent.box = box({-164,-109,-4,164,109,183},{feet.x,feet.y,feet.z});
    tent.floorContact = floorClaimHit(1,walkableFace(canvas46)) || floorClaimHit(2,walkableFace(canvas48));
    assert(!tent.floorContact && classifyExterior(ExtConfig{1},tent,feet,eye.z,false).reason == Reason::extCover);
    // Entrance pieces whose own step, landing or ledge is beside the player (outer probe, walkable face):
    // each is cover over the player and, without the floor refusal, would be hidden (the f42a707 defect).
    struct Entrance { const char* name; float sx,sy,sz,bottom,face; };
    for (const Entrance& e : {Entrance{"MrkTempleDoorR01 067E80",237,520,713,-20,31},Entrance{"WRGuardTowerEntrance01 066352",231,363,469,-66,18},
             Entrance{"MrkKeepDoorwayBalcony01 0B3993",231,455,481,-20,26},Entrance{"RifRmSmExSm01 057F08",155,277,195,-20,44},
             Entrance{"RifRmSmExSm01 104BA2",155,277,195,-20,4},Entrance{"MrkWarrensCliffs04 0B3A29",751,795,469,17,20}}) {
        ExtCandidate x; x.kind = ExtKind::structure; x.rayHit = true; x.overhead = true;
        const int hx = static_cast<int>(e.sx/2),hy = static_cast<int>(e.sy/2);
        x.box = box({-hx,-hy,0,hx,hy,static_cast<int>(e.sz)},{feet.x,feet.y,feet.z+e.bottom});
        ExtConfig cfg; cfg.mode = 1;
        assert(classifyExterior(cfg,x,feet,eye.z,false).reason == Reason::extCover); // f42a707: hidden
        x.floorContact = floorClaimHit(1,walkableFace(up));                         // the step 96 units out, face e.face above the feet
        assert(x.floorContact && e.face < 48.0f && classifyExterior(cfg,x,feet,eye.z,false).skip == ExtSkip::floorContact);
    }
    // WRHouseShack01 at the 0001A6FE door: an outer probe meets its floor (walkable): floor-contact; without it, ground.
    ExtCandidate shackPiece; shackPiece.kind = ExtKind::structure; shackPiece.rayHit = true; shackPiece.overhead = true;
    shackPiece.box = box(shack,{24864.0f,-9888.008f,-3376.0f},rotZ(2.4260077f));
    const Vec doorFeet{24757.9f,-9727.5f,-3363.3f};
    assert(classifyExterior(ExtConfig{1},shackPiece,doorFeet,doorFeet.z+683.8f,false).skip == ExtSkip::ground);
    shackPiece.floorContact = floorClaimHit(4,walkableFace(up));
    assert(classifyExterior(ExtConfig{1},shackPiece,doorFeet,doorFeet.z+683.8f,false).skip == ExtSkip::floorContact);
    // The stable: no outer probe meets a stable segment, cover unchanged.
    ExtCandidate s3c; s3c.kind = ExtKind::structure; s3c.rayHit = true; s3c.overhead = true; s3c.box = box(stable,e3,rotZ(stableAngle));
    s3c.floorContact = floorClaimHit(1,false);
    assert(classifyExterior(ExtConfig{1},s3c,feet,eye.z,false).reason == Reason::extCover);
    ExtConfig c; c.mode = 1;
    const auto piece = [&](float sx,float sy,float sz,float bottom,float ahead,bool kit) {
        ExtCandidate x; x.kind = ExtKind::structure; x.rayHit = true; x.kit = kit; x.openBelow = true;
        const float hx = sx/2,hy = sy/2;
        x.box = box({static_cast<int>(-hx),static_cast<int>(-hy),0,static_cast<int>(hx),static_cast<int>(hy),static_cast<int>(sz)},
            {feet.x+ahead,feet.y,feet.z+bottom});
        x.hit = {feet.x+ahead-hx,feet.y,feet.z+bottom+sz}; // its top edge toward the player
        return x;
    };
    // DweFacadeLiftDown01ice 000A2D70 512x512x960, bottom 112 below the feet, from its own door:
    // the doorway let the overhang branch through in e71a03d; the footprint now guards overhang too.
    auto lift = piece(512,512,960,-112,300,true);
    lift.hit = {feet.x+150,feet.y,feet.z+300};
    assert(overhangHit(lift.hit,feet,false) && !covers(lift.box,feet,feet.z+100,false));
    assert(classifyExterior(c,lift,feet,eye.z,false).skip == ExtSkip::ground);
    lift.kit = false;
    assert(classifyExterior(c,lift,feet,eye.z,false).skip == ExtSkip::ground); // fix 1 alone suffices here
    // DweFacadeHallSm1way01 (230x384x328 as placed) at its door: narrower than the footprint, so only the kit rule stops it.
    auto hall = piece(230,384,328,0,250,true);
    hall.hit = {feet.x+150,feet.y,feet.z+300};
    assert(classifyExterior(c,hall,feet,eye.z,false).skip == ExtSkip::groundKit);
    hall.overhead = true;
    assert(classifyExterior(c,hall,feet,eye.z,false).skip == ExtSkip::groundKit); // cover is guarded the same way
    // The same box from Architecture\ (a porch or stall) still opens.
    auto porch = hall; porch.kit = false; porch.overhead = false;
    assert(classifyExterior(c,porch,feet,eye.z,false).reason == Reason::extOverhang);
    // A dungeon bridge or platform lifted over the player (bottom 250 above the feet) still opens.
    auto bridge = piece(230,600,80,250,150,true);
    bridge.hit = {feet.x+100,feet.y,feet.z+330};
    assert(classifyExterior(c,bridge,feet,eye.z,false).reason == Reason::extOverhang);
    // A Landscape rock over a cave mouth (bottom 200 above the feet) opens; the same rock on the ground does not.
    auto rock = piece(244,321,330,200,120,true);
    rock.hit = {feet.x+60,feet.y,feet.z+530};
    assert(classifyExterior(c,rock,feet,eye.z,false).reason == Reason::extOverhang);
    auto groundRock = piece(244,321,530,0,150,true);
    groundRock.hit = {feet.x+60,feet.y,feet.z+530};
    assert(classifyExterior(c,groundRock,feet,eye.z,false).skip == ExtSkip::groundKit);
    // The stable is Architecture\: cover and overhang unchanged.
    ExtCandidate s3; s3.kind = ExtKind::structure; s3.rayHit = true; s3.overhead = true; s3.box = box(stable,e3,rotZ(stableAngle));
    assert(classifyExterior(c,s3,feet,eye.z,false).reason == Reason::extCover);
    // Trees are never kit-guarded or ground (mode 2 tree cover with the pick).
    c.mode = 2;
    ExtCandidate tree; tree.kind = ExtKind::tree; tree.rayHit = true; tree.overhead = true; tree.kit = true;
    tree.box = box(pine,{feet.x+100,feet.y,feet.z});
    assert(classifyExterior(c,tree,feet,eye.z,false).reason == Reason::extCover);
}

void classify() {
    const auto r = rotZ(stableAngle);
    ExtConfig c; c.mode = 1;
    ExtCandidate s3; s3.kind = ExtKind::structure; s3.box = box(stable,e3,r); s3.rayHit = true; s3.overhead = true;
    // Under the stable roof, and the upward pick hit it: cover.
    assert(classifyExterior(c,s3,feet,eye.z,false).reason == Reason::extCover);
    // The neighbour segment the chest ray crosses: overhang only when open below.
    ExtCandidate s4 = s3; s4.box = box(stable,e4,r); s4.overhead = false;
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
    ExtCandidate shell = s3; shell.box = box(stableHouse,{feet.x,feet.y,feet.z}); // a pick hit it too
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
    // Mode 2: a tree over the player only when the upward pick hit it (collision fact, review m6;
    // with no canopy collision this never happens and canopy below decides), or pierced above the head.
    c.mode = 2;
    assert(classifyExterior(c,tree,feet,eye.z,false).skip == ExtSkip::beside);
    tree.overhead = true;
    assert(classifyExterior(c,tree,feet,eye.z,false).reason == Reason::extCover); // trees are never "ground"
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
    // Exit hysteresis: a hidden cover's box test is 48 looser, but the pick must still hit it.
    const Vec edge{feet.x+(215+30)*std::cos(stableAngle),feet.y-(215+30)*std::sin(stableAngle),feet.z};
    c.mode = 1;
    assert(classifyExterior(c,s3,edge,eye.z,true).reason == Reason::extCover && classifyExterior(c,s3,edge,eye.z,false).skip == ExtSkip::beside);
    ExtCandidate gone = s3; gone.overhead = false;
    assert(classifyExterior(c,gone,edge,eye.z,true).skip == ExtSkip::beside); // then the 0.6 s exit dwell restores it
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
    // the fan geometry (trees). Picks per scan: floor 5 x 2, upward 4, sightline
    // 19 x 4, and per probed overhang 3 corridor + 3 down, for at most 12.
    static_assert(maxFloorProbes*2+4+extRayTargets*extRaySegments+maxOverhangProbed*(openProbeCount+downProbeHits) == 162);
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
    auto& k = s.c;
    for (auto* v : {&k.scans,&k.cells,&k.visited,&k.rays,&k.rayHits,&k.rayNoRef,&k.rayFailed,&k.rayCap,&k.matched,&k.unmatched,
        &k.overhead,&k.probed,&k.open,&k.downSame,&k.probeCap,&k.trees,&k.treeTests,&k.hides,&k.gridSkips,&k.visitCap,&k.cap,
        &k.noWorld,&k.floorFailed}) *v = 9999999999ull;
    for (auto& v : s.c.skips) v = 9999999999ull;
    for (auto& v : s.exits) v = 9999999999ull;
    char line[768]{};
    assert(formatExtSummary(line,sizeof(line),s)); // fits a trace row even with ten-digit counters
    const std::string text(line);
    assert(text.find("event=ext-summary cell=0000961B mode=2 hidden=32") != std::string::npos);
    assert(text.find("skipBeside=9999999999 skipGround=9999999999 skipGroundKit=9999999999 feetZ=-12345.67") != std::string::npos && text.find("sample=cumulative") != std::string::npos);
    assert(text.find("overhead=9999999999 probed=") != std::string::npos && text.find("floorFailed=9999999999") != std::string::npos);
    assert(text.size() < 768);
    assert(extSummaryLines == 7200 && ExtState{}.summaryLines.limit == extSummaryLines);
    char exits[768]{};
    assert(formatExtExits(exits,sizeof(exits),s));
    const std::string second(exits);
    assert(second.find("event=ext-exits cell=0000961B hidden=32 maxHidden=32 gridSkips=9999999999 restores=9999999999 ") != std::string::npos);
    char tight[32]{};
    assert(!formatExtExits(tight,sizeof(tight),s));
    s.conflicts = 9999999999ull; s.foreign = 9999999999ull;
    assert(formatExtExits(exits,sizeof(exits),s));
    assert(std::string(exits).find("restores=9999999999 conflicts=9999999999 foreign=9999999999 exits=9999999999,") != std::string::npos);
    assert(std::string(exits).size() < 768);
    char tiny[32]{};
    assert(!formatExtSummary(tiny,sizeof(tiny),s));
    assert(std::string(extSkipNames[static_cast<std::size_t>(ExtSkip::large)]) == "large");
}
} // namespace
int main() {
    settings();
    boxes();
    cover();
    groundPieces();
    kitAndOverhang();
    large();
    overhang();
    classify();
    fan();
    cells();
    summary();
    return 0;
}
