// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "fixed_spike_policy.h"
#include <cassert>
#include <limits>
#include <string>
#include <vector>
using namespace skycrpg::camera;
using namespace skycrpg::camera::fixed;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool near(float a,float b,float tolerance = 1e-4f) { return std::abs(a-b) <= tolerance; }
const std::array allStates{State::disabled,State::invalidWorld,State::special,State::menu,State::firstPerson,
    State::precision,State::targeting,State::obstruction,State::normal,State::targetLock};

void settings() {
    const Settings base;
    bool adjusted = true;
    const Config defaults = validate(Config{},base,48,&adjusted);
    assert(!adjusted && !defaults.enabled && defaults.heading == 45.0f && defaults.snapSeconds == .25f && defaults.zoom == .65f);
    assert(defaults.snapLeftKey == 51 && defaults.snapRightKey == 52 && defaults.castProbe);
    assert(defaults.roomSeedMode == 0 && defaults.seeThroughMode == 0 && defaults.cutHeight == 160.0f);
    assert(!defaults.rigTrace && !defaults.roomTrace && !defaults.hideTrace);
    Config c;
    c.heading = -90; c.snapSeconds = 0; c.zoom = 5; c.roomSeedMode = 9; c.seeThroughMode = 2; c.cutHeight = nan;
    c.roomSeedRadius = -1; c.hidePerFrame = 0; c.hideTotal = 100000;
    c = validate(c,base,48,&adjusted);
    assert(adjusted && c.heading == 270.0f && c.snapSeconds == .05f && c.zoom == 1.0f && c.roomSeedMode == 0);
    assert(c.seeThroughMode == 0 && c.cutHeight == 160.0f && c.roomSeedRadius == 0.0f && c.hidePerFrame == 1 && c.hideTotal == maxHidden);
    c = {}; c.heading = nan; assert(validate(c,base,48).heading == 45.0f);
    c = {}; c.heading = 405; assert(validate(c,base,48).heading == 45.0f);
    // Reserved or owned keys are unbound, never silently remapped.
    for (const int taken : {66,37,38,22,26,27,64,65,48,0,256,-2}) {
        c = {}; c.snapLeftKey = taken;
        assert(validate(c,base,48,&adjusted).snapLeftKey == -1 && adjusted);
        c = {}; c.snapRightKey = taken;
        assert(validate(c,base,48).snapRightKey == -1);
    }
    c = {}; c.snapLeftKey = 48; assert(validate(c,base,-1).snapLeftKey == 48); // cursor mode off: B is free
    c = {}; c.snapRightKey = c.snapLeftKey; assert(validate(c,base,48).snapRightKey == -1);
    c = {}; c.snapLeftKey = -1; c.snapRightKey = -1;
    assert(validate(c,base,48,&adjusted).snapLeftKey == -1 && !adjusted);
    assert(snapDirection(defaults,51) == -1 && snapDirection(defaults,52) == 1 && snapDirection(defaults,50) == 0);
    assert(snapDirection(c,-1) == 0); // an unbound key never matches
}

void headings() {
    // Exactly four headings, 90 degrees apart, from the configured one.
    for (const float base : {45.0f,0.0f,300.0f,359.5f}) {
        for (unsigned i = 0;i < 4;++i) {
            assert(near(wrap(headingRadians(base,i)-radians(base)-radians(90.0f*static_cast<float>(i))),0));
            assert(headingRadians(base,i) == headingRadians(base,i+4));
        }
    }
    assert(near(degrees(headingRadians(45,0)),45) && near(degrees(headingRadians(45,1)),135));
    assert(near(degrees(headingRadians(45,2)),-135) && near(degrees(headingRadians(45,3)),-45));
    assert(ease(0) == 0 && ease(1) == 1 && ease(-1) == 0 && ease(2) == 1 && near(ease(.5f),.5f));
    for (float t = 0;t < 1;t += .05f) assert(ease(t) <= ease(t+.05f)); // monotonic: no overshoot

    SnapTurn turn;
    assert(turn.target(45,10,.25f) == headingRadians(45,0) && !turn.turning);
    turn.snap(1,45,10,.25f);
    assert(turn.index == 1 && turn.turning && near(turn.remaining,pi/2));
    assert(near(turn.target(45,10,.25f),headingRadians(45,0))); // continuous at the press
    float previous = 0;
    for (double t = 10;t < 10.25;t += .01) {
        const float travelled = wrap(turn.target(45,t,.25f)-headingRadians(45,0));
        assert(travelled >= previous-1e-5f && travelled <= pi/2+1e-4f);
        previous = travelled;
    }
    // Lands on the heading itself: bit-identical to the definition, then stays.
    assert(turn.target(45,10.25,.25f) == headingRadians(45,1) && !turn.turning);
    assert(turn.target(45,99,.25f) == headingRadians(45,1));
    // Left from heading 1 returns to heading 0 the short way.
    turn.snap(-1,45,20,.25f);
    assert(turn.index == 0 && near(turn.remaining,-pi/2));
    assert(turn.target(45,21,.25f) == headingRadians(45,0));
    // Presses during a turn keep the direction and still end on a heading.
    turn.reset();
    turn.snap(1,45,0,.25f); turn.snap(1,45,.1,.25f); turn.snap(1,45,.15,.25f);
    assert(turn.index == 3 && turn.remaining > pi && turn.remaining < 1.5f*pi+1e-3f);
    assert(turn.target(45,.4,.25f) == headingRadians(45,3));
    turn.reset();
    turn.snap(1,45,0,.25f); turn.snap(-1,45,.1,.25f); // reversed mid-turn
    assert(turn.index == 0 && turn.remaining < 0 && turn.remaining > -pi/2);
    assert(turn.target(45,1,.25f) == headingRadians(45,0));
    for (unsigned i = 0;i < 9;++i) turn.snap(1,45,2+.01*i,.25f);
    assert(turn.index == 1 && turn.target(45,5,.25f) == headingRadians(45,1));
    turn.snap(0,45,6,.25f); assert(turn.index == 1 && !turn.turning); // not a snap key
    // A broken clock or duration ends the turn on the heading, never between.
    turn.snap(1,45,7,.25f);
    assert(turn.target(45,std::numeric_limits<double>::quiet_NaN(),.25f) == headingRadians(45,2) && !turn.turning);
    turn.snap(1,45,8,0);
    assert(turn.target(45,8,0) == headingRadians(45,3));
    // F7: shortest way back to the configured heading.
    turn.home(45,9,.25f);
    assert(turn.index == 0 && turn.turning && near(turn.remaining,pi/2));
    assert(turn.target(45,10,.25f) == headingRadians(45,0));
    turn.home(45,11,.25f); assert(!turn.turning); // already there

    // The look request lands world heading (player + freeRotation.x) on target.
    const float gain = 2.5f,dt = 1.0f/60;
    for (const float player : {0.0f,1.0f,-3.0f,6.0f}) for (const float freeX : {0.0f,.4f,-2.0f,3.1f}) {
        const float target = headingRadians(45,2);
        const float input = holdYawInput(target,player,freeX,gain,dt);
        assert(near(wrap(player+freeX+input*gain*dt-target),0,2e-5f));
    }
    // The player turning (TDM changes both terms) needs no request at all.
    assert(near(holdYawInput(1.0f,.25f,.75f,gain,dt),0));
    for (const float bad : {0.0f,-1.0f,nan,.2f}) assert(holdYawInput(1,0,0,gain,bad) == 0);
    assert(holdYawInput(1,0,0,0,dt) == 0 && holdYawInput(1,0,0,nan,dt) == 0 && holdYawInput(nan,0,0,gain,dt) == 0);
    // Player yaw input is discarded; a POV slide keeps the raw x the engine cancels.
    assert(rigYawInput(false,123.0f,.5f) == .5f && rigYawInput(true,123.0f,.5f) == 123.5f);
}

void eligibility() {
    for (const auto state : allStates) {
        assert(rigEligible(true,state) == (state == State::normal || state == State::targetLock));
        assert(!rigEligible(false,state)); // disabled equals base
    }
    Context c;
    c.enabled = c.prerequisites = c.world = true;
    assert(rigEligible(true,select(c)));
    c.targetLock = true; assert(rigEligible(true,select(c)));
    for (auto field : {&Context::special,&Context::menu,&Context::firstPerson,&Context::precision}) {
        Context off = c; off.*field = true;
        assert(!rigEligible(true,select(off)));
    }
    c.slide = true; assert(rigEligible(true,select(c))); // a POV slide does not hand the view back
    // Zoom hold: the fixed scalar while the rig holds, the base request otherwise.
    assert(heldZoom(true,.65f,.3f) == .65f && heldZoom(false,.65f,.3f) == .3f);
    // Disabled equals base: obstruction sampled, every cast native.
    assert(obstructionSampled(false) && !obstructionSampled(true));
    for (unsigned mask = 0;mask < 16;++mask)
        assert((castDecision((mask&1)!=0,(mask&2)!=0,(mask&4)!=0,(mask&8)!=0) == Cast::suppress) == (mask == 15));
    assert(castDecision(false,true,true,true) == Cast::native);
}

void rigTrace() {
    RigSample s;
    s.seconds = 12.5; s.frame = 77; s.eligible = true; s.state = static_cast<unsigned>(State::normal);
    s.camera = {106,208,300}; s.player = {96,188,40}; s.previousCamera = {100,200,300}; s.preCollision = {100,200,300};
    s.goal = {103,204,300}; s.pair = true;
    s.heading = 44.99f; s.target = 45; s.pitch = 55; s.zoom = .65f; s.heldZoom = .65f; s.suppressed = 12; s.wouldHit = 5;
    char line[768]{};
    assert(formatRig(line,sizeof(line),s));
    const std::string text = line;
    for (const char* field : {"FixedRig t=12.500","frame=77","eligible=1","state=8","cam=106.00,208.00,300.00","player=96.00,188.00,40.00",
        "offset=10.00,20.00,260.00","prevCam=100.00,200.00,300.00","prevPre=100.00,200.00,300.00","pair=1","pullIn=0.000",
        "heading=44.990","target=45.000","headingError=-0.010","pitch=55.000",
        "zoom=0.6500","suppressed=12","nativeWouldHit=5","passedNative=0","passedHit=0"})
        assert(text.find(field) != std::string::npos);
    // Pull-in pairs one frame's rendered position with that frame's
    // pre-collision position: the camera moving 10 units since then (cam) or
    // trailing its goal (follow lag) does not enter it.
    s.previousCamera = {100,200,280};
    assert(formatRig(line,sizeof(line),s) && std::string(line).find("pullIn=20.000") != std::string::npos);
    assert(span(s.previousCamera,s.preCollision) > pullInTolerance);
    // No pair (first eligible frame, or a frame was skipped): never a number.
    s.pair = false;
    assert(formatRig(line,sizeof(line),s) && std::string(line).find("pair=0 pullIn=nan") != std::string::npos);
    assert(pullInPair(true,76,77) && !pullInPair(false,76,77) && !pullInPair(true,75,77) && !pullInPair(true,77,77));
    // The window sees every eligible frame, not only the sampled one.
    RigWindow window;
    for (const float distance : {0.0f,.2f,pullInTolerance}) window.pull(distance);
    assert(window.pairs == 3 && window.pullInFrames == 0 && window.maxPullIn == pullInTolerance);
    window.pull(37.5f); window.pull(nan);
    assert(window.pairs == 5 && window.pullInFrames == 2 && window.maxPullIn == 37.5f);
    window.heading(-.004f); window.heading(.002f);
    assert(window.frames == 2 && near(window.maxHeadingError,.004f));
    window.heading(nan); assert(std::isnan(window.maxHeadingError));
    s.window = {}; s.window.pull(12); s.window.heading(-1.5f);
    assert(formatRig(line,sizeof(line),s));
    assert(std::string(line).find("frames=1 pairs=1 pullInFrames=1 maxPullIn=12.000 maxHeadingError=1.500 sample=window") != std::string::npos);
    assert(near(headingError(179,-179),-2) && near(headingError(-179,179),2));
    char small[16]{};
    assert(!formatRig(small,sizeof(small),s));
}

void rooms() {
    RoomView view;
    view.viewer = {0,0,500}; view.eye = {0,0,64}; view.radius = 300; view.tolerance = viewerTolerance;
    const auto inside = [](float low,float high) { return [=](Vec p) { return p.z >= low && p.z <= high; }; };
    const Vec asked{0,0,490}; // camera + near * forward
    // Mode 0 never claims; neither does an unknown mode.
    for (const unsigned mode : {0u,4u,99u}) { view.mode = mode; assert(!seedClaim(view,asked,{},0,inside(-1e9f,1e9f))); }
    // Mode 1: only the room that holds the player.
    view.mode = 1;
    assert(seedClaim(view,asked,{},0,inside(0,128)) && !seedClaim(view,asked,{},0,inside(200,300)));
    // Mode 2: any room crossed by the sightline samples (eye, .75, .5, .25).
    view.mode = 2;
    assert(seedClaim(view,asked,{},0,inside(0,128)));     // player end
    assert(seedClaim(view,asked,{},0,inside(250,300)));   // midpoint 277
    assert(seedClaim(view,asked,{},0,inside(380,390)));   // quarter point 383.5
    assert(!seedClaim(view,asked,{},0,inside(400,480)));  // between samples and the camera
    std::vector<float> order;
    assert(!seedClaim(view,asked,{},0,[&](Vec p) { order.push_back(p.z); return false; }));
    assert(order.size() == 4 && near(order[0],64) && order[1] < order[2] && order[2] < order[3] && near(order[3],383.5f));
    // Mode 3: rooms whose bound reaches within the radius of the player.
    view.mode = 3;
    const auto never = [](Vec) { return false; };
    assert(seedClaim(view,asked,{0,350,64},100,never) && !seedClaim(view,asked,{0,450,64},100,never));
    assert(!seedClaim(view,asked,{nan,0,0},100,never) && !seedClaim(view,asked,{0,0,0},nan,never));
    // Queries that are not the main view (reference-to-room attach, other
    // cameras) keep the vanilla answer in every mode.
    for (const unsigned mode : {1u,2u,3u}) {
        view.mode = mode;
        assert(!seedClaim(view,{0,0,64},{0,0,64},1e6f,inside(-1e9f,1e9f)));
        assert(!seedClaim(view,{nan,0,0},{0,0,64},1e6f,inside(-1e9f,1e9f)));
    }
    assert(viewerQuery(view,{0,0,500+viewerTolerance}) && !viewerQuery(view,{0,0,500+viewerTolerance+1}));
    view.mode = 1; view.eye = {nan,0,0};
    assert(!seedClaim(view,asked,{},0,inside(-1e9f,1e9f)));
    RoomSample s;
    s.mode = 2; s.hook = s.active = s.graph = true; s.rooms = 14; s.playerRoom = 3; s.cell = 0x1605E; s.claims = 9; s.viewerCalls = 28;
    char line[640]{};
    assert(formatRoom(line,sizeof(line),s));
    const std::string text = line;
    for (const char* field : {"mode=2","hook=1","active=1","cell=0001605E","graph=1","rooms=14","playerRoom=3","cameraRoom=-1",
        "viewerCalls=28","claims=9"}) assert(text.find(field) != std::string::npos);
}

void hide() {
    const float feet = 100,cut = feet+160;
    const Vec upright{0,0,1};
    // A ceiling tile: thin, wide, wholly above the cut.
    const Extent ceiling = worldExtent({-256,-256,0},{256,256,16},upright,1,300);
    assert(ceiling.valid && ceiling.minZ == 300 && ceiling.maxZ == 316 && ceiling.spanX == 512 && ceiling.spanY == 512);
    assert(classify(Kind::architecture,ceiling,false,false,false,cut,feet,false).reason == Reason::aboveCut);
    assert(classify(Kind::object,ceiling,false,false,false,cut,feet,false).reason == Reason::aboveCut); // upper-floor clutter too
    // Never an actor, the player or any other excluded kind, wherever it is.
    for (const bool ray : {false,true}) {
        const auto v = classify(Kind::excluded,ceiling,false,ray,false,cut,feet,false);
        assert(v.reason == Reason::none && v.skip == Skip::kind);
    }
    // A wall from the floor up: below the cut, so only a ray can hide it.
    const Extent wall = worldExtent({-128,-8,0},{128,8,320},upright,1,feet);
    assert(wall.minZ == 100 && wall.maxZ == 420);
    assert(classify(Kind::architecture,wall,false,false,false,cut,feet,false).reason == Reason::none);
    assert(classify(Kind::architecture,wall,false,true,false,cut,feet,false).reason == Reason::onRay);
    assert(classify(Kind::object,wall,false,true,false,cut,feet,false).reason == Reason::none); // rays hide architecture only
    // Anything below the cut that no ray hits is left alone.
    const Extent table = worldExtent({-40,-40,0},{40,40,60},upright,1,feet);
    assert(classify(Kind::architecture,table,false,false,false,cut,feet,false).reason == Reason::none);
    // The cut is tested on the whole box: just below stays, at the cut hides.
    const Extent justBelow = worldExtent({-10,-10,0},{10,10,50},upright,1,cut-.5f);
    const Extent atCut = worldExtent({-10,-10,0},{10,10,50},upright,1,cut);
    assert(classify(Kind::architecture,justBelow,false,false,false,cut,feet,false).reason == Reason::none);
    assert(classify(Kind::architecture,atCut,false,false,false,cut,feet,false).reason == Reason::aboveCut);
    // Hysteresis in height: once hidden it stays hidden a margin below the cut.
    assert(classify(Kind::architecture,justBelow,false,false,false,cut,feet,true).reason == Reason::aboveCut);
    const Extent wellBelow = worldExtent({-10,-10,0},{10,10,50},upright,1,cut-cutExitMargin-1);
    assert(classify(Kind::architecture,wellBelow,false,false,false,cut,feet,true).reason == Reason::none);
    // The floor the player stands on is never hidden, even on a ray.
    const Extent floorTile = worldExtent({-256,-256,-16},{256,256,0},upright,1,feet);
    auto v = classify(Kind::architecture,floorTile,false,true,true,cut,feet,false);
    assert(v.reason == Reason::none && v.skip == Skip::floorContact);
    // One-piece cave or room mesh: floor and ceiling in one box. Its bottom is
    // below the cut, so "above the cut" never applies, and a ray hit is
    // refused because hiding it would remove floor.
    const Extent cave = worldExtent({-600,-500,-40},{600,500,700},upright,1,feet);
    assert(classify(Kind::architecture,cave,false,false,false,cut,feet,false).reason == Reason::none);
    v = classify(Kind::architecture,cave,false,true,false,cut,feet,false);
    assert(v.reason == Reason::none && v.skip == Skip::floorSpan);
    // A column reaches the floor but has no room-sized footprint: it may go.
    const Extent column = worldExtent({-40,-40,0},{40,40,400},upright,1,feet);
    assert(classify(Kind::architecture,column,false,true,false,cut,feet,false).reason == Reason::onRay);
    // A big piece that starts above the floor (an arch, a balcony) may go.
    const Extent balcony = worldExtent({-300,-300,0},{300,300,40},upright,1,feet+floorReach+1);
    assert(classify(Kind::architecture,balcony,false,true,false,cut,feet,false).reason == Reason::onRay);
    // No OBND: the sphere can prove "wholly above" but never clears a ray hit.
    assert(!worldExtent({0,0,0},{0,0,0},upright,1,0).valid && !worldExtent({1,0,0},{0,1,1},upright,1,0).valid);
    assert(!worldExtent({0,0,0},{1,1,1},upright,0,0).valid && !worldExtent({0,0,0},{1,1,nan},upright,1,0).valid);
    const Extent high = sphereExtent(600,100),low = sphereExtent(300,100);
    assert(high.valid && high.minZ == 500 && !sphereExtent(0,0).valid && !sphereExtent(nan,1).valid);
    assert(classify(Kind::architecture,high,true,false,false,cut,feet,false).reason == Reason::aboveCut);
    v = classify(Kind::architecture,low,true,true,false,cut,feet,false);
    assert(v.reason == Reason::none && v.skip == Skip::noBounds);
    v = classify(Kind::architecture,Extent{},false,true,false,cut,feet,false);
    assert(v.reason == Reason::none && v.skip == Skip::noBounds);
    // Rotated and scaled boxes: z range follows the world rotation's third row.
    const Extent tipped = worldExtent({0,0,0},{100,10,10},{1,0,0},2,50); // local x points up
    assert(tipped.minZ == 50 && tipped.maxZ == 250 && tipped.spanX == 200);
    const Extent flipped = worldExtent({0,0,0},{10,10,100},{0,0,-1},1,50); // upside down
    assert(flipped.minZ == -50 && flipped.maxZ == 50);
    assert(std::string(reasonName(Reason::aboveCut)) == "above-cut" && std::string(reasonName(Reason::onRay)) == "on-ray");
    assert(std::string(skipName(Skip::floorSpan)) == "floor-span" && std::string(skipName(Skip::floorContact)) == "floor-contact");
}

void dwellAndCaps() {
    Track t;
    t.seen(10.0);
    assert(!t.shouldHide(10.0)); // one scan is not enough
    t.seen(10.1); assert(!t.shouldHide(10.1));
    t.seen(10.2); assert(t.shouldHide(10.2));
    assert(!t.shouldHide(10.3)); // not seen in the 10.3 scan
    t.hidden = true;
    assert(!t.shouldHide(10.2) && !t.shouldRestore(10.5) && t.shouldRestore(10.8));
    t.seen(10.7); assert(!t.shouldRestore(10.8)); // seen again inside the exit dwell
    // A gap longer than the exit dwell restarts the enter dwell.
    Track flicker;
    flicker.seen(1.0); flicker.seen(1.1); flicker.seen(2.0);
    assert(!flicker.shouldHide(2.0) && flicker.first == 2.0);
    assert(!flicker.expired(2.5) && flicker.expired(2.7));
    flicker.hidden = true; assert(!flicker.expired(99)); // a hidden slot is restored, never just dropped

    HideBudget b;
    b.perFrame = 2; b.total = 3;
    assert(b.take() && b.take() && !b.take() && b.frameCap == 1 && b.hidden == 2);
    b.frame();
    assert(b.take() && !b.take() && b.totalCap == 1 && b.hidden == 3); // total cap wins
    b.frame(); b.release();
    assert(b.take() && b.hidden == 3);
    for (unsigned i = 0;i < 5;++i) b.release();
    assert(b.hidden == 0);

    CutPlane cut;
    assert(cut.update(100,0) == 100 && cut.valid);
    assert(cut.update(100+CutPlane::band,1) == 100); // inside the band
    // A jump: outside the band for less than the settle time.
    assert(cut.update(160,2.0) == 100 && cut.update(170,2.3) == 100 && cut.update(101,2.5) == 100);
    assert(cut.update(170,3.0) == 100 && cut.update(170,3.5) == 100); // the timer restarted after landing
    // Stairs: it follows once the feet have settled elsewhere, up or down.
    assert(cut.update(180,3.61) == 180);
    assert(cut.update(60,5.0) == 180 && cut.update(40,5.7) == 40);
    assert(cut.update(nan,6) == 40);
    cut.reset(); assert(!cut.valid && cut.update(-500,7) == -500);

    const auto targets = rayTargets({10,20,30});
    assert(targets.size() == maxRayTargets);
    for (const auto p : targets) assert(p.z >= 30+40 && p.z <= 30+120 && std::hypot(p.x-10,p.y-20) <= 48.01f); // never the floor
    assert(near(std::hypot(targets[3].x-10,targets[3].y-20),48) && near(std::hypot(targets[10].x-10,targets[10].y-20),48,1e-3f));
    const auto probes = floorProbes({10,20,30});
    assert(probes[0].x == 10 && probes[1].x == 106 && probes[4].y == -76 && probes[2].z == 30);
    // Marching a ray: a bounded step past each hit, never to or beyond the end.
    const auto next = rayContinue({0,0,0},{100,0,0},.5f);
    assert(next && near(next->x,58));
    assert(!rayContinue({0,0,0},{100,0,0},.95f) && !rayContinue({0,0,0},{4,0,0},.1f));
    assert(!rayContinue({0,0,0},{100,0,0},nan) && !rayContinue({0,0,0},{100,0,0},1.5f) && !rayContinue({nan,0,0},{100,0,0},.5f));
    Vec from{0,0,0}; unsigned segments = 0;
    while (const auto step = rayContinue(from,{100,0,0},0)) { from = *step; ++segments; assert(segments < 64); }
    assert(segments == 12 && near(from.x,96,1e-3f)); // even a zero-fraction hit always makes progress
}

void restores() {
    assert(exitNames.size() == static_cast<std::size_t>(Exit::count));
    // Every lifecycle exit in the brief has its own cause and is immediate.
    for (const auto cause : {Exit::disable,Exit::f8,Exit::firstPerson,Exit::menu,Exit::load,Exit::cell,Exit::unload,
        Exit::death,Exit::special,Exit::expired}) assert(restoreNow(cause,0));
    assert(std::string(exitNames[static_cast<std::size_t>(Exit::f8)]) == "f8");
    assert(std::string(exitNames[static_cast<std::size_t>(Exit::firstPerson)]) == "first-person");
    assert(std::string(exitNames[static_cast<std::size_t>(Exit::death)]) == "death");
    assert(std::string(exitNames[static_cast<std::size_t>(Exit::expired)]) == "expired");
    // A camera update that silently did not run gets a short grace only.
    assert(!restoreNow(Exit::hookSilent,1) && !restoreNow(Exit::hookSilent,2) && restoreNow(Exit::hookSilent,silentGraceFrames));
    assert(firstExit(exitBit(Exit::menu)) == Exit::menu && firstExit(exitBit(Exit::load)|exitBit(Exit::cell)) == Exit::load);
    assert(firstExit(exitBit(Exit::cell)|exitBit(Exit::f8)) == Exit::f8 && firstExit(0) == Exit::special);
    // Only our own write is undone: visible before, still hidden now.
    assert(restoreWrite(true,false) && !restoreWrite(false,false) && !restoreWrite(true,true) && !restoreWrite(false,true));
    HideCounters counters;
    assert(counters.exits.size() == static_cast<std::size_t>(Exit::count) && counters.skips.size() == 5);
}

void trace() {
    LineBudget lines{2};
    assert(lines.take() && lines.take() && !lines.take() && lines.used == 2);
    LineBudget off;
    assert(!off.take());
    const auto path = std::filesystem::temp_directory_path()/"skycrpg_fixed_spike_trace_test.log";
    {
        TraceWriter writer;
        assert(!writer.append("before start")); // no storage, no write
        assert(!writer.start(path,0));
        assert(writer.start(path,3) && !writer.start(path,3));
        assert(writer.append("FixedRig one") && writer.append("FixedRoom two") && writer.append("FixedHide three"));
        assert(!writer.append("over capacity") && !writer.append(nullptr));
        writer.stop();
    }
    std::ifstream file(path);
    std::string a,b,c,d;
    std::getline(file,a); std::getline(file,b); std::getline(file,c);
    assert(a == "FixedRig one" && b == "FixedRoom two" && c == "FixedHide three" && !std::getline(file,d));
    file.close();
    std::filesystem::remove(path);
    const std::string longText(2000,'x');
    TraceWriter clipped;
    const auto clippedPath = std::filesystem::temp_directory_path()/"skycrpg_fixed_spike_trace_clip.log";
    assert(clipped.start(clippedPath,1) && clipped.append(longText.c_str()));
    clipped.stop();
    std::ifstream clippedFile(clippedPath);
    std::getline(clippedFile,a);
    assert(a.size() == std::tuple_size<TraceRow>::value-1); // truncated, always terminated
    clippedFile.close();
    std::filesystem::remove(clippedPath);
}
} // namespace
int main() {
    settings();
    headings();
    eligibility();
    rigTrace();
    rooms();
    hide();
    dwellAndCaps();
    restores();
    trace();
    return 0;
}
