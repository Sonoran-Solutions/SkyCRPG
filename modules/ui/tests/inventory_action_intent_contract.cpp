// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// History-file adapter for the AS harness: replay edges and dispatches through
// the actual portable policy rather than duplicating its rules in JavaScript.
#include "inventory/action_intent.h"
#include <fstream>
#include <iostream>
#include <string>
using namespace skycrpg::ui::inventory;

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    std::ifstream input(argv[1]);
    if (!input) return 1;
    InventorySnapshots snapshots; snapshots.World(true); snapshots.Menu(true);
    ItemTraits traits;
    traits.base = {0x00012EB7, 0x12EB7, "Skyrim.esm"};
    traits.name = "Stack";
    snapshots.Publish({Entry{{0, traits}, 8, false, true}});
    ActionIntents intents;
    std::string command;
    while (input >> command) {
        if (command == "edge") {
            unsigned key{}; bool down{};
            input >> key >> down;
            intents.Edge(key, down);
        } else if (command == "snapshot") {
            intents.Observe();
        } else if (command == "cancel" || command == "view") {
            intents.Cancel();
        } else if (command == "begin") {
            std::uint64_t token{}; bool drop{}, focused{}, quantity{};
            std::uint32_t amount{};
            input >> token >> drop >> focused >> amount >> quantity;
            const Selection selected{snapshots.Get().stamp, snapshots.Get().entries.front(), token};
            std::cout << Reason(intents.Begin(selected, snapshots.Get(), focused,
                drop ? Action::Drop : Action::Activate, amount, quantity)) << '\n';
        } else if (command == "confirm") {
            bool focused{}; std::uint32_t amount{};
            input >> focused >> amount;
            std::cout << Reason(intents.Confirm(snapshots.Get(), focused, amount)) << '\n';
        } else {
            return 2;
        }
        if (!input) return 3;
    }
}
