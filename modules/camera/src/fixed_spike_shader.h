// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike.h. P5.5-02 wave 2 package "shader" (DYL-217) adapter:
// [FixedCameraSpike] iSeeThroughMode=3. Nothing here is allocated, installed
// or evaluated in any other mode.
//
// What a cut pass is. Between SetupGeometry and RestoreGeometry of one pass
// the device has (a) a patched copy of the vertex shader the engine bound for
// the technique and (b) one 16-byte constant buffer in vertex slot 13. The
// engine's own objects and its render state are never written: the patched
// shaders are separate device objects, and slot 13 is one no engine shader
// reads and the engine never binds. RestoreGeometry puts the engine's shader
// back and empties the slot; the engine also clears all device state at the
// start of every frame (RVA 1009E72).
//
// Threads. The camera frame service publishes the plane on Main::threadID.
// The hooks (BSShader slots 6 and 7 of Lighting, Utility and Effect, and the
// void colour's call site) are expected on the same thread, because the
// engine draws through one immediate device context; that is checked on
// every call. A call on another thread touches nothing and stays native
// (FCS-71 Lighting/Utility, FCS-88 Effect, FCS-83 void; logged once). Other
// threads only clear `active`.
//
// Second pass (owner decision 2026-11-10): Effect passes are cut exactly like
// Lighting passes but counted apart (group 1, the FixedShaderLook row), and
// while the cut is active the main view clears to the configured colour
// (fixed_spike_shader_void.h). Both stay native whenever the cut is inactive.
// The void colour is this mode's only change to engine code: the displacement
// of one call, made once at install; nothing in this file writes code.
//
// Third pass: in Show Sky interiors the engine draws the sky dome over the
// void; while the cut is active its root is hidden as the engine hides it in
// other interiors (fixed_spike_shader_sky.h), and a selected Effect pass is
// clipped whole instead of at the plane (bShaderCutEffectWhole).
//
// Fourth pass (owner decision 2026-11-10, Stage 1 seamless cutaway): with
// iCutStyle=1 a Lighting or Utility pass of the published view goes through
// the per-reference policy of fixed_spike_shader_style.h instead of the one
// plane; iCutStyle=0 leaves this file's flat cut exactly as it was.
//
// Fifth pass (owner go 2026-11-11, Stage 2 of the seamless cutaway): with
// bFramePass=1 one full-screen pass of our own is drawn over the finished
// scene after the main world render returns (fixed_spike_shader_frame.h); this
// file only captures the main view's matrices during a pass of the published
// view and calls its install, frame-start check and trace.
#include "fixed_spike_shader_policy.h"
#include <cwchar>

namespace skycrpg::camera::fixed {
// Defined in fixed_spike.h (hide section).
inline Kind kindOf(const RE::TESObjectREFR* ref,const RE::TESBoundObject* base);
}
namespace skycrpg::camera::fixed::shader {
// The engine functions are called with every register argument passed on and
// RAX returned, whatever their real arity.
using PassFunction = std::uintptr_t(*)(void*,void*,std::uintptr_t,std::uintptr_t);
// A vertex shader object the engine has had current for a selected pass, and
// the patched device shader made from its bytecode (null: drawn natively).
struct Entry { const std::uint8_t* object{}; void* patched{}; };
// SetClearColor, ID 77249: (renderer, r, g, b, a), as the call at 656DAA passes them.
using ClearColor = void(*)(void*,float,float,float,float);
// Patched, rejected (FCS-73/74/78 or 84/85/87) and unaudited (FCS-75/86) shaders of one group.
struct Made { unsigned patched{},rejected{},unaudited{}; };
struct StyleRuntime; // fixed_spike_shader_style.h: only with iCutStyle=1
struct FrameRuntime; // fixed_spike_shader_frame.h: only with bFramePass=1
struct FillRuntime;  // fixed_spike_shader_fill.h: only with iCutFill=1
struct Runtime {
    Config config;
    void (*logger)(const char*){};
    void (*emitter)(const char*){};
    void (*lookEmitter)(const char*){};
    void (*voidEmitter)(const char*){};
    void (*styleEmitter)(const char*){};
    void (*style2Emitter)(const char*){};
    void (*frameEmitter)(const char*){};
    void (*fillEmitter)(const char*){};
    std::atomic<std::uint64_t> refusals{};
    std::atomic_bool pending{};  // hooks to be installed by the next main-thread camera frame
    std::atomic_bool active{};   // a snapshot is published
    std::atomic_bool latched{};  // a run-time refusal: native for the rest of the session
    std::atomic_bool opened{};   // a pass is being drawn cut
    std::array<std::atomic<unsigned long long>,2> offMain{}; // hook calls off the main thread, per group
    std::atomic<unsigned long long> voidOffMain{};
    // Main thread only from here on.
    bool installed{},haveView{},haveUploaded{},haveOther{};
    Feature voidState{},effectState{},skyState{};
    std::uint8_t* skyRoot{};  // the sky root whose hidden bit we set and have not put back
    int skyMode{-1};          // the Sky's mode at the last main-thread frame, -1 no Sky
    VoidWindow voidWindow{};
    ClearColor clearColor{}; // the engine's SetClearColor; set before the call site is redirected
    std::uintptr_t base{};
    const std::uint8_t* state{};
    void* device{};
    void* context{};
    void* buffer{};
    std::array<PassFunction,shaderClasses.size()> setup{},restore{};
    View view;
    CutPlane plane;
    const std::uint8_t* openObject{}; // the engine's vertex shader object our copy stands in for
    Constant uploaded{};
    std::array<Entry,2048> shaders{}; // 1,072 vertex shader objects in the three classes
    std::array<Made,2> made{};
    Vec otherEye{},otherDirection{};
    std::array<Budget,2> budgets{};
    std::array<Window,2> windows{};
    LookWindow look{};
    unsigned long long frame{},published{},framePasses{},frameMatched{};
    unsigned missFrames{},tick{};
    double clock{},nextTrace{};
    StyleRuntime* style{}; // fourth pass; null with iCutStyle=0
    FrameRuntime* framePass{}; // fifth pass; null with bFramePass=0
    FillRuntime* fill{};       // Stage 3.1; null with iCutFill=0
    bool openFill{};           // the open pass is cut at a plane (not removed whole) in the main view: a fill candidate
    const void* openPass{};    // the pass the open cut belongs to (set by its SetupGeometry hook; the fill draws only for it)
};
// Process lifetime, like the spike runtime: device objects are never released.
inline Runtime* runtime = nullptr;
template<class T> T peek(const void* object,std::size_t at) {
    T value{};
    std::memcpy(&value,static_cast<const std::uint8_t*>(object)+at,sizeof(T));
    return value;
}
// A COM method by its vtable byte offset (x64 has one calling convention).
template<class Result,class... Arguments>
Result invoke(void* object,std::size_t slot,Arguments... arguments) {
    const auto table = *static_cast<void***>(object);
    return reinterpret_cast<Result(*)(void*,Arguments...)>(table[slot/sizeof(void*)])(object,arguments...);
}
// Main thread only. One line per code: FixedCameraSpike refused code=FCS-NN.
inline void refuse(Refusal code,const char* text) {
    auto& r = *runtime;
    const std::uint64_t bit = refusalBit(code);
    if (r.refusals.fetch_or(bit,std::memory_order_acq_rel) & bit) return;
    char line[448]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike refused code=FCS-%02u %s",static_cast<unsigned>(code),text);
    r.logger(line);
}
inline void latch() {
    runtime->latched.store(true,std::memory_order_release);
    runtime->active.store(false,std::memory_order_release);
}
// RestoreGeometry of the cut pass has run. The engine still holds its own
// shader object as current; that shader goes back on the device unless the
// engine has bound another one since (then the device already has it). Slot
// 13 is emptied, as the engine leaves it. False = the shader had changed.
inline bool finish() {
    auto& r = *runtime;
    const bool same = peek<const std::uint8_t*>(r.state,offset::currentVertexShader) == r.openObject;
    if (same) invoke<void>(r.context,method::vsSetShader,peek<void*>(r.openObject,offset::shaderObject),static_cast<void* const*>(nullptr),0u);
    void* none[1]{};
    invoke<void>(r.context,method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(none));
    r.openObject = nullptr;
    r.openFill = false; r.openPass = nullptr;
    r.opened.store(false,std::memory_order_release);
    return same;
}
inline void fault(Fault where) {
    if (runtime->opened.load(std::memory_order_acquire)) static_cast<void>(finish());
    latch();
    ++runtime->windows[0].unbalanced;
    char text[256]{};
    std::snprintf(text,sizeof(text),"cause=%u: a cut pass was still open at %s; it was closed, native rendering for the rest of the session",
        static_cast<unsigned>(where),faultName(where));
    refuse(Refusal::bracket,text);
}
// The patched device shader for a vertex shader object, made once. Only
// bytecode on its group's audited list is patched; anything else stays native.
inline void prepare(Entry& entry,std::size_t group) {
    auto& r = *runtime;
    auto& made = r.made[group];
    const PrepareCodes& codes = prepareCodes[group];
    const char* what = group ? "an Effect" : "a";
    char text[256]{};
    const std::uint32_t size = peek<std::uint32_t>(entry.object,offset::shaderCodeSize);
    const std::uint8_t* code = entry.object+offset::shaderCode;
    if (size < containerHeader || size > largestBlob || !peek<void*>(entry.object,offset::shaderObject)) {
        ++made.rejected;
        std::snprintf(text,sizeof(text),"%s current vertex shader object has no device shader or an impossible bytecode size (%u); such passes are drawn natively",
            what,size);
        refuse(codes.object,text); return;
    }
    const std::uint64_t hash = codeHash(code,size);
    if (!auditedFor(group,hash)) {
        ++made.unaudited;
        std::snprintf(text,sizeof(text),"%s selected pass used vertex shader bytecode that is not on its audited list (hash %016llx, %u bytes); "
            "such passes are drawn natively",what,static_cast<unsigned long long>(hash),size);
        refuse(codes.unaudited,text); return;
    }
    const Patched patched = patchVertexShader(code,size);
    if (patched.result != Patch::ok) {
        ++made.rejected;
        std::snprintf(text,sizeof(text),"audited bytecode %016llx was not patched (%s); such passes are drawn natively",
            static_cast<unsigned long long>(hash),patchNames[static_cast<std::size_t>(patched.result)]);
        refuse(codes.patch,text); return;
    }
    void* created = nullptr;
    const std::int32_t answer = invoke<std::int32_t>(r.device,method::createVertexShader,static_cast<const void*>(patched.code.data()),
        patched.code.size(),static_cast<void*>(nullptr),&created);
    if (answer < 0 || !created) {
        ++made.rejected;
        std::snprintf(text,sizeof(text),"the device refused a patched vertex shader (hash %016llx, result %08X); such passes are drawn natively",
            static_cast<unsigned long long>(hash),static_cast<unsigned>(answer));
        refuse(codes.create,text); return;
    }
    entry.patched = created;
    ++made.patched; ++r.windows[group].created;
}
// The vertex shader the engine bound for this pass's technique, by object
// (they live for the session; each belongs to one class). Null when the
// table is crowded: native.
inline Entry* shaderFor(std::size_t group) {
    auto& r = *runtime;
    const auto object = peek<const std::uint8_t*>(r.state,offset::currentVertexShader);
    if (!object) return nullptr;
    const std::size_t first = (reinterpret_cast<std::uintptr_t>(object) >> 4) % r.shaders.size();
    for (std::size_t probe = 0;probe < 16;++probe) {
        auto& entry = r.shaders[(first+probe) % r.shaders.size()];
        if (entry.object == object) return &entry;
        if (!entry.object) { entry.object = object; prepare(entry,group); return &entry; }
    }
    return nullptr;
}
// The reference that owns a geometry, found as the engine finds it (ID 19750,
// RVA 2EED1F..2EED74): up the parents, a user only on a fade node, and past a
// carried ammunition reference to its carrier.
inline RE::TESObjectREFR* ownerOf(const std::uint8_t* node) {
    RE::TESObjectREFR* found = nullptr;
    for (unsigned hop = 0;node && hop < maxOwnerHops;++hop) {
        const auto fade = invoke<const std::uint8_t*>(const_cast<std::uint8_t*>(node),offset::asFadeNode);
        if (const auto ref = fade ? peek<RE::TESObjectREFR*>(fade,offset::objectUser) : nullptr) {
            found = ref;
            const auto base = ref->GetBaseObject();
            if (!base || static_cast<std::uint32_t>(base->GetFormType()) != ammoForm) break;
        }
        node = peek<const std::uint8_t*>(node,offset::objectParent);
    }
    return found;
}
inline Skip ownerSkip(const std::uint8_t* geometry,std::uint32_t& owner) {
    const auto ref = ownerOf(geometry);
    if (!ref) return ownerRule(false,false,Kind::excluded);
    owner = ref->GetFormID();
    const bool actor = ref->GetFormType() == RE::FormType::ActorCharacter;
    return ownerRule(true,actor,actor ? Kind::excluded : kindOf(ref,ref->GetBaseObject()));
}
// fixed_spike_shader_style.h (included at the end of this file).
inline bool styled();
inline void styledPass(void* pass,std::size_t shaderClass,Vec eye);
inline void shadowPass(void* pass,std::size_t shaderClass);
// fixed_spike_shader_frame.h (included at the end of this file).
inline void captureFrame(Vec eye);
// fixed_spike_shader_fill.h (included at the end of this file).
inline void cutFill(void* pass,std::size_t shaderClass);
// The pass is drawn with the patched shader and the plane in slot 13. Both
// device calls that can fail come before anything is bound.
inline void apply(const Entry& entry,Constant wanted,std::size_t shaderClass) {
    auto& r = *runtime;
    if (!r.haveUploaded || std::memcmp(&wanted,&r.uploaded,sizeof(wanted)) != 0) {
        MappedBuffer mapped{};
        const std::int32_t answer = invoke<std::int32_t>(r.context,method::map,r.buffer,0u,mapWriteDiscard,0u,&mapped);
        if (answer < 0 || !mapped.data) {
            latch();
            char text[192]{};
            std::snprintf(text,sizeof(text),"the plane buffer could not be mapped (result %08X); native rendering for the rest of the session",
                static_cast<unsigned>(answer));
            refuse(Refusal::map,text); return;
        }
        std::memcpy(mapped.data,&wanted,sizeof(wanted));
        invoke<void>(r.context,method::unmap,r.buffer,0u);
        r.uploaded = wanted; r.haveUploaded = true;
        ++r.windows[groupOf(shaderClass)].uploads;
    }
    void* buffers[1]{r.buffer};
    invoke<void>(r.context,method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(buffers));
    invoke<void>(r.context,method::vsSetShader,entry.patched,static_cast<void* const*>(nullptr),0u);
    r.openObject = entry.object;
    r.openFill = usable(wanted); // Stage 3.1: a plane cut, not the whole-draw constant
    r.opened.store(true,std::memory_order_release);
    auto& w = r.windows[groupOf(shaderClass)];
    ++w.cut; ++w.byClass[shaderClass];
}
// A pass of a hooked shader class, after its SetupGeometry. Main thread.
// Effect passes take the same path with their own counters, budget and
// audited list; they do not feed the FCS-76 view check.
inline void consider(void* pass,std::size_t shaderClass) {
    auto& r = *runtime;
    if (r.latched.load(std::memory_order_relaxed)) return;
    const std::size_t group = groupOf(shaderClass);
    if (!thin::onMain()) { r.offMain[group].fetch_add(1ull,std::memory_order_relaxed); return; }
    auto& w = r.windows[group];
    ++w.passes; if (!group) ++r.framePasses;
    if (r.opened.load(std::memory_order_relaxed)) return; // a nested SetupGeometry: never two cuts at once
    if (!(r.tick++ & 63u)) r.clock = thin::seconds();
    if (!r.haveView || !fresh(r.view,r.clock)) { ++w.stale; return; }
    const Vec eye = peek<Vec>(r.state,offset::posAdjust),direction = peek<Vec>(r.state,offset::viewDirection);
    if (!sameView(r.view,eye,direction)) {
        ++w.otherView;
        if (!group) { r.otherEye = eye; r.otherDirection = direction; r.haveOther = true; }
        if (!group && styled()) shadowPass(pass,shaderClass); // fourth pass: shadows of what the main view removed whole
        return;
    }
    if (!group) ++r.frameMatched;
    if (!group) captureFrame(eye); // fifth pass: the main view's matrices for the frame pass, once per render
    if (!group && styled()) { styledPass(pass,shaderClass,eye); return; } // fourth pass: the Stage 1 per-reference policy
    const auto geometry = peek<const std::uint8_t*>(pass,offset::passGeometry);
    const Constant wanted = cutConstant(r.view.planeZ,eye.z);
    Skip skip = Skip::noBound;
    std::uint32_t owner = 0;
    Bound bound{};
    if (geometry && usable(wanted)) {
        bound = {peek<Vec>(geometry,offset::objectBound),peek<float>(geometry,offset::objectBound+sizeof(Vec))};
        skip = reach(r.view,bound,peek<const void*>(geometry,offset::geometrySkin) != nullptr);
        if (skip == Skip::none) skip = ownerSkip(geometry,owner);
    }
    const Entry* entry = skip == Skip::none ? shaderFor(group) : nullptr;
    if (skip == Skip::none && (!entry || !entry->patched)) skip = Skip::shader;
    if (skip == Skip::none && !r.budgets[group].take()) skip = Skip::cap;
    if (skip != Skip::none) { ++w.skips[static_cast<std::size_t>(skip)]; return; }
    // Third pass: an Effect pass centred in the upper half below the plane or higher is not drawn at all.
    const bool whole = group && r.config.effectWhole && wholeEffect(r.view,bound);
    if (group) { r.voidWindow.note(owner); if (whole) ++r.voidWindow.whole; }
    // Fourth pass: what the floor rule kept at the plane, and the largest radius clipped whole.
    if (group && r.config.effectWhole && !whole && upperEffect(r.view,bound)) ++r.voidWindow.floorKept;
    if (whole && bound.radius > r.voidWindow.wholeRadius) r.voidWindow.wholeRadius = bound.radius;
    apply(*entry,whole ? wholeConstant : wanted,shaderClass);
}

// ---- hooks ------------------------------------------------------------------
template<std::size_t Index>
std::uintptr_t hookSetup(void* self,void* pass,std::uintptr_t flags,std::uintptr_t extra) {
    auto& r = *runtime;
    if (r.opened.load(std::memory_order_acquire) && thin::onMain()) fault(Fault::setup);
    const auto result = r.setup[Index](self,pass,flags,extra);
    if (pass && r.active.load(std::memory_order_acquire)) {
        const double started = r.config.trace ? thin::seconds() : 0.0;
        consider(pass,Index);
        if (r.opened.load(std::memory_order_relaxed) && thin::onMain()) r.openPass = pass; // Stage 3.1: the pass this cut belongs to
        if (r.config.trace && thin::onMain()) r.windows[groupOf(Index)].seconds += thin::seconds()-started;
    }
    return result;
}
template<std::size_t Index>
std::uintptr_t hookRestore(void* self,void* pass,std::uintptr_t flags,std::uintptr_t extra) {
    auto& r = *runtime;
    if (r.fill) cutFill(pass,Index); // Stage 3.1: the second draw goes before the engine restores its state
    const auto result = r.restore[Index](self,pass,flags,extra);
    if (r.opened.load(std::memory_order_acquire)) {
        if (!thin::onMain()) r.offMain[groupOf(Index)].fetch_add(1ull,std::memory_order_relaxed);
        else if (!finish()) fault(Fault::shaderChanged);
    }
    return result;
}

// ---- install ----------------------------------------------------------------
// Neither is on the pinned stack. Community Shaders replaces these shaders
// and their vfuncs; ENB and other wrappers stand in for d3d11.dll from the
// game directory, which the vtable guards below would not see.
inline const char* foreignRenderer() {
    if (GetModuleHandleW(L"CommunityShaders.dll")) return "CommunityShaders.dll";
    if (GetModuleHandleW(L"d3dcompiler_46e.dll")) return "d3dcompiler_46e.dll (ENB)";
    wchar_t loaded[MAX_PATH]{},folder[MAX_PATH]{};
    const HMODULE d3d = GetModuleHandleW(L"d3d11.dll");
    const DWORD length = d3d ? GetModuleFileNameW(d3d,loaded,MAX_PATH) : 0;
    const UINT prefix = GetSystemDirectoryW(folder,MAX_PATH);
    if (!length || length >= MAX_PATH || !prefix || prefix >= MAX_PATH) return "d3d11.dll (path unknown)";
    if (length <= prefix || _wcsnicmp(loaded,folder,prefix) != 0 || loaded[prefix] != L'\\') return "d3d11.dll outside the system directory (a wrapper)";
    return nullptr;
}
// 0 = every ID resolves to the audited RVA. 1-3 state, device, context;
// 10-11 the lighting and utility vtables (Effect: effectGuard).
inline unsigned addressGuard(std::uintptr_t base) {
    constexpr std::array<Address,3> data{stateAddress,deviceAddress,contextAddress};
    for (unsigned i = 0;i < data.size();++i)
        if (REL::ID(data[i].id).address() != base+data[i].rva) return 1u+i;
    for (unsigned i = 0;i < mainClasses;++i)
        if (REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva) return 10u+i;
    return 0;
}
// 0 = slots 6 and 7 of the class hold the audited functions with the audited
// first bytes. Otherwise 1 setup slot, 2 restore slot, 3 setup bytes, 4
// restore bytes: another module owns the slot or patched it.
inline unsigned slotGuard(std::uintptr_t base,std::size_t index) {
    const auto& c = shaderClasses[index];
    const auto slots = reinterpret_cast<const std::uintptr_t*>(base+c.table.rva);
    if (slots[setupSlot] != base+c.setupRva) return 1u;
    if (slots[restoreSlot] != base+c.restoreRva) return 2u;
    if (std::memcmp(reinterpret_cast<const void*>(slots[setupSlot]),c.setupBytes.data(),c.setupBytes.size()) != 0) return 3u;
    if (std::memcmp(reinterpret_cast<const void*>(slots[restoreSlot]),c.restoreBytes.data(),c.restoreBytes.size()) != 0) return 4u;
    return 0;
}
// Lighting and Utility, as enabled: 0, or 10 * (class + 1) + the slot check.
inline unsigned tableGuard(std::uintptr_t base,unsigned mask) {
    for (unsigned i = 0;i < mainClasses;++i) {
        if (!shaderEnabled(mask,i)) continue;
        if (const unsigned failed = slotGuard(base,i)) return 10u*(i+1u)+failed;
    }
    return 0;
}
// Effect: 0, 1 its vtable ID, or 1 + the slot check. A failure leaves the
// Lighting/Utility cut as it is (FCS-80).
inline unsigned effectGuard(std::uintptr_t base) {
    const auto& c = shaderClasses[effectClass];
    if (REL::ID(c.table.id).address() != base+c.table.rva) return 1u;
    const unsigned failed = slotGuard(base,effectClass);
    return failed ? 1u+failed : 0u;
}
template<std::size_t Index>
void hookClass() {
    auto& r = *runtime;
    if (!classEnabled(r.config,Index)) return;
    REL::Relocation<std::uintptr_t> table{REL::ID(shaderClasses[Index].table.id)};
    const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
    // Published before the slots change; restore first, so a setup hook never runs without it.
    r.setup[Index] = reinterpret_cast<PassFunction>(slots[setupSlot]);
    r.restore[Index] = reinterpret_cast<PassFunction>(slots[restoreSlot]);
    static_cast<void>(table.write_vfunc(restoreSlot,&hookRestore<Index>));
    static_cast<void>(table.write_vfunc(setupSlot,&hookSetup<Index>));
}
// fixed_spike_shader_void.h and fixed_spike_shader_sky.h (included at the end of this file).
inline void installVoid(std::uintptr_t base);
inline void installSky(std::uintptr_t base);
inline void settleSky(bool want);
inline void installStyle();
inline Feature styleState();
inline void configureStyle();
inline void styleFrame();
inline void traceStyle(double now,const VoidWindow& effect);
inline void installFrame(std::uintptr_t base);
inline Feature frameState();
inline void configureFrame();
inline void frameStartFrame();
inline void traceFrame(double now);
inline void installFill(std::uintptr_t base);
inline Feature fillState();
inline void configureFill();
inline void traceFill(double now);
// Main thread, from commit, after the Lighting/Utility hooks. The Effect class
// is hooked only with bShaderCutEffect=1 and when its own guard passes.
inline void installEffect(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.config.effect) { r.effectState = Feature::off; return; }
    if (const unsigned failed = effectGuard(base)) {
        r.effectState = Feature::refused;
        char text[224]{};
        std::snprintf(text,sizeof(text),"check=%u: the Effect vtable ID, slot or first bytes differ; Effect passes are drawn natively, "
            "the Lighting/Utility cut is unchanged",failed);
        refuse(Refusal::effectTable,text); return;
    }
    hookClass<effectClass>();
    r.effectState = Feature::installed;
}
// Main thread, inside the camera frame service. Every guard and the one
// device object the mode needs up front come before the first slot is written.
inline void commit() {
    auto& r = *runtime;
    const auto base = r.base;
    char text[256]{};
    if (const char* found = foreignRenderer()) {
        std::snprintf(text,sizeof(text),"%s is loaded; the shader cut is not installed, native rendering",found);
        refuse(Refusal::foreign,text); return;
    }
    if (const unsigned failed = addressGuard(base)) {
        std::snprintf(text,sizeof(text),"check=%u: an address ID does not resolve to the audited RVA; the shader cut is not installed, native rendering",failed);
        refuse(Refusal::address,text); return;
    }
    if (const unsigned failed = tableGuard(base,r.config.shaders)) {
        std::snprintf(text,sizeof(text),"check=%u: a shader vtable slot or its first bytes differ (already hooked or wrong build); "
            "the shader cut is not installed, native rendering",failed);
        refuse(Refusal::table,text); return;
    }
    void* const device = peek<void*>(reinterpret_cast<const void*>(base+deviceAddress.rva),0);
    void* const context = peek<void*>(reinterpret_cast<const void*>(base+contextAddress.rva),0);
    BufferDescription description;
    description.bytes = static_cast<std::uint32_t>(sizeof(Constant));
    void* created = nullptr;
    const std::int32_t answer = device && context ? invoke<std::int32_t>(device,method::createBuffer,
        static_cast<const BufferDescription*>(&description),static_cast<const void*>(nullptr),&created) : -1;
    if (answer < 0 || !created) {
        std::snprintf(text,sizeof(text),"device=%u context=%u result=%08X: no engine device or context, or the device refused the plane buffer; "
            "the shader cut is not installed, native rendering",static_cast<unsigned>(device != nullptr),static_cast<unsigned>(context != nullptr),
            static_cast<unsigned>(answer));
        refuse(Refusal::device,text); return;
    }
    r.state = reinterpret_cast<const std::uint8_t*>(base+stateAddress.rva);
    r.device = device; r.context = context; r.buffer = created;
    hookClass<0>(); hookClass<1>();
    r.installed = true;
    std::snprintf(text,sizeof(text),"FixedCameraSpike shader cut installed shaders=%u height=%.1f radius=%.1f cap=%u trace=%u audited=%u",
        r.config.shaders,static_cast<double>(r.config.height),static_cast<double>(r.config.radius),r.config.maxPerFrame,
        static_cast<unsigned>(r.config.trace),static_cast<unsigned>(auditedVertexShaders.size()));
    r.logger(text);
    // Second pass. Each refuses on its own and leaves the cut as it is.
    installEffect(base);
    installVoid(base);
    std::snprintf(text,sizeof(text),"FixedCameraSpike shader look installed void=%s color=%.3f,%.3f,%.3f effect=%s effectAudited=%u",
        featureNames[static_cast<std::size_t>(r.voidState)],static_cast<double>(r.config.color.red),static_cast<double>(r.config.color.green),
        static_cast<double>(r.config.color.blue),featureNames[static_cast<std::size_t>(r.effectState)],
        static_cast<unsigned>(auditedEffectVertexShaders.size()));
    r.logger(text);
    // Third pass: refuses on its own and leaves the cut and the void colour as they are.
    installSky(base);
    std::snprintf(text,sizeof(text),"FixedCameraSpike shader void installed sky=%s effectWhole=%s",
        featureNames[static_cast<std::size_t>(r.skyState)],r.config.effectWhole && r.effectState == Feature::installed ? "on" : "off");
    r.logger(text);
    // Fourth pass: refuses on its own (FCS-91) and leaves the flat cut as it is.
    installStyle();
    std::snprintf(text,sizeof(text),"FixedCameraSpike shader style installed style=%u state=%s gallery=%u shadows=%u",r.config.style,
        r.style ? featureNames[static_cast<std::size_t>(styleState())] : "off",static_cast<unsigned>(r.config.keepFarGallery),
        static_cast<unsigned>(r.config.styleShadows));
    r.logger(text);
    // Fifth pass: only with bFramePass=1; refuses on its own (FCS-93..95) and leaves everything above as it is.
    if (r.framePass) {
        installFrame(base);
        std::snprintf(text,sizeof(text),"FixedCameraSpike shader frame installed pass=%s band=%.1f headroom=%.1f radii=%.1f,%.1f vignette=%.3f "
            "vs=%u ps=%u",featureNames[static_cast<std::size_t>(frameState())],static_cast<double>(r.config.frame.band),
            static_cast<double>(r.config.frame.headroom),static_cast<double>(r.config.frame.radiusInner),
            static_cast<double>(r.config.frame.radiusOuter),static_cast<double>(r.config.frame.vignette),
            static_cast<unsigned>(frame_bytes::vertexShader.size()),static_cast<unsigned>(frame_bytes::pixelShader.size()));
        r.logger(text);
    }
    // Stage 3.1: only with iCutFill=1; refuses on its own (FCS-98/99) and leaves everything above as it is.
    if (r.fill) {
        installFill(base);
        std::snprintf(text,sizeof(text),"FixedCameraSpike shader fill installed fill=%s colour=%.3f,%.3f,%.3f cap=%u",
            featureNames[static_cast<std::size_t>(fillState())],static_cast<double>(r.config.fillColor.red),
            static_cast<double>(r.config.fillColor.green),static_cast<double>(r.config.fillColor.blue),
            static_cast<unsigned>(frame_bytes::capPixelShader.size()));
        r.logger(text);
    }
}

// ---- called by the spike's frame bracket ------------------------------------
// kDataLoaded can run off the main thread: only ask for the install.
inline void install(std::uintptr_t base) {
    if (!runtime) return;
    runtime->base = base;
    runtime->pending.store(true,std::memory_order_release);
}
// Any thread. On the main thread the sky root is put back at once.
inline void deactivate() {
    if (!runtime) return;
    runtime->active.store(false,std::memory_order_release);
    if (thin::onMain()) settleSky(false);
}
// Main thread, before the camera update.
inline void frameStart() {
    if (!runtime) return;
    auto& r = *runtime;
    if (r.pending.exchange(false,std::memory_order_acq_rel)) commit();
    if (!r.installed) return;
    if (r.opened.load(std::memory_order_acquire)) fault(Fault::frame);
    if (r.latched.load(std::memory_order_acquire)) settleSky(false);
    if (r.offMain[0].load(std::memory_order_relaxed))
        refuse(Refusal::thread,"a render hook ran off the main thread; such calls touch nothing and are drawn natively");
    if (r.offMain[1].load(std::memory_order_relaxed))
        refuse(Refusal::effectThread,"an Effect render hook ran off the main thread; such calls touch nothing and are drawn natively");
    if (r.voidOffMain.load(std::memory_order_relaxed))
        refuse(Refusal::voidThread,"the main view's clear colour was set off the main thread; those calls kept the engine's colour");
    frameStartFrame(); // fifth pass: an off-thread return of the main world render
    if (!r.latched.load(std::memory_order_acquire) && peek<void*>(reinterpret_cast<const void*>(r.base+contextAddress.rva),0) != r.context) {
        latch();
        refuse(Refusal::context,"the engine's device context changed; native rendering for the rest of the session");
    }
    // Passes were drawn while a plane was published, and none in the published view.
    r.missFrames = r.framePasses && !r.frameMatched && r.active.load(std::memory_order_acquire) ? r.missFrames+1u : 0u;
    r.framePasses = r.frameMatched = 0;
    if (r.missFrames == 120u) {
        char text[320]{};
        std::snprintf(text,sizeof(text),"no pass has matched the published view for 120 frames (published eye %.1f,%.1f,%.1f; "
            "last render-state eye %.1f,%.1f,%.1f direction %.3f,%.3f,%.3f); nothing is cut",
            static_cast<double>(r.view.eye.x),static_cast<double>(r.view.eye.y),static_cast<double>(r.view.eye.z),
            static_cast<double>(r.otherEye.x),static_cast<double>(r.otherEye.y),static_cast<double>(r.otherEye.z),
            static_cast<double>(r.otherDirection.x),static_cast<double>(r.otherDirection.y),static_cast<double>(r.otherDirection.z));
        refuse(Refusal::view,text);
    }
}
// Main thread, after the camera update of an eligible frame.
inline void publish(Vec eye,Vec forward,Vec feet,bool interiorCell,unsigned long long frame,double now) {
    if (!runtime) return;
    auto& r = *runtime;
    r.frame = frame;
    // The plane's reference height starts again after any frame without a plane.
    if (r.published+1u != frame) r.plane.reset();
    std::optional<View> next;
    if (publishes(r.installed,r.latched.load(std::memory_order_acquire),interiorCell,feet.z))
        next = makeView(r.config,eye,forward,feet,r.plane.update(feet.z,now),frame,now);
    if (!next) { r.active.store(false,std::memory_order_release); settleSky(false); return; }
    r.view = *next; r.haveView = true; r.published = frame;
    for (auto& budget : r.budgets) budget.frame();
    styleFrame(); // fourth pass: the storey line from the frame just drawn
    r.active.store(true,std::memory_order_release);
    settleSky(true);
}
inline void trace(double now) {
    if (!runtime) return;
    auto& r = *runtime;
    if (!r.config.trace || now < r.nextTrace) return;
    r.nextTrace = now+.1;
    Sample s;
    s.seconds = now; s.frame = r.frame; s.offMain = r.offMain[0].load(std::memory_order_relaxed);
    s.installed = r.installed;
    s.active = r.active.load(std::memory_order_acquire); s.latched = r.latched.load(std::memory_order_acquire);
    // The snapshot is reported only while it is in use; an inactive window reads nan.
    if (s.active && r.haveView) { s.haveView = true; s.view = r.view; }
    s.haveOther = r.haveOther; s.otherEye = r.otherEye; s.otherDirection = r.otherDirection;
    s.offset = r.uploaded.offset;
    s.peak = r.budgets[0].peak; s.cap = r.budgets[0].limit;
    s.patched = r.made[0].patched; s.rejected = r.made[0].rejected; s.unaudited = r.made[0].unaudited;
    s.window = r.windows[0];
    LookSample look;
    look.seconds = now; look.frame = r.frame;
    look.voidOffMain = r.voidOffMain.load(std::memory_order_relaxed); look.effectOffMain = r.offMain[1].load(std::memory_order_relaxed);
    look.voidState = r.voidState; look.effectState = r.effectState; look.color = r.config.color;
    look.look = r.look; look.effect = r.windows[1];
    look.peak = r.budgets[1].peak;
    look.patched = r.made[1].patched; look.rejected = r.made[1].rejected; look.unaudited = r.made[1].unaudited;
    VoidSample voids;
    voids.seconds = now; voids.frame = r.frame; voids.skyState = r.skyState; voids.held = r.skyRoot != nullptr; voids.mode = r.skyMode;
    voids.effectWhole = r.config.effectWhole && r.effectState == Feature::installed; voids.window = r.voidWindow;
    for (auto& budget : r.budgets) budget.peak = 0;
    r.windows = {}; r.look = {}; r.voidWindow = {}; r.haveOther = false;
    char line[768]{};
    if (format(line,sizeof(line),s)) r.emitter(line);
    if (formatLook(line,sizeof(line),look)) r.lookEmitter(line);
    if (formatVoid(line,sizeof(line),voids)) r.voidEmitter(line);
    traceStyle(now,voids.window); // fourth pass: FixedShaderStyle, only with iCutStyle=1
    traceFrame(now); // fifth pass: FixedShaderFrame, only with bFramePass=1
    traceFill(now); // Stage 3.1: FixedShaderFill, only with iCutFill=1
}
inline bool tracing() { return runtime && runtime->config.trace; }
// Shader trace line kinds: FixedShader, FixedShaderLook, FixedShaderVoid, FixedShaderStyle and
// FixedShaderStyle2 with iCutStyle=1, FixedShaderFrame with bFramePass=1, and FixedShaderFill with iCutFill=1.
inline unsigned traceKinds() {
    return (runtime && runtime->config.style == stageOneStyle ? 5u : 3u)+(runtime && runtime->framePass ? 1u : 0u)+(runtime && runtime->fill ? 2u : 0u);
}
inline const char* status() { return !runtime ? "off" : runtime->installed ? "installed" : "pending"; }
// loadSettings, only when [FixedCameraSpike] bEnabled=1 and iSeeThroughMode=3.
inline void configure(const Config& requested,void (*logger)(const char*),void (*emitter)(const char*),void (*lookEmitter)(const char*),
    void (*voidEmitter)(const char*),void (*styleEmitter)(const char*),void (*style2Emitter)(const char*),void (*frameEmitter)(const char*),
    void (*fillEmitter)(const char*)) {
    runtime = new Runtime;
    auto& r = *runtime;
    r.logger = logger; r.emitter = emitter; r.lookEmitter = lookEmitter; r.voidEmitter = voidEmitter; r.styleEmitter = styleEmitter;
    r.style2Emitter = style2Emitter; r.frameEmitter = frameEmitter; r.fillEmitter = fillEmitter;
    bool adjusted = false;
    r.config = validate(requested,&adjusted);
    for (auto& budget : r.budgets) budget.limit = r.config.maxPerFrame;
    r.voidState = r.config.voidColor ? Feature::pending : Feature::off;
    r.effectState = r.config.effect ? Feature::pending : Feature::off;
    r.skyState = r.config.hideSky ? Feature::pending : Feature::off;
    configureStyle();
    configureFrame();
    configureFill();
    if (adjusted) refuse(Refusal::settings,"one or more shader-cut values were out of range; safe values applied");
}
} // namespace skycrpg::camera::fixed::shader
#include "fixed_spike_shader_void.h" // second pass: the void colour
#include "fixed_spike_shader_sky.h"  // third pass: the sky dome over the void
#include "fixed_spike_shader_style.h" // fourth pass: Stage 1 of the seamless cutaway
#include "fixed_spike_shader_frame.h" // fifth pass: Stage 2, the frame pass
#include "fixed_spike_shader_fill.h" // Stage 3.1: the back-face fill
