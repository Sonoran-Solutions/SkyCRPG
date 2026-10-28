// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "lifecycle_policy.h"
#include "obstruction_policy.h"
#include <cassert>
using namespace skycrpg::camera;
int main() {
    assert(!leaseWanted(false,false,false)); // disabled at startup
    assert(leaseWanted(true,false,false)); // enabled at startup
    assert(leaseWanted(false,true,false)); // runtime enable
    assert(!enableAfterLease(false)); // refusal keeps Camera disabled
    assert(enableAfterLease(true));
    assert(!leaseWanted(false,true,true)); // F6 precision retains its lease
    FailureDebounce failure;
    assert(!failure.observe(true,10.0));
    assert(!failure.observe(true,10.9));
    assert(!failure.observe(false,11.0)); // menu, load, recovery or no world resets
    assert(!failure.observe(true,12.0));
    assert(failure.observe(true,13.0));
    bool owned = true, recenter = true, pending = false;
    std::array<FreshPress,4> keys{};
    keys[1].down = true; // held across the reset
    resetTransient(owned,recenter,pending,keys);
    assert(!owned && !recenter && pending);
    // Idle keys fire on the first press after load or a cell change.
    assert(keys[0].update(true,true));
    // A held key must be released before it can fire again.
    assert(!keys[1].update(true,true));
    assert(!keys[1].update(false,false));
    assert(keys[1].update(true,true));
    Context c{};
    assert(!applyDeferredPreferences(c,pending,true));
    c.enabled = c.prerequisites = c.world = true;
    c.menu = true; assert(!applyDeferredPreferences(c,pending,true));
    c.menu = false; c.special = true; assert(!applyDeferredPreferences(c,pending,true));
    c.special = false; c.firstPerson = true; assert(!applyDeferredPreferences(c,pending,true));
    c.firstPerson = false; assert(!applyDeferredPreferences(c,pending,false));
    assert(applyDeferredPreferences(c,pending,true));
    pending = false; assert(!applyDeferredPreferences(c,pending,true));
    assert(!specialHazard({}));
    assert(specialHazard(Hazards{.mount=true}));
    assert(specialHazard(Hazards{.killmove=true}));
    assert(specialHazard(Hazards{.swimming=true}));
    assert(specialHazard(Hazards{.furniture=true}));
    assert(specialHazard(Hazards{.bleedout=true}));
    assert(specialHazard(Hazards{.beast=true}));
    assert(specialHazard(Hazards{.scripted=true}));
    // Slide is orthogonal to lifecycle selection and ownership.
    Context normal{}; normal.enabled = normal.prerequisites = normal.world = true;
    Context sliding = normal; sliding.slide = true;
    assert(select(sliding) == select(normal));
    assert(obstructionFramingAllowed(sliding) && pauseCorrection(sliding));
    assert(toggleRequest(sliding,false) == ToggleRequest::none);
    Context first = sliding; first.firstPerson = true;
    assert(retainPrecisionOwnership(first,true,false));
    assert(!claimPrecisionOwnership(normal,first));
    // A skipped slide sample preserves the latch and blend, including its timers.
    ObstructionSettings os{}; os.enabled = true; os.warmupSeconds = .1f; os.enterSeconds = .05f;
    ObstructionPolicy policy;
    auto frame = policy.step(normal,.8f,.1,os);
    frame = policy.step(normal,.8f,.1,os);
    frame = policy.step(normal,.8f,.1,os);
    assert(frame.obstructed && frame.blend > 0);
    const auto held = frame;
    for (int i = 0; i < 100; ++i) if (!pauseCorrection(sliding)) frame = policy.step(sliding,0.f,.1,os);
    assert(frame.obstructed == held.obstructed && frame.blend == held.blend);
    frame = policy.step(normal,.8f,.1,os);
    assert(frame.obstructed && frame.blend > held.blend);
    LifecycleTraceDebounce trace;
    for (unsigned i = 0; i < 9; ++i) assert(!trace.observe(State::normal,9));
    assert(trace.observe(State::normal,9));
    for (unsigned i = 0; i < 100; ++i) assert(!trace.observe(State::normal,9));
    for (unsigned i = 0; i < 9; ++i) assert(!trace.observe(State::special,9));
    assert(trace.observe(State::special,9));
    for (unsigned i = 0; i < 9; ++i) assert(!trace.observe(State::special,10));
    assert(trace.observe(State::special,10));
    LifecycleTraceDebounce alternating;
    unsigned lines = 0;
    for (unsigned i = 0; i < 500; ++i) lines += alternating.observe(i % 2 ? State::special : State::normal,9);
    assert(lines <= 1);
    alternating.resetCandidate();
    for (unsigned i = 0; i < 10; ++i) lines += alternating.observe(State::normal,9);
    assert(lines == 1);
    alternating.resetCandidate();
    for (unsigned i = 0; i < 10; ++i) assert(!alternating.observe(State::normal,9));
    // A camera-hook observation revokes a previously owned session without input.
    Context observed{}; observed.prerequisites = observed.world = true;
    owned = retainPrecisionOwnership(observed,true,false);
    assert(!owned);
    observed.firstPerson = true;
    assert(!retainPrecisionOwnership(observed,owned,false));
}
