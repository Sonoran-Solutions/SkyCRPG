// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 base rig: portable checks of the player fade hold (fixed_spike_fade_policy.h).
#include "fixed_spike_policy.h"
#include "fixed_spike_fade_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
using namespace skycrpg::camera::fixed;

int main() {
    // The audited facts.
    assert(fadeSettingId == 382599 && fadeSettingRva == 0x20ba460 && settingValue == 8 && settingName == 0x10 && playerFadeHandle == 0x130);
    assert(std::string(fadeSettingName) == "fCharControllerCheckHeightOffset:Camera" && fadeHoldValue == -5000.0f);
    assert(Config{}.holdFade); // on by default with the rig
    // The check at three image bases.
    for (const std::uintptr_t base : {std::uintptr_t{0x140000000},std::uintptr_t{0x7ff600000000},std::uintptr_t{0x10000}}) {
        assert(fadeSettingCheck(base,base+fadeSettingRva,"fCharControllerCheckHeightOffset:Camera") == 0);
        assert(fadeSettingCheck(base,base+fadeSettingRva+0x18,"fCharControllerCheckHeightOffset:Camera") == 1);
        assert(fadeSettingCheck(base,base+fadeSettingRva,nullptr) == 2);
        assert(fadeSettingCheck(base,base+fadeSettingRva,"fCameraCasterSize:Camera") == 3);
        assert(fadeSettingCheck(base,base+fadeSettingRva,"fCharControllerCheckHeightOffset:Camerax") == 3);
        assert(fadeSettingCheck(base,base+fadeSettingRva,"fCharControllerCheckHeightOffset") == 3);
    }
    // Every input of one frame's step against the table.
    for (unsigned bits = 0;bits < 16;++bits) {
        const bool installed = bits & 1u,eligible = bits & 2u,held = bits & 4u,ours = bits & 8u;
        const float current = ours ? fadeHoldValue : 124.0f;
        const FadeStep step = fadeStep(installed,eligible,held,current);
        FadeStep expected = FadeStep::none;
        if (installed && eligible) expected = !held ? FadeStep::hold : ours ? FadeStep::keep : FadeStep::rehold;
        else if (installed && held) expected = ours ? FadeStep::release : FadeStep::leave;
        assert(step == expected);
        // Never written when not installed; never released while the rig holds; never written back unless still ours.
        if (!installed) assert(step == FadeStep::none);
        if (eligible) assert(step != FadeStep::release && step != FadeStep::leave);
        if (step == FadeStep::release) assert(held && ours && !eligible);
    }
    // A run: engage, hold, a console setini under the hold, a menu, engage again, a load while held, someone's write.
    float value = 124.0f,saved = 0.0f;
    bool held = false;
    const auto frame = [&](bool eligible) {
        const FadeStep step = fadeStep(true,eligible,held,value);
        if (step == FadeStep::hold || step == FadeStep::rehold) { saved = value; value = fadeHoldValue; held = true; }
        else if (step == FadeStep::release) { value = saved; held = false; }
        else if (step == FadeStep::leave) held = false;
        return step;
    };
    assert(frame(true) == FadeStep::hold && value == fadeHoldValue && saved == 124.0f && frame(true) == FadeStep::keep);
    value = 100.0f; // the user's setini while held
    assert(frame(true) == FadeStep::rehold && saved == 100.0f && value == fadeHoldValue);
    assert(frame(false) == FadeStep::release && value == 100.0f && !held && frame(false) == FadeStep::none && value == 100.0f);
    assert(frame(true) == FadeStep::hold && frame(false) == FadeStep::release && value == 100.0f);  // a load or a menu: back at once
    assert(frame(true) == FadeStep::hold);
    value = 50.0f; // someone wrote it, then the rig let go: theirs stays
    assert(frame(false) == FadeStep::leave && value == 50.0f && !held);
    // FixedFade: the row, its nan forms, its longest length.
    char line[768]{};
    FadeSample s; s.seconds = 1234.5; s.frame = 99; s.state = FadeFeature::installed; s.held = true; s.current = fadeHoldValue; s.saved = 124.0f;
    s.window.frames = 7; s.window.steps[static_cast<std::size_t>(FadeStep::keep)] = 7; s.window.playerFaded = 1; s.window.heldPlayerFaded = 1;
    assert(formatFade(line,sizeof(line),s));
    assert(std::string(line) == "FixedFade t=1234.500 frame=99 state=installed held=1 holdValue=-5000.0 current=-5000.0 saved=124.0 frames=7 "
        "holds=0 keeps=7 reholds=0 releases=0 leaves=0 playerFaded=1 heldPlayerFaded=1 sample=window");
    FadeSample idle; idle.state = FadeFeature::refused; idle.current = idle.saved = std::numeric_limits<float>::quiet_NaN();
    assert(formatFade(line,sizeof(line),idle) && std::string(line).find("state=refused held=0 holdValue=-5000.0 current=nan saved=nan") != std::string::npos);
    FadeSample big; big.seconds = 1e7; big.frame = ~0ull; big.current = big.saved = -1e30f;
    for (auto& v : big.window.steps) v = ~0ull;
    big.window.frames = big.window.playerFaded = big.window.heldPlayerFaded = ~0ull;
    assert(formatFade(line,sizeof(line),big) && std::strlen(line) < sizeof(TraceRow));
    std::printf("player fade hold PASS (longest FixedFade row %zu of %zu bytes)\n",std::strlen(line),sizeof(TraceRow));
}
