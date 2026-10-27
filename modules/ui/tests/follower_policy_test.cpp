#include "follower/policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::ui::follower;
int main()
{
    Observation o{true};
    assert(Classify(o) == State::none);
    o.count = 1; o.humanoid = true; o.loaded = true; o.named = true; o.current = 50; o.maximum = 100;
    assert(Classify(o) == State::live); // recruit, including absent portrait provider
    o.loaded = false; assert(Classify(o) == State::unknown); // unload
    o.loaded = true; assert(Classify(o) == State::live); // reload
    o.dead = true; assert(Classify(o) == State::dead);
    o.dead = false; o.count = 0; assert(Classify(o) == State::none); // dismiss
    o.available = false; assert(Classify(o) == State::unknown); // stale/load/expiry
    o.available = true; o.count = 2; assert(Classify(o) == State::unsupported);
    o.count = 1; o.humanoid = false; assert(Classify(o) == State::unsupported);
    o.humanoid = true; o.named = false; assert(Classify(o) == State::unknown);
    o.named = true; o.maximum = std::numeric_limits<float>::quiet_NaN(); assert(Classify(o) == State::unknown);
}
