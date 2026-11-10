// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike.h after its runtime helpers. P5.5-02 wave 2 package
// "light" (DYL-217). Nothing is allocated, read or installed unless
// [FixedCameraSpike] iLightFillMode=1 or iLightTrace=1. No hook: the light
// trace reads the main ShadowSceneNode under its own light lock, and the fill
// light is an ordinary dynamic point light handed to the engine's AddLight.
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
// CommonLib declarations this file reads or writes, pinned to the audited offsets.
static_assert(offsetof(RE::BSLight,lodDimmer) == 0x14);    // written with +5C by 108278 / 108618 / 108625
static_assert(offsetof(RE::BSLight,light) == 0x48);        // NiLight; SetLight (108289) at 1576AF2
static_assert(offsetof(RE::BSLight,frustrumCull) == 0x5c); // 0 visible / 0xFF culled
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
    double nextCheck{},nextTrace{};
    LineBudget lines{lightTraceLines};
};
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
inline LightView view(const RE::BSLight& entry) {
    const auto& source = *entry.light;
    const auto& at = source.world.translate;
    return {{at.x,at.y,at.z},source.GetLightRuntimeData().radius.x,entry.lodDimmer,entry.frustrumCull,source.GetAppCulled()};
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
inline void census(RE::ShadowSceneNode* node,Vec at,LightSample& s) {
    const RE::NiLight* mine = lights->fill.get();
    auto& data = node->GetRuntimeData();
    RE::BSSpinLockGuard guard(data.lightQueueLock);
    for (const auto& entry : data.activeLights) {
        if (!entry || !entry->light) continue;
        if (entry->light.get() == mine) {
            s.fill.found = true; s.fill.cull = entry->frustrumCull; s.fill.dimmer = entry->lodDimmer;
            continue;
        }
        s.census.addPoint(view(*entry),at);
    }
    for (const auto& entry : data.activeShadowLights) {
        if (!entry || !entry->light) continue;
        s.census.addShadow(view(*entry),entry->GetRuntimeData().maskIndex,at);
    }
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
    if (!l.fillReady) return;
    if (wanted && !l.fill && !createFill()) return;
    if (!l.fill) return;
    if (wanted) { place(fillPoint(feet,l.config.fillHeight)); show(true,cause); }
    else show(false,cause);
    const bool changed = wanted != l.lastWanted;
    l.lastWanted = wanted;
    if (!node || (!changed && runtime->now < l.nextCheck)) return;
    if (!wanted && !l.track.requested && l.presence == Presence::absent && !changed) return; // idle
    l.nextCheck = runtime->now+.1;
    l.presence = presenceNow(node);
    switch (fillStep(wanted,l.presence,l.track)) {
    case FillAction::add: addFill(node); fillEvent("add",cause); break;
    case FillAction::remove: node->RemoveLight(l.fill.get()); ++l.removes; fillEvent("remove",cause); break;
    case FillAction::dropLimit:
        fillEvent("drop-limit",cause);
        refuse(Refusal::fillDropped,"the engine dropped the fill light repeatedly in one cell; no further adds until the cell changes");
        break;
    case FillAction::removeLimit:
        fillEvent("remove-limit",cause);
        refuse(Refusal::fillStuck,"the fill light stayed active after repeated RemoveLight; it stays hidden (contributes nothing)");
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
    l.fillReady = true;
}
// endFrame(), main thread, both eligible and ineligible frames. `at` is the
// census point (feet + eyeLift), `camera` the rendered camera position.
inline void frameLight(bool eligible,RE::TESObjectCELL* parent,Vec feet,Vec at,Vec camera,const char* cause) {
    if (!lights) return;
    auto& l = *lights;
    const RE::FormID cellId = parent ? parent->GetFormID() : 0u;
    if (cellId != l.cell) { l.cell = cellId; l.track.newCell(); }
    const bool interior = parent && parent->IsInteriorCell();
    const auto node = scene();
    const bool wanted = fillWanted(l.config.fillMode,eligible && finite(feet),interior,l.fillReady && node != nullptr);
    serviceFill(node,wanted,feet,cause);
    if (!l.config.trace || !node || !eligible || runtime->now < l.nextTrace) return;
    l.nextTrace = runtime->now+.1;
    LightSample s;
    s.seconds = runtime->now; s.frame = runtime->frame; s.cell = cellId; s.interior = interior; s.eligible = eligible;
    s.wanted = wanted; s.eye = at; s.camera = camera;
    census(node,at,s);
    s.fill.created = l.fill != nullptr; s.fill.hidden = l.fillHidden; s.fill.presence = l.presence;
    s.fill.at = l.fill ? Vec{l.fill->world.translate.x,l.fill->world.translate.y,l.fill->world.translate.z} : Vec{};
    s.fill.adds = l.adds; s.fill.removes = l.removes; s.fill.drops = l.track.drops;
    char line[768]{};
    if (formatLight(line,sizeof(line),s)) emitLight(line);
}
} // namespace skycrpg::camera::fixed::light
