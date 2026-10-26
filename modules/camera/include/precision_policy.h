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
// Retain only while the provider, world and owned first-person state remain
// valid. Never force a camera transition from this predicate.
constexpr bool retainPrecisionOwnership(Context c, bool owned) {
    return owned && c.prerequisites && c.world && !c.special && c.firstPerson;
}
} // namespace skycrpg::camera
