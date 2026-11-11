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
        Refusal::bracket,Refusal::create,Refusal::patch,Refusal::unaudited,Refusal::view,Refusal::map,Refusal::object,Refusal::context,
        Refusal::effectTable,Refusal::voidSite,Refusal::voidMemory,Refusal::voidThread,Refusal::effectCreate,Refusal::effectPatch,
        Refusal::effectUnaudited,Refusal::effectObject,Refusal::effectThread,Refusal::skySite,Refusal::skyLeft,Refusal::styleBuffers,Refusal::styleMap};
    std::uint32_t seen = 0;
    for (const auto code : all) {
        // The wave 1 once-mask holds codes below 64; these have their own.
        assert(static_cast<unsigned>(code) >= firstRefusal && static_cast<unsigned>(code) <= lastRefusal && static_cast<unsigned>(code) > 65);
        assert(refusalBit(code) != 0 && !(seen & refusalBit(code)));
        seen |= refusalBit(code);
    }
    // 66..79 are the first pass's, unchanged; the second pass continues at 80 without a gap.
    assert(all.size() == lastRefusal-firstRefusal+1u && static_cast<unsigned>(Refusal::context) == 79 && static_cast<unsigned>(Refusal::effectTable) == 80);
    // The third pass continues at 89 without a gap; 32 codes is the once-mask's limit.
    assert(static_cast<unsigned>(Refusal::effectThread) == 88 && static_cast<unsigned>(Refusal::skySite) == 89 &&
        static_cast<unsigned>(Refusal::skyLeft) == 90 && lastRefusal-firstRefusal < 32u);
    // The fourth pass continues at 91.
    assert(static_cast<unsigned>(Refusal::styleBuffers) == 91 && static_cast<unsigned>(Refusal::styleMap) == 92 && lastRefusal == 92);
    assert(seen == (std::uint32_t{1} << (lastRefusal-firstRefusal+1u))-1u);
    // A shader preparation reports through its own group's codes.
    for (std::size_t i = 0;i < 2;++i) {
        const auto& c = prepareCodes[i];
        assert(c.object != c.unaudited && c.unaudited != c.patch && c.patch != c.create && c.object != c.create);
        for (const auto code : {c.object,c.unaudited,c.patch,c.create})
            for (const auto other : {prepareCodes[1-i].object,prepareCodes[1-i].unaudited,prepareCodes[1-i].patch,prepareCodes[1-i].create})
                assert(code != other);
    }
    assert(prepareCodes[0].unaudited == Refusal::unaudited && prepareCodes[1].unaudited == Refusal::effectUnaudited);
    assert(shaderClasses[0].table.id == 255053 && shaderClasses[0].table.rva == 0x1b47420 && shaderClasses[0].setupRva == 0x1549550);
    assert(shaderClasses[1].table.id == 255302 && shaderClasses[1].table.rva == 0x1b4ab58 && shaderClasses[1].restoreRva == 0x15689c0);
    assert(shaderClasses.size() == 3 && effectClass == 2 && mainClasses == 2 && std::string(shaderClasses[effectClass].name) == "effect");
    assert(shaderClasses[2].table.id == 255194 && shaderClasses[2].table.rva == 0x1b48ab0 && shaderClasses[2].setupRva == 0x1557090 &&
        shaderClasses[2].restoreRva == 0x1557f50);
    assert(shaderClasses[2].setupBytes[0] == 0x48 && shaderClasses[2].setupBytes[15] == 0x8d && shaderClasses[2].restoreBytes[0] == 0x48 &&
        shaderClasses[2].restoreBytes[15] == 0x0f);
    assert(groupOf(0) == 0 && groupOf(1) == 0 && groupOf(effectClass) == 1);
    assert(stateAddress.id == 388819 && stateAddress.rva == 0x20cfc00 && deviceAddress.rva == 0x3330190 && contextAddress.rva == 0x3331f30);
    assert(offset::viewDirection == 0x380+0x20 && offset::posAdjust == 0x35c && offset::currentVertexShader == 0x348);
    assert(setupSlot == 6 && restoreSlot == 7 && cutBuffer == 13 && cutRegister == 16);
    assert(sizeof(BufferDescription) == 24 && sizeof(MappedBuffer) == 16 && sizeof(Constant) == 16);
    const BufferDescription description{};
    assert(description.usage == 2 && description.bind == 4 && description.access == 0x10000 && !description.misc && !description.stride);
    assert(std::is_sorted(auditedVertexShaders.begin(),auditedVertexShaders.end()));
    assert(std::adjacent_find(auditedVertexShaders.begin(),auditedVertexShaders.end()) == auditedVertexShaders.end());
    assert(auditedVertexShaders.size() == 84 && audited(auditedVertexShaders.front()) && audited(auditedVertexShaders.back()) && !audited(0) && !audited(1));
    // The Effect list: sorted, unique, and disjoint from the Lighting/Utility list; each group patches only from its own.
    assert(std::is_sorted(auditedEffectVertexShaders.begin(),auditedEffectVertexShaders.end()));
    assert(std::adjacent_find(auditedEffectVertexShaders.begin(),auditedEffectVertexShaders.end()) == auditedEffectVertexShaders.end());
    assert(auditedEffectVertexShaders.size() == 114);
    for (const auto hash : auditedEffectVertexShaders) assert(!audited(hash) && auditedFor(1,hash) && !auditedFor(0,hash));
    for (const auto hash : auditedVertexShaders) assert(auditedFor(0,hash) && !auditedFor(1,hash));
    assert(!auditedFor(1,0) && !auditedFor(0,0));
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
    // Second pass: both on by default (the keys are read only in mode 3), black, and Effect has its own key.
    assert(defaults.effect && defaults.voidColor && defaults.color.red == 0 && defaults.color.green == 0 && defaults.color.blue == 0);
    assert(classEnabled(defaults,0) && classEnabled(defaults,1) && classEnabled(defaults,effectClass));
    c = {}; c.effect = false; c.shaders = 1;
    assert(classEnabled(c,0) && !classEnabled(c,1) && !classEnabled(c,effectClass) && !classEnabled(c,7));
    c = {}; c.shaders = 7; assert(validate(c).shaders == 3); // iShaderCutShaders never reaches Effect
    // sVoidColor as read: parsed by validate; clamped or unreadable is reported once as FCS-66 by configure.
    c = {}; c.colorText = "0.25,1,0"; c = validate(c,&adjusted);
    assert(!adjusted && c.color.red == .25f && c.color.green == 1.0f && c.color.blue == 0.0f);
    c = {}; c.colorText = "-1,2,0.5"; c = validate(c,&adjusted);
    assert(adjusted && c.color.red == 0.0f && c.color.green == 1.0f && c.color.blue == .5f);
    c = {}; c.colorText = "grey"; c.color = {.5f,.5f,.5f}; c = validate(c,&adjusted);
    assert(adjusted && c.color.red == 0.0f && c.color.green == 0.0f && c.color.blue == 0.0f);
    c = {}; c.colorText = "nan,0,0"; assert(validate(c,&adjusted).color.red == 0.0f && adjusted);
    // Third pass: both on by default (read only in mode 3), and validate leaves them as set.
    assert(defaults.hideSky && defaults.effectWhole);
    c = {}; c.hideSky = false; c.effectWhole = false; c = validate(c,&adjusted);
    assert(!adjusted && !c.hideSky && !c.effectWhole);
    // Fourth pass: Stage 1 by default (read only in mode 3), gallery off, shadows on; an unknown style is Stage 1, reported.
    assert(defaults.style == stageOneStyle && !defaults.keepFarGallery && defaults.styleShadows && flatStyle == 0);
    c = {}; c.style = 0; c.keepFarGallery = true; c.styleShadows = false; c = validate(c,&adjusted);
    assert(!adjusted && c.style == flatStyle && c.keepFarGallery && !c.styleShadows);
    c = {}; c.style = 7; c = validate(c,&adjusted);
    assert(adjusted && c.style == stageOneStyle);
}

void colours() {
    Color out{9,9,9};
    assert(parseColor("0,0,0",out) == ColorText::ok && out.red == 0 && out.green == 0 && out.blue == 0);
    assert(parseColor("0.1,0.2,0.3",out) == ColorText::ok && out.red == .1f && out.green == .2f && out.blue == .3f);
    assert(parseColor(" 1 , 0.5 ,0 ",out) == ColorText::ok && out.red == 1.0f && out.green == .5f && out.blue == 0.0f);
    assert(parseColor("1e-1,1,0",out) == ColorText::ok && out.red == .1f);
    // Clamped to 0..1, the others kept.
    assert(parseColor("2,-1,0.5",out) == ColorText::clamped && out.red == 1.0f && out.green == 0.0f && out.blue == .5f);
    assert(parseColor("255,128,0",out) == ColorText::clamped && out.red == 1.0f && out.green == 1.0f && out.blue == 0.0f);
    // Unreadable: black, never a partial colour.
    for (const char* bad : {"","0,0","0,0,0,0","a,b,c","0;0;0","0,,0","nan,0,0","inf,0,0","0,0,0x","1,1,","#000000","0 0 0",",0,0"}) {
        out = {.5f,.5f,.5f};
        assert(parseColor(bad,out) == ColorText::invalid && out.red == 0 && out.green == 0 && out.blue == 0);
    }
}

// Every state the brief names: the cut's snapshot exists only for an eligible
// camera frame in an interior with the hooks installed and nothing latched,
// and the void colour and the Effect cut follow it. A pass is cut, and the
// void replaced, only on the main thread and with a fresh snapshot.
void gating() {
    using skycrpg::camera::State;
    using skycrpg::camera::fixed::Exit;
    using skycrpg::camera::fixed::rigEligible;
    constexpr std::array states{State::disabled,State::invalidWorld,State::special,State::menu,State::firstPerson,State::precision,
        State::targeting,State::obstruction,State::normal,State::targetLock};
    for (const State state : states)
        for (const bool interior : {false,true})
            for (const bool installed : {false,true})
                for (const bool latched : {false,true}) {
                    // endFrame publishes only for an eligible frame; any other frame deactivates.
                    const bool active = rigEligible(true,state) && publishes(installed,latched,interior,100.0f);
                    const bool expected = (state == State::normal || state == State::targetLock) && interior && installed && !latched;
                    assert(active == expected);
                    for (const bool main : {false,true})
                        for (const bool freshView : {false,true}) {
                            const Look look = lookDecision(main,active,latched,freshView);
                            assert((look == Look::applied) == (expected && main && freshView));
                            if (!main) assert(look == Look::offMain);
                            else if (latched) assert(look == Look::latched);
                            else if (!active) assert(look == Look::inactive);
                        }
                }
    // The rig off (F8), first person and menus are ineligible states; outdoors never publishes.
    assert(!rigEligible(true,State::disabled) && !rigEligible(true,State::firstPerson) && !rigEligible(true,State::menu) && !rigEligible(false,State::normal));
    assert(!publishes(true,false,false,0.0f) && !publishes(true,false,true,nan) && !publishes(true,false,true,infinity) && publishes(true,false,true,-2556.0f));
    // A lifecycle exit of any cause (Exit::disable .. Exit::evicted) clears `active` (fixed_spike.h lifecycle and endFrame, wiring
    // audit), so the next main-view clear is the engine's, with a fresh view or not, until an eligible frame publishes again.
    static_assert(static_cast<unsigned>(Exit::count) == 13);
    assert(lookDecision(true,false,false,true) == Look::inactive && lookDecision(true,false,false,false) == Look::inactive);
    assert(std::string(lookNames[static_cast<std::size_t>(Look::applied)]) == "applied" && lookNames.size() == 5);
    assert(std::string(featureNames[static_cast<std::size_t>(Feature::pending)]) == "pending" && featureNames.size() == 4);
}

void voidSite() {
    // The bytes as the binary has them (656D8B..656DAF) and SetClearColor's first 16.
    for (const std::uintptr_t base : {std::uintptr_t{0x140000000},std::uintptr_t{0x7ff612340000},std::uintptr_t{0x10000}}) {
        const std::uintptr_t site = base+voidCallRva,mainView = base+0x656d10,clearColor = base+0x100a3f0;
        assert(site-mainView == 0x9a && site-voidGuardedBefore == base+0x656d8b);
        assert(callTarget(site,voidSiteBytes.data()+voidGuardedBefore) == clearColor);
        assert(voidSiteCheck(base,mainView,clearColor,voidSiteBytes.data(),clearColorPrologue.data()) == 0);
        assert(voidSiteCheck(base,mainView+16,clearColor,voidSiteBytes.data(),clearColorPrologue.data()) == 1);
        assert(voidSiteCheck(base,mainView,clearColor+16,voidSiteBytes.data(),clearColorPrologue.data()) == 2);
        for (std::size_t i = 0;i < voidSiteBytes.size();++i) {
            auto window = voidSiteBytes;
            window[i] ^= 0x01;
            assert(voidSiteCheck(base,mainView,clearColor,window.data(),clearColorPrologue.data()) == 3);
        }
        // Already redirected (by us or anyone): the displacement differs, refused.
        auto redirected = voidSiteBytes;
        const std::int32_t elsewhere = *callDisplacement(site,base+0x1000);
        std::memcpy(redirected.data()+voidGuardedBefore+1,&elsewhere,sizeof(elsewhere));
        assert(voidSiteCheck(base,mainView,clearColor,redirected.data(),clearColorPrologue.data()) == 3);
        auto detoured = clearColorPrologue;
        detoured[0] = 0xe9;
        assert(voidSiteCheck(base,mainView,clearColor,voidSiteBytes.data(),detoured.data()) == 5);
    }
    // The call's own displacement is the binary's.
    std::int32_t original{};
    std::memcpy(&original,voidSiteBytes.data()+voidGuardedBefore+1,sizeof(original));
    assert(voidSiteBytes[voidGuardedBefore] == 0xe8 && original == 0x009b3641 && *callDisplacement(0x140656daa,0x14100a3f0) == original);
    // rel32 reach, both ways, and out of it.
    const std::uintptr_t site = 0x140656daa;
    assert(callDisplacement(site,site+5) == 0 && callDisplacement(site,site+5+0x7fffffff) && !callDisplacement(site,site+5+0x80000000ull));
    assert(callDisplacement(site,site+5-0x80000000ull) && !callDisplacement(site,site+5-0x80000001ull));
    for (const std::uintptr_t granularity : {std::uintptr_t{0x10000},std::uintptr_t{0x1000}}) {
        const Reach reach = relayReach(site,granularity);
        assert(reach.low % granularity == 0 && reach.high % granularity == 0 && reach.low < site && reach.high > site);
        assert(callDisplacement(site,reach.low) && callDisplacement(site,reach.high) && callDisplacement(site,reach.high+relaySize-1));
        assert(!callDisplacement(site,reach.low-0x80000000ull));
        assert(reach.low >= site+5-0x7fff0000ull && reach.high <= site+5+0x7fff0000ull);
    }
    const Reach early = relayReach(0x20000,0x10000);
    assert(early.low == 0x10000 && callDisplacement(0x20000,early.high));
    // jmp [rip+0] and the absolute target.
    const auto bytes = relay(0x7ff612345678ull);
    assert(bytes.size() == relaySize && bytes[0] == 0xff && bytes[1] == 0x25 && bytes[2] == 0 && bytes[5] == 0);
    std::uint64_t target{};
    std::memcpy(&target,bytes.data()+6,sizeof(target));
    assert(target == 0x7ff612345678ull);
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
    // FixedShader keeps the first pass's fields and order: byClass is Lighting,Utility even with Effect counts present.
    s.window.byClass = {20,11,5};
    assert(format(line,sizeof(line),s) && std::string(line).find("byClass=20,11 peak=") != std::string::npos);

    // FixedShaderLook: the void and the Effect cut, one row per window.
    LookSample k;
    k.seconds = 1234.5; k.frame = 99; k.voidState = Feature::installed; k.effectState = Feature::installed; k.color = {0,0,0};
    k.look.calls = {6,0,0,0,0}; k.look.haveNative = true; k.look.native = {.3765f,.4392f,.4f};
    k.effect.passes = 40; k.effect.cut = 7; k.effect.seconds = .000031; k.effect.created = 2; k.effect.otherView = 3; k.peak = 7; k.patched = 2; k.unaudited = 1;
    k.effect.skips[static_cast<std::size_t>(Skip::below)] = 12; k.effect.skips[static_cast<std::size_t>(Skip::actor)] = 4;
    assert(formatLook(line,sizeof(line),k));
    const std::string look = line;
    for (const char* field : {"FixedShaderLook t=1234.500 frame=99 void=installed ","voidApplied=6 voidInactive=0 voidLatched=0 voidStale=0 voidOffMain=0 ",
        "voidColor=0.000,0.000,0.000 nativeColor=0.377,0.439,0.400 effect=installed fxPasses=40 fxCut=7 fxPeak=7 fxShaders=2,0,1 fxCreated=2 ",
        "fxMicros=31.0 fxOffMain=0 fxOtherView=3 fxStale=0 fxSkipSkin=0 fxSkipBound=0 fxSkipBelow=12 fxSkipOut=0 fxSkipOwner=0 fxSkipActor=4 ",
        "fxSkipKind=0 fxSkipShader=0 fxSkipCap=0 sample=window"})
        assert(look.find(field) != std::string::npos);
    // Every token is name=value with a name the checker can key on, t and frame first.
    assert(look.find("  ") == std::string::npos && look.rfind("FixedShaderLook t=",0) == 0 && look.find(" frame=") < look.find(" void="));
    // Nothing applied: the colour reads nan; no call at all: the engine's reads nan too.
    LookSample quiet; quiet.voidState = Feature::off; quiet.effectState = Feature::pending; quiet.look.calls = {0,4,0,0,0};
    assert(formatLook(line,sizeof(line),quiet));
    assert(std::string(line).find("void=off voidApplied=0 voidInactive=4 ") != std::string::npos);
    assert(std::string(line).find("voidColor=nan,nan,nan nativeColor=nan,nan,nan effect=pending ") != std::string::npos);
    LookSample huge;
    huge.seconds = 1e7; huge.frame = ~0ull; huge.voidOffMain = huge.effectOffMain = ~0ull; huge.voidState = huge.effectState = Feature::refused;
    // Window counters as large as the FixedShader test allows; cumulative ones at their type's limit.
    huge.look.calls = {9999999999ull,9999999999ull,9999999999ull,9999999999ull,9999999999ull};
    huge.look.haveNative = true; huge.look.native = huge.color = {1,1,1};
    huge.effect.passes = huge.effect.cut = huge.effect.created = huge.effect.otherView = huge.effect.stale = 9999999999ull;
    for (auto& value : huge.effect.skips) value = 9999999999ull;
    huge.effect.seconds = 9.9;
    huge.peak = huge.patched = huge.rejected = huge.unaudited = ~0u;
    assert(formatLook(line,sizeof(line),huge) && std::strlen(line) < sizeof(TraceRow));
    std::printf("longest FixedShaderLook row: %zu of %zu bytes\n",std::strlen(line),sizeof(TraceRow));
    assert(!formatLook(tiny,sizeof(tiny),k));
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
    const std::array<unsigned,9> classSizes{2,4,18,6,9,606,127,339,2172}; // BloodSplatter .. Effect, Lighting, Utility, Water
    std::array<std::array<unsigned,static_cast<std::size_t>(Patch::count)>,2> outcome{}; // lighting, utility
    std::array<unsigned,static_cast<std::size_t>(Patch::count)> effectOutcome{};
    std::vector<std::uint64_t> hashes,effectHashes;
    unsigned blobs = 0,sums = 0,vertex = 0,productAgrees = 0,temporaryAgrees = 0,effectProducts = 0;
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
            if (cls == 5) {
                ++effectOutcome[static_cast<std::size_t>(made.result)];
                assert((made.result == Patch::ok) == auditedFor(1,codeHash(&d[p],total)));
                if (made.result == Patch::ok) {
                    effectHashes.push_back(codeHash(&d[p],total));
                    // Effect World is 3x4 in cb2[0..2], as in Lighting: the z row is cb2[2].
                    assert(made.worldRow == 2);
                    // Where the original itself takes dp4 cb2[2], rP (its world position) the patch's product is that one.
                    const std::size_t at0 = p+word(&d[p+containerHeader+8]);
                    Tokens original(word(&d[at0+4])/4);
                    std::memcpy(original.data(),&d[at0+8],original.size()*4);
                    Instruction step;
                    bool own = false;
                    for (std::size_t at = 2;at < original.size();at += step.length) {
                        assert(decode(original,at,step));
                        if ((original[at] & ~token::precise) == token::dot4Row && original[at+3] == token::row && original[at+4] == perGeometryBuffer &&
                            original[at+5] == made.worldRow && original[at+6] == token::tempRead && original[at+7] == made.position) own = true;
                    }
                    if (own) ++effectProducts;
                }
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
    std::sort(effectHashes.begin(),effectHashes.end());
    const std::size_t effectEntries = effectHashes.size();
    effectHashes.erase(std::unique(effectHashes.begin(),effectHashes.end()),effectHashes.end());
    std::printf("archive: effect ok=%u pattern=%u skinned=%u; %zu patchable entries, %zu distinct bytecodes; world-z product of its own in %u\n",
        effectOutcome[static_cast<std::size_t>(Patch::ok)],effectOutcome[static_cast<std::size_t>(Patch::pattern)],
        effectOutcome[static_cast<std::size_t>(Patch::skinned)],effectEntries,effectHashes.size(),effectProducts);
    assert(effectOutcome[static_cast<std::size_t>(Patch::ok)] == 332 && effectOutcome[static_cast<std::size_t>(Patch::skinned)] == 274 &&
        effectOutcome[static_cast<std::size_t>(Patch::pattern)] == 0);
    assert(effectEntries == 332 && effectHashes.size() == auditedEffectVertexShaders.size() &&
        std::equal(effectHashes.begin(),effectHashes.end(),auditedEffectVertexShaders.begin()));
    assert(effectProducts == 196);
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

// ---- third pass: the sky dome over the void, Effect passes clipped whole ----
// A copy of the audited bytes around the two guarded ranges.
struct SkyImage {
    Bytes head{skyModeHead.begin(),skyModeHead.end()};
    Bytes rule{skyRuleBytes.begin(),skyRuleBytes.end()};
};
void skyVoid() {
    // The facts the guard fixes: singleton pointer, SetMode, the layout its bytes read.
    assert(skyAddress.id == 401652 && skyAddress.rva == 0x31e0d80 && skyModeAddress.id == 26222 && skyModeAddress.rva == 0x411660);
    assert(skyRuleRva == 0x411794 && skyModeHead.size() == 47 && skyRuleBytes.size() == 57);
    assert(sky::root == 8 && sky::mode == 0x1bc && sky::flags == 0x1dc && sky::hideSky == 0x80 && sky::objectFlags == 0xf4 && sky::hidden == 1);
    // The bytes say what the constants say: mov eax,[rcx+1BC]; mov eax,[rdi+1DC] / shr 7; mov rcx,[rdi+8]; [rcx+F4] or 1 / and FFFFFFFE.
    const std::array<std::uint8_t,6> readMode{0x8b,0x81,0xbc,0x01,0x00,0x00},readFlags{0x8b,0x87,0xdc,0x01,0x00,0x00};
    const std::array<std::uint8_t,4> readRoot{0x48,0x8b,0x4f,0x08};
    const std::array<std::uint8_t,6> readObject{0x8b,0x81,0xf4,0x00,0x00,0x00},writeObject{0x89,0x81,0xf4,0x00,0x00,0x00};
    const auto has = [](const auto& hay,const auto& needle) {
        return std::search(hay.begin(),hay.end(),needle.begin(),needle.end()) != hay.end();
    };
    assert(has(skyModeHead,readMode) && has(skyRuleBytes,readFlags) && has(skyRuleBytes,readRoot) && has(skyRuleBytes,readObject) &&
        has(skyRuleBytes,writeObject));
    assert(skyModeHead[16] == 0x8d && skyModeHead[17] == 0x42 && skyModeHead[18] == 0xfe && skyModeHead[19] == 0x83 && skyModeHead[21] == 0x01); // lea eax,[rdx-2]; cmp eax,1
    assert(skyRuleBytes[13] == 0x07 && skyRuleBytes[45] == 0x01 && skyRuleBytes[50] == 0xfe); // shr 7; or 1; and FFFFFFFE
    // The check at three image bases.
    for (const std::uintptr_t base : {std::uintptr_t{0x140000000},std::uintptr_t{0x7ff600000000},std::uintptr_t{0x10000}}) {
        SkyImage image;
        const std::uintptr_t pointer = base+skyAddress.rva,setMode = base+skyModeAddress.rva;
        assert(skySiteCheck(base,pointer,setMode,image.head.data(),image.rule.data()) == 0);
        assert(skySiteCheck(base,pointer+8,setMode,image.head.data(),image.rule.data()) == 1);
        assert(skySiteCheck(base,pointer,setMode+16,image.head.data(),image.rule.data()) == 2);
        for (std::size_t i = 0;i < image.head.size();++i) {
            SkyImage changed; changed.head[i] ^= 0x40;
            assert(skySiteCheck(base,pointer,setMode,changed.head.data(),changed.rule.data()) == 3);
        }
        for (std::size_t i = 0;i < image.rule.size();++i) {
            SkyImage changed; changed.rule[i] ^= 0x01;
            assert(skySiteCheck(base,pointer,setMode,changed.head.data(),changed.rule.data()) == 4);
        }
        SkyImage detoured; detoured.head[0] = 0xe9; // a jump written over SetMode's first bytes
        assert(skySiteCheck(base,pointer,setMode,detoured.head.data(),detoured.rule.data()) == 3);
    }
    // SetMode's rule: modes 2 (dome only) and 3 (full) show the sky unless HideSky.
    for (std::uint32_t mode = 0;mode < 6;++mode)
        for (const std::uint32_t flags : {0u,sky::hideSky,0x100u,0x180u,0xffffff7fu})
            assert(engineShowsSky(mode,flags) == ((mode == 2u || mode == 3u) && !(flags & 0x80u)));
    // Every input combination of one frame's step, against the table in the policy.
    for (unsigned bits = 0;bits < 64;++bits) {
        const bool want = bits & 1u,held = bits & 2u,haveRoot = bits & 4u,sameRoot = bits & 8u,shows = bits & 16u,hiddenBit = bits & 32u;
        const SkyStep step = skyStep(want,held,haveRoot,sameRoot,shows,hiddenBit);
        SkyStep expected = SkyStep::none;
        if (want && haveRoot && shows) expected = held && !sameRoot ? SkyStep::leave : !hiddenBit ? SkyStep::hide : held ? SkyStep::keep : SkyStep::none;
        else if (held) expected = haveRoot && sameRoot && shows && hiddenBit ? SkyStep::restore : SkyStep::leave;
        assert(step == expected);
        // Invariants: never hide unless the cut wants it and the engine shows the sky; never
        // restore while wanted or something we did not hide; never write when not held and not wanted.
        if (step == SkyStep::hide) assert(want && haveRoot && shows && !hiddenBit);
        if (step == SkyStep::restore) assert(!want && held && haveRoot && sameRoot && shows && hiddenBit);
        if (!want && !held) assert(step == SkyStep::none);
        if (step == SkyStep::keep) assert(held && hiddenBit);
    }
    // A run of frames in a Show Sky interior, then a menu, then a cell change that runs SetMode.
    struct Root { std::uint32_t flags{0x0e}; } root,other;
    std::uint32_t mode = 2,skyFlags = 0;
    Root* heldRoot = nullptr;
    const auto frame = [&](bool want,Root* now) {
        const SkyStep step = skyStep(want,heldRoot != nullptr,now != nullptr,now == heldRoot,engineShowsSky(mode,skyFlags),now && (now->flags & sky::hidden));
        if (step == SkyStep::hide) { now->flags |= sky::hidden; heldRoot = now; }
        else if (step == SkyStep::restore) { now->flags &= ~sky::hidden; heldRoot = nullptr; }
        else if (step == SkyStep::leave) heldRoot = nullptr;
        return step;
    };
    assert(frame(true,&root) == SkyStep::hide && (root.flags & 1u) && frame(true,&root) == SkyStep::keep);
    assert(frame(false,&root) == SkyStep::restore && root.flags == 0x0e && frame(false,&root) == SkyStep::none);   // menu: back as it was
    assert(frame(true,&root) == SkyStep::hide);
    mode = 1; root.flags |= sky::hidden;                       // a door into a mode 1 interior: SetMode hides it itself
    assert(frame(false,&root) == SkyStep::leave && (root.flags & 1u) && frame(true,&root) == SkyStep::none && (root.flags & 1u));
    mode = 2; root.flags &= ~sky::hidden;                      // back into a Show Sky interior: SetMode shows it
    assert(frame(true,&root) == SkyStep::hide);
    skyFlags = sky::hideSky; root.flags |= sky::hidden;        // HideSky set by the engine under our hold
    assert(frame(false,&root) == SkyStep::leave && (root.flags & 1u));
    skyFlags = 0; root.flags &= ~sky::hidden;
    assert(frame(true,&root) == SkyStep::hide && frame(true,&other) == SkyStep::leave && (root.flags & 1u) && frame(true,&other) == SkyStep::hide);
    assert(frame(true,nullptr) == SkyStep::leave && frame(false,nullptr) == SkyStep::none);
    other.flags |= sky::hidden; heldRoot = nullptr;            // hidden by someone else: never taken over, never restored
    assert(frame(true,&other) == SkyStep::none && frame(false,&other) == SkyStep::none && (other.flags & 1u));
    // Effect passes clipped whole: -1 for every height, and never mistaken for a plane constant.
    for (const float z : {-1.0e7f,-1.0f,0.0f,1.0f,1.0e7f}) assert(clipDistance(wholeConstant,z) == -1.0f);
    assert(!usable(wholeConstant) && usable(cutConstant(100.0f,20.0f)));
    // Whole only when the bound centre is above the midpoint of feet and plane: the Embershard
    // glows (refs 0008F94E at 9392 and 0008F94D at 9423, plane 9420.25, feet 9241) go, the
    // floor mist (0008FB47 at 9153) keeps its lower part.
    View cave; cave.feet = {2487.3f,1485.5f,9241.0f}; cave.planeZ = 9420.25f;
    assert(wholeEffect(cave,{{2449,1197,9392},120}) && wholeEffect(cave,{{2196,859,9423},150}) && !wholeEffect(cave,{{2837,2373,9153},400}));
    assert(!wholeEffect(cave,{{0,0,9330.0f},10}) && wholeEffect(cave,{{0,0,9331.0f},10}) && !wholeEffect(cave,{{0,0,std::numeric_limits<float>::quiet_NaN()},10}));
    // Fourth pass (review MINOR-3): whole only when the bound's lowest point is above the feet too. The glows go
    // whole below radius 151 (centre 9392) and 182 (centre 9423); a ceiling-to-floor shaft centred high keeps the plane cut.
    assert(wholeEffect(cave,{{2449,1197,9392},150.5f}) && !wholeEffect(cave,{{2449,1197,9392},151.5f}));
    assert(wholeEffect(cave,{{2196,859,9423},181.5f}) && !wholeEffect(cave,{{2196,859,9423},182.5f}));
    assert(!wholeEffect(cave,{{0,0,9400},300}) && upperEffect(cave,{{0,0,9400},300}) && !upperEffect(cave,{{0,0,9300},10}));
    assert(!wholeEffect(cave,{{0,0,9400},std::numeric_limits<float>::quiet_NaN()}));
    // FixedShaderVoid: the row, its tokens and the longest it can be.
    char line[sizeof(TraceRow)]{},tiny[64]{};
    VoidSample v;
    v.seconds = 1234.5; v.frame = 99; v.skyState = Feature::installed; v.held = true; v.mode = 2; v.effectWhole = true;
    v.window.steps = {0,1,5,0,0}; v.window.whole = 14; v.window.note(0x0008f94e); v.window.note(0x0008f94d); v.window.note(0x0008f94e);
    assert(formatVoid(line,sizeof(line),v));
    const std::string row = line;
    assert(row == "FixedShaderVoid t=1234.500 frame=99 sky=installed skyMode=2 skyHeld=1 skyHides=1 skyKeeps=5 skyRestores=0 skyLeft=0 "
        "skyEngineHidden=0 skyNone=0 fxWhole=on fxWholeCut=14 fxCutRefs=0008F94E,0008F94D sample=window");
    VoidSample quiet; quiet.skyState = Feature::off;
    assert(formatVoid(line,sizeof(line),quiet));
    assert(std::string(line).find(" sky=off skyMode=-1 skyHeld=0 ") != std::string::npos && std::string(line).find(" fxWhole=off fxWholeCut=0 fxCutRefs=none ") != std::string::npos);
    VoidSample huge; huge.seconds = 1e7; huge.frame = ~0ull; huge.skyState = Feature::refused; huge.mode = -2147483647-1;
    for (auto& value : huge.window.steps) value = ~0ull;
    huge.window.engineHidden = huge.window.noSky = huge.window.whole = ~0ull;
    for (std::uint32_t id = 0xff000000u;id < 0xff000008u;++id) huge.window.note(id);
    assert(huge.window.refCount == effectRefSlots);
    assert(formatVoid(line,sizeof(line),huge) && std::strlen(line) < sizeof(TraceRow));
    assert(std::string(line).find("fxCutRefs=FF000000,FF000001,FF000002,FF000003 sample=window") != std::string::npos);
    std::printf("longest FixedShaderVoid row: %zu of %zu bytes\n",std::strlen(line),sizeof(TraceRow));
    assert(!formatVoid(tiny,sizeof(tiny),v));
    std::printf("sky and whole-Effect rows PASS\n");
}

namespace {
// ---- fourth pass: Stage 1 style ---------------------------------------------
// World boxes and poses are the Bannered Mare's (Skyrim.esm placements, the
// hearth pose of the W2S/W3 routes): feet (-117.1,-640,68), plane 228.
const Vec hearth{-117.1f,-640.0f,68.0f};
Vec eyeAt(Vec feet,unsigned heading) { // headings 45, 135, 225, 315: camera at -x-y, -x+y, +x+y, +x-y
    const float sx = heading == 0 || heading == 1 ? -1.0f : 1.0f,sy = heading == 0 || heading == 3 ? -1.0f : 1.0f;
    return {feet.x+237.3f*sx,feet.y+237.3f*sy,feet.z+683.8f};
}
Box box(Vec low,Vec high) { return worldBox(low,high,{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}},1.0f,{0,0,0}); }
void styleRules() {
    // Removal: a constant `usable` accepts, below zero at every height of this world; the plane constant unchanged.
    assert(usable(removeConstant) && !usable(wholeConstant));
    for (const float z : {-1.0e8f,-1.0e4f,0.0f,1.0e4f,1.0e8f}) assert(clipDistance(removeConstant,z) < 0.0f);
    assert(cutConstant(228.0f,752.0f).scale == -1.0f && cutConstant(228.0f,752.0f).offset == -524.0f);
    // Model families.
    assert(modelFamily("Architecture\\WhiteRun\\WRInteriors\\WRIntWallStr01.nif") == Family::architecture);
    assert(modelFamily("meshes/dungeons/mines/MineLHall1Way128D01.nif") == Family::dungeons && modelFamily("CLUTTER\\Barrel02.nif") == Family::clutter);
    assert(modelFamily("Furniture\\Noble\\NobleWallShelf02.nif") == Family::furniture && modelFamily("Effects\\FXMist.nif") == Family::effects);
    assert(modelFamily("MarkerX.nif") == Family::other && modelFamily(nullptr) == Family::other && modelFamily("") == Family::other);
    assert(modelFamily("Architecture") == Family::other && modelFamily("ArchitectureX\\a.nif") == Family::other);
    // worldBox: identity, a 90-degree turn as the engine's rows give it, scale and an off-centre OBND.
    const Box wall = box({-128,81,-32},{128,120,256});
    assert(wall.valid && wall.low.y == 81 && wall.high.z == 256);
    const Box turned = worldBox({-128,81,-32},{128,120,256},{Vec{0,1,0},Vec{-1,0,0},Vec{0,0,1}},2.0f,{100,200,64});
    assert(turned.valid && turned.low.x == 100+162 && turned.high.x == 100+240 && turned.low.y == 200-256 && turned.high.y == 200+256 &&
        turned.low.z == 0 && turned.high.z == 576);
    assert(!worldBox({0,0,0},{0,0,0},{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}},1,{0,0,0}).valid);
    assert(!worldBox({0,0,0},{1,1,1},{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}},0,{0,0,0}).valid && !box({1,0,0},{0,1,1}).valid);
    // Shapes: Mare OBND spans.
    assert(thinAxis(box({-128,81,-32},{128,120,256})) == Thin::y && thinAxis(box({-8,-128,0},{8,106,249})) == Thin::x);
    assert(thinAxis(box({-128,-128,-32},{120,120,256})) == Thin::none && thinAxis(box({-128,-128,246},{128,128,255})) == Thin::none);
    assert(memberShape(box({-31,-37,-32},{31,36,260})) && memberShape(box({-11,-256,140},{11,256,256})) && memberShape(box({-119,-10,241},{120,11,255})));
    assert(!memberShape(box({-128,81,-32},{128,120,256})) && !memberShape(box({-128,-128,-32},{120,120,256})) && !memberShape(box({-27,-27,0},{27,27,80})));
    assert(pieceOf(Kind::architecture,Family::architecture,wall,true) == Piece::kit);
    assert(pieceOf(Kind::architecture,Family::architecture,box({-31,-37,-32},{31,36,260}),true) == Piece::member);
    assert(pieceOf(Kind::architecture,Family::clutter,wall,true) == Piece::object && pieceOf(Kind::architecture,Family::furniture,wall,true) == Piece::object);
    assert(pieceOf(Kind::object,Family::other,box({0,0,0},{1,1,1}),false) == Piece::object && pieceOf(Kind::excluded,Family::architecture,wall,true) == Piece::flat);
    // No usable box (no 3D, zero bound): the flat cut, never a whole-object decision (review of 30503e0, MINOR-1).
    assert(pieceOf(Kind::object,Family::other,Box{},false) == Piece::flat && pieceOf(Kind::architecture,Family::clutter,Box{},false) == Piece::flat);
    for (const Family f : {Family::dungeons,Family::landscape,Family::effects,Family::other}) assert(pieceOf(Kind::architecture,f,wall,true) == Piece::flat);
    assert(pieceOf(Kind::architecture,Family::architecture,wall,false) == Piece::flat && pieceOf(Kind::architecture,Family::architecture,Box{},true) == Piece::flat);

    // Sides at the four headings. Axis-aligned walls (thin y north and south of the player, thin x west and east).
    const Box north = box({-256,593,32},{0,632,320}),south = box({-256,-1145,32},{0,-1119,320});
    const Box west = box({-776,-900,32},{-760,-400,320}),east = box({500,-900,32},{516,-400,320});
    const std::array<std::array<Side,4>,4> walls{{
        {Side::back,Side::front,Side::front,Side::back},   // north wall at headings 45, 135, 225, 315
        {Side::front,Side::back,Side::back,Side::front},   // south
        {Side::front,Side::front,Side::back,Side::back},   // west
        {Side::back,Side::back,Side::front,Side::front}}}; // east
    const std::array<Box,4> wallBoxes{north,south,west,east};
    for (unsigned h = 0;h < 4;++h) {
        const Vec eye = eyeAt(hearth,h);
        for (std::size_t i = 0;i < 4;++i) assert(sideOf(wallBoxes[i],thinAxis(wallBoxes[i]),hearth,eye,sideEnter) == walls[i][h]);
        // A back wall never meets the sightline, at any height; a front wall would.
        for (std::size_t i = 0;i < 4;++i) if (walls[i][h] == Side::back) assert(!inLane(wallBoxes[i],hearth,eye,0.0f));
    }
    // A corner (L-shaped piece, square box) north-east of the player: diagonal rule.
    const Box corner = box({0,384,32},{248,632,320});
    assert(thinAxis(corner) == Thin::none);
    const std::array<Side,4> cornerSides{Side::back,Side::front,Side::front,Side::back};
    // A corner next to the player (its box ends 20 units from the feet on x) is across at headings where the diagonal splits it.
    assert(sideOf(box({-97.1f,-700,32},{150,-500,320}),Thin::none,hearth,eyeAt(hearth,1),sideEnter) == Side::across);
    for (unsigned h = 0;h < 4;++h) assert(sideOf(corner,Thin::none,hearth,eyeAt(hearth,h),sideEnter) == cornerSides[h]);
    // A diagonal piece (a wall turned 45 degrees: square-ish box, no thin axis).
    const float c45 = std::sqrt(.5f);
    const Box diagonal = worldBox({-128,-8,0},{128,8,256},{Vec{c45,c45,0},Vec{-c45,c45,0},Vec{0,0,1}},1.0f,{-500,-800,68}); // south-west of the player
    assert(diagonal.valid && thinAxis(diagonal) == Thin::none);
    assert(sideOf(diagonal,Thin::none,hearth,eyeAt(hearth,0),sideEnter) == Side::front && sideOf(diagonal,Thin::none,hearth,eyeAt(hearth,2),sideEnter) == Side::back);
    // A piece spanning the player (a beam crossing over, the hearth's own floor alcove): across at every heading.
    const Box spanning = box({-512,-651,204},{256,-629,320});
    for (unsigned h = 0;h < 4;++h) assert(sideOf(spanning,thinAxis(spanning),hearth,eyeAt(hearth,h),sideEnter) == Side::across);
    // Review of 2f5d9f6, MAJOR-1: a back wall (256 x 39 x 288) 60, 100, 150 and 200 units behind the player, at every heading,
    // is back on its OBND and its own geometry (sphere at the box centre, radius the half-diagonal, ~194) does not exceed the
    // box, so it is kept whole; a pass whose sphere centre lies outside the box, or whose radius is much larger, is not.
    for (unsigned h = 0;h < 4;++h) {
        const Vec eye = eyeAt(hearth,h);
        const float sy = eye.y > hearth.y ? -1.0f : 1.0f; // the wall stands beyond the player on y, away from the camera
        for (const float gap : {60.0f,100.0f,150.0f,200.0f}) {
            const float nearY = hearth.y+sy*gap,farY = nearY+sy*39.0f;
            const Box backWall = box({hearth.x-128,(std::min)(nearY,farY),hearth.z-32},{hearth.x+128,(std::max)(nearY,farY),hearth.z+256});
            assert(thinAxis(backWall) == Thin::y && sideOf(backWall,Thin::y,hearth,eye,sideEnter) == Side::back);
            const Vec centre{(backWall.low.x+backWall.high.x)*.5f,(backWall.low.y+backWall.high.y)*.5f,(backWall.low.z+backWall.high.z)*.5f};
            const Spans w = spansOf(backWall);
            const float half = .5f*std::sqrt(w.x*w.x+w.y*w.y+w.z*w.z);
            assert(half > 190.0f && half < 200.0f);
            assert(!geometryExceeds(backWall,centre,half) && !geometryExceeds(backWall,{centre.x+100,centre.y,centre.z},half+10));
            Facts f; f.kitCell = true; f.piece = Piece::kit; f.side = Side::back;
            assert(decide(f).bucket == Bucket::keptFar);
            assert(geometryExceeds(backWall,{centre.x,centre.y-sy*80.0f,centre.z},50.0f));   // centre 60 units in front of the box
            assert(geometryExceeds(backWall,centre,half+40.0f));                           // a geometry larger than its OBND
            assert(geometryExceeds(backWall,{backWall.high.x+20,centre.y,centre.z},10.0f) && !geometryExceeds(backWall,{backWall.high.x+15,centre.y,centre.z},10.0f));
        }
    }
    assert(geometryExceeds(Box{},{0,0,0},1) && geometryExceeds(box({0,0,0},{1,1,1}),{nan,0,0},1));
    // A wall in line with the player (the Mare's WRIntWallStr01WinA west of the hearth: its thin y range ends 8 units from
    // the feet) falls back to the diagonal: front at 45, back at 225.
    const Box inLine = box({-768,-632,32},{-512,-593,320});
    assert(thinAxis(inLine) == Thin::y && alongAxis(inLine.low.y,inLine.high.y,hearth.y,-237.3f,sideEnter) == Side::across);
    assert(sideOf(inLine,Thin::y,hearth,eyeAt(hearth,0),sideEnter) == Side::front && sideOf(inLine,Thin::y,hearth,eyeAt(hearth,2),sideEnter) == Side::back);
    // A camera level with the player on an axis (mid-turn): both sides of that axis are back.
    assert(alongAxis(10,20,0,0.5f,1) == Side::back && alongAxis(-20,-10,0,-0.5f,1) == Side::back && alongAxis(-5,5,0,100,1) == Side::across);
    assert(sideOf(Box{},Thin::none,hearth,eyeAt(hearth,0),sideEnter) == Side::across && diagonalSide(north,hearth,hearth,1) == Side::across);
    assert(quadrantOf(hearth,eyeAt(hearth,0)) == 0 && quadrantOf(hearth,eyeAt(hearth,1)) == 2 && quadrantOf(hearth,eyeAt(hearth,2)) == 3 &&
        quadrantOf(hearth,eyeAt(hearth,3)) == 1 && quadrantOf(hearth,{nan,0,0}) == -1);

    // Hysteresis: the south wall's thin side is 1119; a player walking toward it keeps "front" until inside its extent.
    const Vec eye0 = eyeAt(hearth,0);
    const auto at = [](float y) { return Vec{-117.1f,y,68.0f}; };
    const auto eyeFor = [](Vec feet) { return Vec{feet.x-237.3f,feet.y-237.3f,feet.z+683.8f}; };
    assert(sideOf(south,Thin::y,at(-1100),eyeFor(at(-1100)),sideEnter) == Side::across);           // 19 units: too close to take a side
    assert(settledSide(south,Thin::y,at(-1100),eyeFor(at(-1100)),Side::front,true) == Side::front); // but kept while outside its extent
    assert(settledSide(south,Thin::y,at(-1130),eyeFor(at(-1130)),Side::front,true) == Side::across); // the player is inside it: crossed
    assert(settledSide(south,Thin::y,at(-1100),eyeFor(at(-1100)),Side::front,false) == Side::across); // a snap starts without memory
    assert(settledSide(south,Thin::y,at(-1180),eyeFor(at(-1180)),Side::across,true) == Side::back);   // crossed to the other side
    assert(settledSide(north,Thin::y,hearth,eye0,Side::front,true) == Side::back);                    // a stale class is never kept
    // The lane: from 24 units toward the camera, boxes grown sideways only, enter at 24, stay at 48.
    const Box post = box({-200,-760,32},{-150,-710,320}); // a pillar between the player and the camera at heading 45
    assert(inLane(post,hearth,eye0,laneEnter) && !inLane(post,hearth,eyeAt(hearth,2),0.0f) && sideOf(post,Thin::none,hearth,eyeAt(hearth,2),sideEnter) == Side::back);
    const Box beside = box({-117.1f+15.0f,-700,32},{-117.1f+65.0f,-650,320}); // 32 units east of where the lane leaves the body
    assert(!inLane(beside,hearth,eye0,laneEnter) && settledLane(beside,hearth,eye0,true) && !settledLane(beside,hearth,eye0,false));
    assert(segmentHitsBox({0,0,0},{10,0,0},box({4,-1,-1},{6,1,1}),0) && !segmentHitsBox({0,0,0},{10,0,0},box({4,2,-1},{6,3,1}),0) &&
        segmentHitsBox({0,0,0},{10,0,0},box({4,2,-1},{6,3,1}),2.5f) && !segmentHitsBox({0,0,0},{10,0,0},Box{},100));
    assert(!segmentHitsBox({0,0,0},{10,0,0},box({4,-1,2},{6,1,3}),2.5f)); // never grown in z
    // Review of 30503e0, MAJOR-1: a low stool (30 x 30 x 45) and a tall cabinet (60 x 40 x 200) beside the player, their
    // near faces 30 and 60 units from the feet along both diagonals perpendicular to the view, are not in the lane at any
    // heading (at 30 they would only stay in it if already in it), so they are kept whole; the same pieces between camera and
    // player are removed.
    const auto rule0 = [](Side side,bool lane) { Facts f; f.piece = Piece::object; f.side = side; f.lane = lane; f.kitCell = true; return decide(f); };
    for (unsigned h = 0;h < 4;++h) {
        const Vec eye = eyeAt(hearth,h);
        const float cx = (eye.x-hearth.x)/335.6f,cy = (eye.y-hearth.y)/335.6f; // unit camera direction in the ground plane
        for (const Vec size : {Vec{30,30,45},Vec{60,40,200}})
            for (const float gap : {30.0f,60.0f})
                for (const float turn : {-1.0f,1.0f}) {
                    const float px = -cy*turn,py = cx*turn,reachOut = gap+size.x*.5f*std::abs(px)+size.y*.5f*std::abs(py);
                    const Vec centre{hearth.x+px*reachOut,hearth.y+py*reachOut,hearth.z};
                    const Box piece = box({centre.x-size.x*.5f,centre.y-size.y*.5f,hearth.z},{centre.x+size.x*.5f,centre.y+size.y*.5f,hearth.z+size.z});
                    // Never enters; one already in the lane (it was walked into) leaves it beyond 48.
                    assert(!inLane(piece,hearth,eye,laneEnter) && inLane(piece,hearth,eye,laneExit) == (gap < laneExit));
                    const Decision d = rule0(sideOf(piece,thinAxis(piece),hearth,eye,sideEnter),settledLane(piece,hearth,eye,false));
                    assert(d.treat == Treat::keep && d.bucket == Bucket::keptObject);
                }
        // Between: the stool 40 units toward the camera, the cabinet 100.
        for (const auto& [size,distance] : {std::pair{Vec{30,30,45},40.0f},std::pair{Vec{60,40,200},100.0f}}) {
            const Vec centre{hearth.x+cx*distance,hearth.y+cy*distance,hearth.z};
            const Box piece = box({centre.x-size.x*.5f,centre.y-size.y*.5f,hearth.z},{centre.x+size.x*.5f,centre.y+size.y*.5f,hearth.z+size.z});
            assert(inLane(piece,hearth,eye,laneEnter));
            const Decision d = rule0(sideOf(piece,thinAxis(piece),hearth,eye,sideEnter),true);
            assert(d.treat == Treat::remove && d.bucket == Bucket::removedObject);
        }
        // The stool 100 units toward the camera is under the sightline (the camera is 684 up): not in the lane.
        const Vec far100{hearth.x+cx*100,hearth.y+cy*100,hearth.z};
        assert(!inLane(box({far100.x-15,far100.y-15,hearth.z},{far100.x+15,far100.y+15,hearth.z+45}),hearth,eye,laneEnter));
    }

    // Decisions. Objects on each side; members; kit pieces; the storey above; the gallery switch; not a kit cell.
    const auto rule = [](Piece piece,Side side,bool lane,bool above,bool overhead,bool gallery,bool kit = true,bool around = false,bool low = false,
        bool floor = false,bool interact = false) {
        Facts f; f.piece = piece; f.side = side; f.lane = lane; f.above = above; f.overhead = overhead; f.gallery = gallery; f.kitCell = kit;
        f.around = around; f.low = low; f.floor = floor; f.interact = interact;
        return decide(f);
    };
    const auto is = [](Decision d,Treat t,Bucket b) { return d.treat == t && d.bucket == b; };
    for (const Piece piece : {Piece::flat,Piece::kit,Piece::member,Piece::object})
        for (const Side side : {Side::across,Side::front,Side::back})
            for (unsigned bits = 0;bits < 16;++bits) {
                const Decision d = rule(piece,side,bits & 1u,bits & 2u,bits & 4u,bits & 8u,false);
                assert(is(d,Treat::flat,Bucket::flat)); // caves and ruins: the flat cut, whatever the piece
                const Decision k = rule(piece,side,bits & 1u,bits & 2u,bits & 4u,bits & 8u);
                // Never sliced: objects and members; never anything on the back side, except a piece the policy cannot place.
                if (piece == Piece::object || piece == Piece::member) assert(k.treat == Treat::keep || k.treat == Treat::remove);
                if (side == Side::back && piece != Piece::flat) assert(k.treat == Treat::keep || (k.treat == Treat::remove && !(bits & 8u)));
                if (piece == Piece::flat) assert(is(k,Treat::flat,Bucket::fallback));
            }
    assert(is(rule(Piece::object,Side::front,true,false,false,false),Treat::remove,Bucket::removedObject));
    // Around the player (review of 30503e0, MINOR-2): a low piece (a rug, a bench, a brazier: top at most feet + 120) is kept;
    // a tall one follows the lane rule (the Mare hearth fire, Campfire01Burning, box 580 x 601 x 502 around the hearth pose,
    // is removed while its box meets the lane); a lamp hanging over the player goes.
    assert(is(rule(Piece::object,Side::across,true,false,false,false,true,true,true),Treat::keep,Bucket::keptObject));
    assert(is(rule(Piece::object,Side::across,true,false,false,false,true,true,false),Treat::flat,Bucket::aroundCut)); // tall, around: flat cut
    // Campfire01Burning at the hearth pose (OBND -140..140 x -196..140 x -97..267 scaled to its Skyrim.esm box 580 x 601 x 502):
    // around the feet, taller than 120 above them, bottom below feet + 128: the flat cut at all four headings, never removed.
    {
        const Box fire = box({-433,-717,-101},{147,-116,401});
        for (unsigned h = 0;h < 4;++h) {
            const Vec eye = eyeAt(hearth,h);
            Facts f; f.kitCell = true; f.piece = Piece::object; f.side = sideOf(fire,thinAxis(fire),hearth,eye,sideEnter);
            f.lane = settledLane(fire,hearth,eye,false); f.around = true; f.low = fire.high.z <= hearth.z+laneHigh;
            f.overhead = fire.low.z >= hearth.z+storeyHeadroom; f.above = fire.low.z >= 310.0f-aboveTolerance;
            const Decision d = decide(f);
            assert(f.lane && !f.low && !f.overhead && d.treat == Treat::flat && d.bucket == Bucket::aroundCut);
        }
    }
    assert(is(rule(Piece::object,Side::across,false,false,false,false,true,true,false),Treat::flat,Bucket::aroundCut)); // in the lane or not
    assert(is(rule(Piece::member,Side::across,true,false,false,false,true,true,true),Treat::keep,Bucket::keptMember));
    // The floor under the player (MINOR-4: arriving on the loft, its floor is above the old storey line until the base
    // moves) is never removed: the flat cut, which does not reach it.
    assert(is(rule(Piece::kit,Side::across,false,true,false,false,true,true,true,true),Treat::flat,Bucket::fallback));
    assert(is(rule(Piece::kit,Side::across,false,true,false,false,true,true,true,false),Treat::remove,Bucket::removedWhole));
    // Interactables are never removed whole (MINOR-5): the flat cut at the plane instead, counted apart; kept stays kept.
    assert(is(rule(Piece::object,Side::front,true,false,false,false,true,false,false,false,true),Treat::flat,Bucket::interactCut));
    assert(is(rule(Piece::object,Side::back,false,true,false,false,true,false,false,false,true),Treat::flat,Bucket::interactCut));
    assert(is(rule(Piece::object,Side::front,false,false,false,false,true,false,false,false,true),Treat::keep,Bucket::keptObject));
    assert(is(rule(Piece::object,Side::back,false,true,false,true,true,false,false,false,true),Treat::keep,Bucket::keptGallery));
    // Never removed when interactable or the floor, whatever else holds.
    for (unsigned bits = 0;bits < 512;++bits) {
        Facts f; f.kitCell = true; f.piece = (bits & 1u) ? Piece::object : (bits & 2u) ? Piece::member : Piece::kit;
        f.side = (bits & 4u) ? Side::front : (bits & 8u) ? Side::back : Side::across;
        f.lane = bits & 16u; f.above = bits & 32u; f.overhead = bits & 64u; f.around = bits & 128u; f.low = bits & 256u;
        for (const bool interact : {false,true}) for (const bool floor : {false,true}) {
            f.interact = interact; f.floor = floor;
            if (interact || floor) assert(decide(f).treat != Treat::remove);
        }
    }
    assert(is(rule(Piece::object,Side::across,true,false,true,false,true,true),Treat::remove,Bucket::removedObject));
    assert(is(rule(Piece::object,Side::across,false,true,false,false,true,true),Treat::remove,Bucket::removedObject)); // upper floor wins
    assert(is(rule(Piece::object,Side::across,true,false,false,false),Treat::remove,Bucket::removedObject));
    assert(is(rule(Piece::object,Side::front,false,false,false,false),Treat::keep,Bucket::keptObject));
    assert(is(rule(Piece::object,Side::back,true,false,false,false),Treat::keep,Bucket::keptObject));
    assert(is(rule(Piece::object,Side::back,false,true,false,false),Treat::remove,Bucket::removedObject));   // on the upper floor
    assert(is(rule(Piece::member,Side::front,true,false,false,false),Treat::remove,Bucket::removedWhole));   // a pillar in the way
    assert(is(rule(Piece::member,Side::back,false,false,false,false),Treat::keep,Bucket::keptMember));       // a pillar behind
    assert(is(rule(Piece::member,Side::back,false,false,true,false),Treat::remove,Bucket::removedWhole));    // a beam overhead
    assert(is(rule(Piece::member,Side::back,false,false,true,true),Treat::keep,Bucket::keptGallery));
    assert(is(rule(Piece::kit,Side::back,true,false,false,false),Treat::keep,Bucket::keptFar));
    assert(is(rule(Piece::kit,Side::front,false,false,false,false),Treat::low,Bucket::slicedNear)); // near walls: at the plane, as now
    assert(is(rule(Piece::kit,Side::front,true,false,false,false),Treat::low,Bucket::slicedNear));
    assert(is(rule(Piece::kit,Side::across,false,false,false,false),Treat::slice,Bucket::slicedSpan));
    assert(is(rule(Piece::kit,Side::back,false,true,false,false),Treat::remove,Bucket::removedWhole));      // the upper storey
    assert(is(rule(Piece::kit,Side::back,false,true,false,true),Treat::keep,Bucket::keptGallery));          // ... its back half kept
    assert(is(rule(Piece::kit,Side::front,false,true,false,true),Treat::remove,Bucket::removedWhole));
    assert(is(rule(Piece::kit,Side::across,false,true,false,true),Treat::remove,Bucket::removedWhole));
    assert(sliceHeight(Treat::slice,228,310) == 310 && sliceHeight(Treat::low,228,310) == 228 && sliceHeight(Treat::low,228,200) == 200 &&
        sliceHeight(Treat::flat,228,310) == 228);

    // The storey line at the hearth: the loft floor (WRIntLoftMid01, bottom 310) within 512; beams, low pieces and far slabs never.
    const float base = 68.0f,planeZ = 228.0f;
    const Box loft = box({-256,-384,310},{0,-128,319});
    assert(storeyCandidate(Piece::kit,loft,hearth,base) && !storeyCandidate(Piece::member,loft,hearth,base) && !storeyCandidate(Piece::object,loft,hearth,base));
    assert(!storeyCandidate(Piece::kit,box({-256,-384,190},{0,-128,199}),hearth,base));      // under head height
    assert(!storeyCandidate(Piece::kit,box({-256,-384,900},{0,-128,909}),hearth,base));      // above 768
    assert(!storeyCandidate(Piece::kit,box({600,-384,310},{856,-128,319}),hearth,base));     // 717 away
    assert(!storeyCandidate(Piece::kit,box({-256,-140,310},{0,-128,319}),hearth,base));      // a 12-unit strip
    assert(boxReach(loft,hearth) == 256.0f && boxReach(box({-200,-700,0},{0,-600,1}),hearth) == 0.0f && boxReach(box({600,-640,0},{700,-600,1}),hearth) > 716.0f);
    Storey storey;
    assert(storey.line(planeZ) == planeZ);
    storey.update(infinity,base,planeZ,0.0);                 // first frame in the cell: nothing drawn yet
    assert(!storey.known && storey.line(planeZ) == planeZ);
    storey.update(310,base,planeZ,0.02);                     // the first storey seen is taken at once
    assert(storey.known && storey.line(planeZ) == 310);
    storey.update(314,base,planeZ,0.04);                     // within 8: nothing moves
    assert(storey.line(planeZ) == 310);
    storey.update(500,base,planeZ,0.06);                     // higher: only after 0.6 s
    assert(storey.line(planeZ) == 310);
    storey.update(500,base,planeZ,0.5);
    assert(storey.line(planeZ) == 310);
    storey.update(500,base,planeZ,0.7);
    assert(storey.line(planeZ) == 500);
    storey.update(300,base,planeZ,0.72);                     // lower: at once (more removed: the safe way)
    assert(storey.line(planeZ) == 300);
    storey.update(infinity,base,planeZ,0.74);                // none for a moment: the line holds
    assert(storey.line(planeZ) == 300);
    storey.update(300,base,planeZ,0.76);                     // back again: the hold restarts
    storey.update(infinity,base,planeZ,1.0);
    assert(storey.line(planeZ) == 300);
    storey.update(infinity,base,planeZ,1.7);
    assert(!storey.known && storey.line(planeZ) == planeZ);  // gone for 0.6 s: back to the plane
    const unsigned long long moves = storey.moves;
    storey.update(555,321.5f,481.5f,1.72);                   // up the stairs: a new storey base, taken at once
    assert(storey.known && storey.line(481.5f) == 555 && storey.moves == moves+1u);
    storey.reset();
    assert(!storey.primed && !storey.known && storey.moves == 0);

    // The cell census: Mare, Embershard Mine, Bleak Falls Barrow (ESM counts).
    assert(cellStyle({212,0,319}) == CellStyle::kit && cellStyle({9,622,697}) == CellStyle::flat && cellStyle({0,1444,1700}) == CellStyle::flat);
    assert(cellStyle({}) == CellStyle::unknown && cellStyle({15,0,15}) == CellStyle::flat && cellStyle({16,16,40}) == CellStyle::flat &&
        cellStyle({17,16,40}) == CellStyle::kit);

    // Style 0 is the flat cut: no decision is ever taken, and a flat decision cuts at the plane with the plane constant.
    assert(sliceHeight(Treat::flat,planeZ,555) == planeZ && cutConstant(sliceHeight(Treat::flat,planeZ,555),752).offset == cutConstant(planeZ,752).offset);

    // FixedShaderStyle: tokens, nan forms, the longest row; FixedShaderVoid unchanged by the new window fields.
    char line[sizeof(TraceRow)]{};
    StyleSample st;
    st.seconds = 1234.5; st.frame = 99; st.style = 1; st.state = Feature::installed; st.cell = CellStyle::kit; st.census = {212,0,319};
    st.shadows = true; st.haveView = st.storeyKnown = true; st.storeyZ = 310; st.base = 68; st.lineZ = 310; st.refs = 180; st.moves = 2;
    st.window.frames = 6; st.window.buckets[static_cast<std::size_t>(Bucket::keptFar)] = 40; st.window.buckets[static_cast<std::size_t>(Bucket::removedWhole)] = 300;
    st.window.slicedFar = 0; st.window.shadowRemoved = 12; st.fxFloorKept = 1; st.fxWholeRadius = 120.25f;
    assert(formatStyle(line,sizeof(line),st));
    const std::string row = line;
    for (const char* field : {"FixedShaderStyle t=1234.500 frame=99 style=1 state=installed cell=kit kit=212 dungeon=0 gallery=0 shadows=1 frames=6 ",
        "storeyZ=310.00 relStorey=242.00 lineZ=310.00 keptFar=40 keptObjects=0 ","slicedFar=0 slicedObjects=0 removedWhole=300 removedObjects=0 ",
        "refs=180 ","storeyMoves=2 ","shadowRemoved=12 ","fxFloorKept=1 fxWholeRadius=120.2 interactCut=0 sample=window"})
        assert(row.find(field) != std::string::npos);
    StyleSample idle; idle.state = Feature::refused;
    assert(formatStyle(line,sizeof(line),idle) && std::string(line).find("state=refused cell=unknown") != std::string::npos &&
        std::string(line).find("storeyZ=nan relStorey=nan lineZ=nan ") != std::string::npos);
    // As the FixedShader row's longest: ten-digit window counters (a 0.1 s window), the widest values of the rest, except
    // the census (at most a cell's references: five digits) and refs (at most the 4,096 table slots).
    const unsigned long long most = 9999999999ull;
    StyleSample big; big.seconds = 1e7; big.frame = ~0ull; big.style = 1; big.census = {99999u,99999u,99999u}; big.haveView = big.storeyKnown = true;
    big.state = Feature::installed; big.cell = CellStyle::unknown; big.gallery = big.shadows = true;
    big.storeyZ = big.lineZ = -1e7f; big.base = 1e7f; big.refs = 4096u; big.moves = most; big.fxFloorKept = most; big.fxWholeRadius = 1e7f;
    for (auto& value : big.window.buckets) value = most;
    big.window.frames = big.window.underLine = big.window.slicedLow = big.window.slicedFar = big.window.slicedObjects = big.window.tableFull = most;
    big.window.storeyUploads = big.window.shadowPasses = big.window.shadowRemoved = big.window.shadowSkips = most;
    assert(formatStyle(line,sizeof(line),big) && std::strlen(line) < sizeof(TraceRow));
    std::printf("longest FixedShaderStyle row: %zu of %zu bytes\n",std::strlen(line),sizeof(TraceRow));
    VoidSample v; v.seconds = 1; v.skyState = Feature::installed;
    char before[sizeof(TraceRow)]{};
    assert(formatVoid(before,sizeof(before),v));
    v.window.floorKept = 5; v.window.wholeRadius = 300;
    assert(formatVoid(line,sizeof(line),v) && std::string(line) == before);
    std::printf("style rules PASS\n");
}
} // namespace

int main() {
    codes();
    settings();
    view();
    plane();
    selection();
    colours();
    gating();
    voidSite();
    checksums();
    patchSynthetic();
    patchRejects();
    traces();
    skyVoid();
    styleRules();
    if (!archive()) std::printf("archive rows skipped: SKYCRPG_SHADER_ARCHIVE is not set\n");
}
