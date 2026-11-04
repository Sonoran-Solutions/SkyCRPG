// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "cursor_policy.h"
#include <cassert>
#include <iostream>
using namespace skycrpg::camera::cursor;
int main() {
    const Eligibility ready{true,true,false,true,false,true,false,false,false,false,true,true,true};
    assert(refusal(ready) == Exit::none);
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
