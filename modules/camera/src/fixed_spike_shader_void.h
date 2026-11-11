// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", second
// pass (owner decision 2026-11-10): while the cut is active in an interior
// the main view clears to [FixedCameraSpike] sVoidColor (default black)
// instead of the cell's fog colour. Installed only by the shader cut's commit,
// so only in iSeeThroughMode=3, only after every cut guard (CommunityShaders
// and ENB refuse everything first, FCS-67), and only with bVoidColor=1.
//
// The one code change is the displacement of the `call SetClearColor` at
// RVA 656DAA inside ID 36558: it now lands on a 14-byte relay (jmp [rip]) in
// memory of our own within reach of the call, which jumps to hookClearColor.
// hookClearColor calls the engine's SetClearColor exactly once, with the
// engine's own arguments unless the cut is active, latched off or stale or
// the call is off the main thread. No engine object or global is written:
// the fog-colour global 3D3E70 and the renderer's clear colour are left to
// the engine. The colour reaches exactly one clear, the main colour target's
// (ID 36559 at 65758A), and is replaced by (0,0,0,0) at 6575E5 in the same
// call (docs/evidence/p5.5/spike-wave2-shader.md, "Void colour").

namespace skycrpg::camera::fixed::shader {
// The relay's target, where the engine called SetClearColor. Any thread.
inline void hookClearColor(void* renderer,float red,float green,float blue,float alpha) {
    auto& r = *runtime;
    const bool onMainThread = thin::onMain();
    if (!onMainThread) r.voidOffMain.fetch_add(1ull,std::memory_order_relaxed);
    // Off the main thread nothing but the two atomics is read.
    const Look decision = lookDecision(onMainThread,r.active.load(std::memory_order_acquire),r.latched.load(std::memory_order_acquire),
        onMainThread && r.haveView && fresh(r.view,thin::seconds()));
    if (onMainThread) {
        ++r.look.calls[static_cast<std::size_t>(decision)];
        r.look.native = {red,green,blue}; r.look.haveNative = true;
    }
    if (decision == Look::applied) r.clearColor(renderer,r.config.color.red,r.config.color.green,r.config.color.blue,alpha);
    else r.clearColor(renderer,red,green,blue,alpha);
}
// Committed read-write memory for the relay, within rel32 reach of `site`,
// at an allocation-granularity boundary of a free region. Null if none.
inline std::uint8_t* relayPage(std::uintptr_t site) {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::uintptr_t granularity = info.dwAllocationGranularity ? info.dwAllocationGranularity : 0x10000u;
    const Reach reach = relayReach(site,granularity);
    for (std::uintptr_t at = reach.low;at < reach.high;) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<const void*>(at),&region,sizeof(region))) return nullptr;
        const auto start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        const std::uintptr_t end = start+region.RegionSize;
        if (end <= at) return nullptr;
        const std::uintptr_t aligned = (start+granularity-1u)/granularity*granularity;
        if (region.State == MEM_FREE && aligned < end && end-aligned >= granularity && aligned <= reach.high)
            if (void* page = VirtualAlloc(reinterpret_cast<void*>(aligned),relaySize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE))
                return static_cast<std::uint8_t*>(page);
        at = end;
    }
    return nullptr;
}
// Main thread, from commit. Every check comes before the one write to engine
// code; the relay is sealed execute-read before the call can reach it.
inline void installVoid(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.config.voidColor) { r.voidState = Feature::off; return; }
    r.voidState = Feature::refused;
    char text[256]{};
    const std::uintptr_t site = base+voidCallRva;
    const std::uintptr_t mainView = REL::ID(mainViewAddress.id).address(),clearColor = REL::ID(clearColorAddress.id).address();
    if (const unsigned failed = voidSiteCheck(base,mainView,clearColor,reinterpret_cast<const std::uint8_t*>(site-voidGuardedBefore),
            reinterpret_cast<const std::uint8_t*>(clearColor))) {
        std::snprintf(text,sizeof(text),"check=%u: the main view's SetClearColor call (ID 36558 +9A) or SetClearColor differ (another module "
            "or another build); the void keeps the engine's colour, the cut is unchanged",failed);
        refuse(Refusal::voidSite,text); return;
    }
    // cause 1: no free memory within reach; 2: the relay is out of reach or could not be sealed execute-read.
    std::uint8_t* const page = relayPage(site);
    std::optional<std::int32_t> displacement;
    unsigned cause = page ? 0u : 1u;
    if (page) {
        displacement = callDisplacement(site,reinterpret_cast<std::uintptr_t>(page));
        const auto bytes = relay(reinterpret_cast<std::uintptr_t>(&hookClearColor));
        std::memcpy(page,bytes.data(),bytes.size());
        DWORD previous{};
        if (!displacement || !VirtualProtect(page,relaySize,PAGE_EXECUTE_READ,&previous)) {
            VirtualFree(page,0,MEM_RELEASE);
            cause = 2u;
        }
    }
    if (cause) {
        std::snprintf(text,sizeof(text),"cause=%u: no relay for the main view's SetClearColor call; the void keeps the engine's colour, "
            "the cut is unchanged",cause);
        refuse(Refusal::voidMemory,text); return;
    }
    FlushInstructionCache(GetCurrentProcess(),page,relaySize);
    // Published before the call site changes: the hook calls it at once.
    r.clearColor = reinterpret_cast<ClearColor>(clearColor);
    const std::int32_t value = *displacement;
    REL::safe_write(site+1u,&value,sizeof(value));
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);
    r.voidState = Feature::installed;
}
} // namespace skycrpg::camera::fixed::shader
