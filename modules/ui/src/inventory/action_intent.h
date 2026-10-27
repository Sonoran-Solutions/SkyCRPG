// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "inventory_snapshot.h"
#include <array>
#include <optional>
#include <set>

namespace skycrpg::ui::inventory {
enum class Action { Activate, Favorite, Drop };
enum class Refusal { None, Unavailable, Focus, Stale, Missing, Ambiguous, Identity, Quantity, Restricted, Duplicate, Pending };
inline const char* Reason(Refusal reason) {
    switch (reason) {
    case Refusal::None: return "allow";
    case Refusal::Unavailable: return "unavailable";
    case Refusal::Focus: return "focus";
    case Refusal::Stale: return "stale-generation";
    case Refusal::Missing: return "removed";
    case Refusal::Ambiguous: return "ambiguous-instance";
    case Refusal::Identity: return "identity-changed";
    case Refusal::Quantity: return "invalid-quantity";
    case Refusal::Restricted: return "native-restriction";
    case Refusal::Duplicate: return "duplicate-input";
    case Refusal::Pending: return "pending-intent";
    }
    return "unknown";
}
struct Selection { Stamp stamp; Entry entry; std::uint64_t token{}; };
struct Intent { Selection selection; Action action{}; bool quantity{}, nativeQuestFeedback{}; };
inline Refusal Validate(const Selection& selected, const Snapshot& live, bool focused, Action action,
    std::uint32_t amount, bool nativePermits = true, bool nativeQuestFeedback = false)
{
    if (!focused) return Refusal::Focus;
    if (!live.available) return Refusal::Unavailable;
    if (selected.stamp != live.stamp) return Refusal::Stale;
    const auto* entry = live.Find(selected.entry.identity.nativeIndex);
    if (!entry || entry->count == 0) return Refusal::Missing;
    if (entry->ambiguous || !ValidTraits(entry->identity.traits)) return Refusal::Ambiguous;
    if (entry->identity != selected.entry.identity || entry->count != selected.entry.count) return Refusal::Identity;
    if (amount == 0 || amount > entry->count) return Refusal::Quantity;
    if (!nativePermits || !entry->enabled || (action == Action::Drop && entry->identity.traits.quest && !nativeQuestFeedback)) return Refusal::Restricted;
    return Refusal::None;
}
class ActionIntents {
public:
    // Release all devices before a new gesture can activate. A repeat/down-up
    // duplicate cannot be made fresh by a new snapshot or action kind.
    void Edge(unsigned deviceKey, bool down) {
        if (down) {
            if (_held.empty()) _consumed = false;
            _held.insert(deviceKey);
        } else {
            _held.erase(deviceKey);
        }
    }
    Refusal Begin(const Selection& selected, const Snapshot& live, bool focused, Action action,
        std::uint32_t amount, bool quantity = false, bool nativePermits = true, bool nativeQuestFeedback = false) {
        if (_consumed || selected.token == _lastToken) return Refusal::Duplicate;
        if (_pending) return Refusal::Pending;
        const auto result = Validate(selected, live, focused, action, amount, nativePermits, nativeQuestFeedback);
        // A refused gesture is also spent; a refresh never retries it.
        _consumed = true; _lastToken = selected.token;
        if (result == Refusal::None) _pending = Intent{ selected, action, quantity, nativeQuestFeedback };
        return result;
    }
    Refusal Confirm(const Snapshot& live, bool focused, std::uint32_t amount) {
        if (!_pending || !_pending->quantity) return Refusal::Pending;
        const auto result = Validate(_pending->selection, live, focused, _pending->action, amount, true, _pending->nativeQuestFeedback);
        _pending->quantity = false; // confirmation is at most once, even on refusal
        if (result != Refusal::None) _pending.reset();
        return result;
    }
    void Observe() { if (_pending && !_pending->quantity) _pending.reset(); }
    void Cancel() { _pending.reset(); } // keep gesture spent across a modal close
    void Invalidate() { Cancel(); _held.clear(); _consumed = true; }
    const std::optional<Intent>& Pending() const { return _pending; }
private:
    std::optional<Intent> _pending;
    std::set<unsigned> _held;
    std::uint64_t _lastToken{};
    bool _consumed{};
};
}
