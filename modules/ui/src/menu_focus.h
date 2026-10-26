// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <string_view>

namespace skycrpg::ui {
// Game UI supplies the flags; a missing menu is unknown and fails closed.
constexpr bool MenuBlocks(std::string_view name, bool available, bool focusFlags) {
    if (name == "HUD Menu") return false;
    if (name == "FavoritesMenu" || name == "Dialogue Menu" || name == "Console" || name == "Loading Menu") return true;
    return !available || focusFlags;
}
constexpr bool MenuFocusBlocked(bool uiAvailable, bool uiFocus, bool namedFocus, bool stackFocus) {
    return !uiAvailable || uiFocus || namedFocus || stackFocus;
}
}
