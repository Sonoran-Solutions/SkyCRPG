// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", third
// pass: in an interior flagged Show Sky the engine draws the sky dome at
// depth 1.0, so over every void pixel, on top of the clear colour
// (include/fixed_spike_shader_sky_policy.h has the audit). While the cut is
// active the sky's root carries the hidden bit, exactly as Sky::SetMode
// leaves it in an interior without Show Sky; on the first main-thread frame
// that is not active the bit is put back if the engine's rule still says the
// root is shown. Installed only by the shader cut's commit (mode 3, after
// every cut guard: ENB and Community Shaders refuse it with the cut, FCS-67)
// and only with bVoidHideSky=1.
//
// The one engine write: bit 0 of the sky root's NiAVObject flags (+F4), on
// the main thread, inside the camera frame service (before the frame's
// render). No code is written and nothing is allocated.

namespace skycrpg::camera::fixed::shader {
// Main thread, from commit, after installVoid.
inline void installSky(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.config.hideSky) { r.skyState = Feature::off; return; }
    const std::uintptr_t setMode = REL::ID(skyModeAddress.id).address();
    if (const unsigned failed = skySiteCheck(base,REL::ID(skyAddress.id).address(),setMode,reinterpret_cast<const std::uint8_t*>(setMode),
            reinterpret_cast<const std::uint8_t*>(base+skyRuleRva))) {
        r.skyState = Feature::refused;
        char text[256]{};
        std::snprintf(text,sizeof(text),"check=%u: the Sky singleton or Sky::SetMode (ID 401652, ID 26222) differ (another module or another "
            "build); the sky over the void is left to the engine, the cut and the void colour are unchanged",failed);
        refuse(Refusal::skySite,text); return;
    }
    r.skyState = Feature::installed;
}
// Main thread only: publish (want = this frame is active), deactivate on the
// main thread and frameStart after a latch (want = false).
inline void settleSky(bool want) {
    auto& r = *runtime;
    if (r.skyState != Feature::installed) return;
    const auto object = peek<const std::uint8_t*>(reinterpret_cast<const void*>(r.base+skyAddress.rva),0);
    const auto root = object ? peek<std::uint8_t*>(object,sky::root) : nullptr;
    const std::uint32_t mode = object ? peek<std::uint32_t>(object,sky::mode) : 0u,flags = object ? peek<std::uint32_t>(object,sky::flags) : 0u;
    const std::uint32_t objectFlags = root ? peek<std::uint32_t>(root,sky::objectFlags) : 0u;
    const bool engineShows = object && engineShowsSky(mode,flags);
    r.skyMode = object ? static_cast<int>(mode) : -1;
    if (want && !root) ++r.voidWindow.noSky;
    else if (want && !engineShows) ++r.voidWindow.engineHidden;
    const SkyStep step = skyStep(want,r.skyRoot != nullptr,root != nullptr,root == r.skyRoot,engineShows,(objectFlags & sky::hidden) != 0u);
    ++r.voidWindow.steps[static_cast<std::size_t>(step)];
    if (step == SkyStep::hide) {
        const std::uint32_t value = objectFlags | sky::hidden;
        std::memcpy(root+sky::objectFlags,&value,sizeof(value));
        r.skyRoot = root;
    } else if (step == SkyStep::restore) {
        const std::uint32_t value = objectFlags & ~sky::hidden;
        std::memcpy(root+sky::objectFlags,&value,sizeof(value));
        r.skyRoot = nullptr;
    } else if (step == SkyStep::leave) {
        r.skyRoot = nullptr;
        refuse(Refusal::skyLeft,"the sky's root, mode or HideSky flag changed while the void held the root hidden; its hidden bit is left as the "
            "engine set it (informative)");
    }
}
} // namespace skycrpg::camera::fixed::shader
