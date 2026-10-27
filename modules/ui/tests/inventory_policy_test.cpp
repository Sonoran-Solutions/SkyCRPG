// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "inventory/action_intent.h"
#include "inventory/row_match.h"
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
void RowMatching() {
    auto row = Plain();
    const auto plain = row.identity.traits;
    NativeForm original{plain.base, 1, {}};
    auto live = original; live.count = 2;
    assert(MatchRow(row, {}, plain, live, &original) == RowMatch::Drift);
    row.count = 2; // Even a current-looking row must refuse selected-form drift.
    assert(MatchRow(row, {}, plain, live, &original) == RowMatch::Drift);
    assert(MatchRow(row, {}, plain, live, &live) == RowMatch::Permit);
    assert(MatchRow(row, {}, plain, live) == RowMatch::Permit); // new menu session
    auto decreased = live; decreased.count = 1;
    assert(MatchRow(row, {}, plain, decreased, &live) == RowMatch::Drift);
    assert(MatchRow(row, {}, plain, decreased) == RowMatch::Coverage);

    // Whole-inventory drift in another form is not selected-form drift.
    auto unrelated = original; unrelated.base = {0x123, 0x123, "Skyrim.esm"};
    auto unrelatedChanged = unrelated; ++unrelatedChanged.count;
    const std::vector<NativeForm> before{live, unrelated}, after{live, unrelatedChanged};
    assert(before != after);
    assert(MatchRow(row, {}, plain, after.front(), &before.front()) == RowMatch::Permit);

    std::vector<ItemTraits> variants;
    auto temper = plain; temper.temper = 1.2F; variants.push_back(temper);
    auto enchant = plain; enchant.enchantment = {0xAA, 0xAA, "Skyrim.esm"}; variants.push_back(enchant);
    auto stolen = plain; stolen.stolen = true; variants.push_back(stolen);
    auto owner = plain; owner.owner = {0x123, 0x123, "Skyrim.esm"}; variants.push_back(owner);
    auto named = plain; named.name = "Custom sword"; variants.push_back(named);
    auto poison = plain; poison.poison = {0xAB, 0xAB, "Skyrim.esm"}; variants.push_back(poison);
    auto poisonCount = plain; poisonCount.poisonCount = 1; variants.push_back(poisonCount);
    auto charge = plain; charge.charge = 10.0F; variants.push_back(charge);
    auto worn = plain; worn.worn = true; variants.push_back(worn);
    auto left = plain; left.wornLeft = true; variants.push_back(left);
    auto favorite = plain; favorite.favorite = true; variants.push_back(favorite);
    auto quest = plain; quest.quest = true; variants.push_back(quest);
    for (const auto& variant : variants) {
        row = Plain();
        live = {plain.base, 2, {{variant, 1}}};
        assert(MatchRow(row, {}, plain, live, &original) == RowMatch::Drift);
        // Empty extraLists cannot hide a differing instance even if enough
        // plain count remains, with or without a prior session observation.
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
        assert(MatchRow(row, {}, plain, live, &live) == RowMatch::Coverage);
        row.count = 2; // Total base count is enough; matching count is not.
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
        // A partial list and sufficient matching count still leave the native
        // dispatch ambiguous about its unrepresented remainder.
        live = {plain.base, 3, {{plain, 1}, {variant, 1}}};
        std::vector<RowExtra> represented{{0, plain, 1}};
        assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage);
        row.count = 1; // Fully represented, other same-base row is separate.
        assert(MatchRow(row, represented, plain, live, &live) == RowMatch::Permit);
        row.identity.traits = variant;
        represented = {{1, variant, 1}};
        assert(MatchRow(row, represented, plain, live) == RowMatch::Permit);
        // Reconciled distinct row permits and reaches action validation.
        InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true);
        snapshots.Publish({row});
        const auto selected = Select(snapshots.Get());
        assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
        live.instances[1].first = plain; // no-event trait change, same total
        const NativeForm prior{plain.base, 3, {{plain, 1}, {variant, 1}}};
        assert(MatchRow(row, represented, plain, live, &prior) == RowMatch::Drift);
        // The adapter invalidates on row refusal; a retained token cannot pass
        // Validate or a quantity confirmation while refresh is pending.
        ActionIntents intents;
        assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true, true, true) == Refusal::None);
        snapshots.Invalidate();
        assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Unavailable);
        assert(intents.Confirm(snapshots.Get(), true, 1) == Refusal::Unavailable);
        snapshots.Publish({row});
        assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Stale);
        assert(Validate(Select(snapshots.Get(), 2), snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
    }

    row = Plain(0, 3);
    live = {plain.base, 3, {{plain, 1}, {plain, 1}}};
    std::vector<RowExtra> represented{{0, plain, 1}, {1, plain, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // extras plus plain
    row.count = 2; live.count = 2;
    assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // equal-trait extras
    assert(MatchRow(row, std::span<const RowExtra>(represented).first(1), plain, live) == RowMatch::Coverage);
    represented[1].liveIndex = 0;
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // no double counting
    represented = {{0, plain, 2}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // count mismatch
    represented = {{0, temper, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // identity mismatch
    represented = {{2, plain, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::MissingExtra);
    represented[0].traits.temper = std::numeric_limits<float>::quiet_NaN();
    live.count = 0;
    assert(MatchRow(row, represented, plain, live, &original) == RowMatch::MissingExtra); // membership first
    row = Plain();
    for (const auto count : {0, -1, 2}) {
        live = {plain.base, 1, {{plain, count}}};
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage); // invalid extra accounting
    }
    live = {plain.base, 1, {{temper, 1}}};
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage); // no matching count
    live = {plain.base, 1, {{plain, 1}}}; row.count = 0;
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
    row = Plain(); row.ambiguous = true;
    assert(MatchRow(row, {}, plain, original) == RowMatch::Coverage);
    row = Plain(); row.identity.traits.base = unrelated.base;
    assert(MatchRow(row, {}, plain, original) == RowMatch::Coverage);
    row = Plain(0, 2); live = {plain.base, 2, {{plain, 1}, {temper, 1}}};
    represented = {{0, plain, 1}, {1, temper, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // mixed represented traits
    row = Plain(); live = {plain.base, 1, {{plain, 1}}};
    live.instances.front().first.charge = std::numeric_limits<float>::infinity();
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
    assert(std::string(Reason(RowMatch::Drift)) == "row-drift");
    assert(std::string(Reason(RowMatch::Coverage)) == "row-coverage");
    std::cout << "PASS: selected-form drift, row membership, trait/count coverage and reconciled authorization\n";
}
int main() {
    RowMatching();
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
