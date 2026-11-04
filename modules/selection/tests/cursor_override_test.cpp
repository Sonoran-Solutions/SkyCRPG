// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../include/cursor_override.h"
#include "../include/selection_policy.h"
#include "../include/skycrpg/SelectionAPI.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
using namespace skycrpg::selection;
int main() {
    // Existing v2 six-field prefix remains unchanged.
    struct V2 { std::uint32_t size,version; decltype(api::Interface::getSnapshot) snapshot;
        decltype(api::Interface::copyCandidates) copy; decltype(api::Interface::setTarget) set;
        decltype(api::Interface::clearTarget) clear; };
    static_assert(offsetof(api::Interface,setCursorMode)==sizeof(V2));
    CursorOverride state;
    assert(!CursorOverride::active(state.read()));
    state.begin(); const auto first=state.read();
    assert(CursorOverride::suppress(first,false,99));
    assert(CursorOverride::suppress(first,true,99));
    assert(state.publish(first,99));
    assert(!CursorOverride::suppress(state.read(),true,99));
    assert(CursorOverride::suppress(state.read(),false,99)); // failed ray/contended Pick
    assert(CursorOverride::suppress(state.read(),true,98)); // nearest cannot replace invalid latch
    assert(state.publish(state.read(),0));
    assert(CursorOverride::suppress(state.read(),true,99));
    state.end(); assert(!CursorOverride::suppress(state.read(),false,99));
    state.begin(); assert(!state.publish(first,99)); // stale job cannot authorize new session
    const auto before=state.read(); state.end(); assert(!state.publish(before,99));
    assert(cursorRequestCurrent(true,12,12));
    assert(!cursorRequestCurrent(true,12,13)); // a queued hover cannot overwrite a later cycle
    assert(cursorRequestCurrent(false,12,13)); // legacy v2 behavior unchanged
    Context context;
    context.enabled=context.runtime=context.reads=context.world=context.thirdPerson=true;
    context.loading=context.dead=context.menu=context.dialogue=context.firstPerson=context.special=false;
    const std::array candidates{Candidate{1,{0,10,0}},Candidate{2,{0,20,0}}};
    Settings settings;
    const auto held=cursorLatchSettings(settings,true);
    Policy policy;
    policy.refresh(context,candidates,{},180,0,settings);
    assert(policy.cycle(1,{},1)); const auto cycled=policy.current();
    policy.refresh(context,candidates,{0,70,0},180,100,held);
    assert(policy.latched() && policy.current()==cycled); // neither time nor body movement releases
    policy.invalidate(cycled); assert(!policy.latched());
    policy.refresh(context,candidates,{},180,101,settings);
    assert(policy.cycle(1,{},102));
    policy.refresh(context,candidates,{},180,113,cursorLatchSettings(settings,false));
    assert(!policy.latched()); // ordinary Selection timeout remains unchanged
    std::cout << "PASS cursor override ABI, suppression, generation, cycle race and latch lifetime\n";
}
