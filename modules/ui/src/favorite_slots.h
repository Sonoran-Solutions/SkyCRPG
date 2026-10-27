// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace skycrpg::ui {
enum class ActionKind { None, Weapon, Armor, Consumable, Scroll, Spell, Shout, Power };
enum class HotbarAction { None, Equip, Unequip, Use, EquipSpell, EquipShout, EquipPower };
inline HotbarAction DecideHotbarAction(unsigned index, bool bound, ActionKind kind, bool equipped, int count, bool blocked)
{
    if (index >= 8 || bound || blocked || count <= 0) return HotbarAction::None;
    switch (kind) {
    case ActionKind::Weapon: case ActionKind::Armor: return equipped ? HotbarAction::Unequip : HotbarAction::Equip;
    case ActionKind::Consumable: return HotbarAction::Use;
    case ActionKind::Scroll: return HotbarAction::Equip;
    case ActionKind::Spell: return HotbarAction::EquipSpell;
    case ActionKind::Shout: return HotbarAction::EquipShout;
    case ActionKind::Power: return HotbarAction::EquipPower;
    default: return HotbarAction::None;
    }
}
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
