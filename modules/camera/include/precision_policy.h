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
// A hazard-free tween retains ownership but never acts; the next input in a
// settled non-first-person state (including third person or furniture) revokes
// it before F6 is evaluated. Native POV and provider/world loss still revoke
// immediately. An unsafe transition wholly hidden by menu focus is unobserved.
constexpr bool retainPrecisionOwnership(Context c, bool owned, bool hardSpecial) {
    return owned && c.prerequisites && c.world &&
        (c.menu || (c.firstPerson && !hardSpecial && !c.special) || (c.tween && !hardSpecial));
}
} // namespace skycrpg::camera
