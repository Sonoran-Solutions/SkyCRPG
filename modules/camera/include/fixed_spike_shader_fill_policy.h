// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike_shader_policy.h (after the frame policy). P5.5-02 wave 2 package
// "shader", Stage 3 step 3.1 (owner go "Start solid caps"): the back-face fill, iCutFill. A Lighting draw
// of the main view that the cut slices at a plane is drawn a second time, right after the engine's own
// draw and before its RestoreGeometry, with the same patched vertex shader and plane but front faces
// culled and a cap pixel shader: the back faces under the plane, which the cut lays open, become a flat
// cap. Caves and ruins only (a cell Stage 1's census calls flat). No engine headers, no engine pointers;
// the native side is src/fixed_spike_shader_fill.h, the audit docs/evidence/p5.5/spike-wave2-shader.md
// ("Stage 3.1: solid caps").

namespace skycrpg::camera::fixed::shader {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
// The flush (ID 77386) binds the depth-stencil state from the table ID 411362 at index
// depthMode(+0x88)*40 + stencilMode(+0x90) with reference +0x94 (1010FD1..1011006), and the rasteriser
// state from the table ID 411363 at index ((fill(+0x98)*3 + cull(+0x9C))*12 + depthBias(+0xA0))*2 +
// scissor(+0xA4) (1011019..1011060). ID 77652 builds the rasteriser table with cull 0 NONE, 1 BACK,
// 2 FRONT and FrontCounterClockwise 1 for every entry (102B6F7, 102B775..102B78D); the depth table's mode 3
// is LESS_EQUAL with write (102B1AD..102B1C1).
constexpr Address depthStencilTableAddress{411362,0x3330230};
constexpr Address rasterizerTableAddress{411363,0x33309b0};
constexpr std::size_t depthModes = 6, stencilModes = 40, fillModes = 2, cullModes = 3, depthBiases = 12, scissorModes = 2;
constexpr std::uint32_t cullBack = 1, cullFront = 2, depthTestWrite = 3, depthEqual = 4;
// The blend table's write-mask index (+0xB0, the third factor of the blend index): 2 jumps straight to the
// creation with the defaults, RGBA on all eight targets (jump table at 102B0DC of the blend builder ID 77649, RVA 102ABD0;
// entry 2 = 102AFFB; the defaults at 102AE60..); the others mask channels or targets.
constexpr std::uint32_t allTargetsMask = 2;
// Batch F1 (shader 12a702d) found filled=0: every Lighting draw in Embershard and Bleak Falls Barrow was rejected
// by the write mask or the alpha-test byte. From the binary:
//  - Write mask 1 = target 0 RGB only, every other target 0xF (the blend builder's entry 1, 102AF87: target 0
//    mask 7). Lighting SetupGeometry sets it (1549689..15496A3) for every pass of pass type 3 whose material has
//    shader flag bit 32 (the bit without which depth mode 1, no write, is forced: 154A738): a depth-writing lit
//    draw that does not write the colour target's alpha. Such a draw is a normal opaque draw; the cap still
//    writes through its own blend state.
//  - The alpha-test byte (+0xB4) is set by the batch renderer ID 107642 per pass group (groups 1, 3, 4:
//    1560123, 15600C5, 156017C), with the reference +0xB8 written into the pixel constant buffer bound at slot 11
//    (101117A..10111C0; bound at 10107E3). The Lighting pixel shaders discard when alpha * material alpha -
//    reference < 0 (e.g. Lighting pixel run index 6212: `mad r0.x, cb2[3].z, r0.w, -cb11[0].x; lt r0.x, r0.x, 0;
//    discard_nz`). With a reference <= 0 nothing is ever discarded, so such a draw is opaque.
// iCutFill=2 admits both; iCutFill=1 keeps the strict gates.
constexpr std::uint32_t colourRgbMask = 1;
constexpr unsigned strictFill = 1, widenedFill = 2;
namespace fill_offset {
// RendererShadowState (ID 388819): the fields the flush reads (above, and blend 10110FF..101114C, alpha test
// 101118D..10111AB).
constexpr std::size_t depthMode = 0x88, stencilMode = 0x90, stencilRef = 0x94, fillMode = 0x98, cullMode = 0x9c, depthBias = 0xa0,
    scissor = 0xa4, blendMode = 0xa8, alphaToCoverage = 0xac, writeMask = 0xb0, alphaTest = 0xb4, alphaRef = 0xb8;
// BSRenderPass: the pass-type byte Lighting SetupGeometry (154965E) and RestoreGeometry (154A900) test.
constexpr std::size_t passType = 0x1c;
// BSGeometry: the type byte ID 107637 switches on (155F05D), and for type 3 the triangle count and the
// renderer data it passes to ID 77263 (155F1D4..155F1F1), which draws DrawIndexed(3*triangles, 0, 0) (100BD9E..100BDAC).
constexpr std::size_t geometryType = 0x150, geometryTriangles = 0x158, geometryData = 0x138;
}
constexpr std::uint8_t triShapeType = 3;
constexpr std::array<std::uint8_t,54> depthTableBytes{
    0x48,0x63,0x05,0xb0,0xec,0x0b,0x01,0x48,0x8b,0x0d,0x51,0x0f,0x32,0x02,0x44,0x8b,0x05,0xae,0xec,0x0b,0x01,0x48,0x8d,0x14,
    0x80,0x48,0x63,0x05,0x9f,0xec,0x0b,0x01,0x4c,0x8b,0x09,0x48,0x8d,0x14,0xd0,0x49,0x8b,0x94,0xd6,0x30,0x02,0x33,0x03,0x41,
    0xff,0x91,0x20,0x01,0x00,0x00};
constexpr std::array<std::uint8_t,72> rasterizerTableBytes{
    0x48,0x63,0x05,0x78,0xec,0x0b,0x01,0x48,0x8b,0x0d,0x09,0x0f,0x32,0x02,0x48,0x8d,0x14,0x40,0x48,0x63,0x05,0x6a,0xec,0x0b,
    0x01,0x4c,0x8b,0x09,0x48,0x03,0xd0,0x48,0x63,0x05,0x61,0xec,0x0b,0x01,0x48,0x8d,0x14,0x52,0x4c,0x8d,0x04,0x90,0x48,0x63,
    0x05,0x56,0xec,0x0b,0x01,0x4a,0x8d,0x14,0x40,0x49,0x8b,0x94,0xd6,0xb0,0x09,0x33,0x03,0x41,0xff,0x91,0x58,0x01,0x00,0x00};
constexpr std::array<std::uint8_t,12> typeSwitchBytes{0x4c,0x8b,0x71,0x10,0x41,0x0f,0xb6,0x86,0x50,0x01,0x00,0x00};
constexpr std::array<std::uint8_t,30> triShapeDrawBytes{
    0x45,0x0f,0xb7,0x8e,0x58,0x01,0x00,0x00,0x48,0x8d,0x0d,0x5d,0x2d,0xdd,0x01,0x49,0x8b,0x96,0x38,0x01,0x00,0x00,0x45,0x33,
    0xc0,0xe8,0xde,0xca,0xaa,0xff};
constexpr std::array<std::uint8_t,22> drawIndexedBytes{
    0x48,0x8b,0x0d,0x92,0x61,0x32,0x02,0x8d,0x14,0x7f,0x45,0x33,0xc9,0x44,0x8b,0xc6,0x48,0x8b,0x01,0xff,0x50,0x60};
constexpr std::array<std::uint8_t,14> counterClockwiseBytes{0xc7,0x45,0xe8,0x01,0x00,0x00,0x00,0xc7,0x45,0xd8,0x01,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,25> cullMapBytes{
    0xc7,0x45,0xd4,0x02,0x00,0x00,0x00,0xeb,0x10,0xc7,0x45,0xd4,0x03,0x00,0x00,0x00,0xeb,0x07,0xc7,0x45,0xd4,0x01,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,21> depthWriteBytes{
    0xc7,0x45,0xc0,0x01,0x00,0x00,0x00,0xc7,0x45,0xc4,0x01,0x00,0x00,0x00,0xc7,0x45,0xc8,0x04,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,4> writeMaskAllBytes{0xfb,0xaf,0x02,0x01};    // 102B0E4: entry 2 -> 102AFFB
constexpr std::array<std::uint8_t,10> writeMaskDefaultBytes{0xc6,0x44,0x24,0x44,0x0f,0xc6,0x44,0x24,0x64,0x0f}; // 102AE60: target 0, 1 masks 0xF
// Batch F1 widening: the write-mask entry 1 (102B0E0 -> 102AF87, target 0 mask 7), the alpha-test buffer bound at
// pixel slot 11 (10107C9..10107E8) and its reference written only when the test is on, else 0 (1011178..10111C2).
constexpr std::array<std::uint8_t,4> writeMaskRgbEntryBytes{0x87,0xaf,0x02,0x01};
constexpr std::array<std::uint8_t,5> writeMaskRgbBytes{0xc6,0x44,0x24,0x44,0x07};
constexpr std::array<std::uint8_t,32> alphaSlotBytes{
    0x4c,0x8d,0x0d,0xd8,0x12,0x32,0x02,0x48,0x8b,0x0d,0x59,0x17,0x32,0x02,0xba,0x0b,0x00,0x00,0x00,0x48,0x8b,0x01,0x44,0x8d,
    0x42,0xf6,0xff,0x90,0x80,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,75> alphaRefBytes{
    0x48,0x8b,0x15,0x29,0x09,0x32,0x02,0x45,0x33,0xc0,0x44,0x89,0x7c,0x24,0x20,0x48,0x8b,0x01,0xff,0x50,0x70,0x44,0x38,0x3d,
    0x20,0xeb,0x0b,0x01,0x74,0x0a,0xf3,0x0f,0x10,0x05,0x1a,0xeb,0x0b,0x01,0xeb,0x03,0x0f,0x57,0xc0,0x48,0x8b,0x44,0x24,0x30,
    0x45,0x33,0xc0,0xf3,0x0f,0x11,0x00,0x48,0x8b,0x0d,0x7a,0x0d,0x32,0x02,0x48,0x8b,0x15,0xeb,0x08,0x32,0x02,0x48,0x8b,0x01,
    0xff,0x50,0x78};
constexpr std::array<Evidence,14> fillEvidence{{
    {"depth-stencil table index (ID 77386)",0x1010fd1,depthTableBytes.data(),depthTableBytes.size()},
    {"rasteriser table index (ID 77386)",0x1011019,rasterizerTableBytes.data(),rasterizerTableBytes.size()},
    {"geometry type switch (ID 107637)",0x155f059,typeSwitchBytes.data(),typeSwitchBytes.size()},
    {"type 3 draw arguments (ID 107637)",0x155f1d4,triShapeDrawBytes.data(),triShapeDrawBytes.size()},
    {"DrawIndexed(3n, start, 0) (ID 77263)",0x100bd97,drawIndexedBytes.data(),drawIndexedBytes.size()},
    {"rasteriser FrontCounterClockwise 1, depth clip 1 (ID 77652)",0x102b6f7,counterClockwiseBytes.data(),counterClockwiseBytes.size()},
    {"rasteriser cull 2 FRONT, 1 BACK, 0 NONE (ID 77652)",0x102b775,cullMapBytes.data(),cullMapBytes.size()},
    {"depth mode 3: test LESS_EQUAL, write (depth table builder)",0x102b1ad,depthWriteBytes.data(),depthWriteBytes.size()},
    {"blend write-mask index 2: the defaults (blend table builder)",0x102b0e4,writeMaskAllBytes.data(),writeMaskAllBytes.size()},
    {"blend defaults: masks 0xF (blend table builder)",0x102ae60,writeMaskDefaultBytes.data(),writeMaskDefaultBytes.size()},
    {"blend write-mask index 1 entry (blend table builder)",0x102b0e0,writeMaskRgbEntryBytes.data(),writeMaskRgbEntryBytes.size()},
    {"blend write-mask index 1: target 0 RGB (blend table builder)",0x102af87,writeMaskRgbBytes.data(),writeMaskRgbBytes.size()},
    {"alpha-test buffer at pixel slot 11",0x10107c9,alphaSlotBytes.data(),alphaSlotBytes.size()},
    {"alpha-test reference, 0 when off (ID 77386)",0x1011178,alphaRefBytes.data(),alphaRefBytes.size()}}};
// 0 = both tables' IDs resolve to the audited RVAs and every evidence window is the audited code. Otherwise
// 1 the depth-stencil table ID, 2 the rasteriser table ID, 10+i evidence window i.
template<class Image>
unsigned fillSiteCheck(std::uintptr_t base,std::uintptr_t depthTable,std::uintptr_t rasterizerTable,const Image& at) {
    if (depthTable != base+depthStencilTableAddress.rva) return 1;
    if (rasterizerTable != base+rasterizerTableAddress.rva) return 2;
    for (std::size_t i = 0;i < fillEvidence.size();++i)
        if (std::memcmp(at(fillEvidence[i].rva),fillEvidence[i].bytes,fillEvidence[i].size) != 0) return 10u+static_cast<unsigned>(i);
    return 0;
}
// The engine's own rasteriser entry for the draw's fill, bias and scissor with front faces culled, and its
// depth-stencil entry for test-and-write with the draw's stencil mode; none when an index is out of range.
constexpr std::optional<std::size_t> frontCullIndex(std::uint32_t fill,std::uint32_t bias,std::uint32_t scissor) {
    if (fill >= fillModes || bias >= depthBiases || scissor >= scissorModes) return {};
    return ((static_cast<std::size_t>(fill)*cullModes+cullFront)*depthBiases+bias)*scissorModes+scissor;
}
constexpr std::optional<std::size_t> depthWriteIndex(std::uint32_t stencil) {
    if (stencil >= stencilModes) return {};
    return static_cast<std::size_t>(depthTestWrite)*stencilModes+stencil;
}

// ---- the gate ---------------------------------------------------------------
// Every pass the cut bracketed with a plane (not a whole removal) in the main view reaches the gate once,
// at its RestoreGeometry; the first failing check is counted. `none` = filled.
enum class FillSkip : unsigned { none, shaderClass, cell, actor, type, cull, blend, alpha, depth, mask, budget, layout, guard, count };
constexpr std::array fillSkipNames{"none","class","cell","actor","type","cull","blend","alpha","depth","mask","budget","layout","guard"};
static_assert(fillSkipNames.size() == static_cast<std::size_t>(FillSkip::count));
struct FillFacts {
    bool lighting{};      // Lighting class (the Utility depth pass and Effect are never filled)
    bool flatCell{};      // Stage 1 is on and its census calls the cell flat (a cave or a ruin): never a kit building
    bool skinned{};       // skinned geometry (actors and what they carry are never bracketed; checked again)
    std::uint8_t type{};  // geometry type byte
    std::uint32_t cull{},blend{},alphaToCoverage{},depth{},writeMask{allTargetsMask};
    std::uint8_t alphaTest{};
    float alphaRef{};     // +0xB8: the alpha-test reference
    unsigned mode{strictFill}; // iCutFill
    bool budget{true};    // the per-frame fill budget has room
    bool layout{true};    // triangles > 0, renderer data present, table entries in range and present
};
constexpr FillSkip fillGate(const FillFacts& f) {
    if (!f.lighting) return FillSkip::shaderClass;
    if (!f.flatCell) return FillSkip::cell;
    if (f.skinned) return FillSkip::actor;
    if (f.type != triShapeType) return FillSkip::type;
    if (f.cull != cullBack) return FillSkip::cull;        // two-sided materials (cull none) would cover their own lit back faces
    if (f.blend != 0 || f.alphaToCoverage != 0) return FillSkip::blend;
    // Cards, foliage, cobwebs: the cap has no alpha. Widened: a test whose reference cannot discard anything is no test.
    if (f.alphaTest != 0 && !(f.mode == widenedFill && f.alphaRef <= 0.0f)) return FillSkip::alpha;
    if (f.depth != depthTestWrite && f.depth != depthEqual) return FillSkip::depth; // decals and transparent draws write no depth
    // A draw that writes only some channels or targets; widened: also target 0 RGB with every other target written.
    if (f.writeMask != allTargetsMask && !(f.mode == widenedFill && f.writeMask == colourRgbMask)) return FillSkip::mask;
    if (!f.budget) return FillSkip::budget;
    if (!f.layout) return FillSkip::layout;
    return FillSkip::none;
}
constexpr unsigned fillPerFrame = 2048;

// ---- the cap's constants and the player guard ---------------------------------
// Constant buffer b13 of the cap pixel shader (shaders/fixed_spike_cut_fill_ps.hlsl, cbuffer CutFill).
struct FillConstants {
    std::array<float,4> colour{};     // r, g, b, 1
    std::array<float,4> guardRect{};  // x0, y0, x1, y1 in pixels
    std::array<float,4> guardDepth{}; // x: farthest depth of the player's box, y: 1 when the guard is on
};
static_assert(sizeof(FillConstants) == 48);
constexpr unsigned fillBuffer = 13; // pixel constant-buffer slot: no shipped pixel shader declares cb13 (archive census)
// The guard: the screen rectangle of a box around the player (feet +-48 in x and y, feet to feet + 144),
// padded by 16 pixels, and the farthest depth of its corners. A cap pixel inside the ellipse of that
// rectangle and at or in front of that depth is discarded, so a cap (a low cave ceiling, the outside of a
// near wall) never covers the player; behind the player it is drawn. None when a corner lies behind the eye.
constexpr float guardHalfWidth = 48.0f,guardHeight = 144.0f,guardPad = 16.0f;
struct Guard { bool on{}; float x0{},y0{},x1{},y1{},depth{}; };
inline Guard playerGuard(const Matrix& viewProjection,Vec posAdjust,const Viewport& v,Vec feet) {
    Guard g;
    if (!finite(feet) || !finite(posAdjust) || !(v.width > 0.0f) || !(v.height > 0.0f)) return g;
    float x0 = std::numeric_limits<float>::infinity(),y0 = x0,x1 = -x0,y1 = -x0,deepest = -x0;
    for (unsigned i = 0;i < 8;++i) {
        const float p[4]{feet.x-posAdjust.x+((i&1u) ? guardHalfWidth : -guardHalfWidth),feet.y-posAdjust.y+((i&2u) ? guardHalfWidth : -guardHalfWidth),
            feet.z-posAdjust.z+((i&4u) ? guardHeight : 0.0f),1.0f};
        float clip[4]{};
        for (std::size_t j = 0;j < 4;++j)
            for (std::size_t k = 0;k < 4;++k) clip[j] += p[k]*viewProjection.m[k*4u+j];
        if (!(clip[3] > 1e-3f) || !std::isfinite(clip[0]) || !std::isfinite(clip[1]) || !std::isfinite(clip[2])) return {};
        const float nx = clip[0]/clip[3],ny = clip[1]/clip[3],nz = clip[2]/clip[3];
        const float px = v.x+(nx+1.0f)*.5f*v.width,py = v.y+(1.0f-ny)*.5f*v.height,depth = v.minDepth+nz*(v.maxDepth-v.minDepth);
        x0 = (std::min)(x0,px); x1 = (std::max)(x1,px); y0 = (std::min)(y0,py); y1 = (std::max)(y1,py); deepest = (std::max)(deepest,depth);
    }
    g.on = true; g.x0 = x0-guardPad; g.y0 = y0-guardPad; g.x1 = x1+guardPad; g.y1 = y1+guardPad; g.depth = deepest;
    return g;
}
// Informative (the `guarded` counter): the draw's bound sphere meets the guard box around the player, so the
// guard may be what keeps part of this cap off the player.
inline bool boundMeetsPlayer(Bound b,Vec feet) {
    if (!finite(b.centre) || !std::isfinite(b.radius) || !finite(feet)) return false;
    const float dx = (std::max)(std::abs(b.centre.x-feet.x)-guardHalfWidth,0.0f),dy = (std::max)(std::abs(b.centre.y-feet.y)-guardHalfWidth,0.0f);
    const float dz = b.centre.z < feet.z ? feet.z-b.centre.z : b.centre.z > feet.z+guardHeight ? b.centre.z-feet.z-guardHeight : 0.0f;
    return dx*dx+dy*dy+dz*dz <= b.radius*b.radius;
}
// COM vtable byte offsets the fill adds to the frame pass's (D3D11 ABI order; engine call sites where it has one).
namespace fill_method {
constexpr std::size_t drawIndexed = 0x60;              // ID 77263 at 100BDAA
constexpr std::size_t omSetDepthStencilState = 0x120;  // the flush at 1011000
constexpr std::size_t omGetDepthStencilState = 0x2e0;  // (ABI)
}
// The C++ twin of the cap shader's guard test, for the tests.
constexpr bool guardDiscards(const Guard& g,float px,float py,float depth) {
    if (!g.on) return false;
    const float cx = (g.x0+g.x1)*.5f,cy = (g.y0+g.y1)*.5f;
    const float hx = (g.x1-g.x0)*.5f > 1.0f ? (g.x1-g.x0)*.5f : 1.0f,hy = (g.y1-g.y0)*.5f > 1.0f ? (g.y1-g.y0)*.5f : 1.0f;
    const float dx = (px-cx)/hx,dy = (py-cy)/hy;
    return dx*dx+dy*dy <= 1.0f && depth <= g.depth;
}
inline FillConstants fillConstants(Color colour,const Guard& g) {
    FillConstants k;
    k.colour = {colour.red,colour.green,colour.blue,1.0f};
    k.guardRect = {g.x0,g.y0,g.x1,g.y1};
    k.guardDepth = {g.depth,g.on ? 1.0f : 0.0f,0.0f,0.0f};
    return k;
}
// The blend state of the fill: no blending; writes the colour's RGB (7: target 0's alpha stays as it was, as the
// mask-1 draws the widened gate admits leave it; review of the widened gates) and the normal target (0xF), keeps the
// motion vectors (0) and the optional fourth target (0) of whatever was drawn there before.
inline BlendDescription fillBlend() {
    BlendDescription d{};
    d.independent = 1;
    for (auto& t : d.targets) { t.source = 2; t.destination = 1; t.operation = 1; t.sourceAlpha = 2; t.destinationAlpha = 1; t.operationAlpha = 1; t.mask = 0; }
    d.targets[0].mask = 0x7; d.targets[2].mask = 0xf;
    return d;
}

// ---- trace ------------------------------------------------------------------
// Diagnostics for the FixedShaderFill2 row: over the Lighting-class passes that reach the gate in a flat cell,
// the write-mask index (+0xB0, 0..12, then "other"), the same for alpha-tested ones, the alpha-test reference in
// four buckets (<= 0, <= 0.25, <= 0.5, above or not finite), and the pass-type byte (0..15, then "other").
constexpr std::size_t maskBins = 14, refBins = 4, passBins = 17;
struct FillDiagnostics {
    std::array<unsigned long long,maskBins> mask{},alphaMask{};
    std::array<unsigned long long,refBins> alphaRef{};
    std::array<unsigned long long,passBins> passType{};
    void note(std::uint32_t writeMask,bool alphaTest,float reference,std::uint8_t pass) {
        const std::size_t m = writeMask < maskBins-1u ? writeMask : maskBins-1u;
        ++mask[m];
        ++passType[pass < passBins-1u ? pass : passBins-1u];
        if (!alphaTest) return;
        ++alphaMask[m];
        ++alphaRef[!std::isfinite(reference) ? 3u : reference <= 0.0f ? 0u : reference <= .25f ? 1u : reference <= .5f ? 2u : 3u];
    }
};
struct FillWindow {
    unsigned long long sliced{},filled{},guarded{},restores{};
    std::array<unsigned long long,static_cast<std::size_t>(FillSkip::count)> skips{};
    double seconds{};
    FillDiagnostics diagnostics{};
    bool haveGuard{};
    Guard guard{};
};
struct FillSample {
    double seconds{};
    unsigned mode{};
    unsigned long long frame{},mismatch{};
    Feature state{};
    Color colour{};
    FillWindow window{};
};
inline bool formatFill(char* out,std::size_t size,const FillSample& s) {
    const auto& w = s.window;
    const auto skip = [&](FillSkip which) { return w.skips[static_cast<std::size_t>(which)]; };
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const bool guard = w.haveGuard && w.guard.on;
    const int written = std::snprintf(out,size,
        "FixedShaderFill t=%.3f frame=%llu fill=%s colour=%.3f,%.3f,%.3f sliced=%llu filled=%llu skipClass=%llu skipCell=%llu skipActor=%llu "
        "skipType=%llu skipCull=%llu skipBlend=%llu skipAlpha=%llu skipDepth=%llu skipMask=%llu skipBudget=%llu skipLayout=%llu skipGuard=%llu "
        "guarded=%llu restores=%llu "
        "mismatch=%llu guardRect=%.0f,%.0f,%.0f,%.0f micros=%.1f sample=window",
        s.seconds,s.frame,featureNames[static_cast<std::size_t>(s.state)],static_cast<double>(s.colour.red),static_cast<double>(s.colour.green),
        static_cast<double>(s.colour.blue),w.sliced,w.filled,skip(FillSkip::shaderClass),skip(FillSkip::cell),skip(FillSkip::actor),
        skip(FillSkip::type),skip(FillSkip::cull),skip(FillSkip::blend),skip(FillSkip::alpha),skip(FillSkip::depth),skip(FillSkip::mask),
        skip(FillSkip::budget),skip(FillSkip::layout),skip(FillSkip::guard),w.guarded,w.restores,s.mismatch,guard ? static_cast<double>(w.guard.x0) : none,
        guard ? static_cast<double>(w.guard.y0) : none,guard ? static_cast<double>(w.guard.x1) : none,guard ? static_cast<double>(w.guard.y1) : none,
        w.seconds*1e6);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
template<std::size_t N>
inline int listCounts(char* out,std::size_t size,const std::array<unsigned long long,N>& values) {
    int used = 0;
    for (std::size_t i = 0;i < N;++i) {
        const int n = std::snprintf(out+used,size-static_cast<std::size_t>(used),i ? ",%llu" : "%llu",values[i]);
        if (n < 0 || static_cast<std::size_t>(used+n) >= size) return -1;
        used += n;
    }
    return used;
}
inline bool formatFill2(char* out,std::size_t size,const FillSample& s) {
    const auto& d = s.window.diagnostics;
    char mask[256]{},alphaMask[256]{},refs[96]{},pass[320]{};
    if (listCounts(mask,sizeof(mask),d.mask) < 0 || listCounts(alphaMask,sizeof(alphaMask),d.alphaMask) < 0 ||
        listCounts(refs,sizeof(refs),d.alphaRef) < 0 || listCounts(pass,sizeof(pass),d.passType) < 0) return false;
    const int written = std::snprintf(out,size,"FixedShaderFill2 t=%.3f frame=%llu mode=%u mask=%s alphaMask=%s alphaRef=%s passType=%s sample=window",
        s.seconds,s.frame,s.mode,mask,alphaMask,refs,pass);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
