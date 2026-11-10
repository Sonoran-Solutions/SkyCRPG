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
// The hooks (BSShader slots 6 and 7 of Lighting and Utility) are expected on
// the same thread, because the engine draws through one immediate device
// context; that is checked on every call. A call on another thread touches
// nothing and stays native (FCS-71, logged once). Other threads only clear
// `active`.
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
struct Runtime {
    Config config;
    void (*logger)(const char*){};
    void (*emitter)(const char*){};
    std::atomic<std::uint32_t> refusals{};
    std::atomic_bool pending{};  // hooks to be installed by the next main-thread camera frame
    std::atomic_bool active{};   // a snapshot is published
    std::atomic_bool latched{};  // a run-time refusal: native for the rest of the session
    std::atomic_bool opened{};   // a pass is being drawn cut
    std::atomic<unsigned long long> offMain{};
    // Main thread only from here on.
    bool installed{},haveView{},haveUploaded{},haveOther{};
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
    std::array<Entry,1024> shaders{};
    unsigned patched{},rejected{},unaudited{};
    Vec otherEye{},otherDirection{};
    Budget budget;
    Window window;
    unsigned long long frame{},published{},framePasses{},frameMatched{};
    unsigned missFrames{},tick{};
    double clock{},nextTrace{};
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
    const std::uint32_t bit = refusalBit(code);
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
    r.opened.store(false,std::memory_order_release);
    return same;
}
inline void fault(Fault where) {
    if (runtime->opened.load(std::memory_order_acquire)) static_cast<void>(finish());
    latch();
    ++runtime->window.unbalanced;
    char text[256]{};
    std::snprintf(text,sizeof(text),"cause=%u: a cut pass was still open at %s; it was closed, native rendering for the rest of the session",
        static_cast<unsigned>(where),faultName(where));
    refuse(Refusal::bracket,text);
}
// The patched device shader for a vertex shader object, made once. Only
// bytecode on the audited list is patched; anything else stays native.
inline void prepare(Entry& entry) {
    auto& r = *runtime;
    char text[256]{};
    const std::uint32_t size = peek<std::uint32_t>(entry.object,offset::shaderCodeSize);
    const std::uint8_t* code = entry.object+offset::shaderCode;
    if (size < containerHeader || size > largestBlob || !peek<void*>(entry.object,offset::shaderObject)) {
        ++r.rejected;
        std::snprintf(text,sizeof(text),"a current vertex shader object has no device shader or an impossible bytecode size (%u); such passes are drawn natively",size);
        refuse(Refusal::object,text); return;
    }
    const std::uint64_t hash = codeHash(code,size);
    if (!audited(hash)) {
        ++r.unaudited;
        std::snprintf(text,sizeof(text),"a selected pass used vertex shader bytecode that is not on the audited list (hash %016llx, %u bytes); "
            "such passes are drawn natively",static_cast<unsigned long long>(hash),size);
        refuse(Refusal::unaudited,text); return;
    }
    const Patched made = patchVertexShader(code,size);
    if (made.result != Patch::ok) {
        ++r.rejected;
        std::snprintf(text,sizeof(text),"audited bytecode %016llx was not patched (%s); such passes are drawn natively",
            static_cast<unsigned long long>(hash),patchNames[static_cast<std::size_t>(made.result)]);
        refuse(Refusal::patch,text); return;
    }
    void* created = nullptr;
    const std::int32_t answer = invoke<std::int32_t>(r.device,method::createVertexShader,static_cast<const void*>(made.code.data()),
        made.code.size(),static_cast<void*>(nullptr),&created);
    if (answer < 0 || !created) {
        ++r.rejected;
        std::snprintf(text,sizeof(text),"the device refused a patched vertex shader (hash %016llx, result %08X); such passes are drawn natively",
            static_cast<unsigned long long>(hash),static_cast<unsigned>(answer));
        refuse(Refusal::create,text); return;
    }
    entry.patched = created;
    ++r.patched; ++r.window.created;
}
// The vertex shader the engine bound for this pass's technique, by object
// (they live for the session). Null when the table is crowded: native.
inline Entry* shaderFor() {
    auto& r = *runtime;
    const auto object = peek<const std::uint8_t*>(r.state,offset::currentVertexShader);
    if (!object) return nullptr;
    const std::size_t first = (reinterpret_cast<std::uintptr_t>(object) >> 4) % r.shaders.size();
    for (std::size_t probe = 0;probe < 16;++probe) {
        auto& entry = r.shaders[(first+probe) % r.shaders.size()];
        if (entry.object == object) return &entry;
        if (!entry.object) { entry.object = object; prepare(entry); return &entry; }
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
inline Skip ownerSkip(const std::uint8_t* geometry) {
    const auto ref = ownerOf(geometry);
    if (!ref) return ownerRule(false,false,Kind::excluded);
    const bool actor = ref->GetFormType() == RE::FormType::ActorCharacter;
    return ownerRule(true,actor,actor ? Kind::excluded : kindOf(ref,ref->GetBaseObject()));
}
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
        ++r.window.uploads;
    }
    void* buffers[1]{r.buffer};
    invoke<void>(r.context,method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(buffers));
    invoke<void>(r.context,method::vsSetShader,entry.patched,static_cast<void* const*>(nullptr),0u);
    r.openObject = entry.object;
    r.opened.store(true,std::memory_order_release);
    ++r.window.cut; ++r.window.byClass[shaderClass];
}
// A pass of a hooked shader class, after its SetupGeometry. Main thread.
inline void consider(void* pass,std::size_t shaderClass) {
    auto& r = *runtime;
    if (r.latched.load(std::memory_order_relaxed)) return;
    if (!thin::onMain()) { r.offMain.fetch_add(1ull,std::memory_order_relaxed); return; }
    ++r.window.passes; ++r.framePasses;
    if (r.opened.load(std::memory_order_relaxed)) return; // a nested SetupGeometry: never two cuts at once
    if (!(r.tick++ & 63u)) r.clock = thin::seconds();
    if (!r.haveView || !fresh(r.view,r.clock)) { ++r.window.stale; return; }
    const Vec eye = peek<Vec>(r.state,offset::posAdjust),direction = peek<Vec>(r.state,offset::viewDirection);
    if (!sameView(r.view,eye,direction)) {
        ++r.window.otherView;
        r.otherEye = eye; r.otherDirection = direction; r.haveOther = true;
        return;
    }
    ++r.frameMatched;
    const auto geometry = peek<const std::uint8_t*>(pass,offset::passGeometry);
    const Constant wanted = cutConstant(r.view.planeZ,eye.z);
    Skip skip = Skip::noBound;
    if (geometry && usable(wanted)) {
        const Bound bound{peek<Vec>(geometry,offset::objectBound),peek<float>(geometry,offset::objectBound+sizeof(Vec))};
        skip = reach(r.view,bound,peek<const void*>(geometry,offset::geometrySkin) != nullptr);
        if (skip == Skip::none) skip = ownerSkip(geometry);
    }
    const Entry* entry = skip == Skip::none ? shaderFor() : nullptr;
    if (skip == Skip::none && (!entry || !entry->patched)) skip = Skip::shader;
    if (skip == Skip::none && !r.budget.take()) skip = Skip::cap;
    if (skip != Skip::none) { ++r.window.skips[static_cast<std::size_t>(skip)]; return; }
    apply(*entry,wanted,shaderClass);
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
        if (r.config.trace && thin::onMain()) r.window.seconds += thin::seconds()-started;
    }
    return result;
}
template<std::size_t Index>
std::uintptr_t hookRestore(void* self,void* pass,std::uintptr_t flags,std::uintptr_t extra) {
    auto& r = *runtime;
    const auto result = r.restore[Index](self,pass,flags,extra);
    if (r.opened.load(std::memory_order_acquire)) {
        if (!thin::onMain()) r.offMain.fetch_add(1ull,std::memory_order_relaxed);
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
// 10-11 the lighting and utility vtables.
inline unsigned addressGuard(std::uintptr_t base) {
    constexpr std::array<Address,3> data{stateAddress,deviceAddress,contextAddress};
    for (unsigned i = 0;i < data.size();++i)
        if (REL::ID(data[i].id).address() != base+data[i].rva) return 1u+i;
    for (unsigned i = 0;i < shaderClasses.size();++i)
        if (REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva) return 10u+i;
    return 0;
}
// 0 = slots 6 and 7 of every enabled class hold the audited functions with the
// audited first bytes. Otherwise 10 * (class + 1) + 1 setup slot, 2 restore
// slot, 3 setup bytes, 4 restore bytes: another module owns the slot or patched it.
inline unsigned tableGuard(std::uintptr_t base,unsigned mask) {
    for (unsigned i = 0;i < shaderClasses.size();++i) {
        if (!shaderEnabled(mask,i)) continue;
        const auto& c = shaderClasses[i];
        const auto slots = reinterpret_cast<const std::uintptr_t*>(base+c.table.rva);
        if (slots[setupSlot] != base+c.setupRva) return 10u*(i+1u)+1u;
        if (slots[restoreSlot] != base+c.restoreRva) return 10u*(i+1u)+2u;
        if (std::memcmp(reinterpret_cast<const void*>(slots[setupSlot]),c.setupBytes.data(),c.setupBytes.size()) != 0) return 10u*(i+1u)+3u;
        if (std::memcmp(reinterpret_cast<const void*>(slots[restoreSlot]),c.restoreBytes.data(),c.restoreBytes.size()) != 0) return 10u*(i+1u)+4u;
    }
    return 0;
}
template<std::size_t Index>
void hookClass() {
    auto& r = *runtime;
    if (!shaderEnabled(r.config.shaders,Index)) return;
    REL::Relocation<std::uintptr_t> table{REL::ID(shaderClasses[Index].table.id)};
    const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
    // Published before the slots change; restore first, so a setup hook never runs without it.
    r.setup[Index] = reinterpret_cast<PassFunction>(slots[setupSlot]);
    r.restore[Index] = reinterpret_cast<PassFunction>(slots[restoreSlot]);
    static_cast<void>(table.write_vfunc(restoreSlot,&hookRestore<Index>));
    static_cast<void>(table.write_vfunc(setupSlot,&hookSetup<Index>));
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
}

// ---- called by the spike's frame bracket ------------------------------------
// kDataLoaded can run off the main thread: only ask for the install.
inline void install(std::uintptr_t base) {
    if (!runtime) return;
    runtime->base = base;
    runtime->pending.store(true,std::memory_order_release);
}
// Any thread.
inline void deactivate() {
    if (runtime) runtime->active.store(false,std::memory_order_release);
}
// Main thread, before the camera update.
inline void frameStart() {
    if (!runtime) return;
    auto& r = *runtime;
    if (r.pending.exchange(false,std::memory_order_acq_rel)) commit();
    if (!r.installed) return;
    if (r.opened.load(std::memory_order_acquire)) fault(Fault::frame);
    if (r.offMain.load(std::memory_order_relaxed))
        refuse(Refusal::thread,"a render hook ran off the main thread; such calls touch nothing and are drawn natively");
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
    if (r.installed && !r.latched.load(std::memory_order_acquire) && interiorCell && std::isfinite(feet.z))
        next = makeView(r.config,eye,forward,feet,r.plane.update(feet.z,now),frame,now);
    if (!next) { r.active.store(false,std::memory_order_release); return; }
    r.view = *next; r.haveView = true; r.published = frame;
    r.budget.frame();
    r.active.store(true,std::memory_order_release);
}
inline void trace(double now) {
    if (!runtime) return;
    auto& r = *runtime;
    if (!r.config.trace || now < r.nextTrace) return;
    r.nextTrace = now+.1;
    Sample s;
    s.seconds = now; s.frame = r.frame; s.offMain = r.offMain.load(std::memory_order_relaxed);
    s.installed = r.installed;
    s.active = r.active.load(std::memory_order_acquire); s.latched = r.latched.load(std::memory_order_acquire);
    // The snapshot is reported only while it is in use; an inactive window reads nan.
    if (s.active && r.haveView) { s.haveView = true; s.view = r.view; }
    s.haveOther = r.haveOther; s.otherEye = r.otherEye; s.otherDirection = r.otherDirection;
    s.offset = r.uploaded.offset;
    s.peak = r.budget.peak; s.cap = r.budget.limit;
    s.patched = r.patched; s.rejected = r.rejected; s.unaudited = r.unaudited;
    s.window = r.window;
    r.budget.peak = 0; r.window = {}; r.haveOther = false;
    char line[768]{};
    if (format(line,sizeof(line),s)) r.emitter(line);
}
inline bool tracing() { return runtime && runtime->config.trace; }
inline const char* status() { return !runtime ? "off" : runtime->installed ? "installed" : "pending"; }
// loadSettings, only when [FixedCameraSpike] bEnabled=1 and iSeeThroughMode=3.
inline void configure(const Config& requested,void (*logger)(const char*),void (*emitter)(const char*)) {
    runtime = new Runtime;
    auto& r = *runtime;
    r.logger = logger; r.emitter = emitter;
    bool adjusted = false;
    r.config = validate(requested,&adjusted);
    r.budget.limit = r.config.maxPerFrame;
    if (adjusted) refuse(Refusal::settings,"one or more shader-cut values were out of range; safe values applied");
}
} // namespace skycrpg::camera::fixed::shader
