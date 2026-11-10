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
    // Unwanted calls carry a clock; the light is handed back only after it has
    // been unwanted for removeDelaySeconds (it is app-culled at once by the adapter).
    const double late = 10.0; // far past any delay
    // Normal life: add once, wait while queued and active, remove when unwanted.
    FillTrack t;
    assert(fillStep(true,Presence::absent,t,0) == FillAction::add && t.requested && t.drops == 0);
    assert(fillStep(true,Presence::queuedAdd,t,0) == FillAction::none);
    assert(fillStep(true,Presence::active,t,0) == FillAction::none && t.drops == 0);
    assert(fillStep(false,Presence::active,t,1.0) == FillAction::none && !t.requested && t.unwantedSince == 1.0); // delay starts
    assert(fillStep(false,Presence::active,t,1.0+removeDelaySeconds*.5) == FillAction::none);
    assert(fillStep(false,Presence::active,t,1.0+removeDelaySeconds) == FillAction::remove && t.removeRequests == 1);
    assert(fillStep(false,Presence::queuedRemove,t,late) == FillAction::none); // never a second request while one is queued
    assert(fillStep(false,Presence::absent,t,late) == FillAction::none && t.removeRequests == 0);
    // Re-engaging after our own removal is not a drop.
    assert(fillStep(true,Presence::absent,t,late) == FillAction::add && t.drops == 0 && t.unwantedSince < 0);

    // A short exit (a menu tap) inside the delay keeps the engine's light: no
    // remove, no re-add, no drop.
    FillTrack tap;
    assert(fillStep(true,Presence::absent,tap,0) == FillAction::add);
    assert(fillStep(false,Presence::active,tap,5.0) == FillAction::none);
    assert(fillStep(false,Presence::active,tap,5.2) == FillAction::none);
    assert(fillStep(true,Presence::active,tap,5.3) == FillAction::none && tap.drops == 0 && tap.unwantedSince < 0);
    assert(fillStep(false,Presence::active,tap,6.0) == FillAction::none && tap.unwantedSince == 6.0); // a new exit restarts the delay
    // A broken clock never blocks removal forever and never removes early by wrap-around.
    FillTrack clock;
    assert(fillStep(true,Presence::absent,clock,0) == FillAction::add);
    assert(fillStep(false,Presence::active,clock,nan) == FillAction::remove);
    FillTrack back;
    assert(fillStep(true,Presence::absent,back,0) == FillAction::add);
    assert(fillStep(false,Presence::active,back,9.0) == FillAction::none);
    assert(fillStep(false,Presence::active,back,2.0) == FillAction::none && back.unwantedSince == 2.0); // clock went back: restart

    // An unwanted light that is still only queued for addition is waited out:
    // RemoveLight searches the active arrays only.
    FillTrack q;
    assert(fillStep(true,Presence::absent,q,0) == FillAction::add);
    assert(fillStep(false,Presence::queuedAdd,q,0) == FillAction::none && q.removeRequests == 0);
    assert(fillStep(false,Presence::queuedAdd,q,late) == FillAction::none);
    assert(fillStep(false,Presence::active,q,late) == FillAction::remove);

    // Wanted again while a removal is queued: wait, then add without a drop.
    FillTrack w;
    assert(fillStep(true,Presence::absent,w,0) == FillAction::add);
    assert(fillStep(false,Presence::active,w,0) == FillAction::none);
    assert(fillStep(false,Presence::active,w,late) == FillAction::remove);
    assert(fillStep(true,Presence::queuedRemove,w,late) == FillAction::none);
    assert(fillStep(true,Presence::absent,w,late) == FillAction::add && w.drops == 0);

    // The engine dropping the light while wanted (cell reset) is re-added a few
    // times per cell, then refused until the cell changes.
    FillTrack d;
    assert(fillStep(true,Presence::absent,d,0) == FillAction::add);
    for (unsigned i = 1;i <= maxDropsPerCell;++i) {
        assert(fillStep(true,Presence::active,d,0) == FillAction::none);
        assert(fillStep(true,Presence::absent,d,0) == FillAction::add && d.drops == i);
    }
    assert(fillStep(true,Presence::absent,d,0) == FillAction::dropLimit && d.gaveUp);
    assert(fillStep(true,Presence::absent,d,0) == FillAction::none); // refused once, then quiet
    d.newCell();
    assert(!d.gaveUp && d.drops == 0 && fillStep(true,Presence::absent,d,0) == FillAction::add);

    // RemoveLight that never takes (+300 false) is retried a bounded number of
    // times, then the light stays hidden and nothing more is asked.
    FillTrack s;
    assert(fillStep(true,Presence::absent,s,0) == FillAction::add);
    assert(fillStep(false,Presence::active,s,0) == FillAction::none);
    for (unsigned i = 0;i < maxRemoveRequests;++i) assert(fillStep(false,Presence::active,s,late) == FillAction::remove);
    assert(fillStep(false,Presence::active,s,late) == FillAction::removeLimit && s.stuck);
    assert(fillStep(false,Presence::active,s,late) == FillAction::none);
    assert(fillStep(false,Presence::absent,s,late) == FillAction::none && !s.stuck && s.removeRequests == 0);

    // Unwanted and never added: nothing to do.
    FillTrack idle;
    for (const auto p : {Presence::absent,Presence::queuedAdd,Presence::queuedRemove})
        assert(fillStep(false,p,idle,late) == FillAction::none);
}

void rooms() {
    // A light lights the point only if its sphere contains it and, for a
    // portal-strict light in a cell with rooms, it shares a room with it.
    for (bool contains : {false,true})
        for (bool strict : {false,true})
            for (bool graph : {false,true})
                for (bool shares : {false,true})
                    assert(lightsPoint(contains,strict,graph,shares) == (contains && (!strict || !graph || shares)));
    const int a = 1,b = 2,c = 3;
    const int* lightRooms[] = {&a,&b};
    const int* playerRooms[] = {&c,&b};
    const int* other[] = {&c};
    const int* empty[] = {nullptr};
    assert(anyShared<const int*>(lightRooms,2,playerRooms,2) && !anyShared<const int*>(lightRooms,2,other,1));
    assert(!anyShared<const int*>(lightRooms,0,playerRooms,2) && !anyShared<const int*>(lightRooms,2,playerRooms,0));
    assert(!anyShared<const int*>(empty,1,empty,1));   // null rooms never match each other
    assert(!anyShared<const int*>(nullptr,2,playerRooms,2) && !anyShared<const int*>(lightRooms,2,nullptr,2));
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
    c.cellHasRooms = true;
    assert(c.point == 0 && c.pointNearest == inf && c.shadowNearest == inf && c.starved() == 0);
    c.addPoint({{100,0,0},256,1.0f,0u,false},at);                 // contains the point, lit (not portal-strict)
    c.addPoint({{0,100,0},256,1.0f,culled,false},at);             // contains it, culled by the camera
    c.addPoint({{0,0,100},256,.5f,0u,false},at);                  // contains it, fading with camera distance
    c.addPoint({{1000,0,0},256,1.0f,0u,false},at);                // out of reach
    c.addPoint({{0,0,-50},256,1.0f,0u,true},at);                  // contains it, app-culled
    c.addPoint({{0,0,-60},256,1.0f,0u,false,true,false},at);      // contains it, but portal-strict to another room
    c.addPoint({{0,0,-30},256,1.0f,0u,false,true,true},at);       // portal-strict, shares the player's room
    assert(c.point == 7 && c.pointCulled == 1 && c.pointDimmed == 1 && c.pointReach == 6 && c.pointLit == 5 && c.pointLitLive == 3);
    assert(near(c.pointNearest,-226)); // the other-room light (-196) does not count, the shared one (-226) does
    c.addShadow({{0,0,10},512,1.0f,0u,false},2u,at);          // in reach, holds channel 2
    c.addShadow({{0,0,20},512,1.0f,0u,false},noChannel,at);   // in reach, no channel: contributes nothing
    c.addShadow({{2000,0,0},512,1.0f,0u,false},1u,at);        // far, holds a channel
    c.addShadow({{0,30,0},512,1.0f,0u,true},0u,at);           // hidden: a channel does not light it
    assert(near(c.addShadow({{0,0,1},2000,1.0f,0u,false,true,false},noChannel,at),-1999)); // other room: never starved
    assert(c.shadow == 5 && c.shadowChannel == 2 && c.shadowReach == 4 && c.shadowLit == 3 && c.shadowLitChannel == 1 && c.starved() == 2);
    assert(near(c.shadowNearest,-502));
    // A cell without rooms: portal-strict means nothing (ID 106400's no-graph path).
    Census flat;
    flat.addPoint({{0,0,10},64,1.0f,0u,false,true,false},at);
    assert(flat.pointLit == 1 && flat.pointLitLive == 1);
    // The BFB corridor case (ESM): 0003960F, radius 256 + XRDS 1743.9, contains
    // the player point but is portal-strict in room 3923A, the player in 39233.
    Census bfb;
    bfb.cellHasRooms = true; bfb.playerRooms = 1;
    bfb.addShadow({{1218.66f,-10769.22f,-2534.89f},1999.9f,1.0f,0u,false,true,false},0u,{2048,-9216,-2492});
    assert(bfb.shadowReach == 1 && bfb.shadowLit == 0 && bfb.starved() == 0 && bfb.shadowNearest == inf);

    // The per-light list keeps the nearest maxShadowEntries, nearest first.
    ShadowList list;
    for (unsigned i = 0;i < maxShadowEntries+5;++i) {
        ShadowEntry e; e.ref = 0x1000u+i; e.reach = static_cast<float>(100*((i*7)%15)) - 500.0f;
        list.offer(e);
    }
    assert(list.total == maxShadowEntries+5 && list.count == maxShadowEntries);
    for (unsigned i = 1;i < list.count;++i) assert(list.items[i-1].reach <= list.items[i].reach);
    assert(list.items[0].reach == -500.0f && list.items[list.count-1].reach == 400.0f);
    ShadowList odd;
    ShadowEntry n; n.ref = 1; n.reach = nan; odd.offer(n);
    ShadowEntry m; m.ref = 2; m.reach = 5; odd.offer(m);
    assert(odd.count == 2 && odd.items[0].ref == 2 && odd.items[1].ref == 1); // NaN sorts last
}
void lines() {
    LightSample s;
    s.seconds = 12.5; s.frame = 77; s.cell = 0x371de; s.interior = true; s.eligible = true; s.wanted = true;
    s.eye = {2048,-9216,-2492}; s.camera = {1810.7f,-9453.3f,-1808.2f};
    s.census.cellHasRooms = true; s.census.playerRooms = 1;
    s.census.addPoint({{2100,-9200,-2500},256,1.0f,0u,false,true,true},s.eye);
    static_cast<void>(s.census.addShadow({{0,0,0},512,1.0f,0u,false},noChannel,s.eye));
    s.fill.created = true; s.fill.hidden = false; s.fill.found = true; s.fill.cull = 0; s.fill.dimmer = 1.0f;
    s.fill.presence = Presence::active; s.fill.at = {2048,-9216,-2364}; s.fill.adds = 1;
    char line[768]{};
    assert(formatLight(line,sizeof(line),s));
    const std::string text(line);
    for (const char* part : {"FixedLight t=12.500 frame=77 cell=000371DE interior=1 eligible=1 ","eye=2048.0,-9216.0,-2492.0 ",
        "rooms=1 playerRooms=1 point=1 pointCulled=0 pointDimmed=0 pointReach=1 pointLit=1 pointLitLive=1 ",
        "shadow=1 shadowChannel=0 shadowReach=0 shadowLit=0 shadowLitChannel=0 shadowStarved=0 ","fillWanted=1 fill=active fillCreated=1 fillHidden=0 fillCull=0 fillDimmer=1.000 ",
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
    // The widest census line still fits one trace row.
    LightSample wide = s;
    wide.seconds = 999999.999; wide.frame = 99999999999ull; wide.cell = 0xffffffffu;
    wide.eye = wide.camera = wide.fill.at = {-99999.9f,-99999.9f,-99999.9f};
    wide.census.point = wide.census.pointCulled = wide.census.pointDimmed = wide.census.pointReach = 99999;
    wide.census.pointLit = wide.census.pointLitLive = wide.census.shadow = wide.census.shadowChannel = 99999;
    wide.census.shadowReach = wide.census.shadowLit = wide.census.playerRooms = 99999; wide.census.shadowLitChannel = 0;
    wide.census.pointNearest = wide.census.shadowNearest = -99999.9f;
    wide.fill.presence = Presence::queuedRemove; wide.fill.adds = wide.fill.removes = wide.fill.drops = 99999;
    assert(formatLight(line,std::tuple_size<TraceRow>::value,wide));
    // Shadow list line: reference IDs, ordered, and the full list fits one row.
    ShadowList list;
    for (unsigned i = 0;i < maxShadowEntries;++i) {
        ShadowEntry e; e.ref = 0x000395F1u+i; e.reach = -99999.0f; e.radius = 99999.0f; e.channel = noChannel; e.cull = culled;
        e.portalStrict = e.sharesRoom = e.lit = e.hidden = true;
        list.offer(e);
    }
    assert(formatShadows(line,std::tuple_size<TraceRow>::value,12.5,77,list));
    ShadowList one;
    ShadowEntry e; e.ref = 0x000395F1u; e.reach = 163.2f; e.radius = 1180.7f; e.channel = 2; e.cull = 0; e.portalStrict = true; e.sharesRoom = true;
    one.offer(e); one.total = 11;
    assert(formatShadows(line,sizeof(line),12.5,77,one));
    assert(std::string(line) == "FixedLightShadow t=12.500 frame=77 total=11 listed=1 000395F1:reach=163,r=1181,ch=2,cull=0,strict=1,room=1,lit=0,hidden=0");
    assert(!formatShadows(tiny,sizeof(tiny),12.5,77,one));
}
} // namespace
int main() {
    settings();
    wanted();
    presence();
    steps();
    rooms();
    census();
    lines();
    return 0;
}
