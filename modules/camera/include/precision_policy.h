// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <atomic>
#include <optional>

namespace skycrpg::camera {
// All drawn weapons and spells, including crossbows and concentration spells,
// suppress automatic obstruction framing without classifying the equipment.
constexpr bool precisionSuppressesObstruction(bool weaponDrawn) { return weaponDrawn; }
// Claim only an immediately observed transition from an eligible third-person
// state. A late, unrelated first-person transition cannot claim a session.
constexpr bool claimPrecisionOwnership(Context before, Context after) {
    return before.prerequisites && before.world && !before.special && !before.menu && !before.slide &&
        !before.firstPerson && after.prerequisites && after.world && !after.special && !after.menu && !after.slide &&
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
// One validated input intent, never a queued engine object or callback. Epochs
// prevent an input already in flight from undoing a lifecycle cancellation.
class PrecisionReturn {
public:
    unsigned generation() const { return state.load() & ~pending; }
    bool request(Context c, bool owned, unsigned epoch) {
        if (!c.firstPerson || toggleRequest(c,owned) != ToggleRequest::thirdPerson) return false;
        return state.compare_exchange_strong(epoch,epoch | pending);
    }
    void cancel() { state.fetch_add(2); state.fetch_and(~pending); }
    bool take(Context c, bool owned, bool haveLease, std::optional<bool> locked) {
        auto expected = state.load();
        if (!(expected & pending)) return false;
        if (!c.firstPerson || toggleRequest(c,owned) != ToggleRequest::thirdPerson) {
            cancel(); return false;
        }
        if (!haveLease || !locked || *locked) return false;
        return state.compare_exchange_strong(expected,expected & ~pending);
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned pending = 1;
    std::atomic<unsigned> state{0};
};
} // namespace skycrpg::camera
