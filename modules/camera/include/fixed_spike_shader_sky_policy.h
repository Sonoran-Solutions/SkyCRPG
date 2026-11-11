// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike_shader_policy.h. P5.5-02 wave 2 package
// "shader", third pass: what still painted the void after the clear colour
// went black, and the Effect cut's hard wedge.
//
// The void is not only the clear colour. Interiors flagged Show Sky (CELL DATA
// 0x80; Embershard Mine 01, Bleak Falls Barrow 01 and the Bannered Mare all
// are, none has Use Sky Lighting 0x100) put the Sky in mode 2, "sky dome
// only". The mode is chosen for an interior at RVA 1A3F40 (called from
// 1A2F55) and for an exterior at 1A36AE..1A371E, by the same rule; 36E660
// (ID 22824), which an earlier text named here, is the ToggleSky console
// command's handler (correction from the binary review of 8f1248f). In that mode Sky::SetMode (ID 26222) shows the
// sky's root node (411794..4117C7), and the sky colours come from the
// exterior weather (413384..413491) while only sunlight, ambient and the two
// fog colours are taken from the cell (413828..41387D). The dome's vertex
// shader (BSSkyShader technique Sky, shaders011.fxp entry 68) writes
// position .xyzz, so it lies at depth 1.0 and covers exactly the pixels no
// geometry covers: the void. What the player sees there is the dome over the
// clear colour. In a mode 1 interior (no Show Sky) SetMode hides the same
// root, so the engine itself draws no sky there.
//
// The fix does what SetMode does for a mode 1 interior, for the main view's
// frames while the cut is active: the root's hidden bit (NiAVObject flags +F4
// bit 0) is set, and put back on the first main-thread frame that is not
// active, but only when the engine's own rule (mode 2 or 3 and no HideSky
// flag) still says the root is shown; otherwise the engine has decided again
// (a cell change runs SetMode) and the bit is left alone.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace skycrpg::camera::fixed::shader {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
// The Sky singleton pointer. ID 13878 (RVA 1C7D00, CommonLib Sky::GetSingleton)
// returns it and creates the Sky on first use; reading the pointer creates nothing.
constexpr Address skyAddress{401652,0x31e0d80};
constexpr Address skyModeAddress{26222,0x411660}; // Sky::SetMode(sky, mode)
// SetMode's head: sets Sky+1DC bit 8, r14b = (mode-2) <= 1 (modes 2 and 3
// show the sky), reads the previous mode at +1BC.
constexpr std::array<std::uint8_t,47> skyModeHead{
    0x40,0x57,0x48,0x83,0xec,0x30,0x81,0x89,0xdc,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x8d,0x42,0xfe,0x83,0xf8,0x01,0x48,0x89,
    0x6c,0x24,0x50,0x8b,0x81,0xbc,0x01,0x00,0x00,0x8b,0xea,0x4c,0x89,0x74,0x24,0x28,0x48,0x8b,0xf9,0x41,0x0f,0x96,0xc6};
// RVA 411794..4117CD: shown = r14b && !(Sky+1DC bit 7, HideSky); root = Sky+8;
// root+F4 bit 0 (hidden) cleared when shown, set otherwise.
constexpr std::uintptr_t skyRuleRva = 0x411794;
constexpr std::array<std::uint8_t,57> skyRuleBytes{
    0x45,0x84,0xf6,0x74,0x11,0x8b,0x87,0xdc,0x01,0x00,0x00,0xc1,0xe8,0x07,0xa8,0x01,0x75,0x04,0xb2,0x01,0xeb,0x02,0x32,0xd2,
    0x48,0x8b,0x4f,0x08,0x48,0x85,0xc9,0x74,0x18,0x8b,0x81,0xf4,0x00,0x00,0x00,0x84,0xd2,0x75,0x05,0x83,0xc8,0x01,0xeb,0x03,
    0x83,0xe0,0xfe,0x89,0x81,0xf4,0x00,0x00,0x00};
namespace sky {
constexpr std::size_t root = 0x08, mode = 0x1bc, flags = 0x1dc; // Sky (CommonLib: root, mode, flags)
constexpr std::uint32_t hideSky = 0x80;                          // Sky::Flags::kHideSky
constexpr std::size_t objectFlags = 0xf4;                        // NiAVObject::flags
constexpr std::uint32_t hidden = 0x1;                            // NiAVObject::Flag::kHidden
}
// 0 = the singleton, SetMode and the bytes that fix the field layout are the
// audited ones. Otherwise: 1 ID 401652, 2 ID 26222, 3 SetMode's head, 4 the
// show/hide rule (another module patched them, or another build).
inline unsigned skySiteCheck(std::uintptr_t base,std::uintptr_t skyPointer,std::uintptr_t setMode,const std::uint8_t* head,
    const std::uint8_t* rule) {
    if (skyPointer != base+skyAddress.rva) return 1;
    if (setMode != base+skyModeAddress.rva) return 2;
    if (std::memcmp(head,skyModeHead.data(),skyModeHead.size()) != 0) return 3;
    if (std::memcmp(rule,skyRuleBytes.data(),skyRuleBytes.size()) != 0) return 4;
    return 0;
}
// SetMode's rule: the engine shows the sky root in modes 2 (dome only) and 3
// (full) unless HideSky is set.
constexpr bool engineShowsSky(std::uint32_t mode,std::uint32_t flags) { return (mode == 2u || mode == 3u) && !(flags & sky::hideSky); }

// What one main-thread frame does with the root. `want`: the cut is active
// (published this frame). `held`: we set the bit and have not put it back.
enum class SkyStep : unsigned { none, hide, keep, restore, leave, count };
constexpr std::array skyStepNames{"none","hide","keep","restore","leave"};
static_assert(skyStepNames.size() == static_cast<std::size_t>(SkyStep::count));
// `sameRoot`: the root now is the one we hid (meaningful only when held).
//   wanted, a root, the engine shows it:
//     held another root            leave (that one is not touched again; the next frame hides this one)
//     bit clear                    hide  (also when the engine showed it again under our hold)
//     bit set, held                keep
//     bit set, not held            none  (someone else hid it: not ours to put back)
//   otherwise (not wanted, no root, or the engine hides the sky itself):
//     not held                     none
//     same root, engine shows, bit set: restore (only when not wanted)
//     anything else                leave (the engine has decided again: the bit is left as it is)
constexpr SkyStep skyStep(bool want,bool held,bool haveRoot,bool sameRoot,bool engineShows,bool hiddenBit) {
    if (want && haveRoot && engineShows) {
        if (held && !sameRoot) return SkyStep::leave;
        if (!hiddenBit) return SkyStep::hide;
        return held ? SkyStep::keep : SkyStep::none;
    }
    if (!held) return SkyStep::none;
    return haveRoot && sameRoot && engineShows && hiddenBit ? SkyStep::restore : SkyStep::leave;
}

// ---- Effect passes clipped whole (the wedge) ----------------------------------
// Fog cards and glows are flat, translucent and without thickness; cut at the
// plane they end in a straight bright edge (the wedge at Embershard heading
// 135). A selected Effect pass whose bound centre lies above the midpoint
// between the feet and the plane is therefore drawn with clip = 0 * height - 1
// for every vertex: nothing of it is rasterised. One whose centre is lower
// (floor mist, a fire in a floor brazier) keeps the plane cut, so what lies
// under the plane stays. Fourth pass (binary review of 8f1248f, MINOR-3): a
// tall mesh whose centre is high but whose bound reaches down to the feet (a
// light shaft to the floor, a waterfall or spray card, a placed magic barrier)
// keeps the plane cut, so its part at the player's level is still drawn; whole
// clipping needs the lowest point of the bound above the feet as well. The
// Embershard glows (centres 9392 and 9423, feet 9241) still go whole up to
// radius 151 and 182.
constexpr Constant wholeConstant{0.0f,-1.0f,0.0f,0.0f};
static_assert(clipDistance(wholeConstant,1.0e6f) < 0.0f && clipDistance(wholeConstant,-1.0e6f) < 0.0f);
inline bool wholeEffect(const View& view,Bound bound) {
    return std::isfinite(bound.centre.z) && std::isfinite(bound.radius) && bound.centre.z > (view.planeZ+view.feet.z)*0.5f &&
        bound.centre.z-bound.radius > view.feet.z;
}
// The upper-half test alone: such a pass that reaches the feet is the one the
// fourth pass keeps cut at the plane (FixedShaderStyle fxFloorKept).
inline bool upperEffect(const View& view,Bound bound) {
    return std::isfinite(bound.centre.z) && bound.centre.z > (view.planeZ+view.feet.z)*0.5f;
}

// ---- trace: FixedShaderVoid -----------------------------------------------------
constexpr std::size_t effectRefSlots = 4;
struct VoidWindow {
    std::array<unsigned long long,static_cast<std::size_t>(SkyStep::count)> steps{}; // main-thread frames by step
    unsigned long long engineHidden{},noSky{},whole{};
    unsigned long long floorKept{}; // fourth pass: upper-half Effect passes kept at the plane because they reach the feet
    float wholeRadius{};            // fourth pass: the largest bound radius clipped whole in the window
    // Not on the FixedShaderVoid row (byte-identical to 8f1248f); FixedShaderStyle reports both.
    std::array<std::uint32_t,effectRefSlots> refs{}; // distinct owners of cut Effect passes, first four
    unsigned refCount{};
    void note(std::uint32_t formId) {
        for (unsigned i = 0;i < refCount;++i) if (refs[i] == formId) return;
        if (refCount < refs.size()) refs[refCount++] = formId;
    }
};
struct VoidSample {
    double seconds{};
    unsigned long long frame{};
    Feature skyState{};
    bool held{},effectWhole{};
    int mode{-1};          // the Sky's mode at the last frame, -1 no Sky
    VoidWindow window{};
};
inline bool formatVoid(char* out,std::size_t size,const VoidSample& s) {
    const auto& w = s.window;
    const auto step = [&](SkyStep which) { return w.steps[static_cast<std::size_t>(which)]; };
    // "none", or up to four eight-digit form IDs joined by commas.
    char refs[effectRefSlots*9u+1u] = "none";
    std::size_t used = 0;
    for (unsigned i = 0;i < w.refCount && i < effectRefSlots;++i) {
        const unsigned id = static_cast<unsigned>(w.refs[i]);
        const int part = i ? std::snprintf(refs+used,sizeof(refs)-used,",%08X",id) : std::snprintf(refs+used,sizeof(refs)-used,"%08X",id);
        if (part > 0) used += static_cast<std::size_t>(part);
    }
    const int written = std::snprintf(out,size,
        "FixedShaderVoid t=%.3f frame=%llu sky=%s skyMode=%d skyHeld=%u skyHides=%llu skyKeeps=%llu skyRestores=%llu skyLeft=%llu "
        "skyEngineHidden=%llu skyNone=%llu fxWhole=%s fxWholeCut=%llu fxCutRefs=%s sample=window",
        s.seconds,s.frame,featureNames[static_cast<std::size_t>(s.skyState)],s.mode,static_cast<unsigned>(s.held),step(SkyStep::hide),
        step(SkyStep::keep),step(SkyStep::restore),step(SkyStep::leave),w.engineHidden,w.noSky,s.effectWhole ? "on" : "off",w.whole,refs);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
