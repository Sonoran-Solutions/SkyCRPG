// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace skycrpg::ui::inventory::equipment {
enum class Slot : std::size_t { Head, Body, Hands, Feet, Left, Right, Amulet, Ring, Count };
inline constexpr std::size_t SlotCount = static_cast<std::size_t>(Slot::Count);
// Native BOD2 bits, not armor type flags. A multi-slot item appears in every
// covered MVP slot. Head includes hair-only hoods and circlets, never long hair.
constexpr std::array<bool, SlotCount> ArmorSlots(std::uint32_t mask)
{
    return {(mask & ((1u << 0) | (1u << 1) | (1u << 12))) != 0,
        (mask & (1u << 2)) != 0, (mask & (1u << 3)) != 0,
        (mask & (1u << 7)) != 0, (mask & (1u << 9)) != 0, false,
        (mask & (1u << 5)) != 0, (mask & (1u << 6)) != 0};
}
// Never choose between multiple equipped records occupying the same slot.
constexpr bool Conflicting(std::uint32_t current, std::uint32_t incoming)
{
    return current != 0 && incoming != 0 && current != incoming;
}
}
