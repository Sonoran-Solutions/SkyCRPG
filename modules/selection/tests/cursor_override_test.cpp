// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../include/cursor_override.h"
#include "../include/selection_policy.h"
#include "../include/skycrpg/SelectionAPI.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <thread>
using namespace skycrpg::selection;
// Hold a publication copy at a known point to test contention without timing.
struct HeldCopy {
    int value = 0;
    inline static std::atomic<bool> hold{false}, entered{false}, release{false};
    HeldCopy& operator=(const HeldCopy& other) {
        if (hold.load()) {
            entered.store(true); entered.notify_one();
            release.wait(false);
        }
        value = other.value;
        return *this;
    }
};
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
    // No candidate still has an eligible world; availability is deliberately
    // separate from viability and a completed empty refresh advances freshness.
    policy.refresh(context,{}, {},180,114,settings);
    assert(passthrough(context)==Pass::none && policy.reason()==Pass::noCandidate);
    assert(policy.order().empty());
    api::CursorFrame empty;
    empty.viable=1; empty.completedRefreshes=1;
    assert(empty.viable && !empty.snapshot.available && empty.count==0);
    state.begin();
    assert(state.publish(state.read(),11));
    const auto authorized=state.read();
    assert(CursorOverride::permits(authorized,state.read(),11));
    const auto cleared=state.clear();
    assert(cleared && !CursorOverride::permits(authorized,state.read(),11)); // E before next Pick
    assert(!state.publish(authorized,11)); // late Pick cannot resurrect authorization
    assert(!CursorOverride::permits(cleared,cleared,11)); // entry/empty cannot use native handle
    const auto newer=state.clear();
    assert(!state.publish(cleared,22)); // superseded queued hover
    assert(state.publish(newer,22));
    assert(CursorOverride::permits(state.read(),state.read(),22));
    assert(!CursorOverride::permits(state.read(),state.read(),11));
    state.end(); assert(state.clear()==0 && !CursorOverride::active(state.read()));
    // A hover refresh replaces the frozen eight-item list, without requiring an
    // intervening empty hover or time-based expiry. Cycle still keeps its list.
    std::array<Candidate,9> many{};
    for (std::uint32_t i=0;i<many.size();++i) many[i]={i+1,{0,static_cast<double>((i+1)*10),0}};
    policy.refresh(context,many,{},180,115,settings);
    assert(policy.push(1,{},115,true));
    assert(policy.order().size()==8);
    many[8].position={0,1,0};
    policy.refresh(context,many,{},180,116,held);
    assert(!policy.push(9,{},116,true)); // ordinary frozen behavior
    assert(policy.push(9,{},116,true,true)); // cursor direct hop rebinds
    assert(policy.current()==9 && policy.order().front()==9);
    assert(policy.push(2,{},116,true,true));
    assert(policy.current()==2);
    // Publication gate is independent of Pick's state gate; contention returns
    // immediately, leaves output untouched, and does not end cursor mode.
    CursorExchange<HeldCopy> exchange;
    HeldCopy::hold.store(true);
    std::thread writer([&] { assert(exchange.write(HeldCopy{42})); });
    HeldCopy::entered.wait(false);
    HeldCopy output{7};
    assert(!exchange.read(output) && output.value==7);
    HeldCopy::hold.store(false); HeldCopy::release.store(true); HeldCopy::release.notify_one();
    writer.join();
    AccessGate pickGate;
    assert(pickGate.enter());
    assert(exchange.read(output) && output.value==42);
    pickGate.leave();
    CursorExchange<CursorRequest> mailbox;
    assert(mailbox.write({1,2,3}));
    assert(mailbox.write({4,5,6})); // latest intent replaces pending intent
    CursorRequest request;
    assert(mailbox.take(request) && request.token==4 && request.cycles==5 && request.id==6);
    assert(mailbox.take(request) && request.token==0);
    static_assert(api::Result::busy != api::Result::unavailable);
    std::cout << "PASS cursor override ABI, suppression, generation, cycle race and latch lifetime\n";
}
