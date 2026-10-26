// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include <cassert>
using namespace skycrpg::camera;
int main() {
    Context c{};
    c.prerequisites = c.world = true;
    c.firstPerson = true;
    // The F6 request disables the elevated preference while in first person.
    assert(retainPrecisionOwnership(c, true, false));
    assert(!retainPrecisionOwnership(c, false, false));
    Context before = c; before.firstPerson = false;
    assert(claimPrecisionOwnership(before, c));
    Context delayed = c; delayed.firstPerson = false;
    assert(!claimPrecisionOwnership(before, delayed));
    assert(!claimPrecisionOwnership(c, c)); // pre-existing view is not ours
    auto unsafe = c; unsafe.special = true;
    assert(!claimPrecisionOwnership(before, unsafe));
    unsafe = c; unsafe.prerequisites = false;
    assert(!claimPrecisionOwnership(before, unsafe));
    unsafe = c; unsafe.menu = true;
    assert(!claimPrecisionOwnership(before, unsafe));
    auto unsafeBefore = before; unsafeBefore.menu = true;
    assert(!claimPrecisionOwnership(unsafeBefore, c));
    c.menu = true;
    assert(retainPrecisionOwnership(c, true, false));
    c.special = true; // menu temporarily disables native look/POV controls
    assert(retainPrecisionOwnership(c, true, false));
    assert(!retainPrecisionOwnership(c, true, true)); // menu plus true special state
    c.prerequisites = false;
    assert(!retainPrecisionOwnership(c, true, false)); // provider disabled in menu
    c.prerequisites = true; c.world = false;
    assert(!retainPrecisionOwnership(c, true, false)); // load/death in menu
    c.world = true; c.firstPerson = false;
    assert(!retainPrecisionOwnership(c, true, false)); // departed during menu
    c.firstPerson = true;
    assert(toggleRequest(c, true) == ToggleRequest::none);
    c.menu = false; c.special = false;
    assert(toggleRequest(c, true) == ToggleRequest::thirdPerson);
    c.special = true;
    assert(!retainPrecisionOwnership(c, true, false));
    c.special = false; c.world = false;
    assert(!retainPrecisionOwnership(c, true, false));
    c.world = true; c.prerequisites = false;
    assert(!retainPrecisionOwnership(c, true, false));
    c.prerequisites = true;
    c.firstPerson = false;
    assert(!retainPrecisionOwnership(c, true, false)); // observed departure revokes ownership
    c.firstPerson = true;
    // A relinquished session never spontaneously reacquires ownership.
    bool owned = retainPrecisionOwnership(c, false, false);
    assert(!retainPrecisionOwnership(c, owned, false));
    // Obstruction cannot supersede first person or precision.
    c.enabled = true; c.obstruction = true;
    assert(select(c) == State::firstPerson);
    c.firstPerson = false; c.precision = true;
    assert(select(c) == State::precision);

    // Model the real event-sink's input decisions using the existing fresh-press
    // and toggle policy; no engine camera transition is simulated as evidence.
    FreshPress key;
    Context input{};
    input.enabled = input.prerequisites = input.world = true;
    assert(key.update(true, true));
    assert(toggleRequest(input, false) == ToggleRequest::firstPerson);
    key.suspend();
    key.update(false, false);
    input.enabled = false; input.firstPerson = true;
    bool session = true;
    input.menu = true; input.special = true; // menu-disabled control-map flags
    session = retainPrecisionOwnership(input, session, false);
    key.observeSuppressed(true);
    assert(toggleRequest(input, session) == ToggleRequest::none);
    input.menu = false; input.special = false;
    assert(!key.update(true, false)); // holding F6 across focus cannot return
    assert(!key.update(false, false));
    assert(key.update(true, true));
    assert(toggleRequest(input, session) == ToggleRequest::thirdPerson);

    // A provider interruption observed on input revokes return permission.
    input.prerequisites = false;
    session = retainPrecisionOwnership(input, session, false);
    input.prerequisites = true;
    assert(toggleRequest(input, session) == ToggleRequest::none);
    input.firstPerson = false;
    assert(!retainPrecisionOwnership(input, true, false)); // external third-person transition
    assert(!retainPrecisionOwnership(Context{.prerequisites = true, .world = true, .firstPerson = true}, false, false));
    // Native POV may return to third person; F6 then enables preference only.
    input.firstPerson = false;
    assert(toggleRequest(input, session) == ToggleRequest::enableOnly);
}
