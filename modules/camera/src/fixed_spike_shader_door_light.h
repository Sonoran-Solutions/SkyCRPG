// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader": the door
// void's lights (iDoorVoidLights=1; only with iDoorVoid=1, bFramePass=1 and
// iCutStyle=1; the policy and the rules for which lights are in
// include/fixed_spike_shader_door_light_policy.h). Nothing here exists, is
// read or is written with iDoorVoidLights=0.
//
// Mechanism (chosen after reading the engine, see the audit below). A placed
// light behind a chosen load door is put out by setting bit 0 (app-culled) of
// its NiLight's flags, the bit the engine itself uses to put a light out: the
// per-geometry light list (ID 107784, built when a geometry's passes are
// rebuilt) skips an app-culled NiLight, and the per-frame light update (ID
// 106335) detaches a dynamic one from all geometry (ID 108285) and links it
// again (ID 106342) once the bit is clear. A static light's geometry keeps its
// passes until their light list is marked changed, so each write is followed
// by the same marking the engine's camera cull (ID 108278) makes when a light
// appears or disappears: lightListChanged on the light data of every geometry
// in the light's own list. Rejected: the NiLight fade (+134) is rewritten every
// frame for the many flickering lights (ID 17610's updater at 274EC0-274FE1);
// the BSLight dimmer (+14) and camera-cull word (+5C) are rewritten by ID
// 108278 in every render; RemoveLight / AddLight would hand engine-owned lights
// back and forth and lose their BSLight state. The bit lives on the NiLight,
// which outlives nothing it is not held by: the BSLight that holds it is the
// only way we reach it, and only while that BSLight is in the active list.
//
// Threads and locks: main thread only (the camera frame service, the shader's
// frameStart and a lifecycle call made on the main thread); every read of the
// scene's light arrays and every write is made while holding the scene node's
// light lock (+1A8, the lock ID 106335 takes), with no engine call inside it.
// The plan for the frame is pure (planLights); this file only reads the
// entries into it and applies its writes.
//
// Exits (lightDuty): an eligible frame serves this frame's chosen doors (the
// frame pass's own selectDoors over the same census, feet and radius); a menu
// frame, or a hook-silent one within the grace or explained by a pausing menu,
// holds (nothing read or written); every other ineligible frame, an inactive
// publish, a latch, a cell change, a game load and the Loading Menu restore
// everything held. A held light whose BSLight left the active list, or holds
// another NiLight, is dropped without any write (FCS-108 once, counted).
// There is no plugin shutdown path; the bit is runtime state of the scene
// graph, never saved.

namespace skycrpg::camera::fixed::shader {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
// BSShaderManager::State, ID 390951: its first pointer is the main ShadowSceneNode (the light builder ID 17610 loads
// [20D68B0] as AddLight's scene node at 2748B9). The node is checked by class (vtable ID 254646) on every use.
constexpr Address lightStateAddress{390951,0x20d68b0};
constexpr Address sceneVtableAddress{254646,0x1b3dba0};
// BSLight, ID 255404: the vtable its constructor (ID 108276) writes at 157595E; AddLight builds the plain point
// lights of the active list with it. Only entries of exactly this class are ever written.
constexpr Address lightVtableAddress{255404,0x1b4ca30};
constexpr Address lightUpdateAddress{106335,0x150d410}; // per-frame light update (render, main thread)
constexpr Address lightListAddress{107784,0x1569550};   // per-geometry light list
constexpr Address lightCullAddress{108278,0x1575d20};   // point-light camera cull
namespace light_offset {
// ShadowSceneNode: activeLights +130, activeShadowLights +148, lightQueueLock +1A8 (static_asserts below).
// BSLight: geometry list head +18 (108278 at 1575E8A), NiLight +48 (106335 at 150DA8E, 107784 at 156971C).
constexpr std::size_t lightGeometry = 0x18, lightObject = 0x48;
// NiTPointerList item: next +0, element +10 (108278 at 1575EA0-1575EA7).
constexpr std::size_t itemNext = 0x0, itemElement = 0x10;
// NiAVObject flags +F4, bit 0 app-culled (106335 at 150DAA9, 107784 at 1569720); world translate +A0 (108278 at
// 1575D45-1575D5E, 106335 at 150DAC5); NiLight radius +128 (108278 at 1575D66).
constexpr std::size_t objectFlags = 0xf4, worldTranslate = 0xa0, lightRadius = 0x128;
constexpr std::uint32_t appCulled = 1u;
// BSGeometry shader property +128, BSShaderProperty light data +70, BSShaderPropertyLightData lightListChanged +20
// (108278 at 1575EAF-1575EBF; the pass builder clears it at 151A528-151A52C when it rebuilds the passes).
constexpr std::size_t geometryShader = 0x128, shaderLightData = 0x70, lightListChanged = 0x20;
} // namespace light_offset
// CommonLib declarations this file uses, pinned to the audited offsets (the combined build pins the same ones).
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,activeLights) == 0x130-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,activeShadowLights) == 0x148-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,lightQueueLock) == 0x1a8-0x128);
static_assert(offsetof(RE::BSLight,light) == light_offset::lightObject);
static_assert(offsetof(RE::NiLight::LIGHT_RUNTIME_DATA,radius) == light_offset::lightRadius-0x110);
// The menu hold reads what the fill light's reads (combined build, fixed_spike_light.h): UI +160 counts open menus
// that pause the game (116A552-116A559 / 116A0CA-116A0D1); PlayerCamera +28 the current state, its id +18, the
// Tween state id 7 (ID 50819 at 8F9A06-8F9A10).
static_assert(offsetof(RE::UI,numPausesGame) == 0x160);
static_assert(offsetof(RE::TESCamera,currentState) == 0x28);
static_assert(offsetof(RE::TESCameraState,id) == 0x18);
static_assert(RE::CameraState::kTween == 7);
// The instructions whose behaviour this file relies on, compared byte for byte at install (they also prove the
// offsets above on this build).
struct LightCode { std::uintptr_t rva{}; const std::uint8_t* bytes{}; std::size_t size{}; };
// 106335: a dynamic point light (+46, +44) whose NiLight is app-culled is detached (call 108285); else its position
// is copied and ID 106342 links it again. The refcount test before it drops a light only its BSLight still holds.
constexpr std::array<std::uint8_t,46> updateBytes{0x48,0x8b,0x47,0x48,0x48,0x85,0xc0,0x74,0x79,0x83,0x78,0x08,0x01,0x74,0x73,0x80,0x7f,0x46,
    0x00,0x74,0x54,0x80,0x7f,0x44,0x00,0x74,0x4e,0xf6,0x80,0xf4,0x00,0x00,0x00,0x01,0x74,0x0a,0x48,0x8b,0xcf,0xe8,0xb6,0x8d,0x06,0x00,0xeb,0x3b};
// 106335: the light lock it holds for the whole update (lea rdx,[rcx+1A8]; the lock body 199FA0).
constexpr std::array<std::uint8_t,7> lockBytes{0x48,0x8d,0x91,0xa8,0x01,0x00,0x00};
// 107784: a point light whose camera cull is 0xFF or whose NiLight is app-culled is left out of the list.
constexpr std::array<std::uint8_t,22> listBytes{0x81,0x7a,0x5c,0xff,0x00,0x00,0x00,0x74,0x15,0x48,0x8b,0x4a,0x48,0xf6,0x81,0xf4,0x00,0x00,0x00,
    0x01,0x75,0x08};
// 108278: the marking walk this file repeats (geometry list, element, shader property, light data, changed = 1).
constexpr std::array<std::uint8_t,62> markBytes{0x49,0x8b,0x4a,0x18,0x48,0x85,0xc9,0x74,0x35,0x0f,0x1f,0x40,0x00,0x66,0x0f,0x1f,0x84,0x00,0x00,
    0x00,0x00,0x00,0x48,0x8d,0x41,0x10,0x48,0x8b,0x09,0x48,0x8b,0x00,0x48,0x85,0xc0,0x74,0x14,0x48,0x8b,0x80,0x28,0x01,0x00,0x00,0x48,0x8b,
    0x50,0x70,0x48,0x85,0xd2,0x74,0x04,0xc6,0x42,0x20,0x01,0x48,0x85,0xc9,0x75,0xd8};
// The pass builder that calls 107784 (151A88C): it rebuilds when lightListChanged is set and clears it.
constexpr std::array<std::uint8_t,8> rebuildBytes{0x49,0x8b,0x47,0x70,0x44,0x88,0x60,0x20};
constexpr std::array<LightCode,5> lightCode{{{0x150da8e,updateBytes.data(),updateBytes.size()},{0x150d439,lockBytes.data(),lockBytes.size()},
    {0x1569713,listBytes.data(),listBytes.size()},{0x1575e8a,markBytes.data(),markBytes.size()},{0x151a528,rebuildBytes.data(),rebuildBytes.size()}}};
constexpr unsigned maxLinkedGeometry = 16384; // the marking walk's bound (a light's own geometry list)

struct DoorLightRuntime {
    Feature state{Feature::pending};
    const std::uint8_t* stateSlot{};
    std::uintptr_t sceneVtable{},lightVtable{};
    LightBook book{};
    LightBook traced{};          // the held set as last written to the trace
    LightCounts window{};        // dimmed / restored / lost / conflict since the last row
    LightCounts last{};          // the last served frame's foreign / unplaced / shadow / other / full
    LightSilence silence{};
    std::uint32_t cell{};        // the cell the book belongs to
    std::size_t doors{};         // doors chosen in the last served frame
    const char* duty{"off"};
    const char* cause{"none"};
    std::atomic<unsigned> requests{}; // asked from a lifecycle event: 1 cell, 2 load, 4 Loading Menu
    bool loadPending{};
    unsigned long long dims{},restores{},lost{};
    unsigned long long truncated{}; // marking walks cut at maxLinkedGeometry
};
constexpr unsigned lightAskCell = 1u, lightAskLoad = 2u, lightAskLoading = 4u;
constexpr const char* lightAskName(unsigned asked) {
    return (asked & lightAskLoading) ? "loading-menu" : (asked & lightAskLoad) ? "load" : (asked & lightAskCell) ? "cell" : "none";
}

// The main ShadowSceneNode, or null. Another class in the slot ends the lights for the session; what is held is
// dropped without a write (no list can be read to check it).
inline RE::ShadowSceneNode* lightScene() {
    auto& l = *runtime->doorLights;
    const auto node = peek<RE::ShadowSceneNode*>(l.stateSlot,0);
    if (!node) return nullptr;
    if (peek<std::uintptr_t>(node,0) == l.sceneVtable) return node;
    l.lost += l.book.count; l.window.lost += static_cast<unsigned>(l.book.count);
    l.book = {}; l.state = Feature::refused;
    refuse(Refusal::doorLightScene,"the main ShadowSceneNode slot holds another class; the door void's lights are off for the session, held lights "
        "dropped without a write");
    return nullptr;
}
// ID 108278's marking walk (1575E8A-1575EC3), the same reads and the same write, bounded.
inline void markLinked(const void* entry) {
    auto item = peek<const std::uint8_t*>(entry,light_offset::lightGeometry);
    unsigned hops = 0;
    for (;item && hops < maxLinkedGeometry;++hops) {
        const auto geometry = peek<const std::uint8_t*>(item,light_offset::itemElement);
        item = peek<const std::uint8_t*>(item,light_offset::itemNext);
        const auto property = geometry ? peek<const std::uint8_t*>(geometry,light_offset::geometryShader) : nullptr;
        const auto data = property ? peek<std::uint8_t*>(property,light_offset::shaderLightData) : nullptr;
        if (data) data[light_offset::lightListChanged] = 1u;
    }
    if (item) ++runtime->doorLights->truncated;
}
// Main thread. want: serve `chosen` (keep); false: restore everything held (off). The arrays are read and every
// write is made under the light lock; no engine function is called while it is held.
inline void serveDoorLights(bool want,const DoorSelection& chosen,const char* duty,const char* cause) {
    auto& r = *runtime;
    auto& l = *r.doorLights;
    l.duty = duty; l.cause = cause; l.doors = want ? chosen.count : 0u;
    if (l.state != Feature::installed) return;
    if (!want && !l.book.count) { l.book.leftCount = 0; l.last = {}; return; } // nothing held: nothing read
    RE::ShadowSceneNode* const node = lightScene();
    if (!node) return; // no scene node now: what is held is checked against the lists once there is one
    const float depth = r.config.frame.doorDepth;
    const LightCensus& census = r.style->lights;
    std::array<LightSeen,maxSeenLights> seen{};
    std::size_t n = 0;
    LightCounts extra{};
    LightCounts now{};
    {
        auto& data = node->GetRuntimeData();
        RE::BSSpinLockGuard guard(data.lightQueueLock);
        for (const auto& entry : data.activeLights) {
            const void* const e = entry.get();
            const auto light = e ? peek<const std::uint8_t*>(e,light_offset::lightObject) : nullptr;
            if (!light) continue;
            const Vec at = peek<Vec>(light,light_offset::worldTranslate);
            const bool held = l.book.holds(e);
            if (!held && (!want || laneOf(chosen,depth,at) < 0)) continue;
            // Room for every held entry is kept: a held entry missing from the list would be dropped unwritten.
            if (!held && n >= maxSeenLights-maxDoorLights) { ++extra.full; continue; }
            if (n >= maxSeenLights) { ++extra.full; continue; }
            seen[n++] = {e,light,at,peek<float>(light,light_offset::lightRadius),
                (peek<std::uint32_t>(light,light_offset::objectFlags) & light_offset::appCulled) != 0u,peek<std::uintptr_t>(e,0) == l.lightVtable};
        }
        // Shadow lights are never written; those in a lane at a placed reference are counted.
        if (want) for (const auto& entry : data.activeShadowLights) {
            const void* const e = entry.get();
            const auto light = e ? peek<const std::uint8_t*>(e,light_offset::lightObject) : nullptr;
            const Vec at = light ? peek<Vec>(light,light_offset::worldTranslate) : Vec{};
            if (light && laneOf(chosen,depth,at) >= 0 && placedAt(census,at)) ++extra.shadow;
        }
        const LightPlan plan = planLights(l.book,seen.data(),n,want,chosen,depth,census);
        for (std::size_t i = 0;i < plan.count;++i) {
            const LightSeen& s = seen[plan.actions[i].seen];
            auto* const light = const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(s.light));
            std::uint32_t flags = peek<std::uint32_t>(light,light_offset::objectFlags);
            flags = plan.actions[i].act == LightAct::dim ? (flags | light_offset::appCulled) : (flags & ~light_offset::appCulled);
            std::memcpy(light+light_offset::objectFlags,&flags,sizeof(flags));
            markLinked(s.entry);
        }
        l.book = plan.book;
        now = plan.counts;
    } // the light lock ends here: counters and log lines after it
    now.shadow += extra.shadow; now.full += extra.full;
    l.window.dimmed += now.dimmed; l.window.restored += now.restored; l.window.lost += now.lost; l.window.conflict += now.conflict;
    l.last = now;
    l.dims += now.dimmed; l.restores += now.restored; l.lost += now.lost;
    if (now.lost) refuse(Refusal::doorLightLost,"a light the door void had put out left the scene's active lights (or holds another NiLight) before "
        "it was restored; dropped without a write (informative)");
    if (now.conflict) refuse(Refusal::doorLightConflict,"a light the door void had put out was lit again by another writer; it is left as that "
        "writer set it and not put out again while its door stays chosen (informative)");
}
// Main thread: an off pass for a lifecycle request (cell, load, Loading Menu) made on any thread.
inline void takeDoorLightRequests() {
    auto& l = *runtime->doorLights;
    const unsigned asked = l.requests.exchange(0u,std::memory_order_acq_rel);
    if (!asked) return;
    if (asked & lightAskLoad) l.loadPending = true;
    serveDoorLights(false,{},"off",lightAskName(asked));
}

// ---- entry points ---------------------------------------------------------------------------------
// Main thread, from commit, after the frame pass: every address and instruction this file relies on is checked once.
inline void installDoorLights(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.doorLights) return;
    auto& l = *r.doorLights;
    l.state = Feature::refused;
    unsigned failed = frameState() == Feature::installed ? 0u : 1u;
    constexpr std::array<Address,6> ids{lightStateAddress,sceneVtableAddress,lightVtableAddress,lightUpdateAddress,lightListAddress,lightCullAddress};
    for (unsigned i = 0;!failed && i < ids.size();++i)
        if (REL::ID(ids[i].id).address() != base+ids[i].rva) failed = 10u+i;
    for (unsigned i = 0;!failed && i < lightCode.size();++i)
        if (std::memcmp(reinterpret_cast<const void*>(base+lightCode[i].rva),lightCode[i].bytes,lightCode[i].size) != 0) failed = 20u+i;
    if (failed) {
        char text[256]{};
        std::snprintf(text,sizeof(text),"check=%u: the frame pass is not installed, or the light state, scene or BSLight IDs or the light code "
            "the door void's lights rely on differ (another module or another build); the lights behind load doors are left as they are",failed);
        refuse(Refusal::doorLightSite,text); return;
    }
    l.stateSlot = reinterpret_cast<const std::uint8_t*>(base+lightStateAddress.rva);
    l.sceneVtable = base+sceneVtableAddress.rva;
    l.lightVtable = base+lightVtableAddress.rva;
    l.state = Feature::installed;
}
inline Feature doorLightState() { return runtime && runtime->doorLights ? runtime->doorLights->state : Feature::off; }
// configure, after validate: the runtime exists only with iDoorVoidLights=1 (validate keeps it 0 without the door void).
inline void configureDoorLights() {
    auto& r = *runtime;
    if (r.config.frame.enabled && r.config.frame.doorVoid && r.config.frame.doorLights) r.doorLights = new DoorLightRuntime;
}
// Main thread, publish (an eligible frame): `active` is this frame's published cut. A new cell or a game load first
// restores everything held; then the doors the frame pass will draw are served.
inline void doorLightsFrame(bool active) {
    auto& r = *runtime;
    if (!r.doorLights) return;
    auto& l = *r.doorLights;
    l.silence.reset();
    takeDoorLightRequests();
    const std::uint32_t cell = styled() ? r.style->cell : 0u;
    if (cell != l.cell || l.loadPending) {
        serveDoorLights(false,{},"off",l.loadPending ? "load" : "cell");
        l.cell = cell; l.loadPending = false;
        return;
    }
    const bool want = active && r.framePass && r.framePass->state == Feature::installed && !r.framePass->broken &&
        !r.latched.load(std::memory_order_acquire) && styled() && r.style->cellStyle == CellStyle::flat;
    serveDoorLights(want,want ? selectDoors(r.style->doors,r.view.feet,r.config.frame.doorRadius) : DoorSelection{},want ? "keep" : "off",
        active ? "notflat" : "inactive");
}
// Main thread, the camera frame service's ineligible path: hold or off by the fill light's rule (lightDuty).
inline void doorLightsIdle(bool exiting,Exit cause,std::uint32_t cellId,double now) {
    if (!runtime || !runtime->doorLights || !thin::onMain()) return;
    auto& l = *runtime->doorLights;
    takeDoorLightRequests();
    if (!l.book.count) { l.duty = "off"; l.cause = exitNames[static_cast<std::size_t>(cause)]; l.doors = 0; return; }
    const auto ui = RE::UI::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const bool paused = ui && ui->numPausesGame != 0u;
    const bool tween = camera && camera->currentState && camera->currentState->id == RE::CameraState::kTween;
    const bool held = l.silence.step(paused,tween,now);
    const LightDuty duty = settleLightDuty(lightDuty(false,exiting,cause,held,false),cellId != l.cell,l.loadPending);
    if (duty == LightDuty::hold) { l.duty = lightDutyName(duty); l.cause = exitNames[static_cast<std::size_t>(cause)]; return; }
    serveDoorLights(false,{},"off",exitNames[static_cast<std::size_t>(cause)]);
}
// Any thread: a lifecycle exit. A load, a cell change and the Loading Menu restore at once on the main thread (else
// at the next main-thread frame start); a menu holds (lightDuty).
inline void doorLightsExit(unsigned asked) {
    if (!runtime || !runtime->doorLights || !asked) return;
    runtime->doorLights->requests.fetch_or(asked,std::memory_order_acq_rel);
    if (thin::onMain()) takeDoorLightRequests();
}
// Main thread, frameStart: a request made off the main thread, or a latch (the cut is native from now on).
inline void doorLightsStart() {
    auto& r = *runtime;
    if (!r.doorLights) return;
    takeDoorLightRequests();
    if (r.latched.load(std::memory_order_acquire) && r.doorLights->book.count) serveDoorLights(false,{},"off","latched");
}
// FixedDoorLight, through the frame row's emitter and budget, on change only.
inline void traceDoorLights(double now) {
    auto& r = *runtime;
    if (!r.doorLights || !r.config.trace) return;
    auto& l = *r.doorLights;
    if (sameHeld(l.book,l.traced) && !l.window.any()) return;
    LightCounts counts = l.last;
    counts.dimmed = l.window.dimmed; counts.restored = l.window.restored; counts.lost = l.window.lost; counts.conflict = l.window.conflict;
    LightTrace t;
    t.seconds = now; t.frame = r.frame; t.cell = l.cell; t.duty = l.duty; t.cause = l.cause; t.doors = l.doors; t.book = &l.book;
    t.counts = counts; t.dims = l.dims; t.restores = l.restores; t.lost = l.lost;
    char line[sizeof(TraceRow)]{};
    if (formatDoorLight(line,sizeof(line),t)) r.frameEmitter(line);
    l.traced = l.book; l.window = {};
}
} // namespace skycrpg::camera::fixed::shader
