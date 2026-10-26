// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/menu_focus.h"
#include <cassert>

int main() {
    using namespace skycrpg::ui;
    assert(!MenuBlocks("HUD Menu", true, false));
    assert(!MenuBlocks("HUD Menu", false, true));
    assert(MenuBlocks("FavoritesMenu", true, false));
    assert(MenuBlocks("Dialogue Menu", true, false));
    assert(MenuBlocks("Console", true, false));
    assert(MenuBlocks("Loading Menu", true, false));
    assert(MenuBlocks("Unknown", false, false));
    assert(MenuBlocks("InventoryMenu", true, true));
    assert(!MenuBlocks("Passive overlay", true, false));
    assert(MenuFocusBlocked(false, false, false, false));
    assert(!MenuFocusBlocked(true, false, false, false));
    assert(MenuFocusBlocked(true, true, false, false));
    assert(MenuFocusBlocked(true, false, true, false));
    assert(MenuFocusBlocked(true, false, false, true));
    // Closing one menu cannot uncover the hotbar while another still has focus.
    assert(MenuFocusBlocked(true, false, false, true));
    assert(!MenuFocusBlocked(true, false, false, false));
}
