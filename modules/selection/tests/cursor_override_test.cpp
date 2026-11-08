// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../include/cursor_override.h"
#include "../include/selection_policy.h"
#include "../include/skycrpg/SelectionAPI.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
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
    // Visibility member appended after the v2 cursor extension; 72 bytes on x64.
    static_assert(offsetof(api::Interface,setCursorVisible)==offsetof(api::Interface,getCursorFrame)+sizeof(api::Interface::getCursorFrame));
    static_assert(sizeof(api::Interface)==offsetof(api::Interface,setCursorVisible)+sizeof(api::Interface::setCursorVisible));
    static_assert(sizeof(void*)!=8 || sizeof(api::Interface)==72);
    static_assert(std::size(api::CursorFrame{}.ids)==api::cursorListCapacity && std::size(CursorVisible{}.ids)==api::cursorListCapacity);
    const std::uint32_t one=7;
    assert(validCursorVisible(nullptr,0) && validCursorVisible(&one,1));
    assert(!validCursorVisible(nullptr,1) && !validCursorVisible(&one,api::cursorListCapacity+1));
    // Session handshake: a stale report cannot restrict or permit a new session,
    // a restricted session shows nothing until its first report, mode-1 shows all.
    CursorVisibility seen;
    CursorVisible stale; stale.session=1; stale.count=1; stale.ids[0]=20;
    seen.enter(1,true); seen.observe(stale);
    assert(seen.shown(20) && !seen.shown(10) && seen.count()==1);
    seen.enter(2,true); seen.observe(stale);
    assert(!seen.shown(20) && !seen.shown(10) && seen.count()==0 && seen.session()==2);
    CursorVisible fresh=stale; fresh.session=2; fresh.ids[0]=10;
    seen.observe(fresh); assert(seen.shown(10) && !seen.shown(20));
    fresh.count=0; seen.observe(fresh); assert(!seen.shown(10)); // an empty report hides everything
    seen.enter(3,false); assert(seen.shown(10) && seen.shown(20) && !seen.restricted());
    assert(cursorRequestCurrent(false,12,13)); // legacy v2 behavior unchanged
    Context context;
    context.enabled=context.runtime=context.reads=context.world=context.thirdPerson=true;
    context.loading=context.dead=context.menu=context.dialogue=context.firstPerson=context.special=false;
    auto sneaking = context; sneaking.sneaking = sneaking.nativeLiveActor = true;
    assert(passthrough(cursorContext(sneaking,false)) == Pass::nativeSneakActor);
    assert(passthrough(cursorContext(sneaking,true)) == Pass::none);
    sneaking.combat = true;
    assert(passthrough(cursorContext(sneaking,false)) == Pass::combat);
    assert(passthrough(cursorContext(sneaking,true)) == Pass::none);
    // Cursor adjustment cannot mask any other native passthrough, even when
    // combat/screen-centre sneaking would otherwise be present too.
    const std::array exits{std::pair{&Context::telekinesis,Pass::telekinesis},
        std::pair{&Context::armed,Pass::armed},std::pair{&Context::special,Pass::special},
        std::pair{&Context::menu,Pass::menu},std::pair{&Context::dead,Pass::dead}};
    for (const auto& [field,reason] : exits) {
        auto blocked = sneaking; blocked.*field = true;
        assert(passthrough(cursorContext(blocked,true)) == reason);
    }
    for (const auto result : {api::Result::ok,api::Result::unavailable,api::Result::wrongThread,
        api::Result::disabled,api::Result::invalidTarget,api::Result::invalidArgument,api::Result::queued})
        assert(legacyResult(result) == result);
    assert(legacyResult(api::Result::busy) == api::Result::unavailable); // legacy setter during cursor mode too
    assert(legacyReadResult(false) == api::Result::unavailable);
    assert(legacyReadResult(true,false) == api::Result::unavailable);
    assert(legacyReadResult(true,true) == api::Result::ok);
    // Definite attempts advance liveness even without any usable selection.
    std::uint64_t alive = 0;
    const std::array occluded{Candidate{1,{0,10,0}},Candidate{2,{0,20,0}}};
    const auto exhausted = lazyValidate(occluded,{},180,false,1,[](const Candidate&) { return Validation::invalid; });
    assert(exhausted.exhausted && !exhausted.unknown && exhausted.accepted.empty());
    if (finishedScan(true,exhausted.unknown,false,false,false)) ++alive;
    assert(alive == 1);
    assert(finishedScan(false,false,false,true,false)); // visit/reference cap
    assert(finishedScan(true,true,false,false,true)); // ray cap is a definite result
    assert(!finishedScan(true,true,true,false,true)); // contention never renews freshness
    assert(!finishedScan(true,true,false,false,false)); // unknown engine reads
    assert(!finishedScan(false,false,false,false,false)); // no world/radius
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
    state.begin();
    assert(state.publish(state.read(),11));
    const auto authorized=state.read();
    assert(CursorOverride::permits(authorized,state.read(),11));
    // A repeated Pick of the same token/handle must retain its permit while
    // validation is in flight; a changed token or resolved ID never permits E.
    std::atomic<std::uint64_t> permit{authorized};
    std::atomic<std::uint32_t> permittedHandle{111};
    unsigned clears = 0;
    const auto clear = [&] { ++clears; };
    {
        CursorPickScope pick{state,permit,permittedHandle,clear};
        assert(permit.load() == authorized && permittedHandle.load() == 111);
        pick.injected = true; pick.id = 11;
    }
    assert(permit.load() == authorized && permittedHandle.load() == 111 && clears == 0);
    {
        CursorPickScope pick{state,permit,permittedHandle,clear};
        // An early-return/refused Pick exercises the real destructor cleanup.
    }
    assert(permit.load() == 0 && clears == 1);
    permit.store(authorized);
    {
        CursorPickScope pick{state,permit,permittedHandle,clear};
        pick.injected = true; pick.id = 12; // wrong injection cannot retain E
    }
    assert(permit.load() == 0 && clears == 2);
    const auto cleared=state.clear();
    assert(cleared && !CursorOverride::permits(authorized,state.read(),11)); // E before next Pick
    assert(!state.publish(authorized,11)); // late Pick cannot resurrect authorization
    assert(!CursorOverride::permits(cleared,cleared,11)); // entry/empty cannot use native handle
    const auto newer=state.clear();
    assert(!state.publish(cleared,22)); // superseded queued hover
    assert(state.publish(newer,22));
    assert(CursorOverride::permits(state.read(),state.read(),22));
    assert(!CursorOverride::permits(state.read(),state.read(),11));
    permit.store(state.read()); permittedHandle.store(222);
    {
        CursorPickScope pick{state,permit,permittedHandle,clear};
        pick.injected = true; pick.id = 22;
        assert(state.end()); // exit while Pick is in flight clears its injection
    }
    assert(permit.load() == 0 && permittedHandle.load() == 0 && clears == 3);
    assert(!state.end()); // failed entry / duplicate exit must not release legacy work
    {
        CursorPickScope pick{state,permit,permittedHandle,clear};
    }
    assert(clears == 3); // ordinary native Pick remains intact
    state.begin(); assert(state.publish(state.read(),22));
    const auto oldTarget = state.read();
    AccessGate heldGate;
    assert(heldGate.enter());
    const auto busy = replaceCursorTarget(state,33,[&](std::uint64_t token, std::uint32_t id) {
        assert(id == 33 && token == state.read() && CursorOverride::target(token) == 0);
        assert(!CursorOverride::permits(oldTarget,state.read(),22));
        assert(!heldGate.enter());
        return api::Result::busy;
    });
    assert(busy == api::Result::busy && CursorOverride::target(state.read()) == 0);
    assert(!state.publish(oldTarget,22)); // in-flight old Pick cannot restore authorization
    heldGate.leave();
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
