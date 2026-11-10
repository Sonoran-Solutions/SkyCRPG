// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "fixed_spike_light_policy.h"
#include <cassert>
#include <limits>
#include <string>
using namespace skycrpg::camera;
using namespace skycrpg::camera::fixed;
using namespace skycrpg::camera::fixed::light;
// fixed::Config is the wave 1 rig configuration; this file tests the light keys.
using LightConfig = skycrpg::camera::fixed::light::Config;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
const float inf = std::numeric_limits<float>::infinity();
bool near(float a,float b,float tolerance = 1e-3f) { return std::abs(a-b) <= tolerance; }
bool has(const std::string& text,const std::string& part) { return text.find(part) != std::string::npos; }

void settings() {
    bool adjusted = true;
    const LightConfig defaults = validate(LightConfig{},&adjusted);
    assert(!adjusted && defaults.fillMode == 0 && !defaults.trace);
    assert(defaults.fillHeight == 192.0f && defaults.fillRadius == 640.0f && defaults.fillFade == .6f);
    assert(defaults.fillRed == 1.0f && defaults.fillGreen == .86f && defaults.fillBlue == .70f);
    // Off by default: nothing is allocated, read or installed.
    assert(!configured(defaults));
    LightConfig c; c.trace = true; assert(configured(c));
    c = {}; c.fillMode = 1; assert(configured(c));
    c = {}; c.fillMode = 2; c.fillHeight = -5; c.fillRadius = 1e9f; c.fillFade = 0; c.fillRed = 3; c.fillGreen = -1; c.fillBlue = nan;
    c = validate(c,&adjusted);
    assert(adjusted && c.fillMode == 0 && c.fillHeight == 0.0f && c.fillRadius == 4096.0f && c.fillFade == .1f);
    assert(c.fillRed == 1.0f && c.fillGreen == 0.0f && c.fillBlue == .70f);
    c = {}; c.fillHeight = nan; c.fillRadius = 1; c.fillFade = 99;
    c = validate(c,&adjusted);
    assert(adjusted && c.fillHeight == 192.0f && c.fillRadius == 64.0f && c.fillFade == 4.0f);
    c = {}; c.fillMode = 1; c.fillFade = nan;
    assert(validate(c).fillFade == .6f && validate(c).fillMode == 1);
    // The validated floor stays above the level at which the engine drops a light.
    assert(validate(LightConfig{.fillMode = 1,.fillFade = 0.0f}).fillFade > engineFadeFloor);
}

void wanted() {
    // Every input must hold; mode 0 never wants the light.
    for (unsigned mode : {0u,1u,2u})
        for (bool eligible : {false,true})
            for (bool interior : {false,true})
                for (bool ready : {false,true})
                    assert(fillWanted(mode,eligible,interior,ready) == (mode == 1 && eligible && interior && ready));
    const Vec at = fillPoint({10,20,30},192);
    assert(at.x == 10 && at.y == 20 && at.z == 222);
}

void presence() {
    assert(presenceOf(false,false,false) == Presence::absent);
    assert(presenceOf(true,false,false) == Presence::active);
    assert(presenceOf(true,true,false) == Presence::queuedAdd);   // an add still queued outranks a stale active copy
    assert(presenceOf(true,true,true) == Presence::queuedRemove); // a pending removal outranks everything
    assert(presenceOf(false,false,true) == Presence::queuedRemove);
    assert(std::string(presenceName(Presence::active)) == "active" && std::string(presenceName(Presence::absent)) == "absent");
    assert(std::string(presenceName(Presence::queuedAdd)) == "queued-add" && std::string(presenceName(Presence::queuedRemove)) == "queued-remove");
}

void steps() {
    // Normal life: add once, wait while queued and active, remove when unwanted.
    FillTrack t;
    assert(fillStep(true,Presence::absent,t) == FillAction::add && t.requested && t.drops == 0);
    assert(fillStep(true,Presence::queuedAdd,t) == FillAction::none);
    assert(fillStep(true,Presence::active,t) == FillAction::none && t.drops == 0);
    assert(fillStep(false,Presence::active,t) == FillAction::remove && !t.requested && t.removeRequests == 1);
    assert(fillStep(false,Presence::queuedRemove,t) == FillAction::none); // never a second request while one is queued
    assert(fillStep(false,Presence::absent,t) == FillAction::none && t.removeRequests == 0);
    // Re-engaging after our own removal is not a drop.
    assert(fillStep(true,Presence::absent,t) == FillAction::add && t.drops == 0);

    // An unwanted light that is still only queued for addition is waited out:
    // RemoveLight searches the active arrays only.
    FillTrack q;
    assert(fillStep(true,Presence::absent,q) == FillAction::add);
    assert(fillStep(false,Presence::queuedAdd,q) == FillAction::none && q.removeRequests == 0);
    assert(fillStep(false,Presence::active,q) == FillAction::remove);

    // Wanted again while a removal is queued: wait, then add without a drop.
    FillTrack w;
    assert(fillStep(true,Presence::absent,w) == FillAction::add);
    assert(fillStep(false,Presence::active,w) == FillAction::remove);
    assert(fillStep(true,Presence::queuedRemove,w) == FillAction::none);
    assert(fillStep(true,Presence::absent,w) == FillAction::add && w.drops == 0);

    // The engine dropping the light while wanted (cell reset) is re-added a few
    // times per cell, then refused until the cell changes.
    FillTrack d;
    assert(fillStep(true,Presence::absent,d) == FillAction::add);
    for (unsigned i = 1;i <= maxDropsPerCell;++i) {
        assert(fillStep(true,Presence::active,d) == FillAction::none);
        assert(fillStep(true,Presence::absent,d) == FillAction::add && d.drops == i);
    }
    assert(fillStep(true,Presence::absent,d) == FillAction::dropLimit && d.gaveUp);
    assert(fillStep(true,Presence::absent,d) == FillAction::none); // refused once, then quiet
    d.newCell();
    assert(!d.gaveUp && d.drops == 0 && fillStep(true,Presence::absent,d) == FillAction::add);

    // RemoveLight that never takes (+300 false) is retried a bounded number of
    // times, then the light stays hidden and nothing more is asked.
    FillTrack s;
    assert(fillStep(true,Presence::absent,s) == FillAction::add);
    for (unsigned i = 0;i < maxRemoveRequests;++i) assert(fillStep(false,Presence::active,s) == FillAction::remove);
    assert(fillStep(false,Presence::active,s) == FillAction::removeLimit && s.stuck);
    assert(fillStep(false,Presence::active,s) == FillAction::none);
    assert(fillStep(false,Presence::absent,s) == FillAction::none && !s.stuck && s.removeRequests == 0);

    // Unwanted and never added: nothing to do.
    FillTrack idle;
    for (const auto p : {Presence::absent,Presence::queuedAdd,Presence::queuedRemove})
        assert(fillStep(false,p,idle) == FillAction::none);
}

void census() {
    // reach: distance from the sphere to the point; inside is <= 0.
    assert(near(reach({0,0,0},100,{50,0,0}),-50) && near(reach({0,0,0},100,{100,0,0}),0) && near(reach({0,0,0},100,{0,300,0}),200));
    assert(reach({nan,0,0},100,{0,0,0}) == inf && reach({0,0,0},nan,{0,0,0}) == inf);
    const LightView live{{0,0,0},256,1.0f,0u,false};
    assert(live.live());
    assert(!LightView({{0,0,0},256,1.0f,culled,false}).live() && !LightView({{0,0,0},256,1.0f,0u,true}).live());
    assert(!LightView({{0,0,0},256,0.0f,0u,false}).live());

    const Vec at{0,0,0};
    Census c;
    assert(c.point == 0 && c.pointNearest == inf && c.shadowNearest == inf && c.starved() == 0);
    c.addPoint({{100,0,0},256,1.0f,0u,false},at);       // contains the point, lit
    c.addPoint({{0,100,0},256,1.0f,culled,false},at);    // contains it, culled by the camera
    c.addPoint({{0,0,100},256,.5f,0u,false},at);         // contains it, fading with camera distance
    c.addPoint({{1000,0,0},256,1.0f,0u,false},at);       // out of reach
    c.addPoint({{0,0,-50},256,1.0f,0u,true},at);         // contains it, app-culled
    assert(c.point == 5 && c.pointCulled == 1 && c.pointDimmed == 1 && c.pointReach == 4 && c.pointReachLive == 2);
    assert(near(c.pointNearest,-206));
    c.addShadow({{0,0,10},512,1.0f,0u,false},2u,at);         // in reach, holds channel 2
    c.addShadow({{0,0,20},512,1.0f,0u,false},noChannel,at);  // in reach, no channel: contributes nothing
    c.addShadow({{2000,0,0},512,1.0f,0u,false},1u,at);       // far, holds a channel
    c.addShadow({{0,30,0},512,1.0f,0u,true},0u,at);          // hidden: a channel does not light it
    assert(c.shadow == 4 && c.shadowChannel == 2 && c.shadowReach == 3 && c.shadowReachChannel == 1 && c.starved() == 2);
    assert(near(c.shadowNearest,-502));
}

void lines() {
    LightSample s;
    s.seconds = 12.5; s.frame = 77; s.cell = 0x371de; s.interior = true; s.eligible = true; s.wanted = true;
    s.eye = {2048,-9216,-2492}; s.camera = {1810.7f,-9453.3f,-1808.2f};
    s.census.addPoint({{2100,-9200,-2500},256,1.0f,0u,false},s.eye);
    s.census.addShadow({{0,0,0},512,1.0f,0u,false},noChannel,s.eye);
    s.fill.created = true; s.fill.hidden = false; s.fill.found = true; s.fill.cull = 0; s.fill.dimmer = 1.0f;
    s.fill.presence = Presence::active; s.fill.at = {2048,-9216,-2364}; s.fill.adds = 1;
    char line[768]{};
    assert(formatLight(line,sizeof(line),s));
    const std::string text(line);
    for (const char* part : {"FixedLight t=12.500 frame=77 cell=000371DE interior=1 eligible=1 ","eye=2048.0,-9216.0,-2492.0 ",
        "point=1 pointCulled=0 pointDimmed=0 pointReach=1 pointReachLive=1 ","shadow=1 shadowChannel=0 shadowReach=0 ",
        "shadowStarved=0 ","fillWanted=1 fill=active fillCreated=1 fillHidden=0 fillCull=0 fillDimmer=1.000 ",
        "fillAt=2048.0,-9216.0,-2364.0 adds=1 removes=0 drops=0 sample=state"})
        assert(has(text,part));
    LightSample none;
    assert(formatLight(line,sizeof(line),none));
    assert(has(line,"pointNearest=inf") && has(line,"fillCull=999 fillDimmer=nan") && has(line,"fill=absent"));
    char tiny[16]{};
    assert(!formatLight(tiny,sizeof(tiny),s));
    assert(formatFillEvent(line,sizeof(line),3.25,9,"hide","menu",Presence::active,2,1,0));
    assert(std::string(line) == "FixedLight t=3.250 frame=9 event=hide cause=menu presence=active adds=2 removes=1 drops=0");
    assert(!formatFillEvent(tiny,sizeof(tiny),3.25,9,"hide","menu",Presence::active,2,1,0));
    assert(lightTraceLines > 0 && culled == 0xffu && noChannel == 0xffu);
}
} // namespace
int main() {
    settings();
    wanted();
    presence();
    steps();
    census();
    lines();
    return 0;
}
