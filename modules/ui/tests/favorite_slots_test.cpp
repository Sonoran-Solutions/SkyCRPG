// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "favorite_slots.h"
#include <cassert>
#include <string>
struct Slot { std::uint32_t formID{}; bool bound{}; std::string name; int count{}; };
int main() {
    using namespace skycrpg::ui;
    assert(DecideHotbarAction(0, true, ActionKind::Weapon, false, 1, false) == HotbarAction::None);
    assert(DecideHotbarAction(0, false, ActionKind::None, false, 1, false) == HotbarAction::None);
    assert(DecideHotbarAction(0, false, ActionKind::Weapon, false, 1, false) == HotbarAction::Equip);
    assert(DecideHotbarAction(0, false, ActionKind::Weapon, true, 1, false) == HotbarAction::Unequip);
    assert(DecideHotbarAction(0, false, ActionKind::Consumable, false, 1, false) == HotbarAction::Use);
    assert(DecideHotbarAction(0, false, ActionKind::Weapon, false, 0, false) == HotbarAction::None);
    assert(DecideHotbarAction(0, false, ActionKind::Weapon, false, 1, true) == HotbarAction::None);
    assert(DecideHotbarAction(8, false, ActionKind::Weapon, false, 1, false) == HotbarAction::None);
    using skycrpg::ui::FillFavoriteSlots;
    using skycrpg::ui::SetNativeBinding;
    std::array<Slot, 8> slots{};
    for (unsigned i = 0; i < 8; ++i) {
        SetNativeBinding(slots, i, Slot{ i + 1, false, "key", 1 });
        assert(slots[i].formID == i + 1 && slots[i].bound);
    }
    SetNativeBinding(slots, 8, Slot{ 99, false, "invalid", 1 });
    assert(slots[7].formID == 8);
    SetNativeBinding(slots, 2, Slot{ 42, false, "magic override", 0 });
    assert(slots[2].formID == 42 && slots[2].bound);
    assert(skycrpg::ui::HasInventoryCount(1));
    assert(!skycrpg::ui::HasInventoryCount(0));
    assert(!skycrpg::ui::HasInventoryCount(-1));
    slots = {};
    slots[2] = { 42, true, "bound", 1 };
    // Preserve real key positions, sort favorites deterministically, and
    // suppress duplicates of both bound and unbound forms.
    FillFavoriteSlots(slots, {{55,false,"later",3}, {42,false,"duplicate",1},
                              {13,false,"first",2}, {13,false,"first",2}});
    assert(slots[0].formID == 13 && !slots[0].bound && slots[0].count == 2);
    assert(slots[1].formID == 55 && !slots[1].bound && slots[1].count == 3);
    assert(slots[2].formID == 42 && slots[2].bound && slots[3].formID == 0);
    // Fresh snapshots do not preserve removed/depleted entries.
    slots = {};
    FillFavoriteSlots(slots, {{55,false,"remaining",1}});
    assert(slots[0].formID == 55 && slots[1].formID == 0);
    // Excess favorites are deterministically truncated, not dispatched.
    slots = {};
    std::vector<Slot> many;
    for (std::uint32_t id = 10; id > 0; --id) many.push_back({id, false, "x", 1});
    FillFavoriteSlots(slots, many);
    for (std::uint32_t i = 0; i < 8; ++i) assert(slots[i].formID == i + 1 && !slots[i].bound);
}
