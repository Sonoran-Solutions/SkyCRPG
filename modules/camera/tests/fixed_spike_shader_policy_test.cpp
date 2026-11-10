// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader" (DYL-217). Portable checks of the shader-cut
// policy. When SKYCRPG_SHADER_ARCHIVE names the shipped `Skyrim - Shaders.bsa`
// the patcher, the checksum and the audited list are also checked against
// every shader blob in it; without it those rows are reported as skipped.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <cstdlib>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::radians;
using skycrpg::camera::fixed::Kind;
using skycrpg::camera::fixed::TraceRow;
using skycrpg::camera::fixed::Vec;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
const float infinity = std::numeric_limits<float>::infinity();
using Tokens = std::vector<std::uint32_t>;
using Bytes = std::vector<std::uint8_t>;

// ---- a synthetic vertex shader with the shape of the shipped ones ----------
struct Element { const char* name; std::uint32_t index,systemValue,reg; std::uint8_t mask,unused; };
Bytes signature(const std::vector<Element>& elements) {
    Bytes out(signatureHeader+elements.size()*elementSize);
    store(out.data(),static_cast<std::uint32_t>(elements.size())); store(out.data()+4,8u);
    for (std::size_t i = 0;i < elements.size();++i) {
        const auto& e = elements[i];
        const auto at = static_cast<std::uint32_t>(out.size());
        out.insert(out.end(),e.name,e.name+std::strlen(e.name)+1);
        std::uint8_t* p = out.data()+signatureHeader+i*elementSize;
        store(p,at); store(p+4,e.index); store(p+8,e.systemValue); store(p+12,3u); store(p+16,e.reg);
        p[20] = e.mask; p[21] = e.unused;
    }
    while (out.size()%4) out.push_back(0xab);
    return out;
}
void append(Tokens& t,std::initializer_list<std::uint32_t> values) { t.insert(t.end(),values); }
struct Shape {
    std::uint32_t worldFirst{1};   // cb2 row of World's x row (1 utility, 0 lighting)
    std::uint32_t temps{3};
    std::uint32_t geometryRows{5};
    std::uint32_t outputRegister{0};
    bool patched{};                // the additions the patcher is expected to make
    bool extraBuffer13{},callInBody{},insideIf{},retFirst{},otherRow{},swapOperands{},lateDeclaration{},samePosition{},noSum{};
    bool unknownOpcode{},twoTemps{},strangeBetween{};
};
Tokens body(const Shape& s) {
    Tokens t{vertexShader50,0u};
    append(t,{0x0100086au});
    append(t,{0x04000059u,token::row,12u,12u});
    append(t,{0x04000059u,token::row,2u,s.geometryRows});
    if (s.patched) append(t,{0x04000059u,token::row,13u,1u});
    if (s.extraBuffer13) append(t,{0x04000059u,token::row,13u,4u});
    append(t,{0x0300005fu,0x00101072u,0u});
    append(t,{0x04000067u,0x001020f2u,s.outputRegister,1u});
    if (s.patched) append(t,{0x04000067u,token::outputX,16u,2u});
    append(t,{0x02000068u,s.temps+(s.patched ? 1u : 0u)});
    if (s.twoTemps) append(t,{0x02000068u,s.temps});
    if (s.retFirst) append(t,{0x0100003eu});
    if (s.insideIf) append(t,{0x0304001fu,0x0010000au,0u}); // if_nz r0.x
    if (s.callInBody) append(t,{0x03000004u,0x00106000u,0u}); // call l0
    for (std::uint32_t row = 8;row <= 11;++row) {
        const std::uint32_t z = s.otherRow && row == 10 ? s.worldFirst+3u : s.worldFirst+2u;
        append(t,{0x09780038u,token::tempAll,0u,0x00208006u,12u,row,token::row,2u,s.worldFirst});
        append(t,{0x09780038u,token::tempAll,1u,0x00208556u,12u,row,token::row,2u,s.worldFirst+1u});
        append(t,{0x07780000u,token::tempAll,0u,token::tempRead,0u,token::tempRead,1u});
        if (s.swapOperands) append(t,{0x09780038u,token::tempAll,1u,token::row,2u,z,token::rowZ,12u,row});
        else append(t,{0x09780038u,token::tempAll,1u,token::rowZ,12u,row,token::row,2u,z});
        if (!s.noSum || row != 8) append(t,{0x07780000u,token::tempAll,0u,token::tempRead,0u,token::tempRead,1u});
        append(t,{0x09780038u,token::tempAll,1u,0x00208ff6u,12u,row,token::row,2u,s.worldFirst+3u});
        if (!s.noSum || row != 8) append(t,{0x07780000u,token::tempAll,0u,token::tempRead,0u,token::tempRead,1u});
        if (row == 8) {
            append(t,{0x05000036u,0x00100072u,2u,0x00101246u,0u}); // mov r2.xyz, v0.xyzx
            append(t,{0x05000036u,0x00100082u,2u,0x00004001u,0x3f800000u}); // mov r2.w, l(1.0)
            if (s.strangeBetween) append(t,{0x0500004bu,0x00100012u,1u,0x0010000au,1u}); // sqrt r1.x, r1.x
            if (s.patched) {
                append(t,{0x08000011u,token::tempX,s.temps,token::row,2u,s.worldFirst+2u,token::tempRead,2u});
                append(t,{0x0b000032u,token::outputX,16u,token::tempReadX,s.temps,token::rowX,13u,0u,token::rowY,13u,0u});
            }
        }
        const std::uint32_t component = 0x00102002u | (0x10u << (row-8u));
        append(t,{0x07000011u,component,s.outputRegister,token::tempRead,0u,token::tempRead,s.samePosition ? 0u : 2u});
    }
    if (s.insideIf) append(t,{0x01000015u}); // endif
    if (s.unknownOpcode) append(t,{0x01000090u});
    if (s.lateDeclaration) append(t,{0x02000068u,s.temps});
    append(t,{0x0100003eu});
    t[1] = static_cast<std::uint32_t>(t.size());
    return t;
}
Bytes container(const Bytes& inputs,const Bytes& outputs,const Tokens& tokens,std::array<const char*,3> tags = {"ISGN","OSGN","SHEX"}) {
    Bytes out(containerHeader+12);
    std::memcpy(out.data(),"DXBC",4); store(out.data()+20,1u); store(out.data()+28,3u);
    const auto chunk = [&](std::size_t index,const std::uint8_t* data,std::size_t size) {
        store(out.data()+containerHeader+index*4,static_cast<std::uint32_t>(out.size()));
        const std::size_t at = out.size();
        out.resize(at+8+size);
        std::memcpy(out.data()+at,tags[index],4); store(out.data()+at+4,static_cast<std::uint32_t>(size));
        std::memcpy(out.data()+at+8,data,size);
    };
    chunk(0,inputs.data(),inputs.size());
    chunk(1,outputs.data(),outputs.size());
    chunk(2,reinterpret_cast<const std::uint8_t*>(tokens.data()),tokens.size()*4);
    store(out.data()+24,static_cast<std::uint32_t>(out.size()));
    const auto sum = checksum(out.data(),out.size());
    for (std::size_t i = 0;i < 4;++i) store(out.data()+checksumAt+i*4,sum[i]);
    return out;
}
const std::vector<Element> rigidInputs{{"POSITION",0,0,0,0xf,0x7}};
const std::vector<Element> plainOutputs{{"SV_POSITION",0,1,0,0xf,0},{"TEXCOORD",0,0,1,0x3,0xc}};
Bytes make(const Shape& s,const std::vector<Element>& inputs = rigidInputs,const std::vector<Element>& outputs = plainOutputs) {
    return container(signature(inputs),signature(outputs),body(s));
}
Patch result(const Bytes& blob) { return patchVertexShader(blob.data(),blob.size()).result; }
Tokens tokensOf(const Bytes& blob) {
    const std::size_t at = word(blob.data()+containerHeader+8);
    Tokens t(word(blob.data()+at+4)/4);
    std::memcpy(t.data(),blob.data()+at+8,t.size()*4);
    return t;
}

void codes() {
    constexpr std::array all{Refusal::settings,Refusal::foreign,Refusal::address,Refusal::table,Refusal::device,Refusal::thread,
        Refusal::bracket,Refusal::create,Refusal::patch,Refusal::unaudited,Refusal::view,Refusal::map,Refusal::object,Refusal::context};
    std::uint32_t seen = 0;
    for (const auto code : all) {
        // The wave 1 once-mask holds codes below 64; these have their own.
        assert(static_cast<unsigned>(code) >= firstRefusal && static_cast<unsigned>(code) <= lastRefusal && static_cast<unsigned>(code) > 65);
        assert(refusalBit(code) != 0 && !(seen & refusalBit(code)));
        seen |= refusalBit(code);
    }
    assert(shaderClasses[0].table.id == 255053 && shaderClasses[0].table.rva == 0x1b47420 && shaderClasses[0].setupRva == 0x1549550);
    assert(shaderClasses[1].table.id == 255302 && shaderClasses[1].table.rva == 0x1b4ab58 && shaderClasses[1].restoreRva == 0x15689c0);
    assert(stateAddress.id == 388819 && stateAddress.rva == 0x20cfc00 && deviceAddress.rva == 0x3330190 && contextAddress.rva == 0x3331f30);
    assert(offset::viewDirection == 0x380+0x20 && offset::posAdjust == 0x35c && offset::currentVertexShader == 0x348);
    assert(setupSlot == 6 && restoreSlot == 7 && cutBuffer == 13 && cutRegister == 16);
    assert(sizeof(BufferDescription) == 24 && sizeof(MappedBuffer) == 16 && sizeof(Constant) == 16);
    const BufferDescription description{};
    assert(description.usage == 2 && description.bind == 4 && description.access == 0x10000 && !description.misc && !description.stride);
    assert(std::is_sorted(auditedVertexShaders.begin(),auditedVertexShaders.end()));
    assert(std::adjacent_find(auditedVertexShaders.begin(),auditedVertexShaders.end()) == auditedVertexShaders.end());
    assert(auditedVertexShaders.size() == 84 && audited(auditedVertexShaders.front()) && audited(auditedVertexShaders.back()) && !audited(0) && !audited(1));
}

void settings() {
    bool adjusted = true;
    const Config defaults = validate(Config{},&adjusted);
    assert(!adjusted && defaults.height == 160.0f && defaults.radius == 0.0f && defaults.maxPerFrame == 4096 && defaults.shaders == 3 && !defaults.trace);
    Config c;
    c.height = nan; c.radius = 100; c.maxPerFrame = 0; c.shaders = 9;
    c = validate(c,&adjusted);
    assert(adjusted && c.height == 160.0f && c.radius == minRadius && c.maxPerFrame == 1 && c.shaders == 3);
    c = {}; c.height = 5; c.radius = 1e9f; c.maxPerFrame = 1u << 30; c.shaders = 0;
    c = validate(c,&adjusted);
    assert(adjusted && c.height == 64.0f && c.radius == 8192.0f && c.maxPerFrame == 16384 && c.shaders == 3);
    c = {}; c.radius = nan; assert(validate(c,&adjusted).radius == 0.0f && adjusted);
    c = {}; c.radius = -1; assert(validate(c).radius == minRadius);
    c = {}; c.height = 2000; assert(validate(c).height == 1024.0f);
    c = {}; c.radius = 1024; c.shaders = 1; c.trace = true;
    c = validate(c,&adjusted);
    assert(!adjusted && c.radius == 1024.0f && c.shaders == 1 && c.trace);
    assert(shaderEnabled(3,0) && shaderEnabled(3,1) && !shaderEnabled(1,1) && !shaderEnabled(2,0) && !shaderEnabled(7,2));
    assert(shaderMode(3) && !shaderMode(0) && !shaderMode(1) && !shaderMode(2) && !shaderMode(4));
}

void view() {
    const Config c = validate(Config{});
    // The wave 1 rig: camera 237.3, 237.3, 683.8 from the feet, looking down at 55 degrees toward heading 45.
    const Vec feet{2048,-9216,-2556},eye{feet.x-237.3f,feet.y-237.3f,feet.z+683.8f};
    const float pitch = radians(55.0f);
    const Vec forward{std::cos(pitch)*std::sqrt(.5f),std::cos(pitch)*std::sqrt(.5f),-std::sin(pitch)};
    const auto made = makeView(c,eye,forward,feet,feet.z,7,100.0);
    assert(made && made->frame == 7 && made->planeZ == feet.z+160.0f && made->radius == 0.0f && made->feet.z == feet.z);
    assert(std::abs(dot(made->forward,made->forward)-1.0f) < 1e-5f);
    // The plane follows the held reference height, not the instantaneous feet.
    assert(makeView(c,eye,forward,{feet.x,feet.y,feet.z+70},feet.z,7,100.0)->planeZ == feet.z+160.0f);
    assert(!makeView(c,{nan,0,0},forward,feet,feet.z,1,0) && !makeView(c,eye,{0,0,0},feet,feet.z,1,0) && !makeView(c,eye,{9,0,0},feet,feet.z,1,0));
    assert(!makeView(c,eye,forward,{0,infinity,0},feet.z,1,0) && !makeView(c,eye,forward,feet,nan,1,0) && !makeView(c,eye,forward,feet,feet.z,1,static_cast<double>(nan)));
    assert(makeView(c,eye,{forward.x*1.5f,forward.y*1.5f,forward.z*1.5f},feet,feet.z,1,0)); // normalised
    assert(fresh(*made,100.0) && fresh(*made,100.2) && !fresh(*made,100.3) && !fresh(*made,99.0) && !fresh(*made,static_cast<double>(nan)));
    // Only the published view: same eye (the wave 1 tolerance) and direction.
    assert(sameView(*made,eye,forward) && sameView(*made,{eye.x+50,eye.y,eye.z+50},forward));
    assert(!sameView(*made,{eye.x+97,eye.y,eye.z},forward) && !sameView(*made,feet,forward));
    assert(!sameView(*made,eye,{-forward.x,-forward.y,-forward.z}) && !sameView(*made,eye,{0,0,-1}));
    assert(!sameView(*made,eye,{forward.x*2,forward.y*2,forward.z*2}) && !sameView(*made,eye,{0,0,0}));
    assert(!sameView(*made,{nan,eye.y,eye.z},forward) && !sameView(*made,eye,{nan,0,0}));
    const float tilt = radians(5.0f); // 5 degrees off is still this view, 10 is not
    assert(sameView(*made,eye,{std::cos(pitch+tilt)*std::sqrt(.5f),std::cos(pitch+tilt)*std::sqrt(.5f),-std::sin(pitch+tilt)}));
    assert(!sameView(*made,eye,{std::cos(pitch+2*tilt)*std::sqrt(.5f),std::cos(pitch+2*tilt)*std::sqrt(.5f),-std::sin(pitch+2*tilt)}));
}

void plane() {
    // World height = height relative to the position adjustment + the adjustment.
    for (const float planeZ : {-2396.0f,160.0f,0.0f,7000.5f}) {
        for (const float adjust : {-1872.2f,0.0f,843.8f,planeZ}) {
            const Constant c = cutConstant(planeZ,adjust);
            assert(usable(c) && c.scale == -1.0f && c.unusedZ == 0 && c.unusedW == 0);
            for (float worldZ = planeZ-900;worldZ <= planeZ+900;worldZ += 37.5f) {
                const float distance = clipDistance(c,worldZ-adjust);
                // The rasteriser keeps distance >= 0: at or under the plane stays, above goes.
                if (worldZ < planeZ-.01f) assert(distance > 0);
                if (worldZ > planeZ+.01f) assert(distance < 0);
                assert(std::abs(distance-(planeZ-worldZ)) < .01f);
            }
        }
    }
    // Nothing bound in slot 13 reads as zero: never cut.
    const Constant none{};
    for (const float z : {-1e6f,0.0f,1e6f}) assert(clipDistance(none,z) == 0.0f);
    assert(!usable(none) && !usable(cutConstant(nan,0)) && !usable(cutConstant(0,infinity)) && !usable({1.0f,5.0f,0,0}));
}

void selection() {
    const Config c = validate(Config{});
    const Vec feet{0,0,100},eye{-237.3f,-237.3f,783.8f},forward{.4056f,.4056f,-.8192f};
    View v = *makeView(c,eye,forward,feet,feet.z,1,0); // plane at 260
    assert(reach(v,{{0,0,600},200},true) == Skip::skinned);
    assert(reach(v,{{nan,0,0},10},false) == Skip::noBound && reach(v,{{0,0,0},0},false) == Skip::noBound && reach(v,{{0,0,0},nan},false) == Skip::noBound);
    // The floor, a table, a chest-high wall, a piece that only touches the plane: untouched.
    assert(reach(v,{{0,0,90},64},false) == Skip::below && reach(v,{{300,0,150},100},false) == Skip::below);
    assert(reach(v,{{0,0,160},100},false) == Skip::below);
    // A ceiling, a tall wall, a one-piece cave mesh, another room's rock between camera and player.
    assert(reach(v,{{0,0,400},128},false) == Skip::none && reach(v,{{0,300,200},128},false) == Skip::none);
    assert(reach(v,{{0,0,0},4000},false) == Skip::none && reach(v,{{-120,-120,500},150},false) == Skip::none);
    // Whole view by default; with a radius, only what reaches near the feet.
    assert(reach(v,{{5000,0,400},128},false) == Skip::none);
    Config limited; limited.radius = 512;
    v = *makeView(validate(limited),eye,forward,feet,feet.z,1,0);
    assert(reach(v,{{5000,0,400},128},false) == Skip::outside && reach(v,{{600,0,400},128},false) == Skip::none);
    assert(reach(v,{{0,640.5f,400},128},false) == Skip::outside && reach(v,{{0,0,90},64},false) == Skip::below);
    // The camera's own ground distance (about 336) is always inside the smallest radius.
    assert(reach(*makeView(validate([] { Config k; k.radius = 1; return k; }()),eye,forward,feet,feet.z,1,0),{{eye.x,eye.y,600},1},false) == Skip::none);
    assert(ownerRule(false,false,Kind::architecture) == Skip::ownerless && ownerRule(true,true,Kind::architecture) == Skip::actor);
    assert(ownerRule(true,false,Kind::excluded) == Skip::kind && ownerRule(true,false,Kind::architecture) == Skip::none);
    assert(ownerRule(true,false,Kind::object) == Skip::none && ownerRule(false,true,Kind::excluded) == Skip::ownerless);
    Budget budget; budget.limit = 2;
    assert(budget.take() && budget.take() && !budget.take() && budget.peak == 2);
    budget.frame();
    assert(budget.take() && budget.peak == 2);
    assert(std::string(faultName(Fault::setup)) != faultName(Fault::frame) && std::string(faultName(Fault::shaderChanged)) != faultName(Fault::frame));
}

void checksums() {
    // Expected values from an independent implementation that reproduces the
    // stored checksum of all 16,044 blobs of the shipped archive.
    struct Case { std::size_t length; std::array<std::uint32_t,4> sum; };
    const std::array<Case,7> cases{{
        {100,{0x185ff944u,0x7e15d457u,0xd542ab4bu,0x22a39f6bu}},{60,{0xc9a19a11u,0xfd7a9113u,0xe74595cau,0xa37c0f47u}},
        {64,{0x1b4bf5c1u,0xd39a4c56u,0x476fe115u,0x18cbffb5u}},{56,{0x66cc71d5u,0xb2b76c53u,0xff80879eu,0x19a341beu}},
        {55,{0x1e7157ddu,0x7efe123eu,0x7ef4c284u,0x416642deu}},{0,{0xf6600d14u,0xbae275b7u,0xd4be4a4eu,0xa1e9b201u}},
        {1000,{0x94bbb08bu,0x605ee1e1u,0xc0ae4f68u,0xf81a8ca9u}}}};
    for (const auto& test : cases) {
        Bytes blob(20+test.length);
        for (std::size_t i = 0;i < blob.size();++i) blob[i] = static_cast<std::uint8_t>(i*7+3);
        assert(checksum(blob.data(),blob.size()) == test.sum);
    }
    Bytes blob = make(Shape{});
    assert(checksumMatches(blob.data(),blob.size()));
    blob[blob.size()-3] ^= 1;
    assert(!checksumMatches(blob.data(),blob.size()) && !checksumMatches(blob.data(),8) && !checksumMatches(nullptr,64));
    assert(codeHash(nullptr,0) == 14695981039346656037ull && codeHash(reinterpret_cast<const std::uint8_t*>("a"),1) == 0xaf63dc4c8601ec8cull);
}

void patchSynthetic() {
    for (const std::uint32_t worldFirst : {1u,0u}) {
        Shape s; s.worldFirst = worldFirst; s.geometryRows = worldFirst+4u;
        const Bytes blob = make(s);
        const Patched made = patchVertexShader(blob.data(),blob.size());
        assert(made.result == Patch::ok && made.worldRow == worldFirst+2u && made.position == 2 && made.temp == 3);
        const Bytes& out = made.code;
        // Exactly the expected token stream: two declarations, one more temp, two instructions before the first product.
        Shape expected = s; expected.patched = true;
        assert(tokensOf(out) == body(expected));
        assert(tokensOf(out).size() == tokensOf(blob).size()+27);
        assert(out.size() == blob.size()+27*4+elementSize+16 && word(out.data()+24) == out.size() && checksumMatches(out.data(),out.size()));
        assert(std::memcmp(out.data(),"DXBC",4) == 0 && word(out.data()+20) == 1 && word(out.data()+28) == 3);
        // Input signature byte for byte; output signature = the old one + SV_ClipDistance0 in o16.x.
        const std::size_t in0 = word(blob.data()+containerHeader),in1 = word(out.data()+containerHeader);
        const std::size_t sig0 = word(blob.data()+containerHeader+4),sig1 = word(out.data()+containerHeader+4);
        assert(in0 == in1 && sig0 == sig1 && std::memcmp(blob.data()+in0,out.data()+in1,sig0-in0) == 0);
        assert(std::memcmp(out.data()+sig1,"OSGN",4) == 0 && word(out.data()+sig1+4) == word(blob.data()+sig0+4)+elementSize+16);
        const std::uint8_t* before = blob.data()+sig0+8;
        const std::uint8_t* after = out.data()+sig1+8;
        assert(word(after) == 3 && word(after+4) == 8);
        for (std::size_t i = 0;i < 2;++i) {
            const std::uint8_t* a = before+signatureHeader+i*elementSize;
            const std::uint8_t* b = after+signatureHeader+i*elementSize;
            assert(std::memcmp(a+4,b+4,elementSize-4) == 0 && word(b) == word(a)+elementSize);
            assert(std::strcmp(reinterpret_cast<const char*>(before+word(a)),reinterpret_cast<const char*>(after+word(b))) == 0);
        }
        const std::uint8_t* added = after+signatureHeader+2*elementSize;
        assert(std::strcmp(reinterpret_cast<const char*>(after+word(added)),"SV_ClipDistance") == 0);
        assert(word(added+4) == 0 && word(added+8) == 2 && word(added+12) == 3 && word(added+16) == 16 && added[20] == 1 && added[21] == 0xe);
        assert(word(added)+16 == word(out.data()+sig1+4));
        const std::size_t code1 = word(out.data()+containerHeader+8);
        assert(code1 == sig1+8+word(out.data()+sig1+4) && std::memcmp(out.data()+code1,"SHEX",4) == 0 && code1+8+word(out.data()+code1+4) == out.size());
        // A patched shader is never patched again.
        assert(result(out) == Patch::signature);
    }
    // Operand order of the mul does not matter; the position is the operand that is not the running sum.
    { Shape s; s.swapOperands = true; assert(result(make(s)) == Patch::ok); }
}

void patchRejects() {
    const Bytes good = make(Shape{});
    assert(result(good) == Patch::ok);
    assert(patchVertexShader(nullptr,100).result == Patch::container && patchVertexShader(good.data(),40).result == Patch::container);
    assert(patchVertexShader(good.data(),good.size()-4).result == Patch::container); // total size disagrees
    { Bytes b = good; b[0] = 'X'; assert(result(b) == Patch::container); }
    { Bytes b = good; store(b.data()+28,4u); assert(result(b) == Patch::container); }
    { Bytes b = good; b[b.size()-1] ^= 0x40; assert(result(b) == Patch::checksum); }
    { Bytes b = good; b[5] ^= 1; assert(result(b) == Patch::checksum); }
    const Bytes in = signature(rigidInputs),out = signature(plainOutputs);
    assert(result(container(out,in,body(Shape{}),{"OSGN","ISGN","SHEX"})) == Patch::chunks);
    assert(result(container(in,out,body(Shape{}),{"ISGN","OSGN","SHDR"})) == Patch::chunks);
    { Tokens t = body(Shape{}); t[0] = 0x00000050u; assert(result(container(in,out,t)) == Patch::version); } // a pixel shader
    { Tokens t = body(Shape{}); t[0] = 0x00010040u; assert(result(container(in,out,t)) == Patch::version); }
    { Tokens t = body(Shape{}); t[1] += 1; assert(result(container(in,out,t)) == Patch::version); }
    // Skinned inputs, an existing clip or cull distance, a register at or above ours, a broken name offset.
    assert(result(make(Shape{},{{"POSITION",0,0,0,0xf,0x7},{"BLENDWEIGHT",0,0,1,0xf,0xf}})) == Patch::skinned);
    assert(result(make(Shape{},{{"POSITION",0,0,0,0xf,0x7},{"BLENDINDICES",0,0,2,0xf,0xf}})) == Patch::skinned);
    assert(result(make(Shape{},rigidInputs,{{"SV_POSITION",0,1,0,0xf,0},{"SV_ClipDistance",0,2,1,0x1,0xe}})) == Patch::signature);
    assert(result(make(Shape{},rigidInputs,{{"SV_POSITION",0,1,0,0xf,0},{"SV_CullDistance",0,3,1,0x1,0xe}})) == Patch::signature);
    assert(result(make(Shape{},rigidInputs,{{"SV_POSITION",0,1,0,0xf,0},{"TEXCOORD",0,0,16,0xf,0}})) == Patch::signature);
    assert(result(container(in,Bytes{0,0,0,0,8,0,0,0},body(Shape{}))) == Patch::signature); // no outputs
    { Bytes o = out; store(o.data()+signatureHeader,4u); assert(result(container(in,o,body(Shape{}))) == Patch::signature); }
    { Bytes o = out; store(o.data()+4,12u); assert(result(container(in,o,body(Shape{}))) == Patch::signature); }
    // Declarations.
    { Shape s; s.extraBuffer13 = true; assert(result(make(s)) == Patch::declarations); }
    { Shape s; s.outputRegister = 16; assert(result(make(s)) == Patch::declarations); }
    { Shape s; s.twoTemps = true; assert(result(make(s)) == Patch::declarations); }
    { Shape s; s.lateDeclaration = true; assert(result(make(s)) == Patch::declarations); }
    { Shape s; s.unknownOpcode = true; assert(result(make(s)) == Patch::declarations); }
    { Shape s; s.geometryRows = 3; assert(result(make(s)) == Patch::declarations); } // World's z row is not declared
    { Shape s; s.temps = 2; assert(result(make(s)) == Patch::declarations); }        // the position register is not a declared temp
    { Shape s; s.temps = mostTemps; assert(result(make(s)) == Patch::declarations); }
    { Tokens t = body(Shape{}); t[2] = 0x0000086au; assert(result(container(in,out,t)) == Patch::declarations); } // a zero-length instruction
    { Tokens t = body(Shape{}); t[t.size()-1] = 0x0900003eu; assert(result(container(in,out,t)) == Patch::declarations); } // runs past the end
    // The arithmetic shape.
    { Shape s; s.otherRow = true; assert(result(make(s)) == Patch::pattern); }
    { Shape s; s.samePosition = true; assert(result(make(s)) == Patch::pattern); }
    { Shape s; s.noSum = true; assert(result(make(s)) == Patch::pattern); }
    { Shape s; s.strangeBetween = true; assert(result(make(s)) == Patch::pattern); }
    { Shape s; s.retFirst = true; assert(result(make(s)) == Patch::pattern); }
    { Tokens t{vertexShader50,0u,0x0100086au,0x04000059u,token::row,2u,5u,0x04000067u,0x001020f2u,0u,1u,0x02000068u,1u,0x0100003eu};
      t[1] = static_cast<std::uint32_t>(t.size()); assert(result(container(in,out,t)) == Patch::pattern); } // no view-projection at all
    // Control flow.
    { Shape s; s.insideIf = true; assert(result(make(s)) == Patch::flow); }
    { Shape s; s.callInBody = true; assert(result(make(s)) == Patch::flow); }
    { Tokens t = body(Shape{}); t.insert(t.end()-1,0x01000015u); t[1] = static_cast<std::uint32_t>(t.size());
      assert(result(container(in,out,t)) == Patch::flow); } // an endif without an if
    { Tokens t = body(Shape{}); t.insert(t.end()-1,{0x0304001fu,0x0010000au,0u}); t[1] = static_cast<std::uint32_t>(t.size());
      assert(result(container(in,out,t)) == Patch::flow); } // an if left open
    // Every single-bit corruption of the header or chunk table is refused.
    for (std::size_t byte = 0;byte < containerHeader+12;++byte)
        for (unsigned bit = 0;bit < 8;++bit) {
            Bytes b = good; b[byte] = static_cast<std::uint8_t>(b[byte] ^ (1u << bit));
            assert(result(b) != Patch::ok);
        }
}

void traces() {
    Sample s;
    s.seconds = 1234.5; s.frame = 99; s.installed = s.active = s.haveView = s.haveOther = true;
    s.view = *makeView(validate(Config{}),{1811,-9453,-1872},{.4f,.4f,-.82f},{2048,-9216,-2556},-2556,99,1234.5);
    s.otherEye = {100,200,300}; s.otherDirection = {0,0,-1}; s.offset = -524.0f;
    s.peak = 31; s.cap = 4096; s.patched = 7; s.rejected = 1; s.unaudited = 2;
    s.window.passes = 5000; s.window.cut = 31; s.window.byClass = {20,11}; s.window.uploads = 6; s.window.otherView = 900;
    s.window.created = 7; s.window.seconds = .00042; s.window.skips[static_cast<std::size_t>(Skip::below)] = 3000;
    char line[768]{};
    assert(format(line,sizeof(line),s));
    const std::string text = line;
    for (const char* field : {"FixedShader t=1234.500 ","frame=99 ","installed=1 active=1 latched=0 ","planeZ=-2396.00 ","radius=0.0 ","relPlane=-524.00 ",
        "passes=5000 cut=31 byClass=20,11 ","peak=31 cap=4096 ","uploads=6 shaders=7,1,2 created=7 ","micros=420.0 ","otherView=900 ",
        "otherEye=100.0,200.0,300.0 ","otherDir=0.00,0.00,-1.00 ","eye=1811.0,-9453.0,-1872.0 feet=2048.0,-9216.0,-2556.0 ","open=0 ","stale=0 ",
        "skipBelow=3000 ","skipCap=0 sample=window"})
        assert(text.find(field) != std::string::npos);
    // Inactive: the snapshot and the last constant read nan.
    Sample idle; idle.installed = true;
    assert(format(line,sizeof(line),idle));
    assert(std::string(line).find("active=0 latched=0 eye=nan,nan,nan feet=nan,nan,nan planeZ=nan radius=nan relPlane=nan ") != std::string::npos);
    // The longest row fits one trace row.
    Sample big;
    big.seconds = 1e7; big.frame = ~0ull; big.offMain = ~0ull; big.installed = big.active = big.latched = big.haveView = big.haveOther = true;
    big.view.eye = big.view.feet = big.otherEye = {-1e7f,-1e7f,-1e7f}; big.view.planeZ = -1e7f; big.view.radius = 8192; big.offset = -1e7f;
    big.otherDirection = {-1,-1,-1}; big.peak = big.cap = big.patched = big.rejected = big.unaudited = ~0u;
    big.window.passes = big.window.cut = big.window.uploads = big.window.otherView = big.window.stale = big.window.created = big.window.unbalanced = 9999999999ull;
    big.window.byClass = {9999999999ull,9999999999ull}; big.window.seconds = 9.9;
    for (auto& value : big.window.skips) value = 9999999999ull;
    assert(format(line,sizeof(line),big) && std::strlen(line) < sizeof(TraceRow));
    char tiny[64]{};
    assert(!format(tiny,sizeof(tiny),s));
}

// ---- the shipped archive, when present --------------------------------------
bool archive() {
    const char* path = std::getenv("SKYCRPG_SHADER_ARCHIVE");
    if (!path || !*path) return false;
    std::ifstream file(path,std::ios::binary);
    assert(file);
    const Bytes d((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    assert(d.size() > 64 && std::memcmp(d.data(),"BSA",4) == 0);
    // Shaders are stored per class, vertex then pixel, in the order the engine loads the classes.
    const std::array<unsigned,9> classSizes{2,4,18,6,9,606,127,339,2172}; // BloodSplatter .. Lighting, Utility, Water
    std::array<std::array<unsigned,static_cast<std::size_t>(Patch::count)>,2> outcome{}; // lighting, utility
    std::vector<std::uint64_t> hashes;
    unsigned blobs = 0,sums = 0,vertex = 0,productAgrees = 0,temporaryAgrees = 0;
    std::size_t cls = 0,inClass = 0;
    for (std::size_t p = 0;p+containerHeader <= d.size();) {
        if (std::memcmp(&d[p],"DXBC",4) != 0) { ++p; continue; }
        const std::size_t total = word(&d[p+24]);
        assert(total >= containerHeader+12 && total <= d.size()-p);
        ++blobs;
        if (checksumMatches(&d[p],total)) ++sums;
        const std::size_t code = word(&d[p+containerHeader+8]);
        if ((word(&d[p+code+8]) >> 16) == 1u) { // a vertex shader
            ++vertex;
            // The loader's entry header: magic, then the bytecode size the engine keeps at object +10h.
            assert(word(&d[p-44]) == 0x11223344u && word(&d[p-40]) == total);
            while (cls < classSizes.size() && inClass == classSizes[cls]) { ++cls; inClass = 0; }
            ++inClass;
            const Patched made = patchVertexShader(&d[p],total);
            if (cls == 6 || cls == 7) {
                ++outcome[cls-6][static_cast<std::size_t>(made.result)];
                assert((made.result == Patch::ok) == audited(codeHash(&d[p],total)));
            }
            if (made.result == Patch::ok) {
                if (cls == 6 || cls == 7) hashes.push_back(codeHash(&d[p],total));
                assert(made.code.size() == total+27*4+elementSize+16 && checksumMatches(made.code.data(),made.code.size()));
                assert(patchVertexShader(made.code.data(),made.code.size()).result == Patch::signature);
                const Tokens t = tokensOf(made.code);
                Instruction ins;
                std::size_t at = 2;
                unsigned buffer13 = 0,output16 = 0,products = 0,temporaries = 0;
                // The register a Lighting vertex shader writes POSITION1 (this frame's world position) to.
                std::uint32_t worldOutput = ~0u;
                const std::size_t osgn = p+word(&d[p+containerHeader+4])+8;
                for (std::size_t i = 0;cls == 6 && i < word(&d[osgn]);++i)
                    if (named(&d[osgn],word(&d[osgn-4]),i,"POSITION") && word(&d[osgn+signatureHeader+i*elementSize+4]) == 1u)
                        worldOutput = word(&d[osgn+signatureHeader+i*elementSize+16]);
                for (;at < t.size();at += ins.length) {
                    assert(decode(t,at,ins));
                    if (ins.code == op::constantBuffer && t[at+2] == cutBuffer) { ++buffer13; assert(t[at+3] == 1); }
                    if (ins.code == op::outputSiv && t[at+2] == cutRegister) { ++output16; assert(t[at+1] == token::outputX && t[at+3] == 2); }
                    // Where the shader itself writes world z (POSITION1.z = dp4 cb2[k], rP) it is the product the patch adds.
                    // 50 write it straight to the output; the other 20 build the world position in a temp first.
                    if ((t[at] & ~token::precise) == token::dot4Row && t[at+1] == 0x00102042u && t[at+2] == worldOutput && t[at+3] == token::row && t[at+4] == perGeometryBuffer &&
                        t[at+6] == token::tempRead) {
                        ++products;
                        assert(t[at+5] == made.worldRow && t[at+7] == made.position);
                    }
                    if (cls == 6 && (t[at] & ~token::precise) == token::dot4Row && t[at+1] == 0x00100042u && t[at+3] == token::row && t[at+4] == perGeometryBuffer &&
                        t[at+5] == made.worldRow && t[at+6] == token::tempRead && t[at+7] == made.position) ++temporaries;
                }
                assert(at == t.size() && t[1] == t.size() && buffer13 == 1 && output16 == 1);
                if (products) ++productAgrees;
                else if (temporaries) ++temporaryAgrees;
            }
        }
        p += total;
    }
    std::sort(hashes.begin(),hashes.end());
    const std::size_t entries = hashes.size();
    hashes.erase(std::unique(hashes.begin(),hashes.end()),hashes.end());
    std::printf("archive: %u blobs, %u checksums reproduced, %u vertex shaders; lighting ok=%u pattern=%u skinned=%u; utility ok=%u pattern=%u skinned=%u; "
        "%zu patchable entries, %zu distinct bytecodes; world-z product confirmed in %u+%u lighting shaders\n",blobs,sums,vertex,
        outcome[0][static_cast<std::size_t>(Patch::ok)],outcome[0][static_cast<std::size_t>(Patch::pattern)],outcome[0][static_cast<std::size_t>(Patch::skinned)],
        outcome[1][static_cast<std::size_t>(Patch::ok)],outcome[1][static_cast<std::size_t>(Patch::pattern)],outcome[1][static_cast<std::size_t>(Patch::skinned)],
        entries,hashes.size(),productAgrees,temporaryAgrees);
    assert(blobs == 16044 && sums == blobs && vertex == 3390);
    assert(outcome[0][static_cast<std::size_t>(Patch::ok)] == 70 && outcome[0][static_cast<std::size_t>(Patch::pattern)] == 8 && outcome[0][static_cast<std::size_t>(Patch::skinned)] == 49);
    assert(outcome[1][static_cast<std::size_t>(Patch::ok)] == 125 && outcome[1][static_cast<std::size_t>(Patch::pattern)] == 53 && outcome[1][static_cast<std::size_t>(Patch::skinned)] == 161);
    assert(entries == 195 && hashes.size() == auditedVertexShaders.size() && std::equal(hashes.begin(),hashes.end(),auditedVertexShaders.begin()));
    assert(productAgrees == 50 && temporaryAgrees == 20); // every patched Lighting shader
    return true;
}
}

int main() {
    codes();
    settings();
    view();
    plane();
    selection();
    checksums();
    patchSynthetic();
    patchRejects();
    traces();
    if (!archive()) std::printf("archive rows skipped: SKYCRPG_SHADER_ARCHIVE is not set\n");
}
