// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "inventory/action_intent.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace skycrpg::ui::inventory;

Entry Plain(std::uint32_t index = 0, std::uint32_t count = 1) {
    ItemTraits traits;
    traits.base = {0x00012EB7, 0x12EB7, "Skyrim.esm"}; traits.name = "Iron Sword";
    return {{index, traits}, count, false, true};
}
Selection Select(const Snapshot& snapshot, std::uint64_t token = 1) {
    return {snapshot.stamp, snapshot.entries.front(), token};
}
int main() {
    const auto plain = Plain(0, 3);
    std::vector<Entry> fixtures{plain};
    auto enchanted = Plain(1); enchanted.identity.traits.enchantment = {0xAA, 0xAA, "Skyrim.esm"}; fixtures.push_back(enchanted);
    auto temper1 = Plain(2); temper1.identity.traits.temper = 1.1F; fixtures.push_back(temper1);
    auto temper2 = Plain(3); temper2.identity.traits.temper = 1.2F; fixtures.push_back(temper2);
    auto stolen = Plain(4); stolen.identity.traits.stolen = true; stolen.identity.traits.owner = {0x123, 0x123, "Skyrim.esm"}; fixtures.push_back(stolen);
    auto renamed = Plain(5); renamed.identity.traits.name = "Named sword"; fixtures.push_back(renamed);
    fixtures.push_back(Plain(6)); // even equal traits in separate native rows differ
    auto mod = Plain(7); mod.identity.traits.base = {0x05001234, 0x1234, "Fixture.esp"}; fixtures.push_back(mod);
    auto esl = Plain(8); esl.identity.traits.base = {0xFE012ABC, 0xABC, "Fixture.esl"}; fixtures.push_back(esl);
    for (std::size_t i = 0; i < fixtures.size(); ++i) for (std::size_t j = i + 1; j < fixtures.size(); ++j)
        assert(fixtures[i].identity != fixtures[j].identity);
    assert(fixtures.front().count == 3); // native duplicate stack is one entry
    auto nonfinite = plain.identity.traits; nonfinite.temper = std::numeric_limits<float>::quiet_NaN();
    assert(!ValidTraits(nonfinite));

    InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true); snapshots.Publish({plain});
    const auto original = Select(snapshots.Get());
    assert(Validate(original, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
    assert(Validate(original, snapshots.Get(), false, Action::Activate, 1) == Refusal::Focus);
    assert(Validate(original, snapshots.Get(), true, Action::Drop, 0) == Refusal::Quantity);
    assert(Validate(original, snapshots.Get(), true, Action::Drop, 4) == Refusal::Quantity);
    assert(Validate(original, snapshots.Get(), true, Action::Activate, 1, false) == Refusal::Restricted);
    for (const auto& changed : fixtures) {
        if (changed == plain) continue;
        snapshots.Publish({changed});
        assert(Validate(original, snapshots.Get(), true, Action::Activate, 1) == Refusal::Stale);
    }
    // Native consumption/additions, favorite and equip changes bump generation.
    for (const auto count : {2U, 7U}) {
        snapshots.Publish({Plain(0, count)});
        assert(Validate(original, snapshots.Get(), true, Action::Drop, 1) == Refusal::Stale);
    }
    auto favorite = plain; favorite.identity.traits.favorite = true;
    auto worn = plain; worn.identity.traits.wornLeft = true;
    for (const auto& changed : {favorite, worn}) {
        snapshots.Publish({changed});
        assert(Validate(original, snapshots.Get(), true, Action::Activate, 1) == Refusal::Stale);
    }
    snapshots.Publish({plain});
    auto selected = Select(snapshots.Get(), 2);
    auto live = snapshots.Get(); live.entries.front().ambiguous = true;
    assert(Validate(selected, live, true, Action::Activate, 1) == Refusal::Ambiguous);
    live = snapshots.Get(); live.entries.clear();
    assert(Validate(selected, live, true, Action::Activate, 1) == Refusal::Missing);
    live = snapshots.Get(); live.entries.front().identity.traits.name = "Replaced";
    assert(Validate(selected, live, true, Action::Activate, 1) == Refusal::Identity);
    auto quest = plain; quest.identity.traits.quest = true;
    snapshots.Publish({quest}); selected = Select(snapshots.Get(), 3);
    assert(Validate(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Restricted);
    // Native quest feedback is allowed only by an explicit adapter choice; the
    // native ItemDrop path, not this policy, is responsible for its refusal.
    assert(Validate(selected, snapshots.Get(), true, Action::Drop, 1, true, true) == Refusal::None);

    snapshots.Publish({plain}); selected = Select(snapshots.Get(), 4);
    ActionIntents intents;
    intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::None);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::Duplicate);
    assert(intents.Confirm(snapshots.Get(), true, 2) == Refusal::None);
    assert(intents.Confirm(snapshots.Get(), true, 2) == Refusal::Pending);
    intents.Observe();
    // Two devices held, key repeats, down+up duplication and a refreshed token
    // cannot synthesize another activation within the same gesture.
    intents.Edge(2, true); intents.Edge(1, false); intents.Edge(2, true);
    selected.token = 5;
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Edge(2, false); // release is never an activation
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
    selected.token = 6; intents.Edge(1, false); intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Pending);
    intents.Observe();
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::None);
    intents.Cancel();
    assert(intents.Confirm(snapshots.Get(), true, 1) == Refusal::Pending);
    selected.token = 7; intents.Edge(1, false); intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::None);
    snapshots.Invalidate(); snapshots.Publish({Plain(0, 2)});
    assert(intents.Confirm(snapshots.Get(), true, 2) == Refusal::Stale);
    assert(!intents.Pending());

    // Each lifecycle transition destroys authority, even if native values match.
    selected = Select(snapshots.Get(), 8);
    snapshots.View(); // sort/filter/category/search
    assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Stale);
    selected = Select(snapshots.Get(), 9);
    snapshots.Menu(false); intents.Invalidate();
    assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Unavailable);
    snapshots.Menu(true); snapshots.Publish({plain});
    assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Stale);
    for (int transition = 0; transition < 3; ++transition) { // load, new game, death
        selected = Select(snapshots.Get(), static_cast<std::uint64_t>(10 + transition));
        snapshots.World(false); intents.Invalidate();
        assert(Validate(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Unavailable);
        snapshots.World(true); snapshots.Menu(true); snapshots.Publish({plain});
        assert(Validate(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Stale);
    }
    std::cout << "PASS: inventory traits, native stacks, generations, restrictions, quantity lifecycle and duplicate gestures\n";
}
