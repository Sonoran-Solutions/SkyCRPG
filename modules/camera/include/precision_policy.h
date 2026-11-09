// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <array>
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
// Pinned engine read-lock count word: low 28 bits count readers, bit 31
// marks a writer. One CAS only; even a same-thread writer defers the query.
class PrecisionMountReadLock {
public:
    explicit PrecisionMountReadLock(std::uint32_t& word) : count(word) {
        auto observed = count.load();
        acquired = observed < 0x0fffffffu && count.compare_exchange_strong(observed,observed+1);
    }
    ~PrecisionMountReadLock() { if (acquired) count.fetch_sub(1); }
    explicit operator bool() const { return acquired; }
    PrecisionMountReadLock(const PrecisionMountReadLock&) = delete;
    PrecisionMountReadLock& operator=(const PrecisionMountReadLock&) = delete;
private:
    static_assert(std::atomic_ref<std::uint32_t>::is_always_lock_free);
    static_assert(std::atomic_ref<std::uint32_t>::required_alignment <= alignof(std::uint32_t));
    std::atomic_ref<std::uint32_t> count;
    bool acquired = false;
};
// Ownership, entry/return intent and epoch share one CAS word. Capture it BEFORE reading
// context; a claim, cancellation or consumption invalidates that observation.
// Saturation permanently closes the session instead of wrapping into an ABA.
class PrecisionReturn {
public:
    using Token = std::uint64_t;
    static constexpr Token epochLimit = (std::numeric_limits<Token>::max)() & ~Token{3};
    explicit PrecisionReturn(Token initialEpoch = 0) : state(initialEpoch & epochLimit) {}
    Token snapshot() const { return state.load(); }
    static bool owned(Token token) { return (token & ownership) != 0; }
    bool owned() const { return owned(snapshot()); }
    static bool requested(Token token) { return (token & pending) != 0 && owned(token); }
    static bool entryRequested(Token token) { return (token & pending) != 0 && !owned(token); }
    bool requestEntry(Context c, Token token) {
        if (exhausted(token) || owned(token) || entryRequested(token) ||
            toggleRequest(c,false) != ToggleRequest::firstPerson) return false;
        return state.compare_exchange_strong(token,token | pending);
    }
    // Main only: fresh validation, synchronous native transition and claim are
    // one service operation. Input never carries an after-context or claims.
    template<class Transition> bool enter(Context before, Token token, Transition transition) {
        if (!entryRequested(token)) return false;
        if (toggleRequest(before,false) != ToggleRequest::firstPerson) {
            static_cast<void>(invalidate(token));
            return false;
        }
        const auto nextEpoch = retired(token,true);
        // Keep the pending bit during the synchronous transition so another
        // input producer cannot enqueue into the in-flight entry's epoch.
        const auto active = exhausted(nextEpoch) ? nextEpoch : nextEpoch | pending;
        if (!state.compare_exchange_strong(token,active) || exhausted(active)) return false;
        const auto after = transition();
        auto expected = active;
        const auto next = retired(active,true);
        if (!claimPrecisionOwnership(before,after) || exhausted(next)) {
            static_cast<void>(invalidate(active));
            return false;
        }
        return state.compare_exchange_strong(expected,next | ownership);
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
// Main-thread departure observations belong to one epoch. Cancellation or a
// completed entry/return discards the previous epoch's unowned FP history.
class PrecisionCameraObservation {
public:
    bool retire(bool firstPerson, PrecisionReturn::Token token) {
        const auto epoch = token & PrecisionReturn::epochLimit;
        const bool departure = epoch == observedEpoch && wasFirstPerson && !firstPerson;
        observedEpoch = epoch;
        wasFirstPerson = firstPerson;
        return !firstPerson && (departure || PrecisionReturn::owned(token) || PrecisionReturn::requested(token));
    }
private:
    PrecisionReturn::Token observedEpoch = 0;
    bool wasFirstPerson = false;
};
// First occurrence per reason, preallocated and lock-free. Producers publish
// packed scalar context only; the non-camera task drains and formats it.
enum class PrecisionReason : unsigned {
    serviceMain, serviceOther, entryClaimed, entryContext, entryToken, inputUnsafe,
    inputUnowned, inputMapped, inputNoEdge, ownershipLost, requestAccepted, requestRejected, servicePending, refreshPending,
    menuBusy, mountBusy, invalidContext, leaseMissing, unlockPending, sessionChanged, returned, returnRefused, entryBefore, count
};
class PrecisionDiagnostics {
public:
    // Bit 8: return pending; entry pending is not encoded. Bit 11: caller-supplied
    // Main-thread marker, including the entry service as well as input callbacks.
    void note(PrecisionReason reason, Context c = {}, PrecisionReturn::Token token = 0, bool changed = false, bool mainThread = false) {
        const auto index = static_cast<unsigned>(reason);
        const std::uint32_t value = 1u | (c.prerequisites?2u:0u) | (c.world?4u:0u) |
            (c.firstPerson?8u:0u) | (c.menu?16u:0u) | (c.special?32u:0u) | (c.slide?64u:0u) |
            (PrecisionReturn::owned(token)?128u:0u) | (PrecisionReturn::requested(token)?256u:0u) |
            (changed?512u:0u) | (c.enabled?1024u:0u) | (mainThread?2048u:0u);
        std::uint32_t empty = 0;
        if (samples[index].compare_exchange_strong(empty,value)) pending.fetch_or(1u << index);
    }
    bool requested() const { return pending.load() != 0; }
    template<class Emit> void drain(Emit emit) {
        const auto bits = pending.exchange(0);
        for (unsigned i = 0; i < static_cast<unsigned>(PrecisionReason::count); ++i)
            if (bits & (1u << i)) emit(i,samples[i].load());
    }
private:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert(static_cast<unsigned>(PrecisionReason::count) < 32);
    std::array<std::atomic<std::uint32_t>,static_cast<unsigned>(PrecisionReason::count)> samples{};
    std::atomic<std::uint32_t> pending{0};
};
} // namespace skycrpg::camera
