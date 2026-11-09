// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "cursor_policy.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>
#include <string_view>
using namespace skycrpg::camera::cursor;
void menuContention() {
    static_assert(menuBusyFrameLimit == 20);
    assert(std::string_view(exitNames[static_cast<unsigned>(Exit::busy)]) == "busy");
    assert(std::string_view(exitNames[static_cast<unsigned>(Exit::menu)]) == "menu");
    MenuContention policy;
    for (std::uint32_t frame = 0; frame < menuBusyFrameLimit-1u; ++frame) {
        assert(policy.observe(false,frame) == MenuAccess::skip);
        assert(policy.failures == frame+1u);
        // Several failing callbacks must not consume multiple frames of grace.
        assert(policy.observe(false,frame) == MenuAccess::skip);
        assert(policy.observe(false,frame) == MenuAccess::skip);
        assert(policy.failures == frame+1u);
    }
    assert(policy.observe(false,menuBusyFrameLimit-1u) == MenuAccess::exit);
    assert(policy.observe(false,menuBusyFrameLimit) == MenuAccess::exit);
    assert(policy.failures == menuBusyFrameLimit); // saturates
    assert(policy.observe(true,menuBusyFrameLimit) == MenuAccess::ready);
    assert(policy.failures == 0 && !policy.counted);
    assert(policy.observe(false,menuBusyFrameLimit) == MenuAccess::skip);
    assert(policy.failures == 1); // success resets even within the same frame
    assert(policy.observe(true,menuBusyFrameLimit) == MenuAccess::ready);
    for (std::uint32_t frame = 100; frame < 100+menuBusyFrameLimit-1u; ++frame)
        assert(policy.observe(false,frame) == MenuAccess::skip);
    assert(policy.observe(true,119) == MenuAccess::ready); // recovery before exit
    assert(policy.observe(false,120) == MenuAccess::skip);
    assert(policy.failures == 1);
    policy = {};
    assert(policy.observe(false,UINT32_MAX) == MenuAccess::skip);
    assert(policy.observe(false,0) == MenuAccess::skip);
    assert(policy.failures == 2); // frame counter wrap is still a new frame
}
void menuLocks() {
    std::vector<int> calls;
    bool outerReady = true, innerReady = true;
    const auto outerAttempt = [&] { calls.push_back(1); return outerReady; };
    const auto innerAttempt = [&] { calls.push_back(2); return innerReady; };
    const auto releaseOuter = [&] { calls.push_back(3); };
    assert(tryMenuLocks(false,outerAttempt,innerAttempt,releaseOuter));
    assert((calls == std::vector<int>{1,2}));
    calls.clear(); innerReady = false;
    assert(!tryMenuLocks(false,outerAttempt,innerAttempt,releaseOuter));
    assert((calls == std::vector<int>{1,2,3}));
    calls.clear(); outerReady = false;
    assert(!tryMenuLocks(false,outerAttempt,innerAttempt,releaseOuter));
    assert((calls == std::vector<int>{1}));
    calls.clear(); outerReady = innerReady = true;
    assert(!tryMenuLocks(true,outerAttempt,innerAttempt,releaseOuter));
    assert(calls.empty()); // a callback holding only inner must never take outer

    // Keep the worker's inner lock held THROUGH a timed-out service attempt and
    // subsequent focus/native-menu decisions. Completion must not need unlock.
    // These exercise the shared acquire policy, not Skyrim's update/IMenu code.
    std::recursive_mutex outer, inner;
    std::latch locked(1), finish(1);
    std::thread worker([&] {
        const std::lock_guard lock(inner);
        locked.count_down(); finish.wait();
    });
    locked.wait();
    const auto attempt = [&] {
        return tryMenuLocks(false,[&] { return outer.try_lock(); },
            [&] { return inner.try_lock(); },[&] { outer.unlock(); });
    };
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::milliseconds(1);
    do { assert(!attempt()); } while (std::chrono::steady_clock::now() < deadline);
    const bool ownMenu = attempt();
    const bool nativeMenu = !attempt();
    assert(!ownMenu && nativeMenu); // fail closed after the service budget
    MenuContention policy;
    for (std::uint32_t frame = 0; frame < menuBusyFrameLimit; ++frame) {
        const auto expected = frame+1u < menuBusyFrameLimit ? MenuAccess::skip : MenuAccess::exit;
        for (unsigned tick = 0; tick < 3; ++tick)
            assert(policy.observe(attempt(),frame) == expected);
    }
    // Even the entire skip/exit continuation completes while inner stays held.
    finish.count_down(); worker.join();
    assert(attempt());
    assert(policy.observe(true,menuBusyFrameLimit) == MenuAccess::ready);
    assert(policy.failures == 0);
    assert(attempt()); // both owned: recursive nativeMenu -> focused -> ownMenu
    inner.unlock(); outer.unlock();
    inner.unlock(); outer.unlock();
    bool released = false;
    std::thread checkRelease([&] {
        released = attempt();
        if (released) { inner.unlock(); outer.unlock(); }
    });
    checkRelease.join();
    assert(released); // no leaked reference from rollback or recursive success
    // Same contention at the OUTER lock: no inner read/acquisition is permitted.
    std::latch outerLocked(1), outerFinish(1);
    std::thread sorter([&] {
        const std::lock_guard lock(outer);
        outerLocked.count_down(); outerFinish.wait();
    });
    outerLocked.wait();
    unsigned innerCalls = 0;
    assert(!tryMenuLocks(false,[&] { return outer.try_lock(); },
        [&] { ++innerCalls; return inner.try_lock(); },[&] { outer.unlock(); }));
    assert(innerCalls == 0);
    outerFinish.count_down(); sorter.join();
}
int main() {
    menuContention();
    menuLocks();
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
    // Point-sized boxes keep the old nearest-point behaviour.
    const auto at = [](float x, float y) { return Rect{x,y,x,y}; };
    const std::array candidates{Candidate{1,at(1,0),100},Candidate{2,at(-1,0),20},Candidate{3,at(2,0),1}};
    assert(choose(candidates,{0,0},10)==2);
    assert(choose(candidates,{0,0},.5f)==0);
    assert(choose(candidates,{0,0},1)==2);
    assert(choose(candidates,{2,0},1)==3);
    assert(choose({}, {}, 10)==0);
    // A long bench is hit anywhere along its box, not only near its centre.
    const std::array bench{Candidate{7,{565,555,1535,665},4000}};
    assert(choose(bench,{960,540},48)==7 && choose(bench,{600,600},48)==7);
    assert(distance2(bench[0].box,Point{960,600})==0 && choose(bench,{960,400},48)==0);
    // Nested boxes: the smaller box wins inside both; the larger one outside it.
    const std::array table{Candidate{20,{100,100,500,300},900},Candidate{21,{280,180,320,220},950}};
    assert(choose(table,{300,200},48)==21 && choose(table,{150,150},48)==20);
    assert(choose(table,{300,200},0)==21); // zero radius still hits inside a box
    // Invalid boxes and ids are ignored.
    const std::array broken{Candidate{0,{0,0,10,10},1},Candidate{5,{10,0,0,10},1},Candidate{6,{NAN,0,1,1},1}};
    assert(choose(broken,{5,5},48)==0 && !within(broken,5,{5,5},48));
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
    const std::array hover{Candidate{11,at(10,10),1},Candidate{22,at(90,10),2}};
    assert(within(hover,11,{10,10},20));
    assert(within(hover,11,{35,10},20) && choose(hover,{35,10},20)==0);
    assert(!within(hover,11,{35.01f,10},20));
    assert(!within(hover,11,{40,10},20) && choose(hover,{40,10},20)==0);
    assert(!within(hover,11,{90,10},20) && choose(hover,{90,10},20)==22);
    // Hysteresis keeps A near the edge, but a neighbour under the pointer wins.
    const std::array neighbours{Candidate{11,at(10,10),1},Candidate{22,at(50,10),2}};
    assert(skycrpg::camera::cursor::hover(neighbours,11,{28,10},48)==11);
    assert(skycrpg::camera::cursor::hover(neighbours,11,{50,10},48)==22);
    assert(skycrpg::camera::cursor::hover(neighbours,0,{12,10},48)==11);
    assert(skycrpg::camera::cursor::hover(neighbours,11,{200,200},48)==0);
    // Moving onto a mug inside the hovered table selects the mug, and back off it the table.
    assert(skycrpg::camera::cursor::hover(table,20,{300,200},48)==21);
    assert(skycrpg::camera::cursor::hover(table,21,{150,150},48)==20);
    assert(skycrpg::camera::cursor::hover(table,21,{300,200},48)==21);
    Freshness aged; aged.observe(1,0,0);
    assert(aged.fresh(.1) && !aged.fresh(.6));
    assert(aged.usable(.6,10,false)); // a running session keeps its update grace
    const float row[4]{0,1,0,-10};
    assert(inFront(row,0,11,0));
    assert(!inFront(row,0,9,0) && !inFront(row,0,10,0));
    assert(!inFront(row,NAN,11,0));
    skycrpg::camera::Settings base;
    auto settings=validate(Settings{},base);
    assert(!settings.enabled && settings.toggleKey==48 && settings.toggleGamepad==-1 && settings.sensitivity==65);
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
