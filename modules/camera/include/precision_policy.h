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
// `hardSpecial` excludes only the two control-map enabled flags: mounts,
// swimming, killmoves, special camera states, missing control interfaces and
// scripted/beast/FOV modes remain hard invalidations even during a menu.
// Menu focus may temporarily disable the control-map flags without revoking a
// valid first-person session. Menu focus still blocks F6 via toggleRequest.
// Never force a camera transition from this predicate.
constexpr bool retainPrecisionOwnership(Context c, bool owned, bool hardSpecial) {
    return owned && c.prerequisites && c.world && c.firstPerson && !hardSpecial &&
        (c.menu || !c.special);
}
} // namespace skycrpg::camera
