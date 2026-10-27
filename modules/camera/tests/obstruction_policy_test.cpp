// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "obstruction_policy.h"
#include <cassert>
#include <iostream>
using namespace skycrpg::camera;
namespace {
constexpr Context normal{true,true,true};
constexpr ObstructionSettings testSettings() {
    ObstructionSettings s;
    s.enabled = true;
    s.warmupSeconds = .125f; s.enterSeconds = .125f; s.exitSeconds = .125f;
    s.tightenRate = 2; s.recoverRate = .5f;
    return s;
}
constexpr auto config = testSettings();
constexpr double dt = .0625;
constexpr bool timing() {
    ObstructionPolicy policy;
    // Warmup then exactly two consecutive above-threshold frames (125 ms).
    if (policy.step(normal,.5f,dt,config).obstructed) return false;
    if (policy.step(normal,.5f,dt,config).obstructed) return false;
    if (policy.step(normal,.5f,dt,config).obstructed) return false;
    return policy.step(normal,.5f,dt,config).obstructed;
}
static_assert(timing()); // The state machine is usable at compile time in C++20.
ObstructionPolicy tightPolicy() {
    ObstructionPolicy policy;
    ObstructionFrame frame;
    for (int i=0; i<20; ++i) frame = policy.step(normal,.5f,dt,config);
    assert(frame.obstructed && frame.blend == 1);
    return policy;
}
void measurements() {
    assert(cameraCompression({50,0,0},{100,0,0},{0,0,0}) == .5f);
    assert(cameraCompression({200,0,0},{100,0,0},{0,0,0}) == 0);
    assert(cameraCompression({1000,1050,1000},{1000,1100,1000},{1000,1000,1000}) == .5f);
    assert(cameraCompression({0,0,0},{100,0,0},{0,0,0}) == 1);
    assert(!cameraCompression({0,0,0},{.5,0,0},{0,0,0}));
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}) {
        for (int which=0; which<3; ++which) {
            for (int axis=0; axis<3; ++axis) {
                ObstructionPoint points[3]{{50,0,0},{100,0,0},{0,0,0}};
                auto& p = points[which];
                if (axis==0) p.x=invalid;
                if (axis==1) p.y=invalid;
                if (axis==2) p.z=invalid;
                assert(!cameraCompression(points[0],points[1],points[2]));
            }
        }
    }
    const double max = std::numeric_limits<double>::max();
    assert(!cameraCompression({max,0,0},{100,0,0},{-max,0,0}));
}
void hysteresis() {
    ObstructionPolicy noise;
    for (int i=0; i<2000; ++i)
        assert(!noise.step(normal,i%2 ? .26f : .24f,dt,config).obstructed);
    auto policy = tightPolicy();
    // Dead band and interrupted low dwell must never release or probe.
    for (int i=0; i<2000; ++i) {
        const auto frame = policy.step(normal,i%2 ? .09f : .11f,dt,config);
        assert(frame.obstructed && frame.blend == 1);
    }
    assert(policy.step(normal,0,dt,config).blend < 1); // previous .09 is first low frame
    float previous = 1;
    ObstructionFrame frame;
    for (int i=0; i<32; ++i) {
        frame = policy.step(normal,0,dt,config);
        assert(frame.obstructed && frame.blend <= previous);
        previous = frame.blend;
    }
    assert(frame.blend == 0 && frame.obstructed); // tight goal cannot certify preferred clearance
    assert(!policy.step(normal,0,dt,config).obstructed); // full dwell at preferred
    assert(!policy.step(normal,.5f,dt,config).obstructed); // entry timer was reset
    assert(policy.step(normal,.5f,dt,config).obstructed);
}
void closedLoop() {
    ObstructionPolicy policy;
    ObstructionFrame frame;
    float previous = 1;
    bool sawTight = false;
    // Static doorway: the current goal clears when tight. Probing back reaches
    // compression again. Hold there instead of retracting/expanding endlessly.
    for (int i=0; i<4000; ++i) {
        const float goal = 1000-650*frame.blend;
        const float compression = std::max(0.0f,1-500/goal);
        frame = policy.step(normal,compression,dt,config);
        if (sawTight) {
            assert(frame.obstructed && frame.blend <= previous);
            previous = frame.blend;
        }
        if (frame.blend == 1) sawTight = true;
    }
    assert(sawTight && frame.obstructed && frame.blend > 0 && frame.blend < 1);
    const float held = frame.blend;
    for (int i=0; i<1000; ++i) {
        frame = policy.step(normal,i%2 ? .09f : .11f,dt,config);
        assert(frame.obstructed && frame.blend == held);
    }
    for (int i=0; i<1000; ++i) frame = policy.step(normal,0,dt,config);
    assert(!frame.obstructed && frame.blend == 0); // entering a large room clears it
}
void priorityAndInvalid() {
    for (int state=0; state<9; ++state) {
        auto policy = tightPolicy();
        auto c = normal;
        switch (state) {
        case 0: c.enabled=false; break;
        case 1: c.prerequisites=false; break;
        case 2: c.world=false; break;
        case 3: c.special=true; break;
        case 4: c.menu=true; break;
        case 5: c.firstPerson=true; break;
        case 6: c.precision=true; break;
        case 7: c.targeting=true; break;
        case 8: c.obstruction=true; break;
        }
        const auto frame = policy.step(c,.5f,dt,config);
        if (state==8) { assert(frame.obstructed); continue; }
        assert(!frame.valid && !frame.obstructed && frame.blend == 0);
        assert(!policy.step(normal,.5f,dt,config).obstructed); // no stale timer
    }
    for (double invalidDt : {0.0,-1.0,.10001,std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity()}) {
        auto policy = tightPolicy();
        assert(!policy.step(normal,.5f,invalidDt,config).valid);
        assert(!policy.step(normal,.5f,dt,config).obstructed);
    }
    for (auto compression : {std::optional<float>{},std::optional<float>{-.1f},std::optional<float>{1.1f},
                             std::optional<float>{std::numeric_limits<float>::quiet_NaN()},
                             std::optional<float>{std::numeric_limits<float>::infinity()}}) {
        auto policy = tightPolicy();
        assert(!policy.step(normal,compression,dt,config).valid);
    }
    auto policy = tightPolicy();
    auto disabled = config; disabled.enabled=false;
    assert(!policy.step(normal,.5f,dt,disabled).valid);
}
void ratesAndPreferences() {
    for (double step : {.01,.025,.0625}) {
        ObstructionPolicy policy;
        ObstructionFrame frame;
        for (int i=0; i<1000; ++i) {
            const auto previous = frame;
            frame = policy.step(normal,.5f,step,config);
            assert(frame.blend-previous.blend <= config.tightenRate*step+1e-6);
        }
        for (int i=0; i<1000; ++i) {
            const auto previous = frame;
            frame = policy.step(normal,0,step,config);
            assert(previous.blend-frame.blend <= config.recoverRate*step+1e-6);
        }
        assert(!frame.obstructed);
    }
    Settings s;
    assert(obstructionPitch(s.pitch,1,config) == 25);
    assert(obstructionPitch(15,1,config) == 15);
    assert(obstructionZoom(.25f,1,config) == .25f);
    assert(std::abs(obstructionZoom(s.zoom,1,config)-.35f)<1e-6f);
    assert(preferredZoomRequest(.35f,.35f,true,s,false) == s.zoom);
    for (float preferred : {.251f,.437f,.713f,.899f}) {
        s.zoom = preferred;
        for (int i=0; i<1000; ++i) s.zoom = preferredZoomRequest(.38f,.38f,true,s,false);
        assert(s.zoom == preferred); // no rounding drift without native input
    }
    s = Settings{};
    assert(std::abs(preferredZoomRequest(.45f,.35f,true,s,false)-.75f)<1e-6f);
    assert(preferredZoomRequest(.35f,.35f,true,s,true) == s.zoom);
    assert(preferredZoomRequest(std::numeric_limits<float>::quiet_NaN(),.35f,true,s,false) == s.zoom);
    auto invalid = config;
    invalid.exitRatio=1; invalid.tightPitch=90; invalid.tightZoom=-1;
    invalid.enterSeconds=std::numeric_limits<float>::quiet_NaN(); invalid.recoverRate=-1;
    const auto clamped = validateObstruction(invalid,s);
    assert(clamped.exitRatio < clamped.enterRatio && clamped.tightPitch == s.pitch);
    assert(clamped.tightZoom == s.minZoom && clamped.enterSeconds == .25f && clamped.recoverRate > 0);
    assert(!ObstructionSettings{}.enabled);
}
void resetZoomRecovery() {
    // All these transitions reach resetObstruction through suspend, lifecycle,
    // or the cell-change path. Exercise the same write/reset/update sequence.
    for (int transition=0; transition<4; ++transition) { // menu, F6/POV, F8/provider, cell
        Settings s;
        ObstructionPolicy policy;
        ObstructionFrame frame;
        for (int i=0; i<8; ++i) frame = policy.step(normal,.5f,dt,config);
        assert(frame.blend > 0);
        float last = obstructionZoom(s.zoom,frame.blend,config);
        assert(last < s.zoom);
        bool adjusted = true, apply = false;
        const float preferred = s.zoom;
        policy.reset();
        resetObstructionZoom(adjusted,last,apply);
        assert(!adjusted && last == 0 && apply);
        // Either the tightened target survived or native changed it in transition.
        for (float native : {obstructionZoom(preferred,frame.blend,config), .9f}) {
            s.zoom = preferredZoomRequest(native,last,adjusted,s,apply);
            assert(s.zoom == preferred);
            assert(obstructionZoom(s.zoom,0,config) == preferred);
        }
        apply = false;
        s.zoom = preferredZoomRequest(preferred+.05f,last,adjusted,s,apply);
        assert(std::abs(s.zoom-(preferred+.05f)) < 1e-6f);
    }
    Settings s;
    float last = obstructionZoom(s.zoom,1,config);
    s.zoom = preferredZoomRequest(last+.1f,last,true,s,false);
    assert(std::abs(s.zoom-.75f) < 1e-6f); // player delta relative to preference
    bool adjusted = false, apply = false;
    last = .42f;
    resetObstructionZoom(adjusted,last,apply);
    assert(!adjusted && last == .42f && !apply);
    apply = true;
    resetObstructionZoom(adjusted,last,apply);
    assert(!adjusted && last == .42f && apply);
}
}
int main() {
    measurements(); hysteresis(); closedLoop(); priorityAndInvalid(); ratesAndPreferences(); resetZoomRecovery();
    std::cout << "PASS obstruction policy: measurements, timing, noisy closed loop, priorities, invalid inputs, rates/preferences\n";
}
