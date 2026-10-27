// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "lifecycle_policy.h"
#include <cassert>
using namespace skycrpg::camera;
int main() {
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
    assert(specialHazard(Hazards{.slide=true}));
    // A camera-hook observation revokes a previously owned session without input.
    Context observed{}; observed.prerequisites = observed.world = true;
    owned = retainPrecisionOwnership(observed,true,false);
    assert(!owned);
    observed.firstPerson = true;
    assert(!retainPrecisionOwnership(observed,owned,false));
}
