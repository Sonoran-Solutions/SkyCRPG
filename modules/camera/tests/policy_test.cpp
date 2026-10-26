// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::camera;
int main() {
    Settings s; s.pitch = std::numeric_limits<float>::quiet_NaN(); s.zoom = 50; s.minZoom = .8f; s.maxZoom = .2f;
    s.toggleKey = 66; s.recenterKey = 37; s.toggleGamepad = 3;
    s = validate(s);
    assert(s.pitch == 55 && s.minZoom == .8f && s.maxZoom == .8f && s.zoom == .8f);
    assert(s.toggleKey == -1 && s.recenterKey == -1 && s.toggleGamepad == -1);
    s.schema = 99; assert(!validate(s).enabled);
    // Nonfinite dependent values must never leave minZoom > maxZoom, even
    // when the valid minimum is above the ordinary default maximum/zoom.
    for (float invalidMax : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        for (float invalidZoom : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
            Settings invalid;
            invalid.minZoom = .95f; invalid.maxZoom = invalidMax; invalid.zoom = invalidZoom;
            const auto recovered = validate(invalid);
            assert(recovered.minZoom == .95f && recovered.maxZoom == .95f && recovered.zoom == .95f);
            assert(recovered.minZoom <= recovered.maxZoom);
            assert(recovered.zoom >= recovered.minZoom && recovered.zoom <= recovered.maxZoom);
        }
    }
    const Settings zoom;
    assert(zoomRequest(-1,zoom,false)==zoom.minZoom);
    assert(zoomRequest(2,zoom,false)==zoom.maxZoom);
    assert(zoomRequest(.4f,zoom,false)==.4f);
    assert(zoomRequest(.4f,zoom,true)==zoom.zoom);
    assert(zoomRequest(std::numeric_limits<float>::quiet_NaN(),zoom,false)==zoom.zoom);
    Context c{true,true,true,false,false,false,false,false,false};
    assert(select(c) == State::normal); c.obstruction = true; assert(select(c) == State::obstruction);
    c.targeting = true; assert(select(c) == State::targeting); c.precision = true; assert(select(c) == State::precision);
    c.firstPerson = true; assert(select(c) == State::firstPerson); c.menu = true; assert(select(c) == State::menu);
    c.special = true; assert(select(c) == State::special); c.world = false; assert(select(c) == State::invalidWorld);
    c.enabled = false; assert(select(c) == State::disabled);
    c = Context{true,true,true,false,false,false,false,false,false};
    assert(toggleRequest(c,false) == ToggleRequest::firstPerson);
    c.firstPerson = true; assert(toggleRequest(c,false) == ToggleRequest::none);
    assert(toggleRequest(c,true) == ToggleRequest::thirdPerson);
    c.menu = true; assert(toggleRequest(c,true) == ToggleRequest::none);
    c.menu = false; c.special = true; assert(toggleRequest(c,true) == ToggleRequest::none);
    c.special = false; c.world = false; assert(toggleRequest(c,true) == ToggleRequest::none);
    FreshPress key; assert(key.update(true,true)); assert(!key.update(true,true)); key.suspend();
    assert(!key.update(true,false)); assert(!key.update(false,false)); assert(key.update(true,true));
    FreshPress idle; idle.suspend(); assert(idle.update(true,true));
    FreshPress focused; focused.observeSuppressed(true); focused.suspend();
    assert(!focused.update(true,false)); focused.observeSuppressed(false); assert(focused.update(true,true));
    assert(correction(1,2,3,0) == 0 && correction(1,2,3,.5f) == 0);
    for (float dt : {.01f,.02f,.05f}) {
        const float step = correction(-1, radians(90),3,dt)*3*dt;
        assert(step < 0 && std::abs(step) <= radians(90)*dt + .00001f);
        assert(std::abs(correction(.001f, radians(90),3,dt)*3*dt-.001f)<.000001f);
    }
    assert(std::abs(wrap(2*pi+.1f)-.1f)<.00001f);
}
