// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "inventory_snapshot.h"
#include <cstddef>
#include <span>
#include <utility>

namespace skycrpg::ui::inventory {
struct NativeForm {
    FormIdentity base;
    std::int32_t count{};
    std::vector<std::pair<ItemTraits, std::int32_t>> instances;
    bool operator==(const NativeForm&) const = default;
};
// Index into this sample's copied live instances, never an engine pointer or a
// persistent instance ID. The adapter proves pointer membership before reading.
struct RowExtra {
    std::size_t liveIndex{};
    ItemTraits traits;
    std::int32_t count{};
};
enum class RowMatch { Permit, Drift, Coverage, MissingExtra };
inline const char* Reason(RowMatch result)
{
    switch (result) {
    case RowMatch::Permit: return "allow";
    case RowMatch::Drift: return "row-drift";
    case RowMatch::Coverage: return "row-coverage";
    case RowMatch::MissingExtra: return "row-extra-missing";
    }
    return "row-coverage";
}
inline RowMatch MatchRow(const Entry& row, std::span<const RowExtra> represented,
    const ItemTraits& plain, const NativeForm& live, const NativeForm* previous = nullptr)
{
    // Membership precedes trait/count inspection, including for malformed input.
    for (const auto& extra : represented) {
        if (extra.liveIndex >= live.instances.size()) return RowMatch::MissingExtra;
    }
    if (previous && *previous != live) return RowMatch::Drift;
    const auto& identity = row.identity.traits;
    if (row.ambiguous || !ValidTraits(identity) || !ValidTraits(plain) ||
        identity.base != live.base || plain.base != live.base || live.count <= 0 || row.count == 0)
        return RowMatch::Coverage;
    const auto total = static_cast<std::uint64_t>(live.count);
    std::uint64_t extraCount = 0, matchingCount = 0, representedCount = 0;
    for (const auto& [traits, count] : live.instances) {
        if (!ValidTraits(traits) || traits.base != live.base || count <= 0) return RowMatch::Coverage;
        extraCount += static_cast<std::uint64_t>(count);
        if (extraCount > total) return RowMatch::Coverage;
        if (traits == identity) matchingCount += static_cast<std::uint64_t>(count);
    }
    std::vector<bool> included(live.instances.size(), false);
    for (const auto& extra : represented) {
        const auto& current = live.instances[extra.liveIndex];
        if (included[extra.liveIndex] || extra.traits != current.first || extra.count != current.second ||
            extra.traits != identity) return RowMatch::Coverage;
        included[extra.liveIndex] = true;
        representedCount += static_cast<std::uint64_t>(extra.count);
        if (representedCount > row.count) return RowMatch::Coverage;
    }
    const auto plainCount = total - extraCount;
    const auto matchingPlain = plain == identity ? plainCount : 0;
    matchingCount += matchingPlain;
    if (row.count > matchingCount || row.count > representedCount + matchingPlain) return RowMatch::Coverage;
    // A fully represented row cannot select another instance. Any remainder
    // relies on plain count: differing, unrepresented extras make that unsafe.
    if (row.count > representedCount) {
        for (std::size_t index = 0; index < live.instances.size(); ++index) {
            if (!included[index] && live.instances[index].first != identity) return RowMatch::Coverage;
        }
    }
    return RowMatch::Permit;
}
}
