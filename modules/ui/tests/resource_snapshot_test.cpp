#include "../src/resource_snapshot.h"
#include <cassert>
#include <limits>
int main() {
    using skycrpg::ui::IsValidResource;
    assert(IsValidResource({0, 100}));
    assert(IsValidResource({-5, 100}));
    assert(IsValidResource({150, 100}));
    assert(IsValidResource({50, 150}));
    assert(!IsValidResource({50, 0}));
    assert(!IsValidResource({50, -1}));
    assert(!IsValidResource({std::numeric_limits<float>::infinity(), 100}));
    assert(!IsValidResource({50, std::numeric_limits<float>::quiet_NaN()}));
}
