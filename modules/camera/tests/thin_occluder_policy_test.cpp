// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "thin_occluder_policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::camera;
using namespace skycrpg::camera::thin;
int main() {
    assert(validateAlpha(-1) == .1f && validateAlpha(2) == .8f && validateAlpha(.35f) == .35f);
    assert(validateAlpha(std::numeric_limits<float>::quiet_NaN()) == .35f);
    assert(!recycle(false,false,true,true,true,false,10,10)); // No same-call reuse/release.
    assert(!recycle(false,false,true,true,true,false,10,11));
    assert(recycle(false,false,true,true,true,false,10,12));
    assert(!recycle(true,false,true,true,true,false,10,12));
    assert(!recycle(false,true,true,true,true,false,10,12));
    assert(!recycle(false,false,false,true,true,false,10,12));
    assert(!recycle(false,false,true,false,true,false,10,12));
    assert(!recycle(false,false,true,true,false,false,10,12));
    assert(!recycle(false,false,true,true,true,true,10,12));
    const auto form = parseForm("Skyrim.esm|0602A6");
    assert(form && form->plugin == "Skyrim.esm" && form->local == 0x602A6);
    for (const auto text : {"", "Skyrim.esm", "|0602A6", "Skyrim.esm|0", "Skyrim.esm|GGGGGG",
        "Skyrim.esm|0x602A6", "Skyrim.esm|10602A6", "../Skyrim.esm|0602A6", "Skyrim.esm|602A6junk"}) assert(!parseForm(text));
    std::array<std::uint32_t,maxForms> resolved{0x060602A6,0xfe123abc};
    assert(matches(0x060602A6,resolved)); // Resolved plugin index, not a masked reference ID.
    assert(matches(0xfe123abc,resolved));
    assert(!matches(0,resolved) && !matches(0x602A6,resolved) && !matches(0x060602AF,resolved));
    for (unsigned mask = 0;mask < 32;++mask)
        assert(install((mask&1)!=0,(mask&2)!=0,(mask&4)!=0,(mask&8)!=0,(mask&16)!=0) == (mask == 31));
    assert(eligible(State::normal) && eligible(State::targetLock));
    for (const auto s : {State::disabled,State::invalidWorld,State::special,State::menu,
        State::firstPerson,State::precision,State::targeting,State::obstruction}) assert(!eligible(s));
    for (bool native : {false,true}) assert(compressionAllowed(false,native)); // Disabled equals branch base.
    assert(!compressionAllowed(true,false) && compressionAllowed(true,true));

    const auto step = advance({0,0,0},{300,0,0},{-.047f,0,0},15,0);
    assert(step && step->x == 30); // Initial penetrating beam still makes hull-diameter progress.
    assert(advance({0,0,0},{300,0,0},{10,0,0},30,0)->x == 70);
    assert(!advance({0,0,0},{30,0,0},{0,0,0},15,0));
    assert(!advance({0,0,0},{0,0,0},{0,0,0},15,0));
    assert(!advance({0,0,0},{300,0,0},{0,0,0},15,maxSteps));
    assert(!advance({0,0,0},{300,0,0},{0,0,0},65,0));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(!advance({nan,0,0},{300,0,0},{0,0,0},15,0));
    assert(!advance({0,0,0},{300,0,0},{0,0,0},nan,0));

    GapDecision gap; gap.originalSeen = true;
    gap.contact(true,true,true,-.047f,0); // Allowed starting beam.
    gap.contact(true,true,false,.1f,1); // Adjacent ALREADY admitted listed beam.
    assert(gap.result() == GapResult::clear);
    gap.contact(true,false,false,.9f,2); // Far wall first in callback order.
    gap.contact(true,false,false,.3f,3); // Near wall is the correct answer.
    assert(gap.result() == GapResult::obstruction && gap.index == 3 && gap.nearest == .3f);
    gap.contact(true,false,false,.7f,4);
    assert(gap.index == 3);
    gap.contact(true,false,true,-.01f,5); // Start overlap cannot manufacture a safe forward endpoint.
    assert(gap.result() == GapResult::refuse);
    for (unsigned defect = 0;defect < 6;++defect) {
        GapDecision bad; bad.originalSeen = true;
        if (defect == 0) bad.complete = false;
        if (defect == 1) bad.originalSeen = false;
        if (defect == 2) bad.contact(false,false,false,.1f,0);
        if (defect == 3) bad.contact(true,false,false,nan,0);
        if (defect == 4) bad.contact(true,false,false,1.01f,0);
        if (defect == 5) bad.contact(true,true,false,nan,0);
        assert(bad.result() == GapResult::refuse);
    }
    // No global failure latch: a next-frame complete proof can resume.
    GapDecision recovered; recovered.originalSeen = true;
    assert(recovered.result() == GapResult::clear);
    assert(nativeIgnored(1,true,false) && !nativeIgnored(17,true,false));
    assert(nativeIgnored(8,false,true) && !nativeIgnored(1,false,true));

    Budget budget;
    for (unsigned i = 0;i < maxCasts;++i) assert(budget.cast());
    assert(!budget.cast());
    for (unsigned i = 0;i < maxGaps;++i) assert(budget.gap());
    assert(!budget.gap());
    budget = {}; assert(budget.cast() && budget.gap());
    assert(intersects({0,0,0},{100,0,0},{50,10,0},11));
    assert(!intersects({0,0,0},{100,0,0},{50,20,0},11));
    assert(!intersects({0,0,0},{0,0,0},{0,0,0},10));
    assert(!intersects({0,0,0},{100,0,0},{nan,0,0},10));

    FadeState fade; fade.touch(0);
    assert(fade.step(.05,.05,.35f,true) && !fade.admitted && fade.alpha < 1 && fade.alpha > .35f);
    fade.touch(.05);
    assert(fade.step(.11,.05,.35f,true) && fade.admitted);
    const auto alpha = fade.alpha;
    // One absent observation does not toggle admission or restore opacity.
    assert(fade.step(.12,.01,.35f,true) && fade.admitted && fade.alpha <= alpha);
    fade.touch(.15);
    for (unsigned i = 0;i < 5;++i) assert(fade.step(.16+i*.03,.03,.35f,true));
    assert(std::abs(fade.alpha-.35f) < .00001f);
    assert(fade.step(.56,.05,.35f,true) && !fade.admitted && fade.alpha > .35f);
    fade.touch(.58); // New enter dwell after expired contact, continuing smooth alpha.
    assert(fade.step(.59,.01,.35f,true) && !fade.admitted);
    assert(fade.step(.69,.05,.35f,true) && fade.admitted);
    assert(!fade.step(.70,.01,.35f,false) && fade.alpha == 1 && !fade.admitted);
    for (const auto exit : {Exit::disable,Exit::f8,Exit::menu,Exit::firstPerson,Exit::special,
        Exit::load,Exit::cell,Exit::unload,Exit::shutdown,Exit::conflict}) assert(immediateRestore(exit));
    for (unsigned mask = 0;mask < 32;++mask) {
        RestoreCheck state{(mask&1)!=0,(mask&2)!=0,(mask&4)!=0,(mask&8)!=0,(mask&16)!=0};
        assert(state.material() == ((mask&11) == 11));
        assert(state.property() == ((mask&21) == 21));
    }
    const auto flags = translucentFlags(0x12EC);
    assert(flags == 0x10ED && (flags&0x200) == 0 && (flags&1) == 1);
    assert(translucentFlags(0) == 0xED);
}
