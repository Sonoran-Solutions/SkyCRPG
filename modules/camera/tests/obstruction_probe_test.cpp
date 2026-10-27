// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "obstruction_probe.h"
#include <cassert>
#include <limits>
using namespace skycrpg::camera;

int main() {
    ObstructionProbe p;
    auto sample = [](std::uint64_t frame, std::uint64_t generation, double seconds) {
        return ProbeSample{frame, generation, seconds,
            {static_cast<float>(frame), 2, 3}, {4, 5, 6}, true};
    };
    assert(!p.observe(sample(10, 1, 1)));
    auto pair = p.observe(sample(11, 1, 1.01));
    assert(pair && pair->rootFrame == 10 && pair->providerFrame == 11);
    assert(pair->previousRoot.x == 10 && pair->providerLast.x == 4 && pair->generation == 1);
    assert(!p.observe(sample(13, 1, 1.03))); // dropped frame
    assert(!p.observe(sample(14, 2, 1.04))); // generation changed
    assert(!p.observe(sample(15, 2, 2))); // stale time
    assert(!p.observe(sample(16, 2, 1))); // clock moved backward
    auto invalid = sample(17, 2, 1.01);
    invalid.root.x = std::numeric_limits<float>::quiet_NaN();
    assert(!p.observe(invalid));
    assert(!p.observe(sample(18, 2, 1.02))); // invalidated prior
    invalid = sample(19, 2, 1.03); invalid.eligible = false;
    assert(!p.observe(invalid));
    assert(!p.observe(sample(20, 2, 1.04)));
    p.suspend();
    assert(!p.observe(sample(21, 2, 1.05)));
    p.start();
    assert(p.emitted() == 0 && !p.observe(sample(0, 3, 0)));
    for (std::uint64_t i = 1; i <= ObstructionProbe::pairLimit; ++i)
        assert(p.observe(sample(i, 3, static_cast<double>(i) / 100.0)));
    assert(p.emitted() == ObstructionProbe::pairLimit);
    assert(!p.observe(sample(129, 3, 1.29)));
    assert(p.emitted() == ObstructionProbe::pairLimit);
}
