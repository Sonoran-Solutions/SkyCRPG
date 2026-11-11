// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", fifth
// pass (owner go 2026-11-11): Stage 2 of the seamless cutaway, the frame pass.
// Exists only in iSeeThroughMode=3 with bFramePass=1, and is installed only by
// the shader cut's commit after every cut guard (so ENB and Community Shaders
// refuse it with the cut, FCS-67).
//
// The one code change is the displacement of the `call` at RVA 153ADE7 in ID
// 107148 (the end of the main world render; its call of the image-space chain,
// ID 105674, after the scene composite ID 105675 has left the HDR scene in
// target 1): it lands on a 14-byte relay (jmp [rip]) in memory of our own,
// which jumps to hookImageSpace. hookImageSpace first draws, on the main
// thread, for the normal-play destination 0 only and only while the cut is
// active, then calls ID 105674 exactly once with every argument register
// passed on. The draw is one full-screen triangle into the main colour target
// (renderer target 1) reading the main depth (renderer depth 0) through the
// engine's own shader-resource view of it. The depth-stencil view is not bound
// while that view is read. Every piece of device state the draw changes is
// read before and set back after, with each reference released; nothing is
// allocated per frame. The policy and the audit behind each offset are in
// include/fixed_spike_shader_frame_policy.h.

namespace skycrpg::camera::fixed::shader {
// ID 105674 as the call at 153ADE7 makes it: (manager, source 1, destination, 1); every register argument is
// passed on and RAX returned.
using ImageChainFunction = std::uintptr_t(*)(void*,std::uintptr_t,std::uintptr_t,std::uintptr_t);
struct FrameRuntime {
    Feature state{Feature::pending};
    ImageChainFunction imageChain{}; // ID 105674; set before the call site is redirected
    const std::uint8_t* renderer{};
    void* vertexShader{};
    void* pixelShader{};
    void* constants{};
    void* blend{};
    void* rasterizer{};
    std::atomic<unsigned long long> offMain{};
    // Main thread only from here on.
    bool captured{};   // a pass of the published view was drawn in this frame's render (FrameCapture filled)
    bool broken{};     // the constant buffer could not be mapped: off for the rest of the session
    FrameCapture capture{};
    const void* checkedColour{};
    const void* checkedDepth{};
    unsigned targets{1}; // targetCheck of the pair last checked (0 = usable)
    TextureDescription colour{},depth{};
    unsigned long long mismatch{}; // restores whose read-back differed (cumulative)
    FrameWindow window{};
};
// FCS-97, once: cause 1..5 the targets (targetCheck: the pass skips while they hold), 10 the constant
// buffer could not be mapped (the pass is off for the session).
inline void refuseRun(const char* text) { refuse(Refusal::frameRun,text); }
inline void releaseObject(void* object) { if (object) static_cast<void>(invoke<std::uint32_t>(object,frame_method::release)); }
// Main thread, from consider: the first pass of the published view in this render. Everything the
// reconstruction needs is taken from the render state as that pass sees it.
inline void captureFrame(Vec eye) {
    auto& r = *runtime;
    if (!r.framePass || r.framePass->captured || r.framePass->state != Feature::installed) return;
    auto& c = r.framePass->capture;
    c.viewProjection = peek<Matrix>(r.state,frame_offset::viewProjection);
    c.posAdjust = eye;
    c.viewport = peek<Viewport>(r.state,frame_offset::viewport);
    c.depthMin = peek<float>(r.state,frame_offset::depthRange);
    c.depthMax = peek<float>(r.state,frame_offset::depthRange+sizeof(float));
    c.viewFrame = r.view.frame;
    r.framePass->captured = true;
}
// The engine's main colour target and main depth, checked again only when either texture changes.
inline bool targetsReady(void*& colourView,void*& depthView) {
    auto& f = *runtime->framePass;
    const auto colourTexture = peek<void*>(f.renderer,frame_offset::mainTexture);
    const auto depthTexture = peek<void*>(f.renderer,frame_offset::depthTexture);
    colourView = peek<void*>(f.renderer,frame_offset::mainTargetView);
    depthView = peek<void*>(f.renderer,frame_offset::depthView);
    const bool pointers = colourTexture && depthTexture && colourView && depthView;
    if (!pointers) { f.targets = 1; return false; }
    if (colourTexture != f.checkedColour || depthTexture != f.checkedDepth) {
        f.colour = {}; f.depth = {};
        invoke<void>(colourTexture,frame_method::textureDescription,&f.colour);
        invoke<void>(depthTexture,frame_method::textureDescription,&f.depth);
        f.checkedColour = colourTexture; f.checkedDepth = depthTexture;
        f.targets = targetCheck(true,f.colour,f.depth,f.capture.viewport);
        if (f.targets) {
            char text[256]{};
            std::snprintf(text,sizeof(text),"cause=%u: the main colour target (%ux%u format %u) or the main depth (%ux%u format %u array %u samples %u "
                "bind %X) cannot be used as audited; the frame pass skips these frames, the cut is unchanged",f.targets,f.colour.width,
                f.colour.height,f.colour.format,f.depth.width,f.depth.height,f.depth.format,f.depth.arraySize,f.depth.samples,f.depth.bind);
            refuseRun(text);
        }
    } else if (f.targets == 5) {
        f.targets = targetCheck(true,f.colour,f.depth,f.capture.viewport); // only the viewport can change with the same textures
    }
    return f.targets == 0;
}
// Device state the draw changes, as the device had it.
struct SavedState {
    std::array<void*,renderTargetSlots> targets{};
    void* depthStencil{};
    void* blend{};
    std::array<float,4> blendFactor{};
    std::uint32_t sampleMask{};
    void* rasterizer{};
    std::uint32_t viewports{};
    std::array<Viewport,viewportSlots> viewport{};
    void* layout{};
    std::uint32_t topology{};
    void* vertexShader{};
    void* pixelShader{};
    void* pixelConstants{};
    std::array<void*,pixelSlots> resources{};
};
inline void save(void* c,SavedState& s) {
    invoke<void>(c,frame_method::omGetRenderTargets,renderTargetSlots,s.targets.data(),&s.depthStencil);
    invoke<void>(c,frame_method::omGetBlendState,&s.blend,s.blendFactor.data(),&s.sampleMask);
    invoke<void>(c,frame_method::rsGetState,&s.rasterizer);
    s.viewports = 0;
    invoke<void>(c,frame_method::rsGetViewports,&s.viewports,static_cast<Viewport*>(nullptr));
    if (s.viewports > viewportSlots) s.viewports = viewportSlots;
    if (s.viewports) invoke<void>(c,frame_method::rsGetViewports,&s.viewports,s.viewport.data());
    invoke<void>(c,frame_method::iaGetInputLayout,&s.layout);
    invoke<void>(c,frame_method::iaGetPrimitiveTopology,&s.topology);
    invoke<void>(c,frame_method::vsGetShader,&s.vertexShader,static_cast<void**>(nullptr),static_cast<std::uint32_t*>(nullptr));
    invoke<void>(c,frame_method::psGetShader,&s.pixelShader,static_cast<void**>(nullptr),static_cast<std::uint32_t*>(nullptr));
    invoke<void>(c,frame_method::psGetConstantBuffers,0u,1u,&s.pixelConstants);
    invoke<void>(c,frame_method::psGetShaderResources,0u,pixelSlots,s.resources.data());
}
// The depth's view leaves pixel slot 0 before the saved targets (the depth-stencil view among them) come
// back, and the saved pixel resources come back after them, so the runtime never sees a resource bound for
// reading and writing at once and unbinds nothing on its own.
inline void restore(void* c,const SavedState& s) {
    void* none[1]{};
    invoke<void>(c,frame_method::psSetShaderResources,0u,1u,static_cast<void* const*>(none));
    invoke<void>(c,frame_method::omSetRenderTargets,renderTargetSlots,static_cast<void* const*>(s.targets.data()),s.depthStencil);
    invoke<void>(c,frame_method::psSetShaderResources,0u,pixelSlots,static_cast<void* const*>(s.resources.data()));
    invoke<void>(c,frame_method::omSetBlendState,s.blend,static_cast<const float*>(s.blendFactor.data()),s.sampleMask);
    invoke<void>(c,frame_method::rsSetState,s.rasterizer);
    invoke<void>(c,frame_method::rsSetViewports,s.viewports,static_cast<const Viewport*>(s.viewports ? s.viewport.data() : nullptr));
    invoke<void>(c,frame_method::iaSetInputLayout,s.layout);
    invoke<void>(c,frame_method::iaSetPrimitiveTopology,s.topology);
    invoke<void>(c,method::vsSetShader,s.vertexShader,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::psSetShader,s.pixelShader,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::psSetConstantBuffers,0u,1u,static_cast<void* const*>(&s.pixelConstants));
}
inline void releaseSaved(SavedState& s) {
    for (void* target : s.targets) releaseObject(target);
    for (void* resource : s.resources) releaseObject(resource);
    for (void* object : {s.depthStencil,s.blend,s.rasterizer,s.layout,s.vertexShader,s.pixelShader,s.pixelConstants}) releaseObject(object);
    s = {};
}
// A cheap read-back after the restore: the first colour target, the depth-stencil view and the pixel shader.
inline bool restoredAsSaved(void* c,const SavedState& s) {
    void* target[1]{};
    void* depthStencil{};
    void* pixel{};
    invoke<void>(c,frame_method::omGetRenderTargets,1u,target,&depthStencil);
    invoke<void>(c,frame_method::psGetShader,&pixel,static_cast<void**>(nullptr),static_cast<std::uint32_t*>(nullptr));
    const bool same = target[0] == s.targets[0] && depthStencil == s.depthStencil && pixel == s.pixelShader;
    for (void* object : {target[0],depthStencil,pixel}) releaseObject(object);
    return same;
}
// True when any of `count` views holds `texture`; every reference read is released.
inline bool viewsHold(std::size_t getter,std::uint32_t count,const void* texture) {
    std::array<void*,computeResourceSlots> views{};
    invoke<void>(runtime->context,getter,0u,count,views.data());
    bool held = false;
    for (std::uint32_t i = 0;i < count;++i) {
        if (!views[i]) continue;
        void* resource = nullptr;
        invoke<void>(views[i],frame_method::viewResource,&resource);
        held = held || resource == texture;
        releaseObject(resource);
        releaseObject(views[i]);
    }
    return held;
}
// A view of the main colour texture bound to the compute or vertex stage (MINOR 1 of the review of 32605a5):
// binding the texture as a render target would make the runtime unbind it there, which the engine's shadow
// state would not know. Pixel-stage views are saved and set back after the targets (restore), so they are fine.
inline bool colourShared() {
    const void* texture = peek<void*>(runtime->framePass->renderer,frame_offset::mainTexture);
    return viewsHold(frame_method::csGetShaderResources,computeResourceSlots,texture) ||
        viewsHold(frame_method::csGetUnorderedAccessViews,computeViewSlots,texture) ||
        viewsHold(frame_method::vsGetShaderResources,vertexResourceSlots,texture);
}
// Inside ID 107148, before the image-space chain: the main world and the scene composite are in target 1,
// depth 0 holds the scene's depth, nothing of the effects, the HDR tonemap or TAA has run.
inline void drawFrame() {
    auto& r = *runtime;
    auto& f = *r.framePass;
    if (!thin::onMain()) { f.offMain.fetch_add(1ull,std::memory_order_relaxed); return; }
    auto& w = f.window;
    ++w.calls;
    const bool captured = f.captured && f.capture.viewFrame == r.view.frame;
    f.captured = false; // the next render needs its own capture
    const double now = thin::seconds();
    FrameSkip skip = frameGate(r.latched.load(std::memory_order_acquire) || f.broken,r.active.load(std::memory_order_acquire),
        r.haveView && fresh(r.view,now),captured);
    void* colourView = nullptr;
    void* depthView = nullptr;
    if (skip == FrameSkip::none && !targetsReady(colourView,depthView)) skip = FrameSkip::targets;
    if (skip == FrameSkip::none && colourShared()) skip = FrameSkip::shared;
    BandState band{};
    float bandFloor{};
    const bool kitCell = styled() && r.style->cellStyle == CellStyle::kit;
    std::optional<FrameConstants> constants;
    if (skip == FrameSkip::none) {
        constants = frameConstants(r.config.frame,r.config.height,r.view,f.capture,r.config.color,kitCell,band,bandFloor);
        if (!constants) skip = FrameSkip::matrix;
    }
    if (skip != FrameSkip::none) { ++w.skips[static_cast<std::size_t>(skip)]; return; }
    MappedBuffer mapped{};
    const std::int32_t answer = invoke<std::int32_t>(r.context,method::map,f.constants,0u,mapWriteDiscard,0u,&mapped);
    if (answer < 0 || !mapped.data) {
        f.broken = true;
        char text[192]{};
        std::snprintf(text,sizeof(text),"cause=10 result=%08X: the frame pass's constant buffer could not be mapped; the pass is off for the session, "
            "the cut is unchanged",static_cast<unsigned>(answer));
        refuseRun(text); return;
    }
    std::memcpy(mapped.data,&*constants,sizeof(FrameConstants));
    invoke<void>(r.context,method::unmap,f.constants,0u);
    void* const c = r.context;
    SavedState saved;
    save(c,saved);
    void* targets[1]{colourView};
    void* resources[1]{depthView};
    void* buffers[1]{f.constants};
    Viewport view = f.capture.viewport;
    view.minDepth = 0.0f; view.maxDepth = 1.0f;
    // The depth-stencil view goes first, then the depth is bound for reading: never both at once.
    invoke<void>(c,frame_method::omSetRenderTargets,1u,static_cast<void* const*>(targets),static_cast<void*>(nullptr));
    invoke<void>(c,frame_method::psSetShaderResources,0u,1u,static_cast<void* const*>(resources));
    invoke<void>(c,frame_method::omSetBlendState,f.blend,static_cast<const float*>(nullptr),0xffffffffu);
    invoke<void>(c,frame_method::rsSetState,f.rasterizer);
    invoke<void>(c,frame_method::rsSetViewports,1u,static_cast<const Viewport*>(&view));
    invoke<void>(c,frame_method::iaSetInputLayout,static_cast<void*>(nullptr));
    invoke<void>(c,frame_method::iaSetPrimitiveTopology,triangleList);
    invoke<void>(c,method::vsSetShader,f.vertexShader,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::psSetShader,f.pixelShader,static_cast<void* const*>(nullptr),0u);
    invoke<void>(c,frame_method::psSetConstantBuffers,0u,1u,static_cast<void* const*>(buffers));
    invoke<void>(c,frame_method::draw,3u,0u);
    restore(c,saved);
    if (!restoredAsSaved(c,saved)) ++f.mismatch;
    releaseSaved(saved);
    ++w.drawn;
    w.haveDrawn = true; w.band = band; w.bandFloor = bandFloor; w.planeZ = r.view.planeZ;
    if (r.config.trace) w.seconds += thin::seconds()-now;
}
// The relay's target, where ID 107148 called the image-space chain. Any thread. The draw comes first (the
// chain reads target 1), and only for the normal-play destination.
inline std::uintptr_t hookImageSpace(void* manager,std::uintptr_t source,std::uintptr_t destination,std::uintptr_t flags) {
    if (static_cast<std::uint32_t>(destination) == playDestination && static_cast<std::uint32_t>(source) == chainSource) drawFrame();
    return runtime->framePass->imageChain(manager,source,destination,flags);
}
// Main thread, from frameStart.
inline void frameStartFrame() {
    if (!runtime->framePass) return;
    runtime->framePass->captured = false; // each frame's render captures its own main view
    if (runtime->framePass->offMain.load(std::memory_order_relaxed))
        refuse(Refusal::frameThread,"the image-space chain was called off the main thread; those frames were not touched");
}
// Main thread, from commit, after every earlier pass. Every check and every device object comes before
// the one write to engine code; the relay is sealed execute-read before the call can reach it.
inline void installFrame(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.framePass) return;
    auto& f = *r.framePass;
    f.state = Feature::refused;
    char text[256]{};
    const std::uintptr_t site = base+frameCallRva;
    const std::uintptr_t imageChain = REL::ID(imageChainAddress.id).address();
    const std::uintptr_t renderer = REL::ID(rendererAddress.id).address();
    const auto image = [base](std::uintptr_t rva) { return reinterpret_cast<const std::uint8_t*>(base+rva); };
    if (const unsigned failed = frameSiteCheck(base,REL::ID(postWorldAddress.id).address(),imageChain,renderer,
            reinterpret_cast<const std::uint8_t*>(site-frameGuardedBefore),reinterpret_cast<const std::uint8_t*>(imageChain),image)) {
        std::snprintf(text,sizeof(text),"check=%u: the image-space chain's call (ID 107148 +1E7), ID 105674 or the renderer code the frame pass "
            "relies on differ (another module or another build); no frame pass, the cut is unchanged",failed);
        refuse(Refusal::frameSite,text); return;
    }
    // Device objects: 1 vertex shader, 2 pixel shader, 3 constant buffer, 4 blend state, 5 rasterizer state.
    void* vertex = nullptr;
    void* pixel = nullptr;
    void* constants = nullptr;
    void* blend = nullptr;
    void* rasterizer = nullptr;
    BufferDescription description;
    description.bytes = static_cast<std::uint32_t>(sizeof(FrameConstants));
    const BlendDescription blendDescription = frameBlend();
    const RasterizerDescription rasterizerDescription = frameRasterizer();
    unsigned object = 1;
    std::int32_t answer = invoke<std::int32_t>(r.device,method::createVertexShader,static_cast<const void*>(frame_bytes::vertexShader.data()),
        frame_bytes::vertexShader.size(),static_cast<void*>(nullptr),&vertex);
    if (answer >= 0 && vertex) {
        object = 2;
        answer = invoke<std::int32_t>(r.device,frame_method::createPixelShader,static_cast<const void*>(frame_bytes::pixelShader.data()),
            frame_bytes::pixelShader.size(),static_cast<void*>(nullptr),&pixel);
    }
    if (answer >= 0 && pixel) {
        object = 3;
        answer = invoke<std::int32_t>(r.device,method::createBuffer,static_cast<const BufferDescription*>(&description),
            static_cast<const void*>(nullptr),&constants);
    }
    if (answer >= 0 && constants) {
        object = 4;
        answer = invoke<std::int32_t>(r.device,frame_method::createBlendState,static_cast<const BlendDescription*>(&blendDescription),&blend);
    }
    if (answer >= 0 && blend) {
        object = 5;
        answer = invoke<std::int32_t>(r.device,frame_method::createRasterizerState,
            static_cast<const RasterizerDescription*>(&rasterizerDescription),&rasterizer);
    }
    if (answer < 0 || !rasterizer) {
        for (void* made : {vertex,pixel,constants,blend,rasterizer}) releaseObject(made);
        std::snprintf(text,sizeof(text),"object=%u result=%08X: the device refused a frame pass object (1 vertex shader, 2 pixel shader, "
            "3 constants, 4 blend, 5 rasterizer); no frame pass, the cut is unchanged",object,static_cast<unsigned>(answer));
        refuse(Refusal::frameDevice,text); return;
    }
    // cause 1: no free memory within reach; 2: the relay is out of reach or could not be sealed execute-read.
    std::uint8_t* const page = relayPage(site);
    std::optional<std::int32_t> displacement;
    unsigned cause = page ? 0u : 1u;
    if (page) {
        displacement = callDisplacement(site,reinterpret_cast<std::uintptr_t>(page));
        const auto bytes = relay(reinterpret_cast<std::uintptr_t>(&hookImageSpace));
        std::memcpy(page,bytes.data(),bytes.size());
        DWORD previous{};
        if (!displacement || !VirtualProtect(page,relaySize,PAGE_EXECUTE_READ,&previous)) {
            VirtualFree(page,0,MEM_RELEASE);
            cause = 2u;
        }
    }
    if (cause) {
        for (void* made : {vertex,pixel,constants,blend,rasterizer}) releaseObject(made);
        std::snprintf(text,sizeof(text),"cause=%u: no relay for the image-space chain's call; no frame pass, the cut is unchanged",cause);
        refuse(Refusal::frameMemory,text); return;
    }
    FlushInstructionCache(GetCurrentProcess(),page,relaySize);
    // Published before the call site changes: the hook uses them at once.
    f.vertexShader = vertex; f.pixelShader = pixel; f.constants = constants; f.blend = blend; f.rasterizer = rasterizer;
    f.renderer = reinterpret_cast<const std::uint8_t*>(renderer);
    f.imageChain = reinterpret_cast<ImageChainFunction>(imageChain);
    f.state = Feature::installed;
    const std::int32_t value = *displacement;
    REL::safe_write(site+1u,&value,sizeof(value));
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);
}
inline Feature frameState() { return runtime && runtime->framePass ? runtime->framePass->state : Feature::off; }
// configure, after validate: the runtime exists only with bFramePass=1.
inline void configureFrame() {
    auto& r = *runtime;
    if (r.config.frame.enabled) r.framePass = new FrameRuntime;
}
inline void traceFrame(double now) {
    auto& r = *runtime;
    if (!r.framePass) return;
    auto& f = *r.framePass;
    FrameSample sample;
    sample.seconds = now; sample.frame = r.frame; sample.offMain = f.offMain.load(std::memory_order_relaxed); sample.mismatch = f.mismatch;
    sample.state = f.state; sample.config = r.config.frame; sample.window = f.window; sample.colour = f.colour; sample.depth = f.depth;
    f.window = {};
    char line[sizeof(TraceRow)]{};
    if (formatFrame(line,sizeof(line),sample)) r.frameEmitter(line);
}
} // namespace skycrpg::camera::fixed::shader
