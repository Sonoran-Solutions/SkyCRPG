// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <atomic>
#include <optional>
#include <cstdint>
#include <limits>

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
// Ownership, intent and epoch share one CAS word. Capture it BEFORE reading
// context; a claim, cancellation or consumption invalidates that observation.
// Saturation permanently closes the session instead of wrapping into an ABA.
class PrecisionReturn {
public:
    using Token = std::uint64_t;
    static constexpr Token epochLimit = std::numeric_limits<Token>::max() & ~Token{3};
    explicit PrecisionReturn(Token initialEpoch = 0) : state(initialEpoch & epochLimit) {}
    Token snapshot() const { return state.load(); }
    static bool owned(Token token) { return (token & ownership) != 0; }
    bool owned() const { return owned(snapshot()); }
    static bool requested(Token token) { return (token & pending) != 0; }
    bool claim(Context before, Context after, Token token) {
        if (owned(token) || requested(token) || exhausted(token) || !claimPrecisionOwnership(before,after)) return false;
        const auto next = retired(token,true);
        const auto claimed = exhausted(next) ? next : next | ownership;
        return state.compare_exchange_strong(token,claimed) && !exhausted(next);
    }
    bool request(Context c, Token token) {
        if (exhausted(token) || requested(token) || !c.firstPerson ||
            toggleRequest(c,owned(token)) != ToggleRequest::thirdPerson) return false;
        return state.compare_exchange_strong(token,token | pending);
    }
    // A focus interruption discards the intent but may retain an owned session.
    // All flags and the epoch change atomically: there is no second clear step.
    void cancel(bool revoke = true) {
        auto token = snapshot();
        while (!state.compare_exchange_weak(token,retired(token,revoke))) {}
    }
    bool invalidate(Token token, bool revoke = true) {
        auto expected = token;
        if (state.compare_exchange_strong(expected,retired(expected,revoke))) return true;
        // A producer may add pending after our snapshot. Retire that same
        // session too, but never revoke a newer claim/cancel/consumption epoch.
        if ((expected & epochLimit) != (token & epochLimit)) return false;
        return state.compare_exchange_strong(expected,retired(expected,revoke));
    }
    void retain(Context c, bool hardSpecial, Token token) {
        if ((owned(token) || requested(token)) && !retainPrecisionOwnership(c,owned(token),hardSpecial))
            static_cast<void>(invalidate(token));
    }
    bool take(Context c, bool haveLease, std::optional<bool> locked, Token token) {
        if (!requested(token)) return false;
        if (!c.firstPerson || toggleRequest(c,owned(token)) != ToggleRequest::thirdPerson) {
            static_cast<void>(invalidate(token,!retainPrecisionOwnership(c,owned(token),c.special)));
            return false;
        }
        if (!haveLease || !locked || *locked) return false;
        return state.compare_exchange_strong(token,retired(token,true));
    }
private:
    static_assert(std::atomic<Token>::is_always_lock_free);
    static constexpr Token pending = 1, ownership = 2;
    static bool exhausted(Token token) { return (token & epochLimit) == epochLimit; }
    static Token retired(Token token, bool revoke) {
        if (exhausted(token) || (token & epochLimit) == epochLimit-4) return epochLimit;
        return ((token & epochLimit)+4) | (revoke ? 0 : token & ownership);
    }
    std::atomic<Token> state;
};
} // namespace skycrpg::camera
