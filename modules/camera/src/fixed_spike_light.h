// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike.h after its runtime helpers. P5.5-02 wave 2 package
// "light" (DYL-217). Nothing is allocated, read or installed unless
// [FixedCameraSpike] iLightFillMode=1 or iLightTrace=1. The light trace reads
// the main ShadowSceneNode under its own light lock, and the fill light is an
// ordinary dynamic point light handed to the engine's AddLight. One code patch,
// only with iLightFillMode=1: the two light-level summers (IDs 106362 and
// 107776, each with a single caller in ID 39946) are wrapped at their entry so
// the fill light's own term can be taken out of every actor's light level.
#include "fixed_spike_light_policy.h"

namespace skycrpg::camera::fixed::light {
// Audited on 1.7.104 (docs/evidence/p5.5/spike-wave2-light.md, binary audit).
constexpr std::uintptr_t stateRva = 0x20d68b0;       // ID 390951, BSShaderManager::State; [0] is the main ShadowSceneNode
constexpr std::uintptr_t sceneVtableRva = 0x1b3dba0; // ID 254646, ShadowSceneNode
constexpr std::uintptr_t pointCtorRva = 0xefe4e0;    // ID 70967, NiPointLight::NiPointLight (0x150-byte object)
constexpr std::uintptr_t pointVtableRva = 0x1a4a658; // ID 237259, NiPointLight (written by the ctor at EFE4EE)
constexpr std::uintptr_t addLightRva = 0x150c810;    // ID 106326, ShadowSceneNode::AddLight(NiLight*, const LIGHT_CREATE_PARAMS&)
constexpr std::uintptr_t removeLightRva = 0x150cff0; // ID 106331, ShadowSceneNode::RemoveLight(NiLight*)
constexpr std::array<std::uint8_t,14> pointCtorPrologue{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xe8,0x52,0xec,0xfe,0xff};
constexpr std::array<std::uint8_t,16> addLightPrologue{0x4c,0x8b,0xdc,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83};
constexpr std::array<std::uint8_t,15> removeLightPrologue{0x48,0x85,0xd2,0x0f,0x84,0x1e,0x01,0x00,0x00,0x53,0x48,0x83,0xec,0x30,0x48};
// Light level (stealth). ID 39946 (726250) sums, for the player, ID 106362 over
// the scene node's active and shadow lights (call 726423), for anyone else ID
// 107776 over the lights linked to that actor's geometry (call 7264AC); both
// add ID 108292 per light, which skips only the caller's own light (exclude)
// and has no hidden, frustum or dimmer test. Both summers start with the same
// 15 bytes of whole, position-independent instructions (three register saves
// into the shadow space); nothing branches into them.
constexpr std::uintptr_t playerLevelRva = 0x150f930; // ID 106362
constexpr std::uintptr_t listLevelRva = 0x1569070;   // ID 107776
constexpr std::uintptr_t lightTermRva = 0x1576ca0;   // ID 108292
constexpr std::uintptr_t playerLevelCallRva = 0x726423, listLevelCallRva = 0x7264ac; // the only callers (E8 rel32)
constexpr std::array<std::uint8_t,15> levelPrologue{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
constexpr std::array<std::uint8_t,16> lightTermPrologue{0x48,0x83,0xec,0x38,0x4c,0x8b,0x49,0x48,0x4d,0x3b,0xc8,0x75,0x08,0x0f,0x57,0xc0};
// CommonLib declarations this file reads or writes, pinned to the audited offsets.
static_assert(offsetof(RE::BSLight,lodDimmer) == 0x14);    // written with +5C by 108278 / 108618 / 108625
static_assert(offsetof(RE::BSLight,light) == 0x48);        // NiLight; SetLight (108289) at 1576AF2
static_assert(offsetof(RE::BSLight,frustrumCull) == 0x5c); // 0 visible / 0xFF culled
static_assert(offsetof(RE::BSLight,portalStrict) == 0x47); // read by 106400 (151257D) / 106401 (15132C9); AddLight 150C9F0
static_assert(offsetof(RE::BSLight,rooms) == 0xd8);        // the light's own rooms, appended by 108279 (1575EF0), removed by 108280
static_assert(offsetof(RE::NiAVObject,parent) == 0x30);    // FindReferenceFor3D (ID 19750) walks +30 and reads node +F8
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,activeLights) == 0x130-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,activeShadowLights) == 0x148-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,lightQueueAdd) == 0x160-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,lightQueueRemove) == 0x178-0x128);
static_assert(offsetof(RE::ShadowSceneNode::RUNTIME_DATA,lightQueueLock) == 0x1a8-0x128);
static_assert(offsetof(RE::BSShadowLight::RUNTIME_DATA,maskIndex) == 0x520-0x148);
static_assert(offsetof(RE::NiLight::LIGHT_RUNTIME_DATA,ambient) == 0x110-0x110);
static_assert(offsetof(RE::NiLight::LIGHT_RUNTIME_DATA,diffuse) == 0x11c-0x110);
static_assert(offsetof(RE::NiLight::LIGHT_RUNTIME_DATA,radius) == 0x128-0x110);
static_assert(offsetof(RE::NiLight::LIGHT_RUNTIME_DATA,fade) == 0x134-0x110);
// AddLight copies +0 -> BSLight+46 (dynamic), +2 -> +47 (portal-strict), +3 -> +61, +4 -> +62,
// +5 -> +63, +20 -> +130, +28 -> +138 (150C9E5-150CA46). CommonLib calls +5
// "neverFades", but +63 ENABLES the camera-distance fade in 108278 (1575DD9):
// false means the light never fades.
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,dynamic) == 0);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,shadowLight) == 1);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,portalStrict) == 2);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,affectLand) == 3);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,affectWater) == 4);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,neverFades) == 5);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,restrictedNode) == 0x20);
static_assert(offsetof(RE::ShadowSceneNode::LIGHT_CREATE_PARAMS,lensFlareData) == 0x28);
// UI +160 counts open menus whose flags (IMenu +1C) have bit 0 (pauses game):
// incremented at 116A552-116A559, decremented at 116A0CA-116A0D1. Read on the
// main thread, which opens and closes menus; a plain counter, no string.
static_assert(offsetof(RE::UI,numPausesGame) == 0x160);
// PlayerCamera (ID 400802, 31A5478) +28 is the current camera state (ID 50819
// compares it with cameraStates[kTween] at +F0, 8F9A06-8F9A10); a state's id is
// at +18 (ID 50816 reads it at 8F9948) and the Tween state is built with id 7
// (ID 50783, 8F82C9-8F82F4). Read on the main thread; no string.
static_assert(offsetof(RE::TESCamera,currentState) == 0x28);
static_assert(offsetof(RE::TESCameraState,id) == 0x18);
static_assert(RE::CameraState::kTween == 7);

// ID 106362: (scene node, position, light count out, sun term out, ambient term
// out, exclude light, shadow-light mask) -> sum of ID 108292 (726414-726423 sets
// rcx, rdx, r8, r9 and [rsp+20/28/30]; the body reads them at 150F930-150F97C).
using PlayerLevel = float (*)(RE::ShadowSceneNode*,const RE::NiPoint3*,std::uint32_t*,float*,float*,RE::NiLight*,std::uint32_t);
// ID 107776: (light list, position, exclude light, first index) -> sum (1569070-15690BF).
using ListLevel = float (*)(RE::BSShaderPropertyLightData*,const RE::NiPoint3*,RE::NiLight*,std::uint32_t);
// ID 108292: (light, position, exclude light) -> fade x mean(diffuse) x falloff; it also stores that value at light+10.
using LightTerm = float (*)(RE::BSLight*,const RE::NiPoint3*,RE::NiLight*);
// Read by the wrappers on whichever thread runs ID 39946: atomics and values
// fixed before the code is patched only.
struct Stealth {
    std::atomic<const RE::NiLight*> fill{nullptr}; // published once the fill light exists, never cleared (never freed)
    bool neutral{};
    PlayerLevel player{};
    ListLevel list{};
    LightTerm term{};
    std::atomic<float> with{std::numeric_limits<float>::quiet_NaN()},without{std::numeric_limits<float>::quiet_NaN()};
    std::atomic<unsigned long long> playerCalls{0},npcCalls{0},npcAdjusted{0};
    bool installed{};
};
inline Stealth stealth;

struct LightRuntime {
    Config config;
    bool ready{};      // main ShadowSceneNode located and of the audited class
    bool fillReady{};  // AddLight / RemoveLight / NiPointLight verified
    RE::NiPointer<RE::NiPointLight> fill; // process lifetime, never released: the engine may still hold it
    bool fillHidden{true};
    bool lastWanted{};
    FillTrack track;
    Presence presence{Presence::absent};
    unsigned adds{},removes{};
    RE::FormID cell{};
    bool loadPending{}; // a game load was signalled since the last serviced frame
    SilenceHold silence; // which hook-silent frames belong to a menu (fillDuty)
    double nextCheck{},nextTrace{},nextShadows{},nextLevel{};
    LineBudget lines{lightTraceLines},levelLines{lightLevelTraceLines};
};
constexpr unsigned maxPlayerRooms = 8;
inline LightRuntime* lights = nullptr;

inline void emitLight(const char* text) { emit(lights->lines,text); }
inline void fillEvent(const char* event,const char* cause) {
    if (!lights->config.trace) return;
    char line[320]{};
    if (formatFillEvent(line,sizeof(line),runtime->now,runtime->frame,event,cause,lights->presence,lights->adds,lights->removes,
        lights->track.drops)) emitLight(line);
}
template<std::size_t Size>
bool sameBytes(std::uintptr_t address,const std::array<std::uint8_t,Size>& bytes) {
    return std::memcmp(reinterpret_cast<const void*>(address),bytes.data(),bytes.size()) == 0;
}
// The main ShadowSceneNode, or null. Checked by class on every use, so a
// different object in the slot ends the light package for the session.
inline RE::ShadowSceneNode* scene() {
    if (!lights || !lights->ready) return nullptr;
    const auto node = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
    if (!node) return nullptr;
    if (*reinterpret_cast<const std::uintptr_t*>(node) != REL::Module::get().base()+sceneVtableRva) {
        lights->ready = lights->fillReady = false;
        refuse(Refusal::sceneReplaced,"main ShadowSceneNode slot holds another class; light trace and fill light off for the session");
        return nullptr;
    }
    return node;
}
struct PlayerRooms {
    std::array<const RE::BSMultiBoundRoom*,maxPlayerRooms> rooms{};
    unsigned count{};
    bool graph{}; // the cell has a portal graph with rooms
};
// Main thread, before the light lock: the rooms whose vanilla QPointWithin
// (the original slot, never a seeded claim) contains the census point.
inline PlayerRooms playerRooms(RE::TESObjectCELL* parent,Vec at) {
    PlayerRooms out;
    const auto loaded = parent ? parent->GetRuntimeData().loadedData : nullptr;
    const auto graph = loaded ? loaded->portalGraph.get() : nullptr;
    if (!graph || graph->rooms.empty()) return out;
    out.graph = true;
    unsigned index{};
    for (const auto& room : graph->rooms) {
        if (index++ == maxRooms || out.count == maxPlayerRooms) break;
        if (room && within(room.get(),at)) out.rooms[out.count++] = room.get();
    }
    return out;
}
inline LightView view(const RE::BSLight& entry,const PlayerRooms& player) {
    const auto& source = *entry.light;
    const auto& at = source.world.translate;
    // Pointer comparison only: no room the light lists is dereferenced.
    const bool shares = anyShared<const RE::BSMultiBoundRoom*>(entry.rooms.data(),entry.rooms.size(),player.rooms.data(),player.count);
    return {{at.x,at.y,at.z},source.GetLightRuntimeData().radius.x,entry.lodDimmer,entry.frustrumCull,source.GetAppCulled(),
        entry.portalStrict,shares};
}
// The placed reference whose 3D holds the NiLight, as ID 19750 finds it.
inline std::uint32_t lightReference(const RE::NiLight& source) {
    const auto ref = source.parent ? source.parent->GetUserData() : nullptr;
    return ref ? ref->GetFormID() : 0u;
}
template<class Array>
bool holds(const Array& array,const RE::NiLight* ours) {
    for (const auto& entry : array) if (entry && entry->light.get() == ours) return true;
    return false;
}
// Main thread. Reads the engine's light arrays under the lock ID 106335 takes
// (+1A8) to move queued lights; holds no engine call while locked.
inline Presence presenceNow(RE::ShadowSceneNode* node) {
    const RE::NiLight* ours = lights->fill.get();
    if (!ours) return Presence::absent;
    auto& data = node->GetRuntimeData();
    RE::BSSpinLockGuard guard(data.lightQueueLock);
    return presenceOf(holds(data.activeLights,ours),holds(data.lightQueueAdd,ours),holds(data.lightQueueRemove,ours));
}
inline void census(RE::ShadowSceneNode* node,const PlayerRooms& player,Vec at,LightSample& s,ShadowList& shadows) {
    const RE::NiLight* mine = lights->fill.get();
    s.census.cellHasRooms = player.graph; s.census.playerRooms = player.count;
    auto& data = node->GetRuntimeData();
    RE::BSSpinLockGuard guard(data.lightQueueLock);
    // A light queued for removal may be losing its 3D: no reference walk for it.
    const auto leaving = [&](const RE::BSLight* light) {
        for (const auto& queued : data.lightQueueRemove) if (queued.get() == light) return true;
        return false;
    };
    for (const auto& entry : data.activeLights) {
        if (!entry || !entry->light) continue;
        if (entry->light.get() == mine) {
            s.fill.found = true; s.fill.cull = entry->frustrumCull; s.fill.dimmer = entry->lodDimmer;
            continue;
        }
        s.census.addPoint(view(*entry,player),at);
    }
    for (const auto& entry : data.activeShadowLights) {
        if (!entry || !entry->light) continue;
        const auto seen = view(*entry,player);
        const auto channel = entry->GetRuntimeData().maskIndex;
        const float r = s.census.addShadow(seen,channel,at);
        shadows.offer({leaving(entry.get()) ? 0u : lightReference(*entry->light),r,seen.radius,channel,seen.cull,seen.portalStrict,seen.sharesRoom,
            lightsPoint(r <= 0.0f,seen.portalStrict,player.graph,seen.sharesRoom),seen.hidden});
    }
}

// ---- light level (stealth) -----------------------------------------------------
// Sum of the fill light's own terms in what the summer just added up, with the
// engine's ID 108292 (same inputs, same arithmetic). The lists are the ones the
// engine walked a moment ago on this same thread; only the entry pointers and
// their light pointers are compared.
inline float hookPlayerLevel(RE::ShadowSceneNode* node,const RE::NiPoint3* at,std::uint32_t* count,float* sun,float* ambient,
    RE::NiLight* exclude,std::uint32_t mask) {
    const float total = stealth.player(node,at,count,sun,ambient,exclude,mask);
    stealth.playerCalls.fetch_add(1ull,std::memory_order_relaxed);
    const RE::NiLight* ours = stealth.fill.load(std::memory_order_acquire);
    float fill = 0.0f;
    if (ours && node && at)
        for (const auto& entry : node->GetRuntimeData().activeLights)
            if (entry && entry->light.get() == ours) fill += stealth.term(entry.get(),at,exclude);
    const float without = withoutFill(total,fill);
    stealth.with.store(total,std::memory_order_relaxed); stealth.without.store(without,std::memory_order_relaxed);
    return stealth.neutral ? without : total;
}
inline float hookListLevel(RE::BSShaderPropertyLightData* data,const RE::NiPoint3* at,RE::NiLight* exclude,std::uint32_t first) {
    const float total = stealth.list(data,at,exclude,first);
    stealth.npcCalls.fetch_add(1ull,std::memory_order_relaxed);
    const RE::NiLight* ours = stealth.fill.load(std::memory_order_acquire);
    if (!ours || !data || !at) return total;
    float fill = 0.0f;
    const auto& list = data->lights;
    for (std::uint32_t i = first;i < list.size();++i)
        if (list[i] && list[i]->light.get() == ours) fill += stealth.term(list[i],at,exclude);
    if (!(fill > 0.0f)) return total;
    stealth.npcAdjusted.fetch_add(1ull,std::memory_order_relaxed);
    return stealth.neutral ? withoutFill(total,fill) : total;
}
// installLight(), kDataLoaded, only with iLightFillMode=1. True = both wrappers
// are in place. Every check precedes the first write; the gateways are sealed
// execute-read before either entry is patched.
inline bool installStealth(std::uintptr_t base) {
    const auto callsTo = [&](std::uintptr_t site,std::uintptr_t target) {
        const auto bytes = reinterpret_cast<const std::uint8_t*>(base+site);
        std::int32_t displacement{};
        std::memcpy(&displacement,bytes+1,sizeof(displacement));
        return bytes[0] == 0xe8 && base+site+5+static_cast<std::uintptr_t>(static_cast<std::intptr_t>(displacement)) == base+target;
    };
    if (REL::ID(106362).address() != base+playerLevelRva || REL::ID(107776).address() != base+listLevelRva ||
        REL::ID(108292).address() != base+lightTermRva || !sameBytes(base+playerLevelRva,levelPrologue) ||
        !sameBytes(base+listLevelRva,levelPrologue) || !sameBytes(base+lightTermRva,lightTermPrologue) ||
        !callsTo(playerLevelCallRva,playerLevelRva) || !callsTo(listLevelCallRva,listLevelRva)) {
        refuse(Refusal::stealthGuard,"light-level summers (IDs 106362, 107776, 108292) or their callers differ from the audited code; fill light not stealth-neutral");
        return false;
    }
    const auto gateway = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,128,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    bool sealed = false;
    if (gateway) {
        constexpr std::array<std::pair<std::size_t,std::uintptr_t>,2> copies{{{0,playerLevelRva},{64,listLevelRva}}};
        for (const auto& [offset,target] : copies) {
            std::memcpy(gateway+offset,levelPrologue.data(),levelPrologue.size());
            const auto back = thin::jump(base+target+levelPrologue.size());
            std::memcpy(gateway+offset+levelPrologue.size(),back.data(),back.size());
        }
        DWORD previous{};
        sealed = VirtualProtect(gateway,128,PAGE_EXECUTE_READ,&previous) != 0;
        if (!sealed) VirtualFree(gateway,0,MEM_RELEASE);
    }
    if (!sealed) {
        refuse(Refusal::stealthMemory,"light-level gateway could not be allocated or sealed execute-read; fill light not stealth-neutral");
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(),gateway,128);
    stealth.player = reinterpret_cast<PlayerLevel>(gateway);
    stealth.list = reinterpret_cast<ListLevel>(gateway+64);
    stealth.term = reinterpret_cast<LightTerm>(base+lightTermRva);
    stealth.neutral = lights->config.stealthNeutral;
    const std::array<std::pair<std::uintptr_t,std::uintptr_t>,2> detours{{{playerLevelRva,reinterpret_cast<std::uintptr_t>(&hookPlayerLevel)},
        {listLevelRva,reinterpret_cast<std::uintptr_t>(&hookListLevel)}}};
    for (const auto& [target,hook] : detours) {
        const auto detour = thin::jump(hook);
        REL::safe_write(base+target,detour.data(),detour.size());
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(base+target),detour.size());
    }
    stealth.installed = true;
    return true;
}

// ---- fill light ----------------------------------------------------------------
inline bool createFill() {
    auto& l = *lights;
    const auto created = RE::NiPointLight::Create();
    if (!created || *reinterpret_cast<const std::uintptr_t*>(created) != REL::Module::get().base()+pointVtableRva) {
        l.fillReady = false;
        refuse(Refusal::fillCreate,"NiPointLight allocation or construction failed (or wrong class); fill light off");
        return false;
    }
    l.fill.reset(created);
    stealth.fill.store(created,std::memory_order_release); // before AddLight: the wrappers see it from its first sum
    auto& data = created->GetLightRuntimeData();
    data.ambient = {0.0f,0.0f,0.0f}; // as TESObjectLIGH lights (ID 17610 at 274666)
    data.diffuse = {l.config.fillRed,l.config.fillGreen,l.config.fillBlue};
    data.radius = {l.config.fillRadius,l.config.fillRadius,l.config.fillRadius};
    data.fade = l.config.fillFade;
    created->local = RE::NiTransform{};
    created->world = RE::NiTransform{};
    created->SetAppCulled(true);
    l.fillHidden = true;
    return true;
}
inline void place(Vec point) {
    auto& ours = *lights->fill;
    ours.local.translate = {point.x,point.y,point.z};
    ours.world.translate = ours.local.translate;
}
inline void show(bool visible,const char* cause) {
    auto& l = *lights;
    if (!l.fill || l.fillHidden == !visible) return;
    l.fill->SetAppCulled(!visible); // engine skips app-culled NiLights (106335 at 150DAA9, 107784 at 1569720)
    l.fillHidden = !visible;
    fillEvent(visible ? "show" : "hide",cause);
}
inline void addFill(RE::ShadowSceneNode* node) {
    RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
    params.dynamic = true;       // repositioned each frame from the NiLight (106335 at 150DAC5)
    params.shadowLight = false;  // never competes for the four shadow-mask channels
    params.portalStrict = false; // lights what is near it, whichever room it belongs to
    params.affectLand = true;
    params.affectWater = false;
    params.neverFades = false;   // +63 = 0: no camera-distance fade (see the static_assert note)
    params.fov = 0.0f; params.falloff = 1.0f; params.nearDistance = 5.0f; params.depthBias = 0.0f;
    params.sceneGraphIndex = 0; params.restrictedNode = nullptr; params.lensFlareData = nullptr;
    static_cast<void>(node->AddLight(lights->fill.get(),params));
    ++lights->adds;
}
// Main thread, every frame the spike bracket ends, eligible or not.
inline void serviceFill(RE::ShadowSceneNode* node,bool wanted,Vec feet,const char* cause) {
    auto& l = *lights;
    // Refused after creation (FCS-53): hide it; no further engine call.
    if (!l.fillReady) { show(false,"refused"); return; }
    if (wanted && !l.fill && !createFill()) return;
    if (!l.fill) return;
    if (wanted) { place(fillPoint(feet,l.config.fillHeight)); show(true,cause); }
    else show(false,cause);
    const bool changed = wanted != l.lastWanted;
    l.lastWanted = wanted;
    if (!node || (!changed && runtime->now < l.nextCheck)) return;
    // Idle only once nothing is owed: an owned light that went absent is still
    // checked until its removal is recorded after the delay.
    if (!wanted && !l.track.requested && !l.track.owned && l.presence == Presence::absent && !changed) return;
    l.nextCheck = runtime->now+.1;
    l.presence = presenceNow(node);
    switch (fillStep(wanted,l.presence,l.track,runtime->now)) {
    case FillAction::add: addFill(node); fillEvent("add",cause); break;
    case FillAction::remove: node->RemoveLight(l.fill.get()); ++l.removes; fillEvent("remove",cause); break;
    // Already out of every engine array (it went with its cell or a load): no
    // engine call; `removes` keeps counting our RemoveLight calls only.
    case FillAction::gone: fillEvent("remove",goneCause); break;
    case FillAction::dropLimit:
        fillEvent("drop-limit",cause);
        refuse(Refusal::fillDropped,"the engine dropped the fill light repeatedly in one cell; no further adds until the cell changes");
        break;
    case FillAction::removeLimit:
        fillEvent("remove-limit",cause);
        refuse(Refusal::fillStuck,"the fill light stayed active after repeated RemoveLight; it stays hidden: nothing on screen, and nothing to light levels while bLightFillStealthNeutral=1");
        break;
    case FillAction::none: break;
    }
}

// ---- entry points (fixed_spike.h) --------------------------------------------
// configure(), only when [FixedCameraSpike] bEnabled=1 and a light key is on.
inline void configureLight(const Config& requested) {
    if (!configured(requested)) return;
    lights = new LightRuntime;
    bool adjusted = false;
    lights->config = validate(requested,&adjusted);
    if (adjusted) refuse(Refusal::lightSettings,"one or more light keys were out of range; safe values applied");
}
// install(), kDataLoaded: every address this package uses is checked once.
inline void installLight(std::uintptr_t base) {
    if (!lights) return;
    auto& l = *lights;
    if (REL::ID(390951).address() != base+stateRva || REL::ID(254646).address() != base+sceneVtableRva) {
        refuse(Refusal::lightScene,"BSShaderManager::State (ID 390951) or ShadowSceneNode vtable (ID 254646) not at the audited RVA; light trace and fill light off");
        return;
    }
    l.ready = true;
    if (l.config.fillMode == 0) return;
    if (REL::ID(70967).address() != base+pointCtorRva || REL::ID(237259).address() != base+pointVtableRva ||
        REL::ID(106326).address() != base+addLightRva || REL::ID(106331).address() != base+removeLightRva ||
        !sameBytes(base+pointCtorRva,pointCtorPrologue) || !sameBytes(base+addLightRva,addLightPrologue) ||
        !sameBytes(base+removeLightRva,removeLightPrologue)) {
        refuse(Refusal::fillFunctions,"NiPointLight ctor / AddLight / RemoveLight differ from the audited 1.7.104 code; fill light off");
        return;
    }
    // Neutral by default: without the wrappers the fill light is refused, not added unneutralised.
    if (!installStealth(base) && l.config.stealthNeutral) return;
    l.fillReady = true;
}
// endFrame(), main thread, after the lifecycle requests are taken: a load
// request (kPreLoadGame, kPostLoadGame, TESLoadGameEvent) ends any hold.
inline void noteExits(unsigned requests) {
    if (lights && (requests & exitBit(Exit::load))) lights->loadPending = true;
}
// endFrame(), main thread, once per ineligible frame: is this frame's hook
// silence a menu's (fillDuty, SilenceHold)? A pausing menu is open, or the
// Tween camera state is still easing back after one.
inline bool silenceHeld() {
    if (!lights) return false;
    const auto ui = RE::UI::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const bool paused = ui && ui->numPausesGame != 0u;
    const bool tween = camera && camera->currentState && camera->currentState->id == RE::CameraState::kTween;
    return lights->silence.step(paused,tween,runtime->now);
}
// endFrame(), main thread, both eligible and ineligible frames. `at` is the
// census point (feet + eyeLift), `camera` the rendered camera position.
// `duty` (fillDuty): hold leaves the fill light exactly as it was; keep lights it
// at the player; off hides it. `loadExit`: a load restarts the drop budget.
inline void frameLight(bool eligible,FillDuty duty,bool loadExit,RE::TESObjectCELL* parent,Vec feet,Vec at,Vec camera,const char* cause) {
    if (!lights) return;
    auto& l = *lights;
    if (eligible) l.silence.reset();
    const RE::FormID cellId = parent ? parent->GetFormID() : 0u;
    const auto settled = settle(duty,cellId != l.cell,l.loadPending,cause);
    duty = settled.duty; cause = settled.cause;
    if (duty == FillDuty::hold) return;
    loadExit = loadExit || l.loadPending;
    l.loadPending = false;
    if (cellId != l.cell || loadExit) { l.cell = cellId; l.track.newCell(); }
    const bool interior = parent && parent->IsInteriorCell();
    if (eligible && !interior) cause = "exterior"; // unwanted only because it is outdoors
    const auto node = scene();
    const bool wanted = fillWanted(l.config.fillMode,duty == FillDuty::keep && finite(feet),interior,l.fillReady && node != nullptr);
    serviceFill(node,wanted,feet,cause);
    if (!l.config.trace || !node || !eligible || runtime->now < l.nextTrace) return;
    l.nextTrace = runtime->now+.1;
    LightSample s;
    s.seconds = runtime->now; s.frame = runtime->frame; s.cell = cellId; s.interior = interior; s.eligible = eligible;
    s.wanted = wanted; s.eye = at; s.camera = camera;
    const PlayerRooms player = playerRooms(parent,at);
    ShadowList shadows;
    census(node,player,at,s,shadows);
    s.fill.created = l.fill != nullptr; s.fill.hidden = l.fillHidden; s.fill.presence = l.presence;
    s.fill.at = l.fill ? Vec{l.fill->world.translate.x,l.fill->world.translate.y,l.fill->world.translate.z} : Vec{};
    s.fill.adds = l.adds; s.fill.removes = l.removes; s.fill.drops = l.track.drops;
    char line[768]{};
    if (formatLight(line,sizeof(line),s)) emitLight(line);
    if (runtime->now >= l.nextShadows) {
        l.nextShadows = runtime->now+1.0;
        if (formatShadows(line,sizeof(line),s.seconds,s.frame,shadows)) emitLight(line);
    }
    if (l.config.fillMode == 0 || runtime->now < l.nextLevel) return;
    l.nextLevel = runtime->now+1.0;
    LevelReading level;
    level.hooked = stealth.installed; level.neutral = stealth.installed && stealth.neutral;
    level.with = stealth.with.load(std::memory_order_relaxed); level.without = stealth.without.load(std::memory_order_relaxed);
    level.playerCalls = stealth.playerCalls.load(std::memory_order_relaxed);
    level.npcCalls = stealth.npcCalls.load(std::memory_order_relaxed); level.npcAdjusted = stealth.npcAdjusted.load(std::memory_order_relaxed);
    if (formatLightLevel(line,sizeof(line),s.seconds,s.frame,level)) emit(l.levelLines,line);
}
} // namespace skycrpg::camera::fixed::light
