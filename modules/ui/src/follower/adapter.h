#pragma once
#include <SKSE/SKSE.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
namespace skycrpg::ui::follower {
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root);
void OnMessage(SKSE::MessagingInterface::Message* message);
}
