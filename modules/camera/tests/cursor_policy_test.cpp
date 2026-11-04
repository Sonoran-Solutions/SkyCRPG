// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "cursor_policy.h"
#include <cassert>
#include <iostream>
using namespace skycrpg::camera::cursor;
int main() {
    const Eligibility ready{true,true,false,true,false,true,false,false,false,false,true,true,true};
    assert(refusal(ready) == Exit::none);
    // DataLoaded may defer once. Nested ProcessMessages must not consume the
    // one main-thread fallback; success and terminal failure never retry.
    OverlayRegistration registration;
    unsigned registrationCalls = 0;
    const auto attempt = [&] { return ++registrationCalls > 1 ? Registration::ready : Registration::deferred; };
    assert(!registration.prepare(false,true,false,false,attempt));
    assert(!registration.prepare(true,false,false,false,attempt));
    assert(registrationCalls == 1 && !registration.fallbackUsed);
    assert(registration.prepare(true,true,false,false,attempt));
    assert(registration.prepare(true,true,true,false,attempt));
    assert(registrationCalls == 2);
    OverlayRegistration collision;
    const auto fail = [&] { ++registrationCalls; return Registration::failed; };
    assert(!collision.prepare(false,true,false,false,fail));
    assert(!collision.prepare(true,true,false,false,fail) && registrationCalls == 3);
    OverlayRegistration deferred;
    const auto unavailable = [&] { ++registrationCalls; return Registration::deferred; };
    assert(!deferred.prepare(false,true,false,false,unavailable));
    assert(!deferred.prepare(true,true,false,false,unavailable));
    assert(!deferred.prepare(true,true,false,false,unavailable) && registrationCalls == 5);
    OverlayRegistration loaded;
    assert(loaded.prepare(false,true,false,false,[] { return Registration::ready; }));
    assert(loaded.prepare(true,true,false,false,fail) && registrationCalls == 5);
    Session session;
    assert(session.toggle(ready,1920,1080));
    assert(session.pointer.point.x == 960 && session.pointer.point.y == 540);
    assert(!session.toggle(ready,1920,1080) && !session.active);
    const std::array cases{
        std::pair{&Eligibility::enabled,Exit::disabled}, std::pair{&Eligibility::lifecycle,Exit::lifecycle},
        std::pair{&Eligibility::menu,Exit::menu}, std::pair{&Eligibility::loaded,Exit::load},
        std::pair{&Eligibility::dead,Exit::death}, std::pair{&Eligibility::sameCell,Exit::cell},
        std::pair{&Eligibility::firstPerson,Exit::firstPerson}, std::pair{&Eligibility::special,Exit::special},
        std::pair{&Eligibility::precision,Exit::precision}, std::pair{&Eligibility::locked,Exit::lock},
        std::pair{&Eligibility::selection,Exit::selection}, std::pair{&Eligibility::external,Exit::selection},
        std::pair{&Eligibility::overlay,Exit::overlay}};
    for (const auto& [field,exit] : cases) {
        assert(session.toggle(ready,1920,1080));
        session.latch.cycle(12,{50,50});
        auto c=ready; c.*field = !(c.*field);
        assert(refusal(c) == exit);
        session.exit();
        assert(!session.active && !session.latch.id && !session.toggle(c,1920,1080));
    }
    assert(interruptKey(false,64,false)==Exit::f6);
    assert(interruptKey(false,66,false)==Exit::f8);
    assert(interruptKey(false,22,false)==Exit::lock);
    assert(interruptKey(true,128,true)==Exit::lock);
    assert(interruptKey(false,33,true)==Exit::firstPerson);
    assert(interruptKey(false,18,false)==Exit::none); // Activate never captured
    assert(interruptKey(true,4096,false)==Exit::none);
    Pointer pointer; pointer.centre(100,80);
    pointer.move({.5f,.25f},20,100,80); assert(pointer.point.x==60 && pointer.point.y==35);
    pointer.move({100,-100},20,100,80); assert(pointer.point.x==100 && pointer.point.y==80);
    pointer.move({-100,100},20,100,80); assert(pointer.point.x==0 && pointer.point.y==0);
    pointer.move({NAN,1},20,100,80); assert(pointer.point.y==0);
    Latch latch; latch.cycle(12,{20,20});
    assert(latch.retain({32,20},12,true));
    assert(!latch.retain({32.01f,20},12,true));
    latch.cycle(12,{20,20}); assert(!latch.retain({20,20},12,false));
    const std::array candidates{Candidate{1,{1,0},100},Candidate{2,{-1,0},20},Candidate{3,{2,0},1}};
    assert(choose(candidates,{0,0},10)==2);
    assert(choose(candidates,{0,0},.5f)==0);
    assert(choose(candidates,{0,0},1)==2);
    assert(choose(candidates,{2,0},1)==3);
    assert(choose({}, {}, 10)==0);
    // Busy/incomplete reads retain the session until a bounded stale window;
    // a known lost session exits immediately, including on a fresh refresh.
    Freshness freshness;
    assert(!freshness.usable(1,10,false));
    freshness.observe(1,1,10);
    assert(freshness.usable(1.49,40,false));
    freshness.observe(1,1.49,40); // busy/stale reads cannot extend the deadline
    assert(freshness.usable(1.5,41,false));
    assert(!freshness.usable(1.55,44,false));
    freshness.observe(2,1.5,41);
    assert(freshness.usable(1.6,42,false) && !freshness.usable(1.6,42,true));
    assert(freshness.usable(10,60,false)); // hitch after many ordinary updates
    assert(freshness.usable(10,62,false) && !freshness.usable(10,63,false));
    freshness.observe(3,11,44,1.0); // maximum legal refresh interval
    assert(freshness.usable(13.99,80,false) && freshness.usable(14,81,false));
    assert(!freshness.usable(14.1,84,false));
    freshness.observe(4,14,81,1.0); // exhausted/capped attempt is still alive
    assert(freshness.usable(16.5,90,false));
    freshness.observe(4,16.5,90,1.0); // repeated publication cannot extend freshness
    assert(freshness.usable(17,91,false) && !freshness.usable(17.1,94,false));
    freshness.observe(5,18,0xfffffffeu);
    assert(freshness.usable(30,0xfffffffeu,false));
    assert(freshness.usable(30,0,false) && !freshness.usable(30,1,false)); // counter wrap
    HoverAttempts attempts;
    assert(attempts.allowed(11,1)); // A accepted
    assert(attempts.allowed(22,1)); // B accepted
    assert(attempts.allowed(11,1)); // A again, within the same scan
    attempts.reject(11,1);
    assert(!attempts.allowed(11,1)); // only a rejection backs off
    assert(attempts.allowed(22,1));
    assert(!attempts.allowed(11,1));
    assert(attempts.allowed(11,2));
    assert(!attempts.allowed(0,2)); // empty revocation is never throttled here
    // Empty list is pointer over nothing, not an eligibility failure.
    assert(session.toggle(ready,100,80));
    assert(session.active && choose({},session.pointer.point,48)==0);
    session.exit();
    // Hover is recomputed without a scan refresh: leave A, clear, then bind B.
    const std::array hover{Candidate{11,{10,10},1},Candidate{22,{90,10},2}};
    assert(within(hover,11,{10,10},20));
    assert(within(hover,11,{35,10},20) && choose(hover,{35,10},20)==0);
    assert(!within(hover,11,{35.01f,10},20));
    assert(!within(hover,11,{40,10},20) && choose(hover,{40,10},20)==0);
    assert(!within(hover,11,{90,10},20) && choose(hover,{90,10},20)==22);
    const float row[4]{0,1,0,-10};
    assert(inFront(row,0,11,0));
    assert(!inFront(row,0,9,0) && !inFront(row,0,10,0));
    assert(!inFront(row,NAN,11,0));
    skycrpg::camera::Settings base;
    auto settings=validate(Settings{},base);
    assert(!settings.enabled && settings.toggleKey==48 && settings.toggleGamepad==-1);
    for (int key : {64,65,66,22,26,27,37,38,2,9,71,81}) {
        settings.toggleKey=key; assert(validate(settings,base).toggleKey==-1);
    }
    settings.toggleGamepad=128; assert(validate(settings,base).toggleGamepad==-1);
    // Feature-off refuses independently of every optional capability.
    auto off=ready; off.enabled=false;
    for (bool present : {false,true}) {
        off.selection=off.external=off.overlay=present;
        assert(!session.toggle(off,100,100) && !session.active);
    }
    std::cout << "PASS cursor policy\n";
}
