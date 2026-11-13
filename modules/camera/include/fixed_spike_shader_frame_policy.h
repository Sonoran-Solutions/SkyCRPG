// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike_shader_policy.h. P5.5-02 wave 2 package
// "shader", fifth pass (owner go 2026-11-11, "Start Stage 2"): Stage 2 of the
// seamless cutaway, the frame pass. While the cut is active, one full-screen
// triangle of our own is drawn over the finished HDR scene inside ID 107148
// (the end of the main world render), after the scene composite (ID 105675:
// its stages and the copy back into target 1) and immediately before the
// image-space chain (ID 105674: effects, HDR tonemap, TAA, into the frame
// buffer). Fix pass after the binary review of 32605a5: the first version
// drew at the return of ID 36558, after that chain had already composed the
// frame into target 0. Its pixel shader reads the main depth and moves
// each pixel towards the void colour by
//   band     soft cut edge: smoothstep over the last N units under the plane
//   falloff  radial: smoothstep(R1, R2, ground distance to the player's feet)
//   both     1 - (1 - band)(1 - falloff), then the vignette the same way
// and the void (depth still the clear value 1.0) becomes exactly the void
// colour. Blending is src*alpha + dst*(1-alpha): a lerp, never an addition.
// No engine headers, no engine pointers; the native side is
// src/fixed_spike_shader_frame.h and the audit behind every number here is
// docs/evidence/p5.5/spike-wave2-shader.md, "Fifth pass".
// The door void (iDoorVoid=1, fixed_spike_shader_door_policy.h) adds a third
// term: surfaces behind a nearby load door's plane, in the doorway's lane,
// move to the void colour (the strongest of the doors' fades, taken with max
// so the frame is the same as before when no door is drawn).
#include "fixed_spike_shader_frame_shaders.h"
#include "fixed_spike_shader_door_policy.h"
#include "fixed_spike_shader_door_light_policy.h" // the door void's lights (iDoorVoidLights=1)
#include <cstddef>

namespace skycrpg::camera::fixed::shader {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
// ID 107148 (RVA 153AC00, its one caller ID 36559 at 6578D1) ends the main
// world render: render-target slots 0..3 and the depth target to none
// (153ACBE..153AD33), the volumetric-lighting stages, 152EA20 (depth 0 kept,
// mode 3), ID 105675 (14EDFD0: three image-space stages from target 1, then an
// ISCopy back into target 1), and at 153ADE7 ID 105674 (14EDBF0) with
// (manager, source 1, destination, 1): image-space effects 0..7, HDR (0x1E) or
// ISCopy (0x22), TAA (manager +0x1F0), the rest, into the destination. The
// destination is ID 36558's second argument, passed down unchanged (36558:
// esi = edx, 36559: [rax+0x10] = edx = [rbp+0x6F] -> ecx of 107148 -> ebp ->
// r8d): 0 in normal play (656A78), 0xE behind a frozen menu background
// (656834). Only the call at 153ADE7 is redirected; the pass draws only for
// destination 0. ID 105674's other caller (98A179) is not touched.
constexpr Address postWorldAddress{107148,0x153ac00};
constexpr Address imageChainAddress{105674,0x14edbf0};
constexpr Address rendererAddress{411393,0x3331f40}; // BSGraphics::Renderer: render-target and depth-target tables
constexpr std::uintptr_t frameCallRva = 0x153ade7;
constexpr std::size_t frameGuardedBefore = 0x2b; // the guarded bytes start at 153ADBC
constexpr std::array<std::uint8_t,0x30> frameSiteBytes{
    0xe8,0x5f,0x3c,0xff,0xff,                 // call 152EA20
    0xba,0x01,0x00,0x00,0x00,                 // mov edx,1
    0x44,0x8b,0xc2,                           // mov r8d,edx
    0x48,0x8b,0x0d,0xd0,0x92,0xe9,0x01,       // mov rcx,[33D40A0]   ImageSpaceManager
    0xe8,0xfb,0x31,0xfb,0xff,                 // call 14EDFD0        ID 105675: the scene composite into target 1
    0x41,0xb1,0x01,                           // mov r9b,1
    0x44,0x8b,0xc5,                           // mov r8d,ebp         the destination
    0xba,0x01,0x00,0x00,0x00,                 // mov edx,1           the source: target 1
    0x48,0x8b,0x0d,0xb9,0x92,0xe9,0x01,       // mov rcx,[33D40A0]
    0xe8,0x04,0x2e,0xfb,0xff};                // call 14EDBF0        ID 105674: the image-space chain
static_assert(frameGuardedBefore+5u == frameSiteBytes.size());
// ID 105674's first 16 bytes: a detour of the chain itself is refused.
constexpr std::array<std::uint8_t,16> imageChainPrologue{
    0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x20,0x44};
// The pass draws for this destination only (normal play).
constexpr std::uintptr_t playDestination = 0, chainSource = 1;
// Code whose operands are the offsets and indices the pass relies on. Each is
// compared byte for byte at install; any difference refuses (FCS-93 check 10+i).
struct Evidence { const char* what; std::uintptr_t rva; const std::uint8_t* bytes; std::size_t size; };
// ID 107131 (RVA 1537F60), called by ID 36559 before the main pass: render target slot 0 = target 1,
// depth target = the index ID 77457 returns, view 0.
constexpr std::array<std::uint8_t,91> mainTargetsBytes{
    0x40,0x53,0x48,0x83,0xec,0x30,0x45,0x33,0xc9,0xc6,0x44,0x24,0x20,0x01,0x33,0xd2,0x48,0x8d,0x0d,0x89,0xd6,0xdf,0x01,0x45,
    0x8d,0x41,0x01,0xe8,0x30,0xd6,0xad,0xff,0x33,0xc0,0x48,0x8d,0x0d,0x77,0xd6,0xdf,0x01,0x38,0x05,0x59,0xf3,0xb9,0x00,0xbb,
    0x03,0x00,0x00,0x00,0x0f,0x45,0xd8,0xe8,0xf4,0xd7,0xad,0xff,0x8b,0xd0,0xc6,0x44,0x24,0x20,0x00,0x45,0x33,0xc9,0x48,0x8d,
    0x0d,0x53,0xd6,0xdf,0x01,0x44,0x8b,0xc3,0xe8,0xbb,0xd6,0xad,0xff,0x48,0x83,0xc4,0x30,0x5b,0xc3};
// ID 77457 (RVA 1015790): xor eax,eax; ret. The main depth target is index 0.
constexpr std::array<std::uint8_t,3> mainDepthIndexBytes{0x33,0xc0,0xc3};
// State flush ID 77386: render target views at renderer+0x10+0xA58+index*0x30 (1010EA0..1010EB7).
constexpr std::array<std::uint8_t,24> flushTargetBytes{
    0x48,0x63,0x43,0xd0,0x83,0xf8,0xff,0x74,0x4a,0x48,0x8d,0x0c,0x40,0x48,0x03,0xc9,0x49,0x8b,0x94,0xc8,0x58,0x0a,0x00,0x00};
// ID 77253 (RVA 100A590), called with the renderer: texture at +0xA58+index*0x30, its render target view at +0xA68.
constexpr std::array<std::uint8_t,47> createTargetBytes{
    0x48,0x8b,0x0d,0x5a,0x5b,0x32,0x02,0x48,0x8b,0x01,0x4d,0x8d,0x8e,0x58,0x0a,0x00,0x00,0xff,0x50,0x28,0x48,0x8b,0x0d,0x46,
    0x5b,0x32,0x02,0x4d,0x8d,0x8e,0x68,0x0a,0x00,0x00,0x49,0x8b,0x96,0x58,0x0a,0x00,0x00,0x45,0x33,0xc0,0x48,0x8b,0x01};
// The depth-target creator at RVA 100A980 (reached from 101548E with the renderer): the shader-resource
// view of the depth texture (+0x2018+index*0x98) goes to +0x20A0+index*0x98 (100AB39..100AB57).
constexpr std::array<std::uint8_t,31> createDepthViewBytes{
    0x48,0x8b,0x0d,0x50,0x56,0x32,0x02,0x4d,0x8d,0x8d,0xa0,0x20,0x00,0x00,0x49,0x8b,0x95,0x18,0x20,0x00,0x00,0x4c,0x8d,0x45,
    0xb7,0x48,0x8b,0x01,0xff,0x50,0x38};
// ID 77343 (RVA 100F8B0): the engine binds that view from +0x20A0+index*0x98 as a compute-shader resource
// (state +0x240, flushed with CSSetShaderResources at 1011497).
constexpr std::array<std::uint8_t,18> bindDepthViewBytes{
    0x49,0x63,0xc0,0x4c,0x69,0xc0,0x98,0x00,0x00,0x00,0x49,0x8b,0x84,0x08,0xa0,0x20,0x00,0x00};
// Per-frame upload ID 77258: rows 8..11 of the per-frame constant buffer (the view-projection every
// world vertex shader multiplies with) are the transposed matrix at state+0x430 (100AF47..100AFA1).
constexpr std::array<std::uint8_t,91> viewProjectionBytes{
    0x0f,0x28,0x15,0x02,0x51,0x0c,0x01,0x0f,0x29,0x45,0xa0,0x0f,0x28,0xc2,0x0f,0xc6,0x05,0x03,0x51,0x0c,0x01,0x44,0x0f,0xc6,
    0x15,0xfb,0x50,0x0c,0x01,0xee,0x0f,0x29,0x5d,0x90,0x0f,0x29,0x64,0x24,0x70,0x0f,0x28,0x25,0xbb,0x50,0x0c,0x01,0x0f,0x28,
    0xdc,0x0f,0x29,0x4d,0x80,0x0f,0xc6,0x1d,0xbc,0x50,0x0c,0x01,0x44,0x0f,0xc6,0x25,0xb4,0x50,0x0c,0x01,0xee,0x0f,0x28,0xcb,
    0x0f,0xc6,0xd8,0xdd,0x0f,0xc6,0xc8,0x88,0x0f,0x28,0xc4,0x0f,0xc6,0xc2,0x88,0x0f,0xc6,0xe2,0xdd};
// State flush: the viewport is state+0x70 (10110DD..10110F8) and its depth range comes from state+0x5C0/+0x5C4
// (101106C..1011086).
constexpr std::array<std::uint8_t,28> viewportBytes{
    0x48,0x8b,0x0d,0x4c,0x0e,0x32,0x02,0x4c,0x8d,0x05,0x85,0xeb,0x0b,0x01,0xba,0x01,0x00,0x00,0x00,0x48,0x8b,0x01,0xff,0x90,
    0x60,0x01,0x00,0x00};
// The destination's way down (see above): ID 36558 `mov esi,edx` (656D32) and `mov edx,esi; mov rcx,rbp;
// call 656E60` (656E28); ID 36559's entry `mov rax,rsp; mov [rax+0x10],edx; ...; lea rbp,[rax-0x5F]`
// (656E60..656E74) and `mov ecx,[rbp+0x6F]; call 153AC00` (6578CE); ID 107148 `mov ebp,ecx` (153AC24).
constexpr std::array<std::uint8_t,2> destinationKeptBytes{0x8b,0xf2};
constexpr std::array<std::uint8_t,10> destinationPassedBytes{0x8b,0xd6,0x48,0x8b,0xcd,0xe8,0x2e,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,21> destinationStoredBytes{
    0x48,0x8b,0xc4,0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa1};
constexpr std::array<std::uint8_t,8> destinationLoadedBytes{0x8b,0x4d,0x6f,0xe8,0x2a,0x33,0xee,0x00};
constexpr std::array<std::uint8_t,2> destinationHeldBytes{0x8b,0xe9};
constexpr std::array<std::uint8_t,27> depthRangeBytes{
    0xf3,0x0f,0x10,0x15,0x4c,0xf1,0x0b,0x01,0xf3,0x0f,0x10,0x05,0x04,0xec,0x0b,0x01,0x0f,0x2e,0xc2,0xf3,0x0f,0x10,0x0d,0x3d,
    0xf1,0x0b,0x01};
constexpr std::array<Evidence,14> frameEvidence{{
    {"main targets (ID 107131)",0x1537f60,mainTargetsBytes.data(),mainTargetsBytes.size()},
    {"main depth index (ID 77457)",0x1015790,mainDepthIndexBytes.data(),mainDepthIndexBytes.size()},
    {"target views (ID 77386)",0x1010ea0,flushTargetBytes.data(),flushTargetBytes.size()},
    {"target creation (ID 77253)",0x100a62f,createTargetBytes.data(),createTargetBytes.size()},
    {"depth view creation",0x100ab39,createDepthViewBytes.data(),createDepthViewBytes.size()},
    {"depth view bind (ID 77343)",0x100f8b0,bindDepthViewBytes.data(),bindDepthViewBytes.size()},
    {"view-projection rows (ID 77258)",0x100af47,viewProjectionBytes.data(),viewProjectionBytes.size()},
    {"viewport (ID 77386)",0x10110dd,viewportBytes.data(),viewportBytes.size()},
    {"viewport depth range (ID 77386)",0x101106c,depthRangeBytes.data(),depthRangeBytes.size()},
    {"destination kept (ID 36558)",0x656d32,destinationKeptBytes.data(),destinationKeptBytes.size()},
    {"destination passed (ID 36558)",0x656e28,destinationPassedBytes.data(),destinationPassedBytes.size()},
    {"destination stored (ID 36559)",0x656e60,destinationStoredBytes.data(),destinationStoredBytes.size()},
    {"destination loaded (ID 36559)",0x6578ce,destinationLoadedBytes.data(),destinationLoadedBytes.size()},
    {"destination held (ID 107148)",0x153ac24,destinationHeldBytes.data(),destinationHeldBytes.size()}}};
namespace frame_offset {
// BSGraphics::Renderer (ID 411393): RenderTargetData[index] at +0xA58, 0x30 each: texture +0x00,
// render target view +0x10; DepthStencilData[index] at +0x2018, 0x98 each: texture +0x00, shader
// resource view of the depth +0x88. Main colour target index 1, main depth index 0.
constexpr std::size_t targets = 0xa58, targetStride = 0x30, targetView = 0x10;
constexpr std::size_t depths = 0x2018, depthStride = 0x98, depthResource = 0x88;
constexpr std::size_t mainTarget = 1, mainDepth = 0;
constexpr std::size_t mainTexture = targets+mainTarget*targetStride;                 // 0xA88
constexpr std::size_t mainTargetView = targets+mainTarget*targetStride+targetView;   // 0xA98
constexpr std::size_t depthTexture = depths+mainDepth*depthStride;                   // 0x2018
constexpr std::size_t depthView = depths+mainDepth*depthStride+depthResource;        // 0x20A0
// RendererShadowState (ID 388819): the viewport (D3D11_VIEWPORT) and the view-projection of the view being drawn.
constexpr std::size_t viewport = 0x70, viewProjection = 0x430, depthRange = 0x5c0;
}
static_assert(frame_offset::mainTexture == 0xa88 && frame_offset::mainTargetView == 0xa98);
static_assert(frame_offset::depthTexture == 0x2018 && frame_offset::depthView == 0x20a0);
// COM vtable byte offsets: the D3D11 interface order (CommonLibSSE-NG 7.2.0 REX/W32/D3D11.h, the
// same order as the Windows SDK's d3d11.h). Where the engine makes the same call through its own
// device or context global, the RVA of one such call is given; the Get* methods, CreateBlendState,
// CreateRasterizerState and Texture2D::GetDesc rest on the ABI order alone (the engine's own calls at
// 0x58, 0x70, 0x108, 0x160, 0x190, 0x1A8 and 0x370 confirm that order from both ends of the table).
namespace frame_method {
constexpr std::size_t release = 0x10;                                                          // IUnknown
constexpr std::size_t createPixelShader = 0x78;                                                // ID3D11Device (100FFC1)
constexpr std::size_t createBlendState = 0xa0, createRasterizerState = 0xb0;                    // ID3D11Device (ABI)
constexpr std::size_t textureDescription = 0x50;                                               // ID3D11Texture2D::GetDesc (ABI)
constexpr std::size_t psSetShaderResources = 0x40, psSetShader = 0x48, draw = 0x68;            // context (10113F8, 10102B1 jmp, 100963F)
constexpr std::size_t psSetConstantBuffers = 0x80, iaSetInputLayout = 0x88;                    // (1009EBB, 100CB7C)
constexpr std::size_t iaSetPrimitiveTopology = 0xc0, omSetRenderTargets = 0x108;               // (10112DC, 1010FC0)
constexpr std::size_t omSetBlendState = 0x118, rsSetState = 0x158, rsSetViewports = 0x160;    // (101114C, 101105A, 10110F3)
constexpr std::size_t psGetShaderResources = 0x248, psGetShader = 0x250, vsGetShader = 0x260, psGetConstantBuffers = 0x268; // (ABI)
constexpr std::size_t iaGetInputLayout = 0x270, iaGetPrimitiveTopology = 0x298, omGetRenderTargets = 0x2c8;                 // (ABI)
constexpr std::size_t omGetBlendState = 0x2d8, rsGetState = 0x2f0, rsGetViewports = 0x2f8;                                  // (ABI)
constexpr std::size_t vsGetShaderResources = 0x2a0, csGetShaderResources = 0x348, csGetUnorderedAccessViews = 0x350;        // (ABI)
constexpr std::size_t viewResource = 0x38;                                                     // ID3D11View::GetResource (ABI)
}
// ---- D3D11 structures, as the ABI lays them out ------------------------------
struct Viewport { float x{},y{},width{},height{},minDepth{},maxDepth{}; };
static_assert(sizeof(Viewport) == 24);
struct TextureDescription {
    std::uint32_t width{},height{},mips{},arraySize{},format{},samples{},quality{},usage{},bind{},access{},misc{};
};
static_assert(sizeof(TextureDescription) == 44);
struct TargetBlend { std::uint32_t enable{},source{},destination{},operation{},sourceAlpha{},destinationAlpha{},operationAlpha{}; std::uint8_t mask{},padding[3]{}; };
struct BlendDescription { std::uint32_t alphaToCoverage{},independent{}; std::array<TargetBlend,8> targets{}; };
static_assert(sizeof(TargetBlend) == 32 && sizeof(BlendDescription) == 264);
struct RasterizerDescription {
    std::uint32_t fill{},cull{},frontCounterClockwise{}; std::int32_t depthBias{}; float depthBiasClamp{},slopeScaledDepthBias{};
    std::uint32_t depthClip{},scissor{},multisample{},antialiasedLines{};
};
static_assert(sizeof(RasterizerDescription) == 40);
// Slot 0 only, the rest disabled: blend SRC_ALPHA (5) / INV_SRC_ALPHA (6), ADD (1); alpha ZERO (1) / ONE (2)
// and never written (mask RGB): the target's alpha keeps whatever the scene put there.
constexpr BlendDescription frameBlend() {
    BlendDescription d{};
    for (auto& t : d.targets) { t.source = 2; t.destination = 1; t.operation = 1; t.sourceAlpha = 2; t.destinationAlpha = 1; t.operationAlpha = 1; t.mask = 0xf; }
    d.targets[0] = {1u,5u,6u,1u,1u,2u,1u,0x7,{}};
    return d;
}
// SOLID (3), no culling (1), depth clip on, no scissor, no multisample.
constexpr RasterizerDescription frameRasterizer() { return {3u,1u,0u,0,0.0f,0.0f,1u,0u,0u,0u}; }
constexpr std::uint32_t triangleList = 4, pixelSlots = 16, viewportSlots = 16, renderTargetSlots = 8;
// Other stages that could hold a view of the main colour texture: the engine's shadow state has 16
// compute-shader resources (state +0x240, flushed with CSSetShaderResources at 1011497) and 8 compute
// UAVs (state +0x300, CSSetUnorderedAccessViews at 101134C); it binds no vertex-shader resource through
// its context (no VSSetShaderResources call). Binding the colour target would make the runtime unbind
// such a view behind the engine's back, so a frame with one is skipped (FrameSkip::shared).
constexpr std::uint32_t computeResourceSlots = 16, computeViewSlots = 8, vertexResourceSlots = 16;
constexpr std::uint32_t bindShaderResource = 0x8;

// ---- the frame's constants -------------------------------------------------
// The matrix the engine uploads as rows 8..11 of the per-frame buffer, transposed: every world vertex
// shader computes clip = p M with p the world position relative to the position adjustment (a row vector).
struct Matrix { std::array<float,16> m{}; };
static_assert(sizeof(Matrix) == 64);
// Everything the pass needs about the main view, read from the render state during a Lighting or Utility
// pass of the published view in the same render (fixed_spike_shader.h consider), never at the insertion
// point: whatever another view leaves in the state after the main pass cannot reach the reconstruction.
struct FrameCapture {
    Matrix viewProjection{};
    Vec posAdjust{};
    Viewport viewport{};
    float depthMin{},depthMax{};
    unsigned long long viewFrame{}; // the published snapshot the geometry was cut with
};
// Constant buffer b0 of the pixel shader (shaders/fixed_spike_frame_ps.hlsl, cbuffer FramePass).
struct FrameConstants {
    std::array<std::array<float,4>,4> inverseColumn{}; // clip -> world relative to the position adjustment: p_j = dot(clip, column j)
    std::array<float,4> viewport{};   // x, y, 2/width, 2/height
    std::array<float,4> depthRange{}; // min, 1/(max-min), void threshold, unused
    std::array<float,4> cut{};        // feet x, feet y, plane z, band floor z (all relative to the position adjustment)
    std::array<float,4> shape{};      // band on, inner radius, outer radius, falloff on
    std::array<float,4> guard{};      // player guard inner, outer, vignette strength, vignette start
    std::array<float,4> voidColor{};  // r, g, b, 1
    // The door void (iDoorVoid=1): zero, and unread by the shader, unless doors are drawn.
    std::array<float,4> doorShape{};  // doors drawn (0..maxDoorVoids), soft edge, depth behind the plane, unused
    std::array<std::array<float,4>,2*maxDoorVoids> door{}; // per door: plane point x, y, normal x, y; lane low z, high z, half width, start
};
static_assert(sizeof(FrameConstants) == 304 && sizeof(FrameConstants)%16 == 0);
static_assert(offsetof(FrameConstants,doorShape) == 160 && offsetof(FrameConstants,door) == 176);
// Depth at or above this is the void: the engine's own "no geometry" test in its image-space fog
// composite (shader entries 16002/16004: lt depth, 0.999999); depth is cleared to 1.0 (1010F8C, 100A534).
constexpr float voidDepth = 9.99998987e-01f;
// Around the player's feet the band is faded out (ground distance 48 -> 96): the player is never darkened
// by it, whatever the plane's lag on stairs. Other actors are protected by the headroom.
constexpr float guardInner = 48.0f, guardOuter = 96.0f, vignetteStart = .35f;
enum class BandState : unsigned { off, on, kit, tilt, count };
constexpr std::array bandNames{"off","on","kit","tilt"};
static_assert(bandNames.size() == static_cast<std::size_t>(BandState::count));
// The band's floor: N under the plane, but never below the held feet + headroom, so an actor standing on
// the player's floor keeps its head out of it. In a kit cell with Stage 1 (iCutStyle=1, cell=kit) back
// walls stand above the plane on purpose and are not cut: the band would darken them, so it is off.
// Stage 3.2: in a cell drawn with the tilted cut (iCutTilt=1, cell=flat) the band is off as well (tilt):
// the band is one level slab under the flat plane, and behind the player the kept wall now rises above
// that plane, so a level band would darken the foot of every kept far wall. The radial falloff and the
// vignette are unchanged.
inline BandState bandState(const FrameConfig& c,float height,float planeZ,bool kitCell,float* floor,bool tiltCell = false) {
    const float held = planeZ-height;
    const float low = (std::max)(planeZ-c.band,held+c.headroom);
    if (floor) *floor = low;
    if (c.band <= 0.0f || !std::isfinite(low)) return BandState::off;
    if (kitCell) return BandState::kit;
    if (tiltCell) return BandState::tilt;
    return low < planeZ-1.0f ? BandState::on : BandState::off;
}
constexpr bool falloffOn(const FrameConfig& c) { return c.radiusOuter > c.radiusInner; }
// A 4x4 inverse in double (cofactors); none when the matrix is singular or not finite.
inline std::optional<std::array<double,16>> invert(const Matrix& in) {
    std::array<double,16> a{},inv{};
    for (std::size_t i = 0;i < 16;++i) { a[i] = static_cast<double>(in.m[i]); if (!std::isfinite(a[i])) return {}; }
    inv[0] = a[5]*a[10]*a[15]-a[5]*a[11]*a[14]-a[9]*a[6]*a[15]+a[9]*a[7]*a[14]+a[13]*a[6]*a[11]-a[13]*a[7]*a[10];
    inv[4] = -a[4]*a[10]*a[15]+a[4]*a[11]*a[14]+a[8]*a[6]*a[15]-a[8]*a[7]*a[14]-a[12]*a[6]*a[11]+a[12]*a[7]*a[10];
    inv[8] = a[4]*a[9]*a[15]-a[4]*a[11]*a[13]-a[8]*a[5]*a[15]+a[8]*a[7]*a[13]+a[12]*a[5]*a[11]-a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14]+a[4]*a[10]*a[13]+a[8]*a[5]*a[14]-a[8]*a[6]*a[13]-a[12]*a[5]*a[10]+a[12]*a[6]*a[9];
    inv[1] = -a[1]*a[10]*a[15]+a[1]*a[11]*a[14]+a[9]*a[2]*a[15]-a[9]*a[3]*a[14]-a[13]*a[2]*a[11]+a[13]*a[3]*a[10];
    inv[5] = a[0]*a[10]*a[15]-a[0]*a[11]*a[14]-a[8]*a[2]*a[15]+a[8]*a[3]*a[14]+a[12]*a[2]*a[11]-a[12]*a[3]*a[10];
    inv[9] = -a[0]*a[9]*a[15]+a[0]*a[11]*a[13]+a[8]*a[1]*a[15]-a[8]*a[3]*a[13]-a[12]*a[1]*a[11]+a[12]*a[3]*a[9];
    inv[13] = a[0]*a[9]*a[14]-a[0]*a[10]*a[13]-a[8]*a[1]*a[14]+a[8]*a[2]*a[13]+a[12]*a[1]*a[10]-a[12]*a[2]*a[9];
    inv[2] = a[1]*a[6]*a[15]-a[1]*a[7]*a[14]-a[5]*a[2]*a[15]+a[5]*a[3]*a[14]+a[13]*a[2]*a[7]-a[13]*a[3]*a[6];
    inv[6] = -a[0]*a[6]*a[15]+a[0]*a[7]*a[14]+a[4]*a[2]*a[15]-a[4]*a[3]*a[14]-a[12]*a[2]*a[7]+a[12]*a[3]*a[6];
    inv[10] = a[0]*a[5]*a[15]-a[0]*a[7]*a[13]-a[4]*a[1]*a[15]+a[4]*a[3]*a[13]+a[12]*a[1]*a[7]-a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14]+a[0]*a[6]*a[13]+a[4]*a[1]*a[14]-a[4]*a[2]*a[13]-a[12]*a[1]*a[6]+a[12]*a[2]*a[5];
    inv[3] = -a[1]*a[6]*a[11]+a[1]*a[7]*a[10]+a[5]*a[2]*a[11]-a[5]*a[3]*a[10]-a[9]*a[2]*a[7]+a[9]*a[3]*a[6];
    inv[7] = a[0]*a[6]*a[11]-a[0]*a[7]*a[10]-a[4]*a[2]*a[11]+a[4]*a[3]*a[10]+a[8]*a[2]*a[7]-a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11]+a[0]*a[7]*a[9]+a[4]*a[1]*a[11]-a[4]*a[3]*a[9]-a[8]*a[1]*a[7]+a[8]*a[3]*a[5];
    inv[15] = a[0]*a[5]*a[10]-a[0]*a[6]*a[9]-a[4]*a[1]*a[10]+a[4]*a[2]*a[9]+a[8]*a[1]*a[6]-a[8]*a[2]*a[5];
    const double det = a[0]*inv[0]+a[1]*inv[4]+a[2]*inv[8]+a[3]*inv[12];
    if (!std::isfinite(det) || std::abs(det) < 1e-30) return {};
    for (auto& value : inv) { value /= det; if (!std::isfinite(value)) return {}; }
    return inv;
}
// The constants for one frame, or none when the capture cannot be used (singular matrix, empty
// viewport, empty depth range, a non-finite snapshot). `band` reports what the band does this frame.
inline std::optional<FrameConstants> frameConstants(const FrameConfig& c,float height,const View& view,const FrameCapture& capture,Color color,
    bool kitCell,BandState& band,float& bandFloor,bool tiltCell = false) {
    band = BandState::off; bandFloor = std::numeric_limits<float>::quiet_NaN();
    const auto inverse = invert(capture.viewProjection);
    const Viewport& v = capture.viewport;
    if (!inverse || !finite(capture.posAdjust) || !finite(view.feet) || !std::isfinite(view.planeZ) || !(v.width > 0.0f) || !(v.height > 0.0f) ||
        !std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.width) || !std::isfinite(v.height) ||
        !std::isfinite(capture.depthMin) || !std::isfinite(capture.depthMax) || !(capture.depthMax > capture.depthMin)) return {};
    FrameConstants k;
    for (std::size_t j = 0;j < 4;++j)
        for (std::size_t i = 0;i < 4;++i) k.inverseColumn[j][i] = static_cast<float>((*inverse)[i*4u+j]);
    k.viewport = {v.x,v.y,2.0f/v.width,2.0f/v.height};
    k.depthRange = {capture.depthMin,1.0f/(capture.depthMax-capture.depthMin),voidDepth,0.0f};
    band = bandState(c,height,view.planeZ,kitCell,&bandFloor,tiltCell);
    const float plane = view.planeZ-capture.posAdjust.z;
    k.cut = {view.feet.x-capture.posAdjust.x,view.feet.y-capture.posAdjust.y,plane,band == BandState::on ? bandFloor-capture.posAdjust.z : plane};
    k.shape = {band == BandState::on ? 1.0f : 0.0f,c.radiusInner,c.radiusOuter,falloffOn(c) ? 1.0f : 0.0f};
    k.guard = {guardInner,guardOuter,c.vignette,vignetteStart};
    k.voidColor = {color.red,color.green,color.blue,1.0f};
    return k;
}
// The door void's rows for the doors chosen this frame (selectDoors), relative to the position adjustment
// like the cut. Not called with iDoorVoid=0: the rows stay zero and the shader skips them.
inline void applyDoors(FrameConstants& k,const DoorSelection& s,Vec posAdjust,float edge,float depth) {
    const std::size_t count = (std::min)(s.count,maxDoorVoids);
    k.doorShape = {static_cast<float>(count),edge,depth,0.0f};
    for (std::size_t i = 0;i < count;++i) {
        const DoorChoice& c = s.chosen[i];
        const DoorPlane& p = c.door.plane;
        k.door[2*i] = {p.point.x-posAdjust.x,p.point.y-posAdjust.y,p.nx,p.ny};
        k.door[2*i+1] = {p.low-posAdjust.z,p.high-posAdjust.z,p.half,c.start};
    }
}
// ---- a C++ copy of the pixel shader (shaders/fixed_spike_frame_ps.hlsl), for the portable tests -------
// It follows the HLSL statement for statement; the reviewer compares the two by reading.
constexpr float smooth(float edge0,float edge1,float x) {
    const float t = std::clamp((x-edge0)/(edge1-edge0),0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
// The door void: behind the plane past `start` (soft over the edge) and short of the depth, inside the lane
// (soft over the edge beyond its half width) and between its heights (soft over the edge outside them).
inline float doorFade(const FrameConstants& k,Vec world) {
    float fade = 0.0f;
    for (std::size_t i = 0;i < maxDoorVoids;++i) {
        if (static_cast<float>(i) < k.doorShape[0]) {
            const std::array<float,4>& a = k.door[2*i];
            const std::array<float,4>& b = k.door[2*i+1];
            const float dx = world.x-a[0],dy = world.y-a[1];
            const float back = -(dx*a[2]+dy*a[3]);
            const float across = std::abs(dx*a[3]-dy*a[2]);
            const float edge = k.doorShape[1],depth = k.doorShape[2];
            const float lane = smooth(b[3],b[3]+edge,back)*(1.0f-smooth(depth,depth+edge,back))*(1.0f-smooth(b[2],b[2]+edge,across))*
                smooth(b[0]-edge,b[0],world.z)*(1.0f-smooth(b[1],b[1]+edge,world.z));
            fade = (std::max)(fade,lane);
        }
    }
    return fade;
}
struct FramePixel { float amount{}; bool voidPixel{}; Vec world{}; };
inline FramePixel framePixel(const FrameConstants& k,float px,float py,float depth) {
    FramePixel out;
    const float ndcX = (px-k.viewport[0])*k.viewport[2]-1.0f,ndcY = 1.0f-(py-k.viewport[1])*k.viewport[3];
    float amount = 1.0f;
    out.voidPixel = !(depth < k.depthRange[2]);
    if (!out.voidPixel) {
        const std::array<float,4> clip{ndcX,ndcY,(depth-k.depthRange[0])*k.depthRange[1],1.0f};
        std::array<float,4> h{};
        for (std::size_t j = 0;j < 4;++j) h[j] = clip[0]*k.inverseColumn[j][0]+clip[1]*k.inverseColumn[j][1]+clip[2]*k.inverseColumn[j][2]+clip[3]*k.inverseColumn[j][3];
        out.world = {h[0]/h[3],h[1]/h[3],h[2]/h[3]};
        const float reach = std::hypot(out.world.x-k.cut[0],out.world.y-k.cut[1]);
        float band = 0.0f;
        if (k.shape[0] > .5f && out.world.z <= k.cut[2]) band = smooth(k.cut[3],k.cut[2],out.world.z)*smooth(k.guard[0],k.guard[1],reach);
        float fall = 0.0f;
        if (k.shape[3] > .5f) fall = smooth(k.shape[1],k.shape[2],reach);
        amount = 1.0f-(1.0f-band)*(1.0f-fall);
        if (k.doorShape[0] > .5f) amount = (std::max)(amount,doorFade(k,out.world));
    }
    const float edge = k.guard[2]*smooth(k.guard[3],1.0f,std::hypot(ndcX,ndcY)*.70710678f);
    amount = 1.0f-(1.0f-amount)*(1.0f-edge);
    out.amount = std::clamp(amount,0.0f,1.0f);
    return out;
}
// What the blend makes of one channel: src*alpha + dst*(1-alpha).
constexpr float blended(float scene,float voidChannel,float alpha) { return voidChannel*alpha+scene*(1.0f-alpha); }

// ---- gate, targets, install checks -------------------------------------------
// One call of the main world render in normal play. The pass draws only when every answer is yes;
// otherwise the frame is counted under the first reason.
enum class FrameSkip : unsigned { none, latched, inactive, stale, capture, targets, matrix, shared, count };
constexpr std::array frameSkipNames{"none","latched","inactive","stale","capture","targets","matrix","shared"};
static_assert(frameSkipNames.size() == static_cast<std::size_t>(FrameSkip::count));
// `active` is the cut's own published flag (an eligible third-person frame in an interior with the cut
// installed and no latch; every lifecycle exit clears it at once). `captured` means a pass of the
// published view was drawn in this render and the snapshot it was cut with is still the published one.
constexpr FrameSkip frameGate(bool latched,bool active,bool freshView,bool captured) {
    return latched ? FrameSkip::latched : !active ? FrameSkip::inactive : !freshView ? FrameSkip::stale : !captured ? FrameSkip::capture : FrameSkip::none;
}
// 0 = the main colour target and the main depth can be drawn to and read as the pass needs; otherwise
// the first check that failed: 1 a pointer is null, 2 the depth is not one 2D single-sample texture with
// a shader-resource binding, 3 the colour target is not one single-sample 2D texture, 4 their sizes
// differ, 5 the main view's viewport does not lie inside them.
inline unsigned targetCheck(bool pointers,const TextureDescription& colour,const TextureDescription& depth,const Viewport& view) {
    if (!pointers) return 1;
    if (depth.arraySize != 1 || depth.samples != 1 || !(depth.bind & bindShaderResource) || !depth.width || !depth.height) return 2;
    if (colour.arraySize != 1 || colour.samples != 1 || !colour.width || !colour.height) return 3;
    if (colour.width != depth.width || colour.height != depth.height) return 4;
    if (!(view.x >= 0.0f) || !(view.y >= 0.0f) || !(view.width > 0.0f) || !(view.height > 0.0f) ||
        view.x+view.width > static_cast<float>(depth.width)+.5f || view.y+view.height > static_cast<float>(depth.height)+.5f) return 5;
    return 0;
}
// 0 = the call site, its callee and every evidence window are the audited ones. Otherwise: 1 ID 107148,
// 2 ID 105674, 3 the renderer ID 411393, 4 the 48 bytes before and of the call, 5 the call does not go to
// ID 105674, 6 ID 105674's first 16 bytes, 10+i evidence window i (frameEvidence).
// `at(rva)` gives the image's bytes at an RVA (the loaded module natively, a test image in the tests).
template<class Image>
unsigned frameSiteCheck(std::uintptr_t base,std::uintptr_t postWorld,std::uintptr_t imageChain,std::uintptr_t renderer,
    const std::uint8_t* window,const std::uint8_t* callee,const Image& at) {
    if (postWorld != base+postWorldAddress.rva) return 1;
    if (imageChain != base+imageChainAddress.rva) return 2;
    if (renderer != base+rendererAddress.rva) return 3;
    if (std::memcmp(window,frameSiteBytes.data(),frameSiteBytes.size()) != 0) return 4;
    if (callTarget(base+frameCallRva,window+frameGuardedBefore) != imageChain) return 5;
    if (std::memcmp(callee,imageChainPrologue.data(),imageChainPrologue.size()) != 0) return 6;
    for (std::size_t i = 0;i < frameEvidence.size();++i)
        if (std::memcmp(at(frameEvidence[i].rva),frameEvidence[i].bytes,frameEvidence[i].size) != 0) return 10u+static_cast<unsigned>(i);
    return 0;
}

// ---- trace ------------------------------------------------------------------
struct FrameWindow {
    unsigned long long calls{},drawn{};
    std::array<unsigned long long,static_cast<std::size_t>(FrameSkip::count)> skips{};
    double seconds{};
    bool haveDrawn{};
    BandState band{};       // at the last drawn frame
    float bandFloor{},planeZ{}; // world heights at the last drawn frame
};
struct FrameSample {
    double seconds{};
    unsigned long long frame{},offMain{},mismatch{};
    Feature state{};
    FrameConfig config{};
    FrameWindow window{};
    TextureDescription colour{},depth{}; // the last targets checked (zero before the first)
};
inline bool formatFrame(char* out,std::size_t size,const FrameSample& s) {
    const auto& w = s.window;
    const auto skip = [&](FrameSkip which) { return w.skips[static_cast<std::size_t>(which)]; };
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const bool banded = w.haveDrawn && w.band == BandState::on;
    const int written = std::snprintf(out,size,
        "FixedShaderFrame t=%.3f frame=%llu pass=%s calls=%llu drawn=%llu skipLatched=%llu skipInactive=%llu skipStale=%llu skipCapture=%llu "
        "skipTargets=%llu skipMatrix=%llu skipShared=%llu offMain=%llu band=%s bandN=%.1f headroom=%.1f bandFrom=%.2f planeZ=%.2f falloff=%s radii=%.1f,%.1f "
        "vignette=%.3f depth=%s target=%ux%u formats=%u,%u mismatch=%llu micros=%.1f sample=window",
        s.seconds,s.frame,featureNames[static_cast<std::size_t>(s.state)],w.calls,w.drawn,skip(FrameSkip::latched),skip(FrameSkip::inactive),
        skip(FrameSkip::stale),skip(FrameSkip::capture),skip(FrameSkip::targets),skip(FrameSkip::matrix),skip(FrameSkip::shared),s.offMain,
        w.haveDrawn ? bandNames[static_cast<std::size_t>(w.band)] : "none",static_cast<double>(s.config.band),static_cast<double>(s.config.headroom),
        banded ? static_cast<double>(w.bandFloor) : none,w.haveDrawn ? static_cast<double>(w.planeZ) : none,
        falloffOn(s.config) ? "on" : "off",static_cast<double>(s.config.radiusInner),static_cast<double>(s.config.radiusOuter),
        static_cast<double>(s.config.vignette),w.drawn ? "read" : "none",s.depth.width,s.depth.height,s.colour.format,s.depth.format,
        s.mismatch,w.seconds*1e6);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
