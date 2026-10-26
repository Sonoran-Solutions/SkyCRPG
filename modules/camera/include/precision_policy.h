// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"

namespace skycrpg::camera {
// A manual precision session grants permission to return from the native view
// only while the camera provider and world remain valid. Menu focus merely
// postpones an explicit return; it does not itself change camera ownership.
// Never force a view transition from this predicate.
constexpr bool retainPrecisionOwnership(Context c, bool owned) {
    return owned && c.prerequisites && c.world && !c.special;
}
} // namespace skycrpg::camera
