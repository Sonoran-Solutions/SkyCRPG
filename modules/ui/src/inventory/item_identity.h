// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cstdint>
#include <string>

namespace skycrpg::ui::inventory {
// Runtime ID plus originating plugin/local ID. Never serialized; ESL and full
// plugins are resolved by the engine, not by truncating the load-order byte.
struct FormIdentity {
    std::uint32_t runtime{}, local{};
    std::string plugin;
    bool operator==(const FormIdentity&) const = default;
};
struct ItemTraits {
    FormIdentity base, enchantment, owner, poison;
    float temper{1.0F}, charge{-1.0F};
    std::uint32_t poisonCount{};
    std::string name;
    bool stolen{}, quest{}, worn{}, wornLeft{}, favorite{};
    bool operator==(const ItemTraits&) const = default;
};
inline bool ValidTraits(const ItemTraits& traits)
{
    return traits.base.runtime != 0 && std::isfinite(traits.temper) && std::isfinite(traits.charge);
}
// Native backing slot disambiguates even identical extra-data entries, ONLY
// inside one snapshot generation. Equivalent copies within a stack stay one row.
struct ItemIdentity {
    std::uint32_t nativeIndex{};
    ItemTraits traits;
    bool operator==(const ItemIdentity&) const = default;
};
}
