// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include <cassert>
using namespace skycrpg::camera;
int main() {
    Context c{};
    c.prerequisites = c.world = true;
    c.firstPerson = true;
    // The F6 request disables the elevated preference while in first person.
    assert(retainPrecisionOwnership(c, true));
    assert(!retainPrecisionOwnership(c, false));
    c.menu = true;
    assert(retainPrecisionOwnership(c, true));
    assert(toggleRequest(c, true) == ToggleRequest::none);
    c.menu = false;
    assert(toggleRequest(c, true) == ToggleRequest::thirdPerson);
    c.special = true;
    assert(!retainPrecisionOwnership(c, true));
    c.special = false; c.world = false;
    assert(!retainPrecisionOwnership(c, true));
    c.world = true; c.prerequisites = false;
    assert(!retainPrecisionOwnership(c, true));
    c.prerequisites = true;
    // A relinquished session never spontaneously reacquires ownership.
    bool owned = retainPrecisionOwnership(c, false);
    assert(!retainPrecisionOwnership(c, owned));
    // Obstruction cannot supersede first person or precision.
    c.enabled = true; c.obstruction = true;
    assert(select(c) == State::firstPerson);
    c.firstPerson = false; c.precision = true;
    assert(select(c) == State::precision);
}
