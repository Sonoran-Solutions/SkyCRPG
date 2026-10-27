// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace skycrpg::ui {
// The engine key index is the presentation index; magic is applied after items.
template<class Slot>
void SetNativeBinding(std::array<Slot, 8>& slots, unsigned index, Slot slot)
{
    if (index >= slots.size() || !slot.formID) return;
    slot.bound = true;
    slots[index] = std::move(slot);
}

inline bool HasInventoryCount(int count) { return count > 0; }
// Fill holes after native bindings have been resolved. IDs exist only during
// this snapshot; this is not an inventory database or an assignment system.
// A duplicate form never consumes multiple auto-favorite slots.
template<class Slot>
void FillFavoriteSlots(std::array<Slot, 8>& slots, std::vector<Slot> favorites)
{
    std::sort(favorites.begin(), favorites.end(), [](const Slot& a, const Slot& b) {
        return a.formID < b.formID;
    });
    for (auto& favorite : favorites) {
        if (!favorite.formID) continue;
        const bool shown = std::any_of(slots.begin(), slots.end(), [&](const Slot& slot) {
            return slot.formID == favorite.formID;
        });
        if (shown) continue;
        const auto empty = std::find_if(slots.begin(), slots.end(), [](const Slot& slot) {
            return slot.formID == 0;
        });
        if (empty == slots.end()) break;
        favorite.bound = false;
        *empty = std::move(favorite);
    }
}
}
