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
namespace fill_offset {
// RendererShadowState (ID 388819): the fields the flush reads (above, and blend 10110FF..101114C, alpha test
// 101118D..10111AB).
constexpr std::size_t depthMode = 0x88, stencilMode = 0x90, stencilRef = 0x94, fillMode = 0x98, cullMode = 0x9c, depthBias = 0xa0,
    scissor = 0xa4, blendMode = 0xa8, alphaToCoverage = 0xac, writeMask = 0xb0, alphaTest = 0xb4;
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
constexpr std::array<Evidence,10> fillEvidence{{
    {"depth-stencil table index (ID 77386)",0x1010fd1,depthTableBytes.data(),depthTableBytes.size()},
    {"rasteriser table index (ID 77386)",0x1011019,rasterizerTableBytes.data(),rasterizerTableBytes.size()},
    {"geometry type switch (ID 107637)",0x155f059,typeSwitchBytes.data(),typeSwitchBytes.size()},
    {"type 3 draw arguments (ID 107637)",0x155f1d4,triShapeDrawBytes.data(),triShapeDrawBytes.size()},
    {"DrawIndexed(3n, start, 0) (ID 77263)",0x100bd97,drawIndexedBytes.data(),drawIndexedBytes.size()},
    {"rasteriser FrontCounterClockwise 1, depth clip 1 (ID 77652)",0x102b6f7,counterClockwiseBytes.data(),counterClockwiseBytes.size()},
    {"rasteriser cull 2 FRONT, 1 BACK, 0 NONE (ID 77652)",0x102b775,cullMapBytes.data(),cullMapBytes.size()},
    {"depth mode 3: test LESS_EQUAL, write (depth table builder)",0x102b1ad,depthWriteBytes.data(),depthWriteBytes.size()},
    {"blend write-mask index 2: the defaults (blend table builder)",0x102b0e4,writeMaskAllBytes.data(),writeMaskAllBytes.size()},
    {"blend defaults: masks 0xF (blend table builder)",0x102ae60,writeMaskDefaultBytes.data(),writeMaskDefaultBytes.size()}}};
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
    if (f.alphaTest != 0) return FillSkip::alpha;          // cards, foliage, cobwebs: the cap has no alpha
    if (f.depth != depthTestWrite && f.depth != depthEqual) return FillSkip::depth; // decals and transparent draws write no depth
    if (f.writeMask != allTargetsMask) return FillSkip::mask; // a draw that writes only some channels or targets
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
// The blend state of the fill: no blending; writes the colour (0xF) and the normal target (0xF), keeps the
// motion vectors (0) and the optional fourth target (0) of whatever was drawn there before.
inline BlendDescription fillBlend() {
    BlendDescription d{};
    d.independent = 1;
    for (auto& t : d.targets) { t.source = 2; t.destination = 1; t.operation = 1; t.sourceAlpha = 2; t.destinationAlpha = 1; t.operationAlpha = 1; t.mask = 0; }
    d.targets[0].mask = 0xf; d.targets[2].mask = 0xf;
    return d;
}

// ---- trace ------------------------------------------------------------------
struct FillWindow {
    unsigned long long sliced{},filled{},guarded{},restores{};
    std::array<unsigned long long,static_cast<std::size_t>(FillSkip::count)> skips{};
    double seconds{};
    bool haveGuard{};
    Guard guard{};
};
struct FillSample {
    double seconds{};
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
} // namespace skycrpg::camera::fixed::shader
