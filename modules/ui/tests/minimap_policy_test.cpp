#include "minimap/compass_policy.h"
#include <cassert>

int main()
{
    using namespace skycrpg::ui::minimap;
    for (int bits = 0; bits < 64; ++bits) {
        Capability state{bool(bits & 1), bool(bits & 2), bool(bits & 4),
            bool(bits & 8), bool(bits & 16), bool(bits & 32)};
        assert(ShouldSuppress(state) == (bits == 31));
        state.ready = FunctionalReadinessProven();
        assert(!ShouldSuppress(state));
    }
}
