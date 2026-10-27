// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_trace.h"
#include <cassert>
#include <cstring>
using namespace skycrpg::camera;
int main() {
    PrecisionTraceState before{};
    before.context.prerequisites = before.context.world = before.context.firstPerson = true;
    before.owned = before.armed = true;
    auto after = before;
    after.context.menu = after.context.special = true;
    after.owned = false;
    assert(traceBits(before) == 0x1a6);
    assert(traceBits(after) == 0x13e);
    PrecisionTraceRecord r{PrecisionTraceEvent::retention,before,after,1,true,true,0};
    char line[192]{};
    assert(formatPrecisionTrace(line,sizeof(line),r));
    assert(std::strstr(line,"event=3 before=0x1a6 after=0x13e") != nullptr);
    char tiny[8]{};
    assert(!formatPrecisionTrace(tiny,sizeof(tiny),r));
    assert(!formatPrecisionTrace(nullptr,0,r));
    PrecisionTraceBudget budget;
    for (unsigned i = 0;i < PrecisionTraceBudget::limit;++i) assert(budget.take());
    assert(!budget.take() && budget.used() == PrecisionTraceBudget::limit);
}
