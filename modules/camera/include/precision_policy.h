// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"

namespace skycrpg::camera {
// Claim only an immediately observed transition from an eligible third-person
// state. A late, unrelated first-person transition cannot claim a session.
constexpr bool claimPrecisionOwnership(Context before, Context after) {
    return before.prerequisites && before.world && !before.special && !before.menu &&
        !before.firstPerson && after.prerequisites && after.world && !after.special && !after.menu &&
        after.firstPerson;
}
// A menu can temporarily replace the first-person camera state, so defer
// state-specific validation while it owns focus. Provider/world loss still
// revokes immediately, and the input sink separately cancels on native POV.
// On return to gameplay, all first-person and special-state guards must pass
// before a subsequent F6 can reclaim the view. This cannot detect an unsafe
// transition that starts and ends entirely while a menu owns focus.
constexpr bool retainPrecisionOwnership(Context c, bool owned, bool hardSpecial) {
    return owned && c.prerequisites && c.world &&
        (c.menu || (c.firstPerson && !hardSpecial && !c.special));
}
} // namespace skycrpg::camera
