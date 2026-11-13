// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader": the door
// void's glow cards (iDoorVoidGlow=1/2; only with iDoorVoid=1, bFramePass=1,
// iCutStyle=1 and bShaderCutEffect=1; the rules for which cards are in
// include/fixed_spike_shader_door_glow_policy.h). Nothing here exists, is read
// or is drawn differently with iDoorVoidGlow=0.
//
// Mechanism. Every vanilla glow-card model is Effect-shader geometry only, so
// its draws are Effect passes, which the shader cut already hooks (second
// pass). publish takes this frame's set: the cards of Stage 1's census that a
// door the frame pass will draw in this frame's render places (the frame pass's
// own selectDoors over the same census, feet and radius), only in a cell the
// census calls flat. consider hands every Effect pass of the published view to
// doorGlowPass while the set is not empty: a pass whose owning reference is in
// the set is drawn with the audited patched Effect vertex shader and the third
// pass's wholeConstant in slot 13 (clip = -1 for every vertex: nothing is
// rasterised), exactly as bShaderCutEffectWhole clips an upper Effect pass
// whole; RestoreGeometry closes it as any cut pass. Passes of other views
// (reflections, cube maps) stay native; Effect geometry is never drawn into a
// shadow map. No engine object, code or state is written.
//
// Engine reads: none the shader cut does not already make. The census branch
// (fixed_spike_shader_style.h noteGlowRef) reads, for a Static or
// MovableStatic whose model path (TESModel, as modelFamily) is a glow card, its
// form flags (IsDisabled / IsDeleted, as noteDoorRef), position (as noteDoorRef
// and noteUpright), scale and base OBND (as noteUpright); the draw reads the
// geometry's owning reference (ownerOf, as ownerSkip) and its form ID.

namespace skycrpg::camera::fixed::shader {
struct DoorGlowRuntime {
    GlowSet set{};       // this frame's cards (publish)
    GlowSet traced{};    // ... as last written to the trace
    GlowCounts window{}; // Effect passes of hidden cards since the last row
};
// configure, after validate: the runtime exists only with iDoorVoidGlow=1/2 (validate keeps it 0 without the door
// void or the Effect class).
inline void configureDoorGlow() {
    auto& r = *runtime;
    if (r.config.frame.enabled && r.config.frame.doorVoid && r.config.frame.doorGlow) r.doorGlow = new DoorGlowRuntime;
}
// Main thread, publish: `active` is this frame's published cut. Off (an empty set) unless the frame pass and the
// Effect class are installed, nothing latched and the cell is flat.
inline void doorGlowFrame(bool active) {
    auto& r = *runtime;
    if (!r.doorGlow) return;
    auto& g = *r.doorGlow;
    const bool want = active && r.framePass && r.framePass->state == Feature::installed && !r.framePass->broken &&
        r.effectState == Feature::installed && !r.latched.load(std::memory_order_acquire) && styled() && r.style->cellStyle == CellStyle::flat;
    if (!want) { g.set = {}; g.set.cell = styled() ? r.style->cell : 0u; return; }
    DoorSelection chosen = selectDoors(r.style->doors,r.view.feet,r.config.frame.doorRadius);
    chosen.cell = r.style->cell;
    g.set = selectGlows(r.style->glows,chosen,r.config.frame.doorDepth,r.config.frame.doorGlow);
}
// consider, an Effect pass of the published view (main thread, no cut open): true when its owner is a card of this
// frame's set; the pass is then not drawn, or drawn natively when no audited patched shader or no budget is left.
inline bool doorGlowPass(const std::uint8_t* geometry,std::size_t shaderClass) {
    auto& r = *runtime;
    auto& g = *r.doorGlow;
    if (!g.set.count || !geometry) return false;
    const auto ref = ownerOf(geometry);
    if (!ref || !g.set.hides(ref->GetFormID())) return false;
    const std::size_t group = groupOf(shaderClass);
    const Entry* entry = shaderFor(group);
    if (!entry || !entry->patched || !r.budgets[group].take()) { ++g.window.native; return true; }
    ++g.window.removed;
    apply(*entry,wholeConstant,shaderClass);
    return true;
}
// FixedDoorGlow, through the frame row's emitter and budget, on change only.
inline void traceDoorGlow(double now) {
    auto& r = *runtime;
    if (!r.doorGlow || !r.config.trace) return;
    auto& g = *r.doorGlow;
    if (sameGlows(g.set,g.traced)) return;
    GlowTrace t;
    t.seconds = now; t.frame = r.frame; t.mode = r.config.frame.doorGlow; t.set = &g.set; t.census = styled() ? &r.style->glows : nullptr;
    t.counts = g.window;
    char line[sizeof(TraceRow)]{};
    if (formatDoorGlow(line,sizeof(line),t)) r.frameEmitter(line);
    g.traced = g.set; g.window = {};
}
} // namespace skycrpg::camera::fixed::shader
