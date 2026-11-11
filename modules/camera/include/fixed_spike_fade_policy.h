// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_policy.h users through src/fixed_spike.h. P5.5-02,
// base rig: the engine's own player fade while the fixed rig holds.
//
// SmoothCam calls the native ThirdPersonState::Update (ID 50896) every frame
// and only then places its camera. The native update builds the vanilla
// camera point and calls PlayerCamera::CheckCameraCollision (ID 50832, RVA
// 8FA5D0) with a_fadeCharacter set. At 8FAAAD..8FAAE9 that function reads
// fCharControllerCheckHeightOffset:Camera (the Setting at RVA 20BA460, ID
// 382599, value at +8, default 124) and calls CheckCharacterCollision (ID
// 33008, RVA 557AD0; its only direct caller) with (player height - offset);
// a true answer makes the player the actor to fade (8FAAF0..8FAB02), and the
// fade bookkeeping at 8FAB10..8FAC5B sets the player's fade state to out. Under
// a low roof (the Bannered Mare loft at headings 135/225) the native point
// overlaps the player and the player vanishes (session-p55/research/
// missing-player-loft.md; W10: the console setini of the offset to -5000 made
// the player drawn at all four headings).
//
// The rig holds that Setting at -5000 while it holds the view (the frames
// that gate the caster detour), and puts the engine's value back on the first
// frame it does not and on every lifecycle exit. The Setting is read by that
// one instruction only (8FAAAD; the other references, B2BB7, 16B9CD0 and
// 17A3CE4..17A3D01, register and unregister it with the INI collection), so
// nothing else changes: NPC fades near the camera come from
// fActorFadeOutLimit inside the cast (5574F0, ID 33007, read at 5577A4), not from this test. When the
// test stops returning true, the engine itself fades a faded player back in:
// the handle at PlayerCamera+130 changes from the player to none and an old
// actor in fade state 2 (out) gets 691A10 (ID 37574, fade in) at 8FAB37..8FAB55.
// No engine code is written.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace skycrpg::camera::fixed {
constexpr std::uint64_t fadeSettingId = 382599;      // Setting fCharControllerCheckHeightOffset:Camera
constexpr std::uintptr_t fadeSettingRva = 0x20ba460;
constexpr std::size_t settingValue = 0x08, settingName = 0x10; // Setting: vtable, value union, name (const char*)
constexpr char fadeSettingName[] = "fCharControllerCheckHeightOffset:Camera";
constexpr float fadeHoldValue = -5000.0f;            // the value W10 set from the console
constexpr unsigned fadeTraceLines = 6000;
constexpr std::size_t playerFadeHandle = 0x130;      // PlayerCamera: objectFadeHandle (u32), lea rsi,[rdi+130] at 8FAB10
// 0 = the ID resolves to the audited RVA and the object there is the named
// Setting; 1 the ID, 2 no name, 3 another name.
inline unsigned fadeSettingCheck(std::uintptr_t base,std::uintptr_t resolved,const char* name) {
    if (resolved != base+fadeSettingRva) return 1;
    if (!name) return 2;
    return std::strncmp(name,fadeSettingName,sizeof(fadeSettingName)) == 0 ? 0u : 3u;
}
// What one camera frame does with the Setting. `eligible`: the rig held this
// frame (the hold covers the next native update). `held`: we wrote the hold
// value and have not put the engine's back.
//   eligible, not held           hold    (save the engine's value, write the hold)
//   eligible, held, still ours   keep
//   eligible, held, changed      rehold  (someone else wrote it: theirs becomes the saved value, ours again)
//   not eligible, held, ours     release (write the saved value back)
//   not eligible, held, changed  leave   (someone else wrote it: left as it is)
//   otherwise                    none
enum class FadeStep : unsigned { none, hold, keep, rehold, release, leave, count };
constexpr std::array fadeStepNames{"none","hold","keep","rehold","release","leave"};
static_assert(fadeStepNames.size() == static_cast<std::size_t>(FadeStep::count));
constexpr FadeStep fadeStep(bool installed,bool eligible,bool held,float current) {
    if (!installed) return FadeStep::none;
    if (eligible) return !held ? FadeStep::hold : current == fadeHoldValue ? FadeStep::keep : FadeStep::rehold;
    if (!held) return FadeStep::none;
    return current == fadeHoldValue ? FadeStep::release : FadeStep::leave;
}

// ---- trace: FixedFade (one row per 0.1 s window while any spike trace is on) --------
enum class FadeFeature : unsigned { off, installed, refused, count };
constexpr std::array fadeFeatureNames{"off","installed","refused"};
static_assert(fadeFeatureNames.size() == static_cast<std::size_t>(FadeFeature::count));
struct FadeWindow {
    std::array<unsigned long long,static_cast<std::size_t>(FadeStep::count)> steps{}; // camera frames by step
    unsigned long long frames{},playerFaded{},heldPlayerFaded{}; // frames, and frames whose fade handle was the player (all / while held)
};
struct FadeSample {
    double seconds{};
    unsigned long long frame{};
    FadeFeature state{};
    bool held{};
    float current{},saved{};
    FadeWindow window{};
};
inline bool formatFade(char* out,std::size_t size,const FadeSample& s) {
    const auto& w = s.window;
    const auto step = [&](FadeStep which) { return w.steps[static_cast<std::size_t>(which)]; };
    const int written = std::snprintf(out,size,
        "FixedFade t=%.3f frame=%llu state=%s held=%u holdValue=%.1f current=%.1f saved=%.1f frames=%llu holds=%llu keeps=%llu reholds=%llu "
        "releases=%llu leaves=%llu playerFaded=%llu heldPlayerFaded=%llu sample=window",
        s.seconds,s.frame,fadeFeatureNames[static_cast<std::size_t>(s.state)],static_cast<unsigned>(s.held),static_cast<double>(fadeHoldValue),
        static_cast<double>(s.current),static_cast<double>(s.saved),w.frames,step(FadeStep::hold),step(FadeStep::keep),step(FadeStep::rehold),
        step(FadeStep::release),step(FadeStep::leave),w.playerFaded,w.heldPlayerFaded);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed
