// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <SKSE/SKSE.h>
#include <string>
namespace skycrpg::ui::inventory::preview {
// Reads [Inventory] from the plugin's own INI (read-only) and logs the result.
void Configure(const std::wstring& iniPath);
// Adds placeItemPreview(centre, half) to the SkyCRPGUI Scaleform object.
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root);
// Registers the menu sink on kDataLoaded; ends the session on load/new game.
void OnSkseMessage(SKSE::MessagingInterface::Message* message);
}
