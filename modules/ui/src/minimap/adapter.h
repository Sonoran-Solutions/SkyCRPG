#pragma once
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <SKSE/SKSE.h>

namespace skycrpg::ui::minimap
{
void Initialize(bool enabled, bool (*menuBlocked)());
void OnMessage(SKSE::MessagingInterface::Message* message);
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root);
}
