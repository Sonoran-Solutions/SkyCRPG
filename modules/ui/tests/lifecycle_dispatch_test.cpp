// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/lifecycle/dispatch_epoch.h"
#include "../src/hud_settings.h"
#include "../src/menu_focus.h"
#include <cassert>

int main()
{
    skycrpg::ui::lifecycle::DispatchEpoch epoch;
    // Open, close, nested menu, pre-load, post-load and new game each retire
    // a queued press, even if the menu is closed again before dispatch.
    for (int transition = 0; transition < 6; ++transition) {
        const auto press = epoch.Capture();
        assert(epoch.Current(press));
        epoch.Invalidate();
        assert(!epoch.Current(press));
    }
    assert(skycrpg::ui::MenuBlocks("Main Menu", true, false));
    const auto missing = skycrpg::ui::ParseHudSettings(L"", L"", L"", L"", L"", L"");
    assert(!missing.supportedVersion && missing.settings.enabled && !missing.settings.replaceCompass);
    const auto invalid = skycrpg::ui::ParseHudSettings(L"1", L"no", L"-500", L"garbage", L"500", L"-1");
    assert(invalid.settings.enabled && !invalid.settings.replaceCompass);
    assert(invalid.settings.scalePercent == 85 && invalid.scaleWasClamped);
    assert(invalid.settings.offsetX == 0 && !invalid.offsetXWasValid);
    assert(invalid.settings.offsetY == 60 && invalid.offsetYWasClamped);
}
