// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_resume.h"
#include <cassert>
using namespace skycrpg::camera;
int main() {
    PrecisionResume s;
    ResumeObservation o{true,true,true,false,true,false,false,false,100};
    assert(s.observe(o) == ResumeDecision::retain);
    o.menu = true; o.firstPerson = false; o.special = true;
    assert(s.observe(o) == ResumeDecision::defer);
    o.menu = false; o.trustedTransition = true; o.milliseconds = 110;
    assert(s.observe(o) == ResumeDecision::defer);
    o.firstPerson = true; o.special = false; o.trustedTransition = false; o.milliseconds = 120;
    assert(s.observe(o) == ResumeDecision::retain);

    // There is never a transition request or spontaneous ownership acquisition.
    o.owned = false;
    assert(s.observe(o) == ResumeDecision::revoke);
    o.owned = true; o.firstPerson = false; o.special = true; o.trustedTransition = true;
    assert(s.observe(o) == ResumeDecision::revoke); // no observed menu link

    o.menu = true; assert(s.observe(o) == ResumeDecision::defer);
    o.menu = false; o.trustedTransition = false;
    assert(s.observe(o) == ResumeDecision::revoke); // unknown transition fails closed

    o.menu = true; assert(s.observe(o) == ResumeDecision::defer);
    o.menu = false; o.trustedTransition = true; o.milliseconds = 1000;
    assert(s.observe(o) == ResumeDecision::defer);
    o.milliseconds += PrecisionResume::graceMs + 1;
    o.firstPerson = true; o.special = false;
    assert(s.observe(o) == ResumeDecision::revoke); // late return cannot reclaim

    o.menu = true; o.firstPerson = false; o.special = true; o.milliseconds = 2000;
    assert(s.observe(o) == ResumeDecision::defer);
    o.menu = false; o.trustedTransition = true;
    assert(s.observe(o) == ResumeDecision::defer);
    --o.milliseconds;
    assert(s.observe(o) == ResumeDecision::revoke); // clock regression

    for (int i = 0;i < 3;++i) {
        o = {true,true,true,true,false,true,false,false,3000};
        assert(s.observe(o) == ResumeDecision::defer);
        if (i == 0) o.prerequisites = false;
        if (i == 1) o.world = false;
        if (i == 2) o.hazard = true; // includes separately-audited physical/script hazards
        assert(s.observe(o) == ResumeDecision::revoke);
    }
    o = {true,true,true,true,false,true,false,false,4000};
    assert(s.observe(o) == ResumeDecision::defer);
    s.cancel(); o.owned = false; o.menu = false; o.firstPerson = true; o.special = false;
    assert(s.observe(o) == ResumeDecision::revoke); // native POV cancellation remains authoritative
}
