// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "inventory/action_intent.h"
#include "inventory/row_match.h"
#include "inventory/list_reconciliation.h"
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
    row.count = 2; // Even exact coverage cannot authorize unevented drift.
    assert(MatchRow(row, {}, plain, live, &original) == RowMatch::Drift);
    assert(MatchRow(row, {}, plain, live, &live) == RowMatch::Permit);
    assert(MatchRow(row, {}, plain, live) == RowMatch::Permit); // event reset / new session
    row.count = 1;
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage); // no partial plain holder
    auto decreased = live; decreased.count = 1;
    assert(MatchRow(row, {}, plain, decreased, &live) == RowMatch::Drift);
    assert(MatchRow(row, {}, plain, decreased) == RowMatch::Permit); // event reset

    // Whole-inventory drift in another form is not selected-form drift.
    auto unrelated = original; unrelated.base = {0x123, 0x123, "Skyrim.esm"};
    auto unrelatedChanged = unrelated; ++unrelatedChanged.count;
    const std::vector<NativeForm> before{original, unrelated}, after{original, unrelatedChanged};
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
        live = {plain.base, 4, {{variant, 1}}};
        row = Plain(0, 3);
        // Native plain row stays usable beside a separate differing extra row.
        assert(MatchRow(row, {}, plain, live, &live) == RowMatch::Permit);
        row.count = 2;
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
        live.count = 2; // stale plain row of 2 versus plain 1 plus tempered/etc 1
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
        row.count = 1;
        assert(MatchRow(row, {}, plain, live, &original) == RowMatch::Drift);
        assert(MatchRow(row, {}, plain, live) == RowMatch::Permit);
        row.identity.traits = variant;
        std::vector<RowExtra> represented{{0, variant, 1}};
        assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // enchanted/etc row
        row.count = 2;
        assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // native merged holder
        live.count = 3; row.count = 3;
        assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // stolen 1 + owned 2
        row.count = 2;
        assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // partial plain remainder
        row.count = 1;

        InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true);
        snapshots.Publish({row});
        const auto selected = Select(snapshots.Get());
        assert(Validate(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
        const auto prior = live;
        live.instances[0].first = plain; // no-event trait change, same total
        assert(MatchRow(row, represented, plain, live, &prior) == RowMatch::Drift);
        assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // stale copied traits
        represented[0].traits = plain; row.identity.traits = plain;
        assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // event reconciliation
        // A retained token cannot pass validation or quantity confirmation while dirty.
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
    row.count = 2;
    assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // extras only, plain elsewhere
    live.count = 2;
    assert(MatchRow(row, represented, plain, live) == RowMatch::Permit);
    assert(MatchRow(row, std::span<const RowExtra>(represented).first(1), plain, live) == RowMatch::Coverage);
    represented[1].liveIndex = 0;
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // duplicate
    represented = {{0, plain, 2}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // represented count mismatch
    represented = {{0, temper, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Coverage); // copied traits mismatch
    represented = {{2, plain, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::MissingExtra);
    represented[0].traits.temper = std::numeric_limits<float>::quiet_NaN();
    live.count = 0;
    assert(MatchRow(row, represented, plain, live, &original) == RowMatch::MissingExtra); // membership first
    row = Plain();
    for (const auto count : {0, -1, 2}) {
        live = {plain.base, 1, {{plain, count}}};
        assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
    }
    live = {plain.base, 1, {{temper, 1}}};
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage); // no plain count
    row.count = 0;
    assert(MatchRow(row, {}, plain, original) == RowMatch::Coverage);
    row = Plain(); row.identity.traits.base = unrelated.base;
    assert(MatchRow(row, {}, plain, original) == RowMatch::Coverage);
    row = Plain(0, 2); row.identity.traits = stolen; row.ambiguous = true;
    auto otherOwner = stolen; otherOwner.owner = owner.owner;
    live = {plain.base, 2, {{stolen, 1}, {otherOwner, 1}}};
    represented = {{0, stolen, 1}, {1, otherOwner, 1}};
    assert(MatchRow(row, represented, plain, live) == RowMatch::Permit); // native mixed extras
    InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true); snapshots.Publish({row});
    const auto selected = Select(snapshots.Get());
    for (const auto action : {Action::Activate, Action::Drop, Action::Favorite})
        assert(Validate(selected, snapshots.Get(), true, action, 1) == Refusal::None);
    row = Plain(); live = {plain.base, 1, {{plain, 1}}};
    live.instances.front().first.charge = std::numeric_limits<float>::infinity();
    assert(MatchRow(row, {}, plain, live) == RowMatch::Coverage);
    assert(std::string(Reason(RowMatch::Drift)) == "row-drift");
    assert(std::string(Reason(RowMatch::Coverage)) == "row-coverage");
    std::cout << "PASS: native split/merged rows, exact coverage, selected-form freshness and consistency\n";
}
void RefusedSelectionReconciliation() {
    ListReconciliation reconciliation;
    unsigned updates = 0;
    auto refuse = [&] { if (reconciliation.Request()) ++updates; };
    assert(!reconciliation.Pending());
    refuse(); // first refused selection queues native reconciliation
    assert(reconciliation.Pending() && updates == 1);
    for (int sample = 0; sample < 32; ++sample) refuse(); // highlights while queued
    assert(updates == 1);
    refuse(); // refused Validate while the same update is pending
    assert(updates == 1);
    // Simulate ItemList::Update calling selection/highlight synchronously. The
    // task must not complete the request until these callbacks have returned.
    for (int callback = 0; callback < 32; ++callback) refuse();
    assert(reconciliation.Pending() && updates == 1);
    reconciliation.Complete();
    assert(!reconciliation.Pending());
    refuse(); // a later refusal episode may request one new reconciliation
    assert(updates == 2);
    reconciliation.Complete();
    std::cout << "PASS: refused highlights/Validate/reentrant selection queue at most one list update\n";
}
void ReentrantListUpdate() {
    ListReconciliation reconciliation;
    InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true);
    snapshots.Publish({Plain(0, 3)});
    const auto selected = Select(snapshots.Get());
    const auto original = snapshots.Get();
    unsigned updates = 0, reads = 0, publications = 0;
    bool dirty = false;
    auto request = [&] { if (reconciliation.Request()) ++updates; };
    auto sample = [&] {
        if (reconciliation.Updating()) return false;
        ++reads; // Includes acquiring the native list, before the dirty check.
        if (dirty) { request(); return false; }
        snapshots.Publish({Plain(0, 3)});
        ++publications;
        return true;
    };
    request(); // Unrelated drift: selected row and snapshot are still valid.
    assert(sample()); // Pending alone must retain its existing semantics.
    reads = 0; publications = 0;
    auto update = [&] {
        const ListReconciliation::UpdateScope updating(reconciliation);
        dirty = true;
        // ItemList::Update synchronously re-enters the snapshot callback while
        // its items are incomplete. Refuse before reads, refresh or Publish.
        for (int callback = 0; callback < 32; ++callback) {
            assert(!sample());
            assert(reads == 0 && publications == 0);
            assert(updates == 1 && reconciliation.Pending());
            assert(snapshots.Get().entries == original.entries);
            assert(snapshots.Get().stamp == original.stamp);
            // Validate must refuse even if it retained a previously valid token.
            const bool allowed = !reconciliation.Updating() &&
                Validate(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::None;
            assert(!allowed);
        }
        {
            const ListReconciliation::UpdateScope nested(reconciliation);
            assert(reconciliation.Updating());
        }
        assert(reconciliation.Updating());
    };
    update();
    assert(!reconciliation.Updating() && reconciliation.Pending());
    dirty = false;
    reconciliation.Complete();
    assert(!reconciliation.Pending() && updates == 1);
    assert(sample() && reads == 1 && publications == 1);
    assert(Validate(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::None);
    // Scope cleanup must also work on unwinding, independently of Pending.
    try {
        const ListReconciliation::UpdateScope updating(reconciliation);
        throw 1;
    } catch (int) {}
    assert(!reconciliation.Updating() && !reconciliation.Pending());
    std::cout << "PASS: reentrant rebuild refuses reads/publication/Validate without requeue; next sample and unrelated row succeed\n";
}
void GestureOrdering() {
    InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true);
    snapshots.Publish({Plain(0, 8)});
    // Both engine callbacks and mouse ItemSelect use Begin. Test callback/down
    // order independently on successive non-modal actions.
    for (const auto action : {Action::Drop, Action::Activate}) {
        for (const bool callbackFirst : {false, true}) {
            ActionIntents intents;
            for (std::uint64_t token = 1; token <= 3; ++token) {
                const auto selected = Select(snapshots.Get(), token);
                if (!callbackFirst) intents.Edge(46, true);
                assert(intents.Begin(selected, snapshots.Get(), true, action, 1) == Refusal::None);
                intents.Observe();
                if (callbackFirst) intents.Edge(46, true);
                assert(intents.Begin(Select(snapshots.Get(), token + 10), snapshots.Get(), true, action, 1) == Refusal::Duplicate);
                intents.Edge(46, true); // held auto-repeat, including a new token
                assert(intents.Begin(Select(snapshots.Get(), token + 20), snapshots.Get(), true, action, 1) == Refusal::Duplicate);
                intents.Edge(99, false); // an unrelated/unmatched up cannot re-arm
                assert(intents.Begin(selected, snapshots.Get(), true, action, 1) == Refusal::Duplicate);
                intents.Edge(46, false);
                assert(!intents.Pending()); // release never retries/dispatches
            }
        }
    }
    ActionIntents intents;
    auto selected = Select(snapshots.Get());
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::None);
    intents.Observe(); intents.Edge(65535, true); intents.Edge(65535, false);
    // An old token remains spent even after release. Refusing it consumes the
    // new press as well: neither its late down nor a refresh can retry it.
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Edge(65535, true); selected.token = 2;
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Invalidate();
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Edge(65535, false);
    assert(intents.Begin(selected, snapshots.Get(), false, Action::Drop, 1) == Refusal::Focus);
    intents.Edge(46, true); selected.token = 3; intents.Observe();
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Duplicate);
    intents.Edge(46, false);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::None);
    intents.Edge(46, true); intents.Edge(46, false);
    intents.Edge(28, true);
    assert(intents.Confirm(snapshots.Get(), true, 2) == Refusal::None);
    intents.Observe();
    assert(intents.Confirm(snapshots.Get(), true, 2) == Refusal::Pending);
    selected.token = 4;
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Duplicate);
    intents.Edge(28, false);
    // Invalidation with nothing held must not manufacture a consumed gesture
    // that would refuse the first engine callback in a new menu context.
    intents.Invalidate();
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
    // Tab closes the menu: its down is seen, its up is not. The next session
    // must still re-arm on the first completed press.
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Duplicate);
    intents.Edge(15, true);
    intents.ResetSession();
    intents.Edge(18, true); intents.Edge(18, false);
    selected.token = 5;
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::None);
    // Without the session reset the lost Tab up would block every action.
    {
        ActionIntents stuck;
        assert(stuck.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
        stuck.Observe(); stuck.Edge(15, true); stuck.Invalidate();
        stuck.Edge(18, true); stuck.Edge(18, false); selected.token = 6;
        assert(stuck.Begin(selected, snapshots.Get(), true, Action::Drop, 1) == Refusal::Duplicate);
    }
    std::cout << "PASS: engine callback/down ordering, mouse gestures, repeats, no retry and one-shot quantity confirm\n";
}
int main() {
    ReentrantListUpdate();
    GestureOrdering();
    RowMatching();
    RefusedSelectionReconciliation();
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
    assert(Validate(selected, live, true, Action::Activate, 1) == Refusal::None);
    live.entries.front().identity.traits.temper = std::numeric_limits<float>::quiet_NaN();
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
    intents.Edge(2, false); // release re-arms but never dispatches an action
    intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::None);
    selected.token = 6; intents.Edge(1, false); intents.Edge(1, true);
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Activate, 1) == Refusal::Pending);
    intents.Observe();
    assert(intents.Begin(selected, snapshots.Get(), true, Action::Drop, 1, true) == Refusal::Duplicate);
    intents.Edge(1, false); intents.Edge(1, true);
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
