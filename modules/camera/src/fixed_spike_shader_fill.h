// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", Stage 3 step 3.1 (owner go "Start
// solid caps"): the back-face fill, iCutFill=1. Exists only with that key; installed only by the shader
// cut's commit after every earlier pass, so ENB and Community Shaders refuse it with the cut (FCS-67).
//
// No engine code is written and nothing new is hooked. In the existing RestoreGeometry hook of the Lighting
// class, before the engine's RestoreGeometry runs, a pass the cut has bracketed with a plane is drawn once
// more: ID 107637 has just drawn it (type 3: ID 77263, DrawIndexed(3 * triangles, 0, 0)) and only restored
// registers since, so the index and vertex buffers, the input layout, our patched vertex shader with the
// plane in slot 13, the World constants, the render targets, the viewport and the depth view are still the
// ones of that draw. The fill changes four things: the rasteriser state (the engine's own entry with front
// faces culled), the depth-stencil state (the engine's own test-and-write entry with the draw's stencil
// mode), the blend state (ours: no blending, motion vectors kept) and the pixel shader (the cap) with its
// constants in pixel slot 13. Each is read before and set back after, every reference released. Kit
// buildings, other classes, skinned geometry, two-sided, blended, alpha-tested and non-depth-writing
// draws are never filled; every bracketed pass that reaches the gate is counted, filled or not.

namespace skycrpg::camera::fixed::shader {
struct FillRuntime {
    Feature state{Feature::pending};
    void* cap{};        // the cap pixel shader
    void* blend{};      // our blend state
    void* constants{};  // FillConstants, pixel slot 13
    void* const* depthTable{};      // ID 411362: 6 * 40 depth-stencil states
    void* const* rasterizerTable{}; // ID 411363: 2 * 3 * 12 * 2 rasteriser states
    bool broken{};      // a Map failed: off for the session
    unsigned long long frame{~0ull}; // the frame the budget and the guard belong to
    unsigned used{};
    bool guardReady{};
    Guard guard{};
    unsigned long long mismatch{}; // restores whose read-back differed (cumulative)
    FillWindow window{};
};
// FCS-100, once: a draw the gate admitted has no triangles, no renderer data or a state index out of range.
inline void refuseFillLayout(const char* text) { refuse(Refusal::fillLayout,text); }
// The guard and the constants of this frame, from the render state of the first filled pass (a pass of the
// published view: consider checked it). False when the buffer cannot be mapped.
inline bool fillFrameConstants() {
    auto& r = *runtime;
    auto& f = *r.fill;
    f.guard = playerGuard(peek<Matrix>(r.state,frame_offset::viewProjection),peek<Vec>(r.state,offset::posAdjust),
        peek<Viewport>(r.state,frame_offset::viewport),r.view.feet);
    const FillConstants k = fillConstants(r.config.fillColor,f.guard);
    MappedBuffer mapped{};
    const std::int32_t answer = invoke<std::int32_t>(r.context,method::map,f.constants,0u,mapWriteDiscard,0u,&mapped);
    if (answer < 0 || !mapped.data) {
        f.broken = true;
        char text[192]{};
        std::snprintf(text,sizeof(text),"result=%08X: the fill's constant buffer could not be mapped; the fill is off for the session, "
            "the cut is unchanged",static_cast<unsigned>(answer));
        refuse(Refusal::fillMap,text); return false;
    }
    std::memcpy(mapped.data,&k,sizeof(FillConstants));
    invoke<void>(r.context,method::unmap,f.constants,0u);
    f.guardReady = true;
    f.window.haveGuard = true; f.window.guard = f.guard;
    return true;
}
// RestoreGeometry hook, before the engine's RestoreGeometry. Main thread only.
inline void cutFill(void* pass,std::size_t shaderClass) {
    auto& r = *runtime;
    auto& f = *r.fill;
    if (!thin::onMain() || f.state != Feature::installed || f.broken || !r.openFill || !r.opened.load(std::memory_order_acquire) ||
        r.openPass != pass || r.latched.load(std::memory_order_acquire)) return;
    const double started = r.config.trace ? thin::seconds() : 0.0;
    auto& w = f.window;
    ++w.sliced;
    if (f.frame != r.frame) { f.frame = r.frame; f.used = 0; f.guardReady = false; }
    const auto geometry = peek<const std::uint8_t*>(pass,offset::passGeometry);
    FillFacts x;
    x.lighting = shaderClass == 0;
    x.flatCell = styled() && r.style->cellStyle == CellStyle::flat;
    x.skinned = !geometry || peek<const void*>(geometry,offset::geometrySkin) != nullptr;
    x.type = geometry ? peek<std::uint8_t>(geometry,fill_offset::geometryType) : std::uint8_t{0};
    x.cull = peek<std::uint32_t>(r.state,fill_offset::cullMode);
    x.blend = peek<std::uint32_t>(r.state,fill_offset::blendMode);
    x.alphaToCoverage = peek<std::uint32_t>(r.state,fill_offset::alphaToCoverage);
    x.alphaTest = peek<std::uint8_t>(r.state,fill_offset::alphaTest);
    x.depth = peek<std::uint32_t>(r.state,fill_offset::depthMode);
    x.writeMask = peek<std::uint32_t>(r.state,fill_offset::writeMask);
    x.budget = f.used < fillPerFrame;
    const std::uint16_t triangles = geometry ? peek<std::uint16_t>(geometry,fill_offset::geometryTriangles) : std::uint16_t{0};
    const void* data = geometry ? peek<const void*>(geometry,fill_offset::geometryData) : nullptr;
    const auto rasterIndex = frontCullIndex(peek<std::uint32_t>(r.state,fill_offset::fillMode),peek<std::uint32_t>(r.state,fill_offset::depthBias),
        peek<std::uint32_t>(r.state,fill_offset::scissor));
    const auto depthIndex = depthWriteIndex(peek<std::uint32_t>(r.state,fill_offset::stencilMode));
    void* const raster = rasterIndex ? f.rasterizerTable[*rasterIndex] : nullptr;
    void* const depth = depthIndex ? f.depthTable[*depthIndex] : nullptr;
    x.layout = triangles && data && raster && depth;
    const FillSkip skip = fillGate(x);
    if (skip == FillSkip::layout)
        refuseFillLayout("a sliced type-3 Lighting draw had no triangles, no renderer data or an engine state index out of range; "
            "such draws are not filled, the cut is unchanged");
    if (skip != FillSkip::none || (!f.guardReady && !fillFrameConstants())) {
        if (skip != FillSkip::none) ++w.skips[static_cast<std::size_t>(skip)];
        if (r.config.trace) w.seconds += thin::seconds()-started;
        return;
    }
    // The player guard could not be placed this frame (a corner of the player's box behind the eye): no cap at all
    // rather than a cap without the hole around the player.
    if (!f.guard.on) {
        ++w.skips[static_cast<std::size_t>(FillSkip::guard)];
        if (r.config.trace) w.seconds += thin::seconds()-started;
        return;
    }
    ++f.used;
    if (boundMeetsPlayer({peek<Vec>(geometry,offset::objectBound),peek<float>(geometry,offset::objectBound+sizeof(Vec))},r.view.feet)) ++w.guarded;
    void* const c = r.context;
    // Read what the draw used.
    void* savedRaster = nullptr;
    void* savedDepth = nullptr;
    std::uint32_t savedRef = 0;
    void* savedBlend = nullptr;
    std::array<float,4> savedFactor{};
    std::uint32_t savedMask = 0;
    void* savedPixel = nullptr;
    void* savedConstants[1]{};
    invoke<void>(c,frame_method::rsGetState,&savedRaster);
    invoke<void>(c,fill_method::omGetDepthStencilState,&savedDepth,&savedRef);
    invoke<void>(c,frame_method::omGetBlendState,&savedBlend,savedFactor.data(),&savedMask);
    invoke<void>(c,frame_method::psGetShader,&savedPixel,static_cast<void**>(nullptr),static_cast<std::uint32_t*>(nullptr));
    invoke<void>(c,frame_method::psGetConstantBuffers,fillBuffer,1u,static_cast<void**>(savedConstants));
    // The second draw: front faces culled, depth tested and written, the cap.
    void* constants[1]{f.constants};
    invoke<void>(c,frame_method::rsSetState,raster);
    invoke<void>(c,fill_method::omSetDepthStencilState,depth,peek<std::uint32_t>(r.state,fill_offset::stencilRef));
    invoke<void>(c,frame_method::omSetBlendState,f.blend,static_cast<const float*>(nullptr),0xffffffffu);
    invoke<void>(c,frame_method::psSetShader,f.cap,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::psSetConstantBuffers,fillBuffer,1u,static_cast<void* const*>(constants));
    invoke<void>(c,fill_method::drawIndexed,3u*static_cast<std::uint32_t>(triangles),0u,0);
    // Back as the draw had it, in reverse order.
    invoke<void>(c,frame_method::psSetConstantBuffers,fillBuffer,1u,static_cast<void* const*>(savedConstants));
    invoke<void>(c,frame_method::psSetShader,savedPixel,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::omSetBlendState,savedBlend,static_cast<const float*>(savedFactor.data()),savedMask);
    invoke<void>(c,fill_method::omSetDepthStencilState,savedDepth,savedRef);
    invoke<void>(c,frame_method::rsSetState,savedRaster);
    // A cheap read-back: the rasteriser state and the pixel shader.
    void* checkRaster = nullptr;
    void* checkPixel = nullptr;
    invoke<void>(c,frame_method::rsGetState,&checkRaster);
    invoke<void>(c,frame_method::psGetShader,&checkPixel,static_cast<void**>(nullptr),static_cast<std::uint32_t*>(nullptr));
    if (checkRaster != savedRaster || checkPixel != savedPixel) ++f.mismatch;
    for (void* object : {checkRaster,checkPixel,savedRaster,savedDepth,savedBlend,savedPixel,savedConstants[0]}) releaseObject(object);
    ++w.filled; ++w.restores;
    if (r.config.trace) w.seconds += thin::seconds()-started;
}
inline Feature fillState() { return runtime && runtime->fill ? runtime->fill->state : Feature::off; }
// Main thread, from commit, after the frame pass. Every check and every device object first; nothing is
// written to the engine, the fill only starts drawing once its state says installed.
inline void installFill(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.fill) return;
    auto& f = *r.fill;
    f.state = Feature::refused;
    char text[256]{};
    const std::uintptr_t depthTable = REL::ID(depthStencilTableAddress.id).address();
    const std::uintptr_t rasterizerTable = REL::ID(rasterizerTableAddress.id).address();
    const auto image = [base](std::uintptr_t rva) { return reinterpret_cast<const std::uint8_t*>(base+rva); };
    if (const unsigned failed = fillSiteCheck(base,depthTable,rasterizerTable,image)) {
        std::snprintf(text,sizeof(text),"check=%u: the engine's state tables or the draw code the fill relies on differ (another build or "
            "module); no fill, the cut is unchanged",failed);
        refuse(Refusal::fillSite,text); return;
    }
    // 1 the cap pixel shader, 2 the blend state, 3 the constant buffer.
    void* cap = nullptr;
    void* blend = nullptr;
    void* constants = nullptr;
    const BlendDescription blendDescription = fillBlend();
    BufferDescription description;
    description.bytes = static_cast<std::uint32_t>(sizeof(FillConstants));
    unsigned object = 1;
    std::int32_t answer = invoke<std::int32_t>(r.device,frame_method::createPixelShader,static_cast<const void*>(frame_bytes::capPixelShader.data()),
        frame_bytes::capPixelShader.size(),static_cast<void*>(nullptr),&cap);
    if (answer >= 0 && cap) {
        object = 2;
        answer = invoke<std::int32_t>(r.device,frame_method::createBlendState,static_cast<const BlendDescription*>(&blendDescription),&blend);
    }
    if (answer >= 0 && blend) {
        object = 3;
        answer = invoke<std::int32_t>(r.device,method::createBuffer,static_cast<const BufferDescription*>(&description),
            static_cast<const void*>(nullptr),&constants);
    }
    if (answer < 0 || !constants) {
        for (void* made : {cap,blend,constants}) releaseObject(made);
        std::snprintf(text,sizeof(text),"object=%u result=%08X: the device refused a fill object (1 cap shader, 2 blend, 3 constants); "
            "no fill, the cut is unchanged",object,static_cast<unsigned>(answer));
        refuse(Refusal::fillDevice,text); return;
    }
    f.cap = cap; f.blend = blend; f.constants = constants;
    f.depthTable = reinterpret_cast<void* const*>(depthTable);
    f.rasterizerTable = reinterpret_cast<void* const*>(rasterizerTable);
    f.state = Feature::installed;
}
// configure, after validate: the runtime exists only with iCutFill=1.
inline void configureFill() {
    auto& r = *runtime;
    if (r.config.fill) r.fill = new FillRuntime;
}
inline void traceFill(double now) {
    auto& r = *runtime;
    if (!r.fill) return;
    auto& f = *r.fill;
    FillSample sample;
    sample.seconds = now; sample.frame = r.frame; sample.mismatch = f.mismatch; sample.state = f.state; sample.colour = r.config.fillColor;
    sample.window = f.window;
    f.window = {};
    char line[sizeof(TraceRow)]{};
    if (formatFill(line,sizeof(line),sample)) r.fillEmitter(line);
}
} // namespace skycrpg::camera::fixed::shader
