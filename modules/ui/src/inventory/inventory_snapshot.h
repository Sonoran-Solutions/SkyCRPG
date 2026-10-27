// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "item_identity.h"
#include <algorithm>
#include <vector>

namespace skycrpg::ui::inventory {
struct Entry {
    ItemIdentity identity;
    std::uint32_t count{};
    bool ambiguous{}, enabled{true};
    bool operator==(const Entry&) const = default;
};
struct Stamp {
    std::uint64_t world{}, session{}, inventory{}, view{};
    bool operator==(const Stamp&) const = default;
};
struct Snapshot {
    Stamp stamp;
    std::vector<Entry> entries;
    bool available{};
    const Entry* Find(std::uint32_t index) const {
        const auto it = std::find_if(entries.begin(), entries.end(), [index](const Entry& entry) {
            return entry.identity.nativeIndex == index;
        });
        return it == entries.end() ? nullptr : &*it;
    }
};
// Copies observations, never predicts mutations. Event invalidation bumps even
// when the next sample looks identical (e.g. remove/re-add the same base item).
class InventorySnapshots {
public:
    void World(bool ready) { ++_current.stamp.world; Menu(false); _ready = ready; }
    void Menu(bool open) {
        ++_current.stamp.session; ++_current.stamp.view;
        _open = open; Invalidate();
    }
    void View() { ++_current.stamp.view; }
    void Invalidate() { ++_current.stamp.inventory; _current.available = false; _current.entries.clear(); }
    void Publish(std::vector<Entry> entries) {
        if (!_ready || !_open) return;
        if (_current.entries != entries) ++_current.stamp.inventory;
        _current.entries = std::move(entries); _current.available = true;
    }
    const Snapshot& Get() const { return _current; }
private:
    Snapshot _current;
    bool _ready{}, _open{};
};
}
