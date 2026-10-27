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
    assert(retainPrecisionOwnership(c, true, true)); // menu camera can report a special state
    c.prerequisites = false;
    assert(!retainPrecisionOwnership(c, true, false)); // provider disabled in menu
    c.prerequisites = true; c.world = false;
    assert(!retainPrecisionOwnership(c, true, false)); // load/death in menu
    c.world = true; c.firstPerson = false;
    assert(retainPrecisionOwnership(c, true, true)); // menu temporarily replaced the camera state
    c.firstPerson = true;
    assert(toggleRequest(c, true) == ToggleRequest::none);
    c.menu = false; c.special = false;
    assert(toggleRequest(c, true) == ToggleRequest::thirdPerson);
    // A menu-hidden interruption is deferred, not accepted: revalidate after focus.
    c.firstPerson = false;
    assert(!retainPrecisionOwnership(c, true, false));
    c.firstPerson = true;
    assert(!retainPrecisionOwnership(c, true, true)); // hard special remains after close
    // Native POV cancellation is performed by the sink even while menu-focused.
    c.menu = true;
    bool canceled = false;
    assert(!retainPrecisionOwnership(c, canceled, true));
    c.menu = false;
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

    // Recorded menu trace: focus is present while the engine reports a
    // non-first-person, special camera; first-person resumes after close.
    Context recorded{};
    recorded.prerequisites = recorded.world = recorded.menu = recorded.special = true;
    bool observedSession = retainPrecisionOwnership(recorded, true, true);
    assert(observedSession && toggleRequest(recorded, observedSession) == ToggleRequest::none);
    recorded.menu = recorded.special = false; recorded.firstPerson = true;
    observedSession = retainPrecisionOwnership(recorded, observedSession, false);
    assert(observedSession && toggleRequest(recorded, observedSession) == ToggleRequest::thirdPerson);

    // Menu close can expose a tween before the settled camera state. Input
    // revalidates the session before deciding what an F6 press may do.
    Context tween{};
    tween.prerequisites = tween.world = tween.menu = tween.special = tween.tween = true;
    bool tweenSession = retainPrecisionOwnership(tween, true, false);
    assert(tweenSession && toggleRequest(tween, tweenSession) == ToggleRequest::none);
    tween.menu = false;
    tweenSession = retainPrecisionOwnership(tween, tweenSession, false);
    assert(tweenSession && toggleRequest(tween, tweenSession) == ToggleRequest::none);
    assert(!claimPrecisionOwnership(before, tween));
    auto settled = tween;
    settled.tween = settled.special = false; settled.firstPerson = true;
    assert(!claimPrecisionOwnership(tween, settled));
    tweenSession = retainPrecisionOwnership(settled, tweenSession, false);
    assert(tweenSession && toggleRequest(settled, tweenSession) == ToggleRequest::thirdPerson);

    // If the tween ends in third person, retain runs before the F6 decision.
    bool departed = retainPrecisionOwnership(tween, true, false);
    settled.firstPerson = false;
    departed = retainPrecisionOwnership(settled, departed, false);
    assert(!departed && toggleRequest(settled, departed) != ToggleRequest::thirdPerson);
    // Mount or script-mode hazards are hard even while the camera is tweening.
    assert(!retainPrecisionOwnership(tween, true, true));
    // Menu focus defers state/hazard validation, but native POV cancels it.
    tween.menu = true;
    assert(retainPrecisionOwnership(tween, true, true));
    bool nativePovSession = false; // input sink clears ownership on Toggle POV
    assert(!retainPrecisionOwnership(tween, nativePovSession, false));
    tween.menu = false;
    assert(!retainPrecisionOwnership(tween, nativePovSession, false));

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
