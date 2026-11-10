// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike.h. P5.5-02 wave 2 package "clip" (DYL-217) adapter:
// [FixedCameraSpike] iSeeThroughMode=2. Nothing here is allocated, installed
// or evaluated in any other mode.
//
// Threads. The camera frame service publishes the plane on Main::threadID and,
// in its first eligible frame with no frame being drawn, installs the hooks.
// The hooks (BSShader slots 6 and 7 of the selected classes, and the per-frame
// constant buffer upload) are expected on the same thread, because the engine
// draws through one immediate device context; that is checked on every call.
// A call on another thread touches no engine state and stays native; while a
// snapshot is active it is counted and reported (FCS-47, logged once). Other
// threads only clear `active`.
#include "fixed_spike_clip_policy.h"
#include <memory>

namespace skycrpg::camera::fixed {
// Defined in fixed_spike.h (hide section).
inline Kind kindOf(const RE::TESObjectREFR* ref,const RE::TESBoundObject* base);
inline Extent extentOf(const RE::NiAVObject* root,const RE::TESBoundObject* base,bool& sphere);
}
namespace skycrpg::camera::fixed::clip {
// Both engine functions are called with every register argument passed on and
// RAX returned, whatever their real arity.
using PassFunction = std::uintptr_t(*)(void*,void*,std::uintptr_t,std::uintptr_t);
using Upload = std::uintptr_t(*)(void*,std::uintptr_t,std::uintptr_t,std::uintptr_t);
using OwnerLookup = RE::TESObjectREFR*(*)(RE::NiAVObject*);
// The engine's camera data as of its last per-frame upload, and the plan made from it.
struct Epoch {
    bool known{},evaluated{};
    std::uintptr_t flags{};
    std::array<std::uint8_t,offset::cameraSize> camera{};
    Vec posAdjust{},previousPosAdjust{};
    std::shared_ptr<const View> view;
    Plan plan{};
};
struct Applied { float depthMin{},viewportMin{},written{}; };
struct ShaderVerdict { const std::uint8_t* shader{}; bool reads{}; };
struct Runtime {
    Config config;
    void (*logger)(const char*){};
    void (*emitter)(const char*){};
    std::atomic<std::uint64_t> refusals{};
    std::atomic_bool pending{};   // hooks to be installed by an eligible main-thread camera frame
    std::atomic_bool active{};    // a fresh snapshot is published
    std::atomic_bool latched{};   // a run-time refusal: native for the rest of the session
    std::atomic_bool opened{};    // a pass is being drawn clipped
    std::atomic_bool epochStale{}; // an upload ran where it could not be recorded
    std::atomic<unsigned long long> offMain{};
    std::atomic<std::shared_ptr<const View>> view;
    // Main thread only from here on.
    bool installed{},planSeen{},deferNoted{};
    unsigned stateNotes{};
    std::uintptr_t base{};
    std::uint8_t* state{};
    const std::uint8_t* gate{};
    const float* bias{};
    void* renderer{};
    Upload upload{};
    OwnerLookup owner{};
    std::array<PassFunction,shaderClasses.size()> setup{},restore{};
    Epoch epoch;
    Applied applied;
    Plan lastPlan;
    int lastTarget{-1};
    std::array<ShaderVerdict,4096> verdicts{};
    Admission budget;
    Window window;
    unsigned long long frame{};
    double nextTrace{};
};
// Process lifetime, like the spike runtime.
inline Runtime* runtime = nullptr;
template<class T> T peek(const void* object,std::size_t at) {
    T value{};
    std::memcpy(&value,static_cast<const std::uint8_t*>(object)+at,sizeof(T));
    return value;
}
template<class T> void poke(void* object,std::size_t at,T value) {
    std::memcpy(static_cast<std::uint8_t*>(object)+at,&value,sizeof(T));
}
// Main thread only. One line per code: FixedCameraSpike refused code=FCS-NN.
inline void refuse(Refusal code,const char* text) {
    auto& r = *runtime;
    const std::uint64_t bit = std::uint64_t{1} << static_cast<unsigned>(code);
    if (r.refusals.fetch_or(bit,std::memory_order_acq_rel) & bit) return;
    char line[448]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike refused code=FCS-%02u %s",static_cast<unsigned>(code),text);
    r.logger(line);
}
inline void latch() {
    runtime->latched.store(true,std::memory_order_release);
    runtime->active.store(false,std::memory_order_release);
}
// Any thread, while a snapshot is active: nothing is read or written but these
// atomics. The call stays native; the main thread forgets the upload it knew
// and reports it (FCS-47).
inline void offMainCall() {
    runtime->offMain.fetch_add(1ull,std::memory_order_relaxed);
    runtime->epochStale.store(true,std::memory_order_release);
}
// A pass in a view the plan cannot be used for. Checks from `gate` on mean the
// render state is not what the audit found; each is logged once.
inline void rejectPass(Reject why) {
    auto& r = *runtime;
    ++r.window.rejects[static_cast<std::size_t>(why)];
    if (!audited(why)) return;
    const unsigned bit = 1u << static_cast<unsigned>(why);
    if (r.stateNotes & bit) return;
    r.stateNotes |= bit;
    char line[448]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike refused code=FCS-%02u check=%u (%s): the render state differs from the audit; "
        "such passes are drawn natively",static_cast<unsigned>(Refusal::state),static_cast<unsigned>(why),
        rejectNames[static_cast<std::size_t>(why)]);
    r.logger(line);
}
inline Camera cameraOf(const Epoch& e) {
    Camera cam;
    cam.posAdjust = e.posAdjust;
    cam.viewDir = peek<Vec>(e.camera.data(),offset::viewDir);
    for (std::size_t i = 0;i < matrixCount;++i) cam.matrices[i] = peek<Matrix>(e.camera.data(),matrixOffsets[i]);
    return cam;
}
// Column 2 of an engine matrix: one float in each 16-byte row.
constexpr std::size_t zColumn(std::size_t matrix,std::size_t row) { return matrixOffsets[matrix]+row*16u+8u; }
inline void restoreDepth() {
    auto& r = *runtime;
    const auto camera = r.state+offset::camera;
    bool conflict = false;
    if (ownWrite(peek<float>(camera,offset::depthRange),r.applied.written)) poke(camera,offset::depthRange,r.applied.depthMin);
    else conflict = true;
    if (ownWrite(peek<float>(r.state,offset::viewportMinDepth),r.applied.written)) poke(r.state,offset::viewportMinDepth,r.applied.viewportMin);
    else conflict = true;
    poke(r.state,offset::stateFlags,peek<std::uint32_t>(r.state,offset::stateFlags)|dirtyViewport);
    if (conflict) ++r.window.conflicts;
}
// RestoreGeometry of the clipped pass has run: the engine's own matrices go
// back into the per-frame buffer and the depth range back to what it was.
inline void finish() {
    auto& r = *runtime;
    const double started = thin::seconds();
    r.upload(r.renderer,r.epoch.flags,0,0);
    restoreDepth();
    r.opened.store(false,std::memory_order_release);
    r.window.seconds += thin::seconds()-started;
}
inline void fault(Fault where) {
    finish();
    latch();
    char text[256]{};
    std::snprintf(text,sizeof(text),"cause=%u: a clipped pass was still open at %s; it was closed, native rendering for the rest of the session",
        static_cast<unsigned>(where),faultName(where));
    refuse(Refusal::bracket,text);
}
// The engine's last upload must still describe the state: what the upload
// reads from the camera data and both position adjustments. The depth range is
// left out; the engine changes it between uploads (RVA 15377F0).
inline bool drifted() {
    const auto& r = *runtime;
    const auto camera = r.state+offset::camera;
    const auto& e = r.epoch;
    return std::memcmp(camera+offset::uploadedFirst,e.camera.data()+offset::uploadedFirst,offset::uploadedSize) != 0 ||
        std::memcmp(camera+offset::uploadedSecond,e.camera.data()+offset::uploadedSecond,offset::uploadedSize) != 0 ||
        std::memcmp(r.state+offset::posAdjust,&e.posAdjust,sizeof(Vec)) != 0 ||
        std::memcmp(r.state+offset::previousPosAdjust,&e.previousPosAdjust,sizeof(Vec)) != 0;
}
// The pass is drawn with the plan's matrices and depth range. The state
// matrices are changed only for the duration of the upload call.
inline void apply(const Plan& plan,std::size_t shaderClass,const void* geometry) {
    auto& r = *runtime;
    const double started = thin::seconds();
    const auto camera = r.state+offset::camera;
    const float cameraMin = peek<float>(camera,offset::depthRange),viewportMin = peek<float>(r.state,offset::viewportMinDepth);
    const bool biasDirty = (peek<std::uint32_t>(r.state,offset::stateFlags) & dirtyDepthBias) != 0;
    const auto bias = biasOf(peek<std::uint32_t>(r.state,offset::depthBiasMode),r.bias);
    Reject why = Reject::none;
    std::optional<float> minDepth;
    if (*r.gate != 1) why = Reject::gate;
    else if (drifted()) why = Reject::drift;
    else if (!bias) why = Reject::bias;
    else {
        const auto range = flushedRange(biasDirty,cameraMin,peek<float>(camera,offset::depthRange+sizeof(float)),viewportMin,
            peek<float>(r.state,offset::viewportMaxDepth),*bias);
        if (range) minDepth = viewportMinDepth(plan.d0,*range);
        if (!minDepth) why = Reject::depthRange;
    }
    if (why != Reject::none) { rejectPass(why); return; }
    // By geometry, for the whole frame: all passes of one geometry are clipped or none is.
    if (!r.budget.admit(geometry)) { ++r.window.skips[static_cast<std::size_t>(Skip::cap)]; return; }
    for (std::size_t i = 0;i < matrixCount;++i)
        for (std::size_t row = 0;row < 4;++row) poke(camera,zColumn(i,row),plan.columns[i][row]);
    r.upload(r.renderer,r.epoch.flags,0,0);
    for (std::size_t i = 0;i < matrixCount;++i)
        for (std::size_t row = 0;row < 4;++row) poke(camera,zColumn(i,row),peek<float>(r.epoch.camera.data(),zColumn(i,row)));
    // If the flush is about to rebuild the range, the viewport's bottom ends up at the camera data's value.
    r.applied = {cameraMin,biasDirty ? cameraMin : viewportMin,*minDepth};
    poke(camera,offset::depthRange,*minDepth);
    poke(r.state,offset::viewportMinDepth,*minDepth);
    poke(r.state,offset::stateFlags,peek<std::uint32_t>(r.state,offset::stateFlags)|dirtyViewport);
    r.opened.store(true,std::memory_order_release);
    r.lastPlan = plan; r.planSeen = true;
    r.lastTarget = peek<std::int32_t>(r.state,offset::depthTarget);
    ++r.window.clipped; ++r.window.byClass[shaderClass];
    if (*bias > 0.0f) ++r.window.biased;
    r.window.seconds += thin::seconds()-started;
}
// The reference that owns a geometry, found by the engine's own walk (ID
// 19750), which knows where an owner may be read: actors, lights and
// floor-level references are never clipped, nor is geometry nobody owns.
// Main thread only; the walk takes no lock and allocates nothing.
inline Skip ownerSkip(RE::NiAVObject* geometry,const View& view) {
    const auto ref = runtime->owner(geometry);
    if (!ref) return ownerRule(false,false,Kind::excluded,{},view.feetZ,view.floorBand);
    const bool actor = ref->GetFormType() == RE::FormType::ActorCharacter;
    const auto form = actor ? nullptr : ref->GetBaseObject();
    const Kind kind = actor ? Kind::excluded : kindOf(ref,form);
    bool sphere = false;
    Extent box;
    if (kind != Kind::excluded) {
        const auto root = ref->Get3D();
        if (!root) return ownerRule(false,false,Kind::excluded,{},view.feetZ,view.floorBand);
        box = extentOf(root,form,sphere);
    }
    return ownerRule(true,actor,kind,box,view.feetZ,view.floorBand);
}
// Whether the vertex shader bound for this pass takes ViewProj from the
// per-frame buffer, remembered per shader object (they live for the session).
inline bool shaderReads() {
    auto& r = *runtime;
    const auto shader = peek<const std::uint8_t*>(r.state,offset::currentVertexShader);
    if (!shader) return false;
    const auto scan = [&] { return readsViewProjection(shader+offset::shaderCode,peek<std::uint32_t>(shader,offset::shaderCodeSize)); };
    const std::size_t first = (reinterpret_cast<std::uintptr_t>(shader) >> 4) % r.verdicts.size();
    for (std::size_t probe = 0;probe < 8;++probe) {
        auto& slot = r.verdicts[(first+probe) % r.verdicts.size()];
        if (slot.shader == shader) return slot.reads;
        if (!slot.shader) { slot = {shader,scan()}; return slot.reads; }
    }
    return scan();
}
// A pass of a hooked shader class, after its SetupGeometry. Main thread.
inline void consider(void* pass,std::size_t shaderClass) {
    auto& r = *runtime;
    if (r.latched.load(std::memory_order_relaxed)) return;
    if (!thin::onMain()) { offMainCall(); return; }
    ++r.window.passes;
    if (r.opened.load(std::memory_order_relaxed)) return; // a nested SetupGeometry: never two clips at once
    auto& e = r.epoch;
    if (r.epochStale.exchange(false,std::memory_order_acq_rel)) e.known = false;
    if (!e.known) { rejectPass(Reject::unknownEpoch); return; }
    if (!e.evaluated) {
        e.view = r.view.load(std::memory_order_acquire);
        e.plan = e.view ? evaluate(cameraOf(e),*e.view,thin::seconds()) : Plan{Reject::stale};
        e.evaluated = true;
    }
    if (e.plan.reject != Reject::none) { rejectPass(e.plan.reject); return; }
    const auto geometry = peek<std::uint8_t*>(pass,offset::passGeometry);
    Skip skip = Skip::noBound;
    if (geometry) {
        const Bound bound{peek<Vec>(geometry,offset::objectBound),peek<float>(geometry,offset::objectBound+sizeof(Vec))};
        skip = reach(e.plan,*e.view,bound,peek<const void*>(geometry,offset::geometrySkin) != nullptr);
        if (skip == Skip::none) skip = ownerSkip(reinterpret_cast<RE::NiAVObject*>(geometry),*e.view);
        if (skip == Skip::none && !shaderReads()) skip = Skip::shader;
    }
    if (skip != Skip::none) { ++r.window.skips[static_cast<std::size_t>(skip)]; return; }
    apply(e.plan,shaderClass,geometry);
}

// ---- hooks ------------------------------------------------------------------
template<std::size_t Index>
std::uintptr_t hookSetup(void* shader,void* pass,std::uintptr_t flags,std::uintptr_t extra) {
    auto& r = *runtime;
    if (r.opened.load(std::memory_order_acquire) && thin::onMain()) fault(Fault::setup);
    const auto result = r.setup[Index](shader,pass,flags,extra);
    if (pass && r.active.load(std::memory_order_acquire)) consider(pass,Index);
    return result;
}
template<std::size_t Index>
std::uintptr_t hookRestore(void* shader,void* pass,std::uintptr_t flags,std::uintptr_t extra) {
    auto& r = *runtime;
    const auto result = r.restore[Index](shader,pass,flags,extra);
    if (r.opened.load(std::memory_order_acquire)) {
        if (thin::onMain()) finish();
        else offMainCall();
    }
    return result;
}
// Every engine upload of the per-frame buffer: remember its flags argument and
// the camera data it was made from, so the clip can repeat it exactly.
inline std::uintptr_t hookUpload(void* renderer,std::uintptr_t flags,std::uintptr_t third,std::uintptr_t fourth) {
    auto& r = *runtime;
    const bool main = thin::onMain();
    if (main && r.opened.load(std::memory_order_acquire)) fault(Fault::upload);
    const auto result = r.upload(renderer,flags,third,fourth);
    if (!main) {
        // Another thread drew (a loading screen, say): the upload the main thread remembers is no longer the buffer's.
        if (r.active.load(std::memory_order_acquire)) offMainCall();
        else r.epochStale.store(true,std::memory_order_release);
        return result;
    }
    r.epochStale.store(false,std::memory_order_release);
    auto& e = r.epoch;
    // With the gate at 0 the engine function returned without writing the buffer.
    e.flags = flags; e.known = *r.gate == 1; e.evaluated = false;
    std::memcpy(e.camera.data(),r.state+offset::camera,offset::cameraSize);
    e.posAdjust = peek<Vec>(r.state,offset::posAdjust);
    e.previousPosAdjust = peek<Vec>(r.state,offset::previousPosAdjust);
    ++r.window.uploads;
    return result;
}

// ---- install ----------------------------------------------------------------
// Neither is on the pinned stack. ENB wraps the device and Community Shaders
// replaces these shaders' vfuncs; the guards below would not see ENB at all.
inline const char* foreignRenderer() {
    if (GetModuleHandleW(L"CommunityShaders.dll")) return "CommunityShaders.dll";
    if (GetModuleHandleW(L"d3dcompiler_46e.dll")) return "d3dcompiler_46e.dll (ENB)";
    return nullptr;
}
// 0 = every ID resolves to the audited RVA. 1-6 state, upload, renderer, gate,
// bias table, owner lookup; 10-12 the lighting, utility and effect vtables.
inline unsigned addressGuard(std::uintptr_t base) {
    constexpr std::array<Address,6> data{stateAddress,uploadAddress,rendererAddress,gateAddress,biasAddress,ownerAddress};
    for (unsigned i = 0;i < data.size();++i)
        if (REL::ID(data[i].id).address() != base+data[i].rva) return 1u+i;
    for (unsigned i = 0;i < shaderClasses.size();++i)
        if (REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva) return 10u+i;
    return 0;
}
// The live unwind table must describe a function exactly as the audited file does.
inline bool extentMatches(std::uintptr_t base,CodeRange rva) {
    DWORD64 image{};
    const auto entry = RtlLookupFunctionEntry(base+rva.begin,&image,nullptr);
    return entry && image == base && entry->BeginAddress == rva.begin && entry->EndAddress == rva.end;
}
// 0 = the owner lookup and slots 6 and 7 of every enabled class are the audited
// functions with the audited first bytes. 1 owner lookup bytes, 2 its unwind
// extent; otherwise 10 * (class + 1) + 1 setup slot, 2 restore slot, 3 setup
// bytes, 4 restore bytes: another module owns the slot or patched the function.
inline unsigned tableGuard(std::uintptr_t base,unsigned mask) {
    if (std::memcmp(reinterpret_cast<const void*>(base+ownerAddress.rva),ownerBytes.data(),ownerBytes.size()) != 0) return 1;
    if (!extentMatches(base,ownerExtent)) return 2;
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
// 0 = the upload entry is the audited one. 1 first bytes differ (another patch
// owns the entry), 2 live unwind extent differs.
inline unsigned uploadGuard(std::uintptr_t target,std::uintptr_t base) {
    if (std::memcmp(reinterpret_cast<const void*>(target),uploadPrologue.data(),uploadPrologue.size()) != 0) return 1;
    return extentMatches(base,uploadExtent) ? 0u : 2u;
}
// 0 = `out` calls the original upload. 3 allocation, 4 protection. 14 bytes of
// whole instructions with no RIP-relative operand, then an absolute jump back,
// as for the caster: no trampoline, never freed once used.
inline unsigned buildGateway(std::uintptr_t target,Upload& out) {
    const auto gateway = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!gateway) return 3;
    std::memcpy(gateway,uploadPrologue.data(),uploadCopied);
    const auto back = thin::jump(target+uploadCopied);
    std::memcpy(gateway+uploadCopied,back.data(),back.size());
    DWORD previous{};
    if (!VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&previous)) { VirtualFree(gateway,0,MEM_RELEASE); return 4; }
    FlushInstructionCache(GetCurrentProcess(),gateway,uploadCopied+back.size());
    out = reinterpret_cast<Upload>(gateway);
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
// Main thread, in the camera frame service of an eligible frame (normal play:
// no load in progress, no menu state), and only while no frame is being drawn.
// The camera update runs in Main::Update before the frame begins (36564 ->
// 40438 at 6588DE -> 40744 -> 40449 -> 50784; the frame, ID 36555, is called at
// 659319 and raises the gate at 6565F7), so the gate reads 0 here unless some
// other thread is inside a frame of its own (IDs 39039 and 90259 also bracket
// frames; their threads are not established). In that case nothing is written
// and the next eligible frame tries again. What remains is a thread that
// begins a frame between the last gate read and the end of the 14-byte write.
inline void commit() {
    auto& r = *runtime;
    const auto base = r.base;
    char text[256]{};
    if (const char* found = foreignRenderer()) {
        std::snprintf(text,sizeof(text),"%s is loaded; the clip is not installed, native rendering",found);
        r.pending.store(false,std::memory_order_release);
        refuse(Refusal::module,text); return;
    }
    if (const unsigned failed = addressGuard(base)) {
        std::snprintf(text,sizeof(text),"check=%u: an address ID does not resolve to the audited RVA; the clip is not installed, native rendering",failed);
        r.pending.store(false,std::memory_order_release);
        refuse(Refusal::address,text); return;
    }
    if (const unsigned failed = tableGuard(base,r.config.shaders)) {
        std::snprintf(text,sizeof(text),"check=%u: the owner lookup, a shader vtable slot or its first bytes differ (already hooked or wrong build); "
            "the clip is not installed, native rendering",failed);
        r.pending.store(false,std::memory_order_release);
        refuse(Refusal::table,text); return;
    }
    const auto target = base+uploadAddress.rva;
    unsigned failed = uploadGuard(target,base);
    if (!failed && !r.upload) failed = buildGateway(target,r.upload);
    if (failed) {
        std::snprintf(text,sizeof(text),"check=%u: the per-frame upload entry differs or its gateway could not be built; "
            "the clip is not installed, native rendering",failed);
        r.pending.store(false,std::memory_order_release);
        refuse(Refusal::prologue,text); return;
    }
    r.state = reinterpret_cast<std::uint8_t*>(base+stateAddress.rva);
    r.gate = reinterpret_cast<const std::uint8_t*>(base+gateAddress.rva);
    r.bias = reinterpret_cast<const float*>(base+biasAddress.rva);
    r.renderer = reinterpret_cast<void*>(base+rendererAddress.rva);
    r.owner = reinterpret_cast<OwnerLookup>(base+ownerAddress.rva);
    const auto detour = thin::jump(reinterpret_cast<std::uintptr_t>(&hookUpload));
    if (*r.gate != 0) {
        // A frame is in progress somewhere: its thread may be in the upload. Not now.
        if (!r.deferNoted) r.logger("FixedCameraSpike clip install deferred: a frame is being drawn; retrying in the next eligible frame");
        r.deferNoted = true;
        return;
    }
    REL::safe_write(target,detour.data(),detour.size());
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(target),detour.size());
    hookClass<0>(); hookClass<1>(); hookClass<2>();
    r.pending.store(false,std::memory_order_release);
    r.installed = true;
    std::snprintf(text,sizeof(text),"FixedCameraSpike clip installed shaders=%u height=%.1f margin=%.1f corridor=%.1f floorBand=%.1f cap=%u exterior=%u trace=%u",
        r.config.shaders,static_cast<double>(r.config.height),static_cast<double>(r.config.margin),static_cast<double>(r.config.corridor),
        static_cast<double>(r.config.floorBand),r.config.maxPerFrame,static_cast<unsigned>(r.config.exterior),static_cast<unsigned>(r.config.trace));
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
    if (r.opened.load(std::memory_order_acquire)) fault(Fault::frame);
    if (r.offMain.load(std::memory_order_relaxed))
        refuse(Refusal::thread,"a render hook ran off the main thread while the clip was active; such calls touch nothing and are drawn natively");
}
// Main thread, after the camera update of an eligible frame.
inline void publish(Vec eye,Vec forward,Vec feet,bool interiorCell,unsigned long long frame,double now) {
    if (!runtime) return;
    auto& r = *runtime;
    r.frame = frame;
    if (r.pending.load(std::memory_order_acquire)) commit();
    std::optional<View> next;
    if (r.installed && !r.latched.load(std::memory_order_acquire) && cellAllowed(interiorCell,r.config.exterior))
        next = makeView(r.config,eye,forward,feet,frame,now);
    if (!next) { r.active.store(false,std::memory_order_release); return; }
    r.view.store(std::make_shared<const View>(*next),std::memory_order_release);
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
    if (const auto current = s.active ? r.view.load(std::memory_order_acquire) : nullptr) { s.haveView = true; s.view = *current; }
    if (r.planSeen) { s.havePlan = true; s.plan = r.lastPlan; s.depthTarget = r.lastTarget; }
    s.peak = r.budget.peak; s.cap = r.budget.limit;
    s.window = r.window;
    r.budget.peak = 0; r.window = {}; r.planSeen = false;
    char line[768]{};
    if (format(line,sizeof(line),s)) r.emitter(line);
}
inline bool tracing() { return runtime && runtime->config.trace; }
inline const char* status() { return !runtime ? "off" : runtime->installed ? "installed" : "pending"; }
// loadSettings, only when [FixedCameraSpike] bEnabled=1 and iSeeThroughMode=2.
inline void configure(const Config& requested,void (*logger)(const char*),void (*emitter)(const char*)) {
    runtime = new Runtime;
    auto& r = *runtime;
    r.logger = logger; r.emitter = emitter;
    bool adjusted = false;
    r.config = validate(requested,&adjusted);
    r.budget.limit = r.config.maxPerFrame;
    if (adjusted) refuse(Refusal::settings,"one or more clip values were out of range; safe values applied");
}
} // namespace skycrpg::camera::fixed::clip
