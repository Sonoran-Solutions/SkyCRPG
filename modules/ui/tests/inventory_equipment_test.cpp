// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "inventory/equipment_policy.h"
#include <cassert>
#include <cstdint>
using namespace skycrpg::ui::inventory::equipment;
int main()
{
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        const auto slots = ArmorSlots(1u << bit);
        for (std::size_t slot = 0; slot < SlotCount; ++slot) {
            const bool expected = (slot == 0 && (bit == 0 || bit == 1 || bit == 12)) ||
                (slot == 1 && bit == 2) || (slot == 2 && bit == 3) ||
                (slot == 3 && bit == 7) || (slot == 4 && bit == 9) ||
                (slot == 6 && bit == 5) || (slot == 7 && bit == 6);
            assert(slots[slot] == expected);
        }
    }
    const auto robe = ArmorSlots((1u << 2) | (1u << 3) | (1u << 7));
    assert(robe[1] && robe[2] && robe[3] && !robe[0] && !robe[5]);
    for (const auto occupied : ArmorSlots(0)) assert(!occupied);
    assert(!Conflicting(0, 12) && !Conflicting(12, 12));
    assert(Conflicting(12, 13));
}
