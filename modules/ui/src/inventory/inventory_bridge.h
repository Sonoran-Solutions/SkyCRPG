// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <SKSE/SKSE.h>
namespace skycrpg::ui::inventory {
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root);
void OnSkseMessage(SKSE::MessagingInterface::Message* message);
}
