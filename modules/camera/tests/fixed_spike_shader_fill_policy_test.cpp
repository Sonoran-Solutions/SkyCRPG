// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", Stage 3 step 3.1: portable checks of the back-face fill (iCutFill). The gate,
// the engine state-table indices, the evidence windows against the offsets they encode, the install check
// against a synthetic image, the player guard against a known view-projection, the blend description, the
// settings, the cap shader's bytes against its source, and the FixedShaderFill row.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::fixed::Vec;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
std::int32_t displacementAt(const std::uint8_t* at) { std::int32_t d{}; std::memcpy(&d,at,4); return d; }

void settings() {
    bool adjusted = true;
    const Config defaults = validate(Config{},&adjusted);
    assert(!adjusted && defaults.fill == 0 && defaults.fillColorText == "0,0,0");
    Config c; c.fill = 1; c.fillColorText = "0.1, 0.2,0.3"; c = validate(c,&adjusted);
    assert(!adjusted && c.fill == 1 && c.fillColor.red == .1f && c.fillColor.green == .2f && c.fillColor.blue == .3f);
    c = {}; c.fill = 2; c = validate(c,&adjusted);
    assert(!adjusted && c.fill == widenedFill);                  // the widened gates (after batch F1)
    c = {}; c.fill = 3; c = validate(c,&adjusted);
    assert(adjusted && c.fill == 0);                             // out of range: off, reported (FCS-66)
    c = {}; c.fill = 1; c.fillColorText = "grey"; c = validate(c,&adjusted);
    assert(adjusted && c.fillColor.red == 0.0f);                 // unreadable: black, reported
    c = {}; c.fill = 0; c.fillColorText = "grey"; c = validate(c,&adjusted);
    assert(!adjusted);                                           // iCutFill=0: the colour is not even read
}

void gate() {
    FillFacts ok;
    ok.lighting = true; ok.flatCell = true; ok.type = triShapeType; ok.cull = cullBack; ok.depth = depthTestWrite;
    assert(fillGate(ok) == FillSkip::none);
    FillFacts x = ok; x.depth = depthEqual; assert(fillGate(x) == FillSkip::none);   // after a depth pre-pass
    // Each refusal on its own, and the order: class, cell, actor, type, cull, blend, alpha, depth, budget, layout.
    x = ok; x.lighting = false; assert(fillGate(x) == FillSkip::shaderClass);
    x = ok; x.flatCell = false; assert(fillGate(x) == FillSkip::cell);
    x = ok; x.skinned = true; assert(fillGate(x) == FillSkip::actor);
    for (const std::uint8_t type : {0,1,2,4,12,13}) { x = ok; x.type = type; assert(fillGate(x) == FillSkip::type); }
    for (const std::uint32_t cull : {0u,2u}) { x = ok; x.cull = cull; assert(fillGate(x) == FillSkip::cull); }
    x = ok; x.blend = 1; assert(fillGate(x) == FillSkip::blend);
    x = ok; x.alphaToCoverage = 1; assert(fillGate(x) == FillSkip::blend);
    x = ok; x.alphaTest = 1; assert(fillGate(x) == FillSkip::alpha);
    for (const std::uint32_t depth : {0u,1u,2u,5u,6u}) { x = ok; x.depth = depth; assert(fillGate(x) == FillSkip::depth); }
    for (const std::uint32_t mask : {0u,1u,3u,9u,12u}) { x = ok; x.writeMask = mask; assert(fillGate(x) == FillSkip::mask); }
    x = ok; x.depth = 1; x.writeMask = 0; assert(fillGate(x) == FillSkip::depth);          // depth before mask
    x = ok; x.writeMask = 0; x.budget = false; assert(fillGate(x) == FillSkip::mask);      // mask before budget
    x = ok; x.budget = false; assert(fillGate(x) == FillSkip::budget);
    // Widened (iCutFill=2): target-0-RGB draws and alpha tests that cannot discard; strict (1) keeps rejecting both.
    FillFacts wide = ok; wide.mode = widenedFill;
    x = wide; x.writeMask = colourRgbMask; assert(fillGate(x) == FillSkip::none);
    x = ok; x.writeMask = colourRgbMask; assert(fillGate(x) == FillSkip::mask);
    for (const std::uint32_t mask : {0u,3u,8u,11u,12u}) { x = wide; x.writeMask = mask; assert(fillGate(x) == FillSkip::mask); }
    x = wide; x.alphaTest = 1; x.alphaRef = 0.0f; assert(fillGate(x) == FillSkip::none);
    x = wide; x.alphaTest = 1; x.alphaRef = -1.0f; assert(fillGate(x) == FillSkip::none);
    x = wide; x.alphaTest = 1; x.alphaRef = .5f; assert(fillGate(x) == FillSkip::alpha);
    x = wide; x.alphaTest = 1; x.alphaRef = nan; assert(fillGate(x) == FillSkip::alpha);
    x = ok; x.alphaTest = 1; x.alphaRef = 0.0f; assert(fillGate(x) == FillSkip::alpha);
    x = wide; x.alphaTest = 1; x.alphaRef = 0.0f; x.writeMask = colourRgbMask; x.cull = 0; assert(fillGate(x) == FillSkip::cull); // the rest unchanged
    x = ok; x.layout = false; assert(fillGate(x) == FillSkip::layout);
    x = ok; x.lighting = false; x.flatCell = false; x.skinned = true; assert(fillGate(x) == FillSkip::shaderClass);
    x = ok; x.flatCell = false; x.type = 1; assert(fillGate(x) == FillSkip::cell);         // a kit cell is counted as such first
    assert(fillSkipNames.size() == 13 && std::string(fillSkipNames[static_cast<std::size_t>(FillSkip::cull)]) == "cull" &&
        std::string(fillSkipNames[static_cast<std::size_t>(FillSkip::mask)]) == "mask" &&
        std::string(fillSkipNames[static_cast<std::size_t>(FillSkip::guard)]) == "guard");
    assert(FillFacts{}.writeMask == allTargetsMask && allTargetsMask == 2 && fill_offset::writeMask == 0xb0);
    assert(fillPerFrame == 2048);
}

void tables() {
    // The engine's entries: front culling with the draw's fill, bias and scissor; test-and-write with its stencil.
    assert(*frontCullIndex(0,0,0) == 48 && *frontCullIndex(1,11,1) == 143 && *frontCullIndex(0,3,1) == ((0*3+2)*12+3)*2+1);
    assert(!frontCullIndex(2,0,0) && !frontCullIndex(0,12,0) && !frontCullIndex(0,0,2));
    assert(*depthWriteIndex(0) == 120 && *depthWriteIndex(39) == 159 && !depthWriteIndex(40));
    assert(fillModes*cullModes*depthBiases*scissorModes == 144 && depthModes*stencilModes == 240);
    // The evidence windows encode what the fill relies on.
    assert(displacementAt(depthTableBytes.data()+43) == 0x3330230 && depthTableBytes[22] == 0x8d && depthTableBytes[24] == 0x80 && // rdx = 5 * mode
        displacementAt(depthTableBytes.data()+50) == static_cast<std::int32_t>(fill_method::omSetDepthStencilState));
    assert(displacementAt(rasterizerTableBytes.data()+61) == 0x33309b0 && displacementAt(rasterizerTableBytes.data()+68) == 0x158);
    assert(displacementAt(typeSwitchBytes.data()+8) == static_cast<std::int32_t>(fill_offset::geometryType) && typeSwitchBytes[3] == 0x10);
    assert(displacementAt(triShapeDrawBytes.data()+4) == static_cast<std::int32_t>(fill_offset::geometryTriangles) &&
        displacementAt(triShapeDrawBytes.data()+18) == static_cast<std::int32_t>(fill_offset::geometryData) &&
        triShapeDrawBytes[22] == 0x45 && triShapeDrawBytes[23] == 0x33 && triShapeDrawBytes[24] == 0xc0); // start index 0
    assert(callTarget(0x155f1ed,triShapeDrawBytes.data()+25) == 0x100bcd0);
    assert(drawIndexedBytes[7] == 0x8d && drawIndexedBytes[8] == 0x14 && drawIndexedBytes[9] == 0x7f &&   // edx = 3 * triangles
        drawIndexedBytes[12] == 0xc9 && drawIndexedBytes[21] == static_cast<std::uint8_t>(fill_method::drawIndexed));
    assert(counterClockwiseBytes[3] == 1 && counterClockwiseBytes[10] == 1 && cullMapBytes[3] == 2 && cullMapBytes[12] == 3 && cullMapBytes[21] == 1);
    assert(depthWriteBytes[3] == 1 && depthWriteBytes[10] == 1 && depthWriteBytes[17] == 4); // enable, write all, LESS_EQUAL
    assert(depthStencilTableAddress.id == 411362 && depthStencilTableAddress.rva == 0x3330230 && rasterizerTableAddress.id == 411363 &&
        rasterizerTableAddress.rva == 0x33309b0 && fillEvidence.size() == 14);
    // The widened gates' windows: entry 1 -> 102AF87 sets target 0's mask to 7; slot 11 holds the alpha-test buffer;
    // the flush writes +0xB8 only when +0xB4 is set.
    assert(displacementAt(writeMaskRgbEntryBytes.data()) == 0x102af87 && writeMaskRgbBytes[3] == 0x44 && writeMaskRgbBytes[4] == 0x07);
    assert(alphaSlotBytes[14] == 0xba && displacementAt(alphaSlotBytes.data()+15) == 11 && displacementAt(alphaSlotBytes.data()+28) == 0x80 &&
        static_cast<std::uintptr_t>(0x10107c9+7+displacementAt(alphaSlotBytes.data()+3)) == 0x3331aa8);
    assert(static_cast<std::uintptr_t>(0x101118d+7+displacementAt(alphaRefBytes.data()+24)) == 0x20cfc00+fill_offset::alphaTest &&
        static_cast<std::uintptr_t>(0x1011196+8+displacementAt(alphaRefBytes.data()+34)) == 0x20cfc00+fill_offset::alphaRef &&
        alphaRefBytes[40] == 0x0f && alphaRefBytes[41] == 0x57);        // xorps: 0 when the test is off
    // Write-mask index 2 jumps (table at 102B0DC, entry 2) straight to the creation at 102AFFB, with the defaults 0xF.
    assert(displacementAt(writeMaskAllBytes.data()) == 0x102affb && writeMaskDefaultBytes[4] == 0x0f && writeMaskDefaultBytes[9] == 0x0f &&
        writeMaskDefaultBytes[3] == 0x44 && writeMaskDefaultBytes[8] == 0x64);
    assert(fill_offset::depthMode == 0x88 && fill_offset::stencilMode == 0x90 && fill_offset::cullMode == 0x9c && fill_offset::blendMode == 0xa8 &&
        fill_offset::alphaTest == 0xb4);
    // The install check at three image bases: exact bytes pass, a moved table or any flipped byte refuses with its number.
    for (const std::uintptr_t base : {std::uintptr_t{0x140000000},std::uintptr_t{0x7ff6a0000000},std::uintptr_t{0x10000}}) {
        std::map<std::uintptr_t,std::vector<std::uint8_t>> image;
        for (const auto& e : fillEvidence) image[e.rva].assign(e.bytes,e.bytes+e.size);
        const auto at = [&](std::uintptr_t rva) { return image.at(rva).data(); };
        const std::uintptr_t d = base+depthStencilTableAddress.rva,r = base+rasterizerTableAddress.rva;
        assert(fillSiteCheck(base,d,r,at) == 0 && fillSiteCheck(base,d+8,r,at) == 1 && fillSiteCheck(base,d,r+8,at) == 2);
        std::size_t index = 0;
        for (const auto& e : fillEvidence) {
            for (std::size_t i = 0;i < e.size;++i) { image[e.rva][i] ^= 0x20; assert(fillSiteCheck(base,d,r,at) == 10+index); image[e.rva][i] ^= 0x20; }
            ++index;
        }
    }
}

// A view-projection like the engine's (row vectors, eye-relative), the rig's pitch at the Embershard pose.
Matrix camera(Vec eye,Vec target) {
    const double fx = target.x-eye.x,fy = target.y-eye.y,fz = target.z-eye.z,fl = std::sqrt(fx*fx+fy*fy+fz*fz);
    const double zx = fx/fl,zy = fy/fl,zz = fz/fl;
    double xx = -zy,xy = zx;
    const double xl = std::sqrt(xx*xx+xy*xy); xx /= xl; xy /= xl;
    const double yx = -zz*xy,yy = zz*xx,yz = zx*xy-zy*xx;
    const double view[16]{xx,yx,zx,0, xy,yy,zy,0, 0,yz,zz,0, 0,0,0,1};
    const double ys = 1.0/std::tan(1.2217305/2.0),xs = ys/(16.0/9.0),q = 353840.0/(353840.0-15.0);
    const double projection[16]{xs,0,0,0, 0,ys,0,0, 0,0,q,1, 0,0,-q*15.0,0};
    Matrix m;
    for (int i = 0;i < 4;++i) for (int j = 0;j < 4;++j) {
        double v = 0; for (int k = 0;k < 4;++k) v += view[i*4+k]*projection[k*4+j];
        m.m[static_cast<std::size_t>(i*4+j)] = static_cast<float>(v);
    }
    return m;
}
struct Screen { float x,y,depth; };
Screen project(const Matrix& m,Vec eye,const Viewport& v,Vec p) {
    const float q[4]{p.x-eye.x,p.y-eye.y,p.z-eye.z,1.0f};
    float c[4]{};
    for (std::size_t j = 0;j < 4;++j) for (std::size_t k = 0;k < 4;++k) c[j] += q[k]*m.m[k*4+j];
    return {v.x+(c[0]/c[3]+1.0f)*.5f*v.width,v.y+(1.0f-c[1]/c[3])*.5f*v.height,c[2]/c[3]};
}
void guard() {
    const Vec feet{2487.0f,1485.0f,9241.0f},eye{feet.x-237.3f,feet.y-237.3f,feet.z+683.8f};
    const Matrix m = camera(eye,{feet.x,feet.y,feet.z+80.0f});
    const Viewport v{0,0,1920,1080,0,1};
    const Guard g = playerGuard(m,eye,v,feet);
    assert(g.on && g.x0 < g.x1 && g.y0 < g.y1 && g.depth > 0.0f && g.depth < 1.0f);
    // The player's head and feet project inside; a cap just in front of the head (a low ceiling over the player)
    // is discarded there; the floor behind the player is not; far outside the rectangle nothing is discarded.
    const Screen head = project(m,eye,v,{feet.x,feet.y,feet.z+128.0f}),toe = project(m,eye,v,feet);
    assert(head.x > g.x0 && head.x < g.x1 && head.y > g.y0 && head.y < g.y1 && toe.y > g.y0 && toe.y < g.y1);
    const Screen lid = project(m,eye,v,{feet.x,feet.y,feet.z+150.0f});
    assert(guardDiscards(g,lid.x,lid.y,lid.depth));
    const Screen nearWall = project(m,eye,v,{feet.x-60.0f,feet.y-60.0f,feet.z+100.0f}); // the outside of a near wall, camera side
    assert(guardDiscards(g,nearWall.x,nearWall.y,nearWall.depth));
    const Screen behind = project(m,eye,v,{feet.x+400.0f,feet.y+400.0f,feet.z});
    assert(!guardDiscards(g,behind.x,behind.y,behind.depth));
    assert(!guardDiscards(g,g.x0-50.0f,(g.y0+g.y1)*.5f,0.0f) && !guardDiscards(Guard{},head.x,head.y,0.0f));
    // The rectangle's corners are outside the ellipse: the hole is round, not a square window.
    assert(!guardDiscards(g,g.x0+1.0f,g.y0+1.0f,0.0f));
    // A camera inside the guard box (never with the rig) or non-finite input: no guard rather than a wrong one.
    assert(!playerGuard(m,eye,v,{eye.x,eye.y,eye.z-50.0f}).on && !playerGuard(m,eye,v,{nan,0,0}).on);
    // The constants as the shader reads them.
    const FillConstants k = fillConstants(Color{.1f,.2f,.3f},g);
    assert(k.colour[0] == .1f && k.colour[3] == 1.0f && k.guardRect[0] == g.x0 && k.guardRect[3] == g.y1 && k.guardDepth[0] == g.depth &&
        k.guardDepth[1] == 1.0f && fillConstants(Color{},Guard{}).guardDepth[1] == 0.0f);
    assert(fillBuffer == 13 && sizeof(FillConstants) == 48);
    // The informative guarded counter: a bound meeting the player's box.
    assert(boundMeetsPlayer({{feet.x,feet.y,feet.z+300.0f},200.0f},feet) && !boundMeetsPlayer({{feet.x+500,feet.y,feet.z},100.0f},feet) &&
        boundMeetsPlayer({{feet.x+120,feet.y,feet.z+50.0f},80.0f},feet) && !boundMeetsPlayer({{nan,0,0},1.0f},feet));
}

void blend() {
    const BlendDescription b = fillBlend();
    assert(!b.alphaToCoverage && b.independent == 1);
    for (const auto& t : b.targets) assert(!t.enable);
    assert(b.targets[0].mask == 0x7 && b.targets[1].mask == 0 && b.targets[2].mask == 0xf && b.targets[3].mask == 0);
    for (std::size_t i = 4;i < b.targets.size();++i) assert(b.targets[i].mask == 0);
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    assert(in && "shader source not found: run the tests from the repository root");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
void shader() {
    using namespace frame_bytes;
    const auto source = readFile(std::filesystem::path(__FILE__).parent_path().parent_path()/"shaders"/"fixed_spike_cut_fill_ps.hlsl");
    assert(codeHash(source.data(),source.size()) == capPixelShaderSource); // edited without a rebuild of the header: fails
    const std::uint8_t* blob = capPixelShader.data();
    assert(checksumMatches(blob,capPixelShader.size()) && word(blob+28) == 3);
    const char* tags[3]{"ISGN","OSGN","SHEX"};
    for (std::uint32_t i = 0;i < 3;++i) assert(std::memcmp(blob+word(blob+32+4*i),tags[i],4) == 0);
    const std::uint8_t* code = blob+word(blob+40)+8;
    const std::uint8_t* outputs = blob+word(blob+36)+8;
    assert(word(code) == 0x00000050u && word(outputs) == 4); // ps_5_0, SV_Target0..3
    for (std::uint32_t i = 0;i < 4;++i) assert(word(outputs+8+24*i+16) == i);
    const std::uint8_t* inputs = blob+word(blob+32)+8;
    assert(word(inputs) == 1 && word(inputs+8+8) == 1); // SV_Position only
    bool buffer = false,discards = false;
    for (std::uint32_t at = 2;at < word(code+4);) {
        const std::uint32_t token = word(code+at*4),opcode = token & 0x7ffu,length = (token >> 24) & 0x7fu;
        if (opcode == op::constantBuffer) buffer = word(code+(at+2)*4) == fillBuffer && word(code+(at+3)*4) == sizeof(FillConstants)/16u;
        if (opcode == 13u) discards = true; // discard
        assert(opcode != 88u && length);    // no texture
        at += length;
    }
    assert(buffer && discards);
}

void trace() {
    FillSample s;
    s.seconds = 1234.5; s.frame = 99; s.state = Feature::installed;
    s.window.sliced = 40; s.window.filled = 22; s.window.skips[static_cast<std::size_t>(FillSkip::shaderClass)] = 12;
    s.window.skips[static_cast<std::size_t>(FillSkip::cull)] = 4; s.window.skips[static_cast<std::size_t>(FillSkip::alpha)] = 2;
    s.window.guarded = 3; s.window.restores = 22; s.window.haveGuard = true; s.window.guard = {true,900,400,1020,640,.98f}; s.window.seconds = 41e-6;
    char line[sizeof(skycrpg::camera::fixed::TraceRow)]{};
    assert(formatFill(line,sizeof(line),s));
    assert(std::string(line) == "FixedShaderFill t=1234.500 frame=99 fill=installed colour=0.000,0.000,0.000 sliced=40 filled=22 skipClass=12 "
        "skipCell=0 skipActor=0 skipType=0 skipCull=4 skipBlend=0 skipAlpha=2 skipDepth=0 skipMask=0 skipBudget=0 skipLayout=0 skipGuard=0 guarded=3 restores=22 "
        "mismatch=0 guardRect=900,400,1020,640 micros=41.0 sample=window");
    FillSample idle;
    assert(formatFill(line,sizeof(line),idle) && std::string(line).find(" fill=off ") != std::string::npos &&
        std::string(line).find(" guardRect=nan,nan,nan,nan ") != std::string::npos);
    FillSample wide = s;
    wide.frame = ~0ull; wide.mismatch = ~0ull; wide.colour = {1,1,1};
    wide.window.sliced = wide.window.filled = wide.window.guarded = wide.window.restores = ~0ull;
    for (auto& v : wide.window.skips) v = ~0ull;
    wide.window.guard = {true,-1e7f,-1e7f,-1e7f,-1e7f,1};
    assert(formatFill(line,sizeof(line),wide));
    std::printf("longest FixedShaderFill row: %zu of %zu bytes\n",std::strlen(line),sizeof(line));
    // FixedShaderFill2: the diagnostics row.
    FillSample d; d.seconds = 1234.5; d.frame = 99; d.mode = widenedFill;
    d.window.diagnostics.note(2,false,0.0f,3); d.window.diagnostics.note(1,false,0.0f,3); d.window.diagnostics.note(1,true,0.0f,3);
    d.window.diagnostics.note(2,true,.5f,1); d.window.diagnostics.note(40,true,nan,200);
    assert(formatFill2(line,sizeof(line),d));
    assert(std::string(line) == "FixedShaderFill2 t=1234.500 frame=99 mode=2 mask=0,2,2,0,0,0,0,0,0,0,0,0,0,1 alphaMask=0,1,1,0,0,0,0,0,0,0,0,0,0,1 "
        "alphaRef=1,0,1,1 passType=0,1,0,3,0,0,0,0,0,0,0,0,0,0,0,0,1 sample=window");
    FillSample big = d;
    for (auto& v : big.window.diagnostics.mask) v = ~0ull;
    for (auto& v : big.window.diagnostics.alphaMask) v = ~0ull;
    for (auto& v : big.window.diagnostics.alphaRef) v = ~0ull;
    for (auto& v : big.window.diagnostics.passType) v = 99999999ull;
    big.frame = ~0ull;
    const bool fits = formatFill2(line,sizeof(line),big);
    assert(!fits); // 49 counts of 20 digits cannot fit: such a row is not written (counts per 0.1 s window are far smaller)
    for (auto& v : big.window.diagnostics.mask) v = 99999999ull;
    for (auto& v : big.window.diagnostics.alphaMask) v = 99999999ull;
    for (auto& v : big.window.diagnostics.alphaRef) v = 99999999ull;
    assert(formatFill2(line,sizeof(line),big) && std::strlen(line) < sizeof(line));
    std::printf("FixedShaderFill2 with 8-digit counts: %zu of %zu bytes\n",std::strlen(line),sizeof(line));
}
} // namespace

int main() {
    settings();
    gate();
    tables();
    guard();
    blend();
    shader();
    trace();
    std::puts("fixed_spike_shader_fill_policy_test PASS");
}
