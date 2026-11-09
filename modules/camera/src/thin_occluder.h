// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by plugin.cpp: keeps the optional adapter in the existing Camera target.
#include "thin_occluder_policy.h"
#include <RE/H/hkpCdPoint.h>
#include <RE/H/hkpLinearCastInput.h>
#include <intrin.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>

namespace skycrpg::camera::thin {
using Caster = bool(*)(void*,RE::bhkWorld*,float*,float*,RE::hkpRootCdPoint*,RE::TESObjectREFR**,float);
using CameraUpdate = void(*)(RE::PlayerCamera*);
using Material = RE::BSLightingShaderMaterialBase;
struct Part {
    RE::NiPointer<RE::BSGeometry> geometry;
    RE::BSShaderProperty* shader{}; // Held by geometry; no extra shader owner.
    RE::NiPointer<RE::NiAlphaProperty> originalProperty,privateProperty;
    Material* original{};
    Material* clone{};
    float originalAlpha{1},writtenAlpha{1};
    std::uint16_t flags{};
    bool active{},conflict{};
    std::uint64_t retiredFrame{};
};
struct Reference {
    RE::ObjectRefHandle handle;
    RE::NiPointer<RE::NiAVObject> root;
    std::array<Part*,maxParts> parts{};
    unsigned count{};
    FadeState fade;
    bool used{},prepared{};
    double lastContact{-1},lastNarrow{-1};
};
struct Runtime {
    std::atomic_bool enabled{};
    bool installed{},resolved{},refused{},trace{},allowed{},frameKept{},previousKept{},shaderCountLogged{};
    bool smoothCastSeen{},missingCastLogged{};
    double castWaitStart{};
    unsigned prepareReasons{},worldGuards{};
    Exit ineligibleCause{Exit::special};
    bool (*eligibleNow)(){};
    RE::NiPoint3 (*goalNow)(){};
    RE::NiPoint3 (*focusNow)(){};
    void (*logger)(const char*){};
    std::atomic_uint requests{};
    std::atomic_bool loaded{};
    std::array<std::string,maxForms> configured;
    std::array<std::uint32_t,maxForms> forms{};
    unsigned formCount{3},poolSize{},traceLines{};
    float targetAlpha{.35f};
    std::array<Part,maxPool> pool;
    std::array<Reference,maxReferences> refs;
    std::array<RE::ObjectRefHandle,maxReferences> carried;
    unsigned carriedCount{};
    Budget budget;
    std::array<std::uint64_t,10> totals{};
    std::array<std::uint64_t,11> restores{};
    HeightWindow heights;
    double hookSeconds{},nativeSeconds{},tickSeconds{},maxHookSeconds{},maxFrameSeconds{};
    std::uint64_t framesOverBudget{},samples{},resumes{};
    Point castOrigin{};
    bool haveOrigin{};
    double now{},last{},nextTrace{},nextScan{};
    std::uint64_t frame{};
    RE::FormID cell{},keptReference{},gapReference{};
    unsigned refusalReason{};
    Caster caster{};
    CameraUpdate update{};
    HMODULE smooth{};
};
// Retain the bounded private allocations through process exit: renderer consumers
// may outlive swap-back. Recycle idle slots only for the exact same source
// material/blend flags after later updates; never change their texture/layout
// data or free a clone. No scene destructor runs under loader lock.
inline Runtime* runtime = nullptr;
inline bool guardResult(const char* name,bool passed);
inline void prepareRefusal(unsigned reason,const char* name);
inline void restoreAll(Exit cause);
inline bool enabled() { return runtime && runtime->enabled; }
inline bool onMain() {
    const auto main = RE::Main::GetSingleton();
    return main && main->threadID == GetCurrentThreadId();
}
inline double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline void refuse(const char* reason) {
    if (!runtime->refused) { runtime->logger(reason); runtime->refused = true; }
}
inline void refuseInstall(const char* reason) {
    if (!enabled()) return;
    refuse(reason); runtime->enabled = false;
}
inline void requestRestore(Exit cause = Exit::special) {
    if (enabled()) runtime->requests.fetch_or(1u << static_cast<unsigned>(cause),std::memory_order_release);
}
inline void setLoaded(bool value) {
    if (enabled()) {
        runtime->loaded.store(value,std::memory_order_release); requestRestore(Exit::load);
        if (!value && onMain()) restoreAll(Exit::load);
    }
}
inline void attach(RE::BSShaderProperty* shader,RE::BSShaderMaterial* material) {
    using Function = void(*)(RE::BSShaderProperty*,RE::BSShaderMaterial*);
    const REL::Relocation<Function> function{REL::ID(105545)};
    function(shader,material);
}
inline RestoreCheck check(const Part& part) {
    const auto& data = part.geometry->GetGeometryRuntimeData();
    if (data.shaderProperty.get() != part.shader) return {}; // Old shader may already be destroyed.
    return {true,part.shader->material == part.clone,
        data.alphaProperty == part.privateProperty,part.clone && part.clone->materialAlpha == part.writtenAlpha,
        part.privateProperty && part.privateProperty->alphaFlags == part.flags && part.privateProperty->alphaThreshold == 0};
}
inline bool fadeReady(const Reference* ref) {
    if (!ref || !ref->prepared || !ref->fade.admitted || !(ref->fade.alpha < .999f)) return false;
    for (unsigned i = 0;i < ref->count;++i) {
        const auto& part = *ref->parts[i];
        const auto state = check(part);
        if (!part.active || !state.material() || !state.property() || part.shader->GetRefCount() != 1) return false;
    }
    return true;
}
inline void restore(Reference& ref,Exit cause = Exit::unload) {
    ++runtime->restores[static_cast<unsigned>(cause)];
    for (unsigned i = 0;i < ref.count;++i) {
        auto& part = *ref.parts[i];
        if (part.active) {
            const auto state = check(part);
            if (state.material()) attach(part.shader,part.original);
            else part.conflict = true;
            if (state.property()) part.geometry->GetGeometryRuntimeData().alphaProperty = part.originalProperty;
            else part.conflict = true;
            part.active = false;
            if (part.conflict) refuse("ThinOccluder: restore conflict; foreign state preserved, affected geometry refused.");
        }
        part.retiredFrame = runtime->frame;
        part.geometry.reset(); part.shader = nullptr; part.originalProperty.reset();
    }
    ref = {};
}
inline void restoreAll(Exit cause = Exit::special) {
    if (!keepsCarried(cause)) runtime->carriedCount = 0;
    for (auto& ref : runtime->refs) if (ref.used) {
        if (carriesAdmission(cause,ref.prepared && ref.fade.admitted && ref.fade.alpha < .999f) &&
            runtime->carriedCount < maxReferences) runtime->carried[runtime->carriedCount++] = ref.handle;
        restore(ref,cause);
    }
    runtime->haveOrigin = false; runtime->nextScan = 0; runtime->castWaitStart = 0;
    runtime->allowed = false;
}
inline bool listed(RE::TESObjectREFR* ref) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto base = ref ? ref->GetBaseObject() : nullptr;
    return player && ref && ref != player && base && base->Is(RE::FormType::Static) &&
        matches(base->GetFormID(),runtime->forms) && ref->GetParentCell() == player->GetParentCell() && ref->Get3D();
}
inline Reference* remember(RE::TESObjectREFR* ref,bool contact = true) {
    if (!listed(ref)) return nullptr;
    const auto handle = ref->GetHandle();
    Reference* empty = nullptr;
    for (auto& entry : runtime->refs) {
        if (entry.used && entry.handle == handle && entry.root.get() == ref->Get3D()) {
            entry.fade.touch(runtime->now);
            if (contact) entry.lastContact = runtime->now;
            return &entry;
        }
        if (!entry.used && !empty) empty = &entry;
    }
    if (!empty) { prepareRefusal(7,"reference-cap"); ++runtime->budget.cap; return nullptr; }
    empty->used = true; empty->handle = handle; empty->root.reset(ref->Get3D());
    empty->fade.touch(runtime->now);
    if (contact) empty->lastContact = runtime->now;
    return empty;
}
inline Part* cached(RE::BSGeometry* geometry) {
    for (unsigned i = 0;i < runtime->poolSize;++i)
        if (runtime->pool[i].geometry.get() == geometry) return &runtime->pool[i];
    return nullptr;
}
inline Part* reusable(RE::BSGeometry* geometry,const Reference& ref) {
    const auto shader = geometry->lightingShaderProp_cast();
    const auto property = geometry->GetGeometryRuntimeData().alphaProperty;
    const auto flags = translucentFlags(property ? property->alphaFlags : std::uint16_t{0});
    for (unsigned i = 0;i < runtime->poolSize;++i) {
        auto& part = runtime->pool[i];
        if (!part.clone || !recycle(part.active,part.conflict,part.original == shader->material,part.flags == flags,
            part.originalAlpha == part.original->materialAlpha,
            std::find(ref.parts.begin(),ref.parts.end(),&part) != ref.parts.end(),part.retiredFrame,runtime->frame)) continue;
        return &part;
    }
    return nullptr;
}
inline void prepareRefusal(unsigned reason,const char* name) {
    if (runtime->prepareReasons & (1u << reason)) return;
    runtime->prepareReasons |= 1u << reason;
    char line[128]{};
    std::snprintf(line,sizeof(line),"ThinOccluder prepare-refused reason=%s; native collision retained",name);
    runtime->logger(line);
}
inline bool prepare(Reference& ref) {
    std::array<RE::BSGeometry*,maxParts> geometries{};
    unsigned count{},visited{};
    bool valid = true;
    ++runtime->budget.scans;
    RE::BSVisit::TraverseScenegraphObjects(ref.root.get(),[&](RE::NiAVObject* object) {
        if (++visited > 256) { prepareRefusal(3,"node-cap"); valid = false; return RE::BSVisit::BSVisitControl::kStop; }
        const auto geometry = object->AsGeometry();
        if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
        const auto shader = geometry->lightingShaderProp_cast();
        if (shader && !runtime->shaderCountLogged) {
            char line[128]{};
            std::snprintf(line,sizeof(line),"ThinOccluder shaderRefCount=%u expected=1 ownership=geometry-only",
                shader->GetRefCount());
            runtime->logger(line); runtime->shaderCountLogged = true;
        }
        // Additional shader owners cannot be proved private by a cell scan.
        // Refuse rather than write through a potentially shared shader.
        if (count == maxParts) { prepareRefusal(0,"part-cap"); valid = false; }
        else if (!shader || !shader->material || shader->material->GetType() != RE::BSShaderMaterial::Type::kLighting) {
            prepareRefusal(1,"unsupported-shader"); valid = false;
        } else if (shader->GetRefCount() != 1 || cached(geometry)) {
            prepareRefusal(2,"shared-or-owned-shader"); valid = false;
        }
        if (!valid) return RE::BSVisit::BSVisitControl::kStop;
        geometries[count++] = geometry;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    if (!valid || !count) { if (!count) prepareRefusal(4,"empty"); return false; }
    for (unsigned i = 0;i < count;++i) {
        const auto geometry = geometries[i];
        auto part = cached(geometry);
        if (!part) {
            part = reusable(geometry,ref);
            if (part) {
                part->geometry.reset(geometry);
                part->shader = geometry->lightingShaderProp_cast();
                part->originalProperty = geometry->GetGeometryRuntimeData().alphaProperty;
                part->retiredFrame = 0;
            }
        }
        if (part) ref.parts[ref.count++] = part;
        if (!part) {
            if (runtime->poolSize == maxPool) { prepareRefusal(5,"pool-cap"); return false; }
            part = &runtime->pool[runtime->poolSize++];
            ref.parts[ref.count++] = part;
            part->geometry.reset(geometry); part->shader = geometry->lightingShaderProp_cast();
            part->original = static_cast<Material*>(part->shader->material);
            static_cast<void>(part->original->IncRef());
            part->originalAlpha = part->original->materialAlpha;
            part->originalProperty = geometry->GetGeometryRuntimeData().alphaProperty;
            part->flags = translucentFlags(part->originalProperty ? part->originalProperty->alphaFlags : std::uint16_t{0});
            using Factory = RE::NiAlphaProperty*(*)();
            const REL::Relocation<Factory> factory{REL::ID(70687)};
            part->privateProperty.reset(factory());
            if (!part->privateProperty || !std::isfinite(part->originalAlpha) || part->originalAlpha <= 0 || part->originalAlpha > 1) {
                part->conflict = true; prepareRefusal(6,"clone-or-property"); return false;
            }
            part->privateProperty->alphaFlags = part->flags;
            part->privateProperty->alphaThreshold = 0;
            part->shader->SetMaterial(part->original,true);
            const auto material = part->shader->material;
            if (!material || material == part->original || material->GetType() != RE::BSShaderMaterial::Type::kLighting) {
                attach(part->shader,part->original); part->conflict = true; prepareRefusal(6,"clone-or-property"); return false;
            }
            part->clone = static_cast<Material*>(material);
            static_cast<void>(part->clone->IncRef());
            // Keep all clones alive, including this temporary install/swap-back.
            attach(part->shader,part->original);
        }
    }
    for (unsigned i = 0;i < ref.count;++i) {
        auto& part = *ref.parts[i];
        part.writtenAlpha = part.originalAlpha;
        part.clone->materialAlpha = part.writtenAlpha;
        ++runtime->budget.fades;
        attach(part.shader,part.clone);
        part.geometry->GetGeometryRuntimeData().alphaProperty = part.privateProperty;
        part.active = true;
    }
    ref.fade.alpha = 1;
    ref.fade.admitted = false;
    ref.fade.first = runtime->now;
    ref.prepared = true;
    return true;
}
inline void resolveForms() {
    const auto data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    for (unsigned i = 0;i < runtime->formCount;++i) {
        const auto entry = parseForm(runtime->configured[i]);
        const auto base = entry ? data->LookupForm<RE::TESObjectSTAT>(entry->local,entry->plugin) : nullptr;
        if (!base) {
            refuse("ThinOccluder: missing/non-static configured base form; native collision retained.");
            runtime->enabled = false; return;
        }
        runtime->forms[i] = base->GetFormID();
    }
    runtime->resolved = true;
}
inline bool safeNow() {
    return runtime->enabled && runtime->loaded.load(std::memory_order_acquire) && runtime->resolved &&
        runtime->requests.load(std::memory_order_acquire) == 0 && runtime->allowed;
}
inline void scanSightline() {
    if (runtime->now < runtime->nextScan) return;
    runtime->nextScan = runtime->now+.1;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!player || !player->GetParentCell() || !camera || !camera->cameraRoot) return;
    const auto focus = runtime->focusNow();
    const Point from = runtime->haveOrigin ? runtime->castOrigin : Point{focus.x,focus.y,focus.z};
    const auto goal = runtime->goalNow();
    const auto eye = camera->cameraRoot->world.translate;
    unsigned visited{};
    // Bounds only locate ALREADY listed forms near the sightline. They never
    // classify a form as thin or authorize discarding any collision.
    player->GetParentCell()->ForEachReference([&](RE::TESObjectREFR* ref) {
        if (++visited > 1024) return RE::BSContainer::ForEachResult::kStop;
        if (listed(ref)) {
            const auto& bound = ref->Get3D()->worldBound;
            const Point center{bound.center.x,bound.center.y,bound.center.z};
            if (intersects({from.x,from.y,from.z},{goal.x,goal.y,goal.z},center,bound.radius+30) ||
                intersects({from.x,from.y,from.z},{eye.x,eye.y,eye.z},center,bound.radius+30))
                static_cast<void>(remember(ref,false));
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    ++runtime->budget.scans;
}
inline bool narrowSightline(RE::TESObjectREFR* ref,Point from,Point to) {
    const auto base = ref->GetBaseObject();
    const auto& transform = ref->Get3D()->world;
    if (!std::isfinite(transform.scale) || transform.scale <= .001f) return false;
    const auto local = [&](Point p) {
        const RE::NiPoint3 delta{p.x-transform.translate.x,p.y-transform.translate.y,p.z-transform.translate.z};
        const auto& m = transform.rotate.entry;
        return Point{(m[0][0]*delta.x+m[1][0]*delta.y+m[2][0]*delta.z)/transform.scale,
            (m[0][1]*delta.x+m[1][1]*delta.y+m[2][1]*delta.z)/transform.scale,
            (m[0][2]*delta.x+m[1][2]*delta.y+m[2][2]*delta.z)/transform.scale};
    };
    const auto& bounds = base->boundData;
    return intersectsBox(local(from),local(to),
        {static_cast<float>(bounds.boundMin.x),static_cast<float>(bounds.boundMin.y),static_cast<float>(bounds.boundMin.z)},
        {static_cast<float>(bounds.boundMax.x),static_cast<float>(bounds.boundMax.y),static_cast<float>(bounds.boundMax.z)});
}
inline void traceFrame(double dt) {
    const auto& b = runtime->budget;
    const std::array counts{b.casts,b.gaps,b.fades,b.scans,b.discarded,b.kept,b.refused,b.unknown,b.overlap,b.cap};
    for (unsigned i = 0;i < counts.size();++i) runtime->totals[i] += counts[i];
    if (!runtime->loaded.load(std::memory_order_acquire)) return;
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!camera || !camera->cameraRoot || !player || !player->GetParentCell()) return;
    runtime->heights.observe(camera->cameraRoot->world.translate.z);
    runtime->maxFrameSeconds = (std::max)(runtime->maxFrameSeconds,dt);
    if (dt > 1.0/60.0) ++runtime->framesOverBudget;
    ++runtime->samples;
    if (runtime->traceLines >= maxTraceLines || runtime->now < runtime->nextTrace) return;
    const auto pos = player->GetPosition();
    const auto goal = runtime->allowed ? runtime->goalNow() : RE::NiPoint3{};
    unsigned active{},parts{},fadedRefs{},fadedParts{}; float alpha = 1;
    for (const auto& ref : runtime->refs) if (ref.used && ref.prepared) {
        ++active; parts += ref.count;
        for (unsigned i = 0;i < ref.count;++i) alpha = (std::min)(alpha,ref.parts[i]->writtenAlpha);
        if (ref.fade.alpha < .999f) { ++fadedRefs; fadedParts += ref.count; }
    }
    char line[1600]{};
    const auto& h = runtime->heights; const auto& t = runtime->totals; const auto& r = runtime->restores;
    std::snprintf(line,sizeof(line),
        "ThinOccluder frame=%llu seconds=%.3f engaged=%u z=%.3f minZ=%.3f maxZ=%.3f maxDeltaZ=%.3f goalZ=%.3f playerX=%.3f playerY=%.3f alpha=%.3f refs=%u parts=%u fadedRefs=%u fadedParts=%u casts=%llu gaps=%llu fades=%llu scans=%llu discarded=%llu kept=%llu refused=%llu unknown=%llu overlap=%llu cap=%llu pool=%u keptRef=%08X gapRef=%08X reason=%u restoresDisable=%llu restoresF8=%llu restoresMenu=%llu restoresPOV=%llu restoresSpecial=%llu restoresLoad=%llu restoresCell=%llu restoresUnload=%llu restoresConflict=%llu restoresExpired=%llu hookUs=%.3f nativeUs=%.3f adapterUs=%.3f maxHookUs=%.3f tickUs=%.3f maxFrameMs=%.3f framesOverBudget=%llu frames=%llu resumes=%llu sample=cumulative",
        static_cast<unsigned long long>(runtime->frame),runtime->now,static_cast<unsigned>(runtime->allowed),
        h.previous,h.minimum,h.maximum,h.maxDelta,goal.z,pos.x,pos.y,alpha,active,parts,fadedRefs,fadedParts,
        static_cast<unsigned long long>(t[0]),static_cast<unsigned long long>(t[1]),static_cast<unsigned long long>(t[2]),
        static_cast<unsigned long long>(t[3]),static_cast<unsigned long long>(t[4]),static_cast<unsigned long long>(t[5]),
        static_cast<unsigned long long>(t[6]),static_cast<unsigned long long>(t[7]),static_cast<unsigned long long>(t[8]),
        static_cast<unsigned long long>(t[9]),runtime->poolSize,runtime->keptReference,runtime->gapReference,runtime->refusalReason,
        static_cast<unsigned long long>(r[0]),static_cast<unsigned long long>(r[1]),static_cast<unsigned long long>(r[2]),
        static_cast<unsigned long long>(r[3]),static_cast<unsigned long long>(r[4]),static_cast<unsigned long long>(r[5]),
        static_cast<unsigned long long>(r[6]),static_cast<unsigned long long>(r[7]),static_cast<unsigned long long>(r[9]),
        static_cast<unsigned long long>(r[10]),
        runtime->hookSeconds*1e6,runtime->nativeSeconds*1e6,(runtime->hookSeconds-runtime->nativeSeconds)*1e6,runtime->maxHookSeconds*1e6,runtime->tickSeconds*1e6,runtime->maxFrameSeconds*1e3,
        static_cast<unsigned long long>(runtime->framesOverBudget),static_cast<unsigned long long>(runtime->samples),
        static_cast<unsigned long long>(runtime->resumes));
    runtime->logger(line); ++runtime->traceLines;
    runtime->heights.sampled(); runtime->nextTrace = runtime->now+.1;
}
struct Cost {
    bool tick{};
    double start{};
    explicit Cost(bool isTick = false) : tick(isTick), start(runtime->trace ? seconds() : 0) {}
    ~Cost() {
        if (!start) return;
        const double elapsed = seconds()-start;
        if (tick) runtime->tickSeconds += elapsed;
        else { runtime->hookSeconds += elapsed; runtime->maxHookSeconds = (std::max)(runtime->maxHookSeconds,elapsed); }
    }
};
inline void tick() {
    if (!onMain()) return;
    Cost cost(true);
    runtime->now = seconds();
    const double dt = runtime->last ? runtime->now-runtime->last : 0;
    runtime->last = runtime->now;
    ++runtime->frame;
    runtime->previousKept = runtime->frameKept; runtime->frameKept = false;
    if (runtime->trace) traceFrame(dt);
    runtime->budget = {};
    runtime->keptReference = runtime->gapReference = runtime->refusalReason = 0;
    const unsigned requests = runtime->requests.exchange(0,std::memory_order_acq_rel);
    if (requests) {
        for (unsigned i = 0;i < runtime->restores.size();++i)
            if (requests & (1u << i)) { restoreAll(static_cast<Exit>(i)); break; }
    }
    if (!runtime->enabled || !runtime->loaded.load(std::memory_order_acquire)) { restoreAll(Exit::load); return; }
    if (!runtime->resolved) resolveForms();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto cell = player && player->GetParentCell() ? player->GetParentCell()->GetFormID() : 0;
    if (cell != runtime->cell) { restoreAll(Exit::cell); runtime->cell = cell; }
    // Exactly one eligibility/context computation per tick, never inside a cast.
    runtime->allowed = runtime->enabled && runtime->resolved && runtime->eligibleNow();
    if (!runtime->allowed) { restoreAll(runtime->ineligibleCause); return; }
    if (runtime->installed && !runtime->smoothCastSeen && !runtime->missingCastLogged) {
        if (!runtime->castWaitStart) runtime->castWaitStart = runtime->now;
        else if (runtime->now-runtime->castWaitStart >= 2) {
            runtime->logger("ThinOccluder: no SmoothCam cast observed after 2 eligible seconds; caller filter may not match; native compression measurement retained.");
            runtime->missingCastLogged = true;
        }
    }
    // Before this frame's casts, or the kept beam hit pulls the camera in on menu close.
    for (unsigned i = 0;i < runtime->carriedCount;++i) {
        const auto live = runtime->carried[i].get();
        const auto ref = live ? remember(live.get(),false) : nullptr;
        if (!ref) continue;
        if (!ref->prepared && !prepare(*ref)) { restore(*ref,Exit::conflict); ++runtime->budget.refused; continue; }
        ref->fade.resume(runtime->now);
        ++runtime->resumes;
    }
    runtime->carriedCount = 0;
    scanSightline();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->cameraRoot) { restoreAll(); return; }
    const auto focus = runtime->focusNow();
    const Point from = runtime->haveOrigin ? runtime->castOrigin : Point{focus.x,focus.y,focus.z};
    const auto eye = camera->cameraRoot->world.translate;
    // Hit references first; at most eight preparations/tick (128 parts), no
    // global 100 ms throttle. Broadly prepared references stay opaque.
    for (unsigned priority = 0;priority < 2;++priority) for (auto& ref : runtime->refs) {
        if (!ref.used || (ref.lastContact >= 0 && runtime->now-ref.lastContact <= .4) != (priority == 0)) continue;
        const auto live = ref.handle.get();
        if (!live || !listed(live.get()) || live->Get3D() != ref.root.get()) { restore(ref); continue; }
        if (!ref.prepared && !prepare(ref)) { restore(ref,Exit::conflict); ++runtime->budget.refused; continue; }
        if (narrowSightline(live.get(),from,{eye.x,eye.y,eye.z})) ref.lastNarrow = runtime->now;
        const bool visible = (ref.lastContact >= 0 && runtime->now-ref.lastContact <= .4) ||
            (ref.lastNarrow >= 0 && runtime->now-ref.lastNarrow <= .4);
        if (!ref.fade.step(runtime->now,dt,runtime->targetAlpha,true,!visible)) { restore(ref,Exit::expired); continue; }
        bool valid = true;
        for (unsigned i = 0;i < ref.count;++i) {
            const auto& part = *ref.parts[i]; const auto state = check(part);
            if (!state.material() || !state.property() || part.shader->GetRefCount() != 1) { valid = false; break; }
        }
        if (!valid) { restore(ref,Exit::conflict); ++runtime->budget.refused; continue; }
        for (unsigned i = 0;i < ref.count;++i) {
            auto& part = *ref.parts[i]; part.writtenAlpha = part.originalAlpha*ref.fade.alpha;
            part.clone->materialAlpha = part.writtenAlpha; ++runtime->budget.fades;
        }
    }
}
inline void updateCamera(RE::PlayerCamera* camera) {
    if (onMain()) tick();
    runtime->update(camera); // Chain the previous CALL target, including TDM's relay.
    if (onMain() && runtime->allowed && !safeNow()) restoreAll();
}

struct Collector;
struct CollectorTable {
    void* (*destroy)(Collector*,unsigned);
    void (*add)(Collector*,const RE::hkpCdPoint&);
    void (*reset)(Collector*);
};
struct Collector {
    const CollectorTable* vtable{};
    float earlyOut{(std::numeric_limits<float>::max)()};
    std::uint32_t padding{};
    std::array<RE::hkpRootCdPoint,maxContacts> contacts{};
    unsigned count{};
    bool invalid{};
    std::array<std::byte,11> tailPadding{};
    static void* destroy(Collector* self,unsigned) { self->invalid = true; return self; }
    static const RE::hkpCollidable* root(const RE::hkpCdBody* body) {
        unsigned depth{};
        while (body && body->parent && depth < 32) { body = body->parent; ++depth; }
        return body && !body->parent ? static_cast<const RE::hkpCollidable*>(body) : nullptr;
    }
    static void add(Collector* self,const RE::hkpCdPoint& point) {
        if (self->count == maxContacts) { self->invalid = true; self->earlyOut = -1; return; }
        std::array<float,8> values{};
        std::memcpy(values.data(),&point.contact,sizeof(point.contact));
        if (std::any_of(values.begin(),values.end(),[](float value) { return !std::isfinite(value); })) {
            self->invalid = true; return;
        }
        const auto a = root(point.cdBodyA), b = root(point.cdBodyB);
        if (!a || !b || !point.cdBodyA || !point.cdBodyB) { self->invalid = true; return; }
        auto& hit = self->contacts[self->count++];
        hit.contact = point.contact;
        hit.rootCollidableA = a; hit.rootCollidableB = b;
        hit.shapeKeyA = point.cdBodyA->shapeKey; hit.shapeKeyB = point.cdBodyB->shapeKey;
    }
    static void reset(Collector* self) { self->invalid = true; }
};
static_assert(offsetof(Collector,earlyOut) == 8);
static_assert(offsetof(Collector,contacts) == 16);
static_assert(offsetof(RE::hkpCdPoint,cdBodyA) == 0x30);
static_assert(offsetof(RE::hkpCdPoint,cdBodyB) == 0x38);
static_assert(offsetof(RE::BSShaderProperty,material) == 0x78);
static_assert(offsetof(Material,materialAlpha) == 0x80);
static_assert(offsetof(RE::NiAlphaProperty,alphaFlags) == 0x30);
static_assert(offsetof(RE::NiAVObject,worldBound) == 0xE4);
static_assert(offsetof(RE::NiAVObject,world) == 0x7C);
static_assert(offsetof(RE::TESBoundObject,boundData) == 0x20);
static_assert(offsetof(RE::BSGeometry::GEOMETRY_RUNTIME_DATA,shaderProperty) == 8);
static_assert(offsetof(RE::TESObjectCELL::RUNTIME_DATA,references) == 0x20);
static_assert(offsetof(RE::TESObjectCELL::RUNTIME_DATA,spinLock) == 0xB8);
static_assert(sizeof(RE::hkpRootCdPoint) == 0x40);
inline const CollectorTable collectorTable{Collector::destroy,Collector::add,Collector::reset};
inline Point position(const RE::hkVector4& value) {
    std::array<float,4> floats{}; std::memcpy(floats.data(),&value,sizeof(value));
    return {floats[0],floats[1],floats[2]};
}
inline float fraction(const RE::hkpRootCdPoint& hit) {
    std::array<float,4> values{};
    std::memcpy(values.data(),&hit.contact.separatingNormal,16); return values[3];
}
struct Gap {
    RE::hkpRootCdPoint hit{};
    GapResult result{GapResult::refuse};
    std::array<std::byte,12> padding{};
};
inline Gap gap(void* context,RE::bhkWorld* world,Point next,const RE::hkpCollidable* original) {
    if (!runtime->budget.gap()) { ++runtime->budget.cap; return {}; }
    if (!context || !world) { ++runtime->budget.unknown; return {}; }
    void* wrapper{}; RE::hkpWorldObject* phantom{};
    std::memcpy(&wrapper,context,sizeof(wrapper));
    if (wrapper) std::memcpy(&phantom,static_cast<const std::byte*>(wrapper)+0x10,sizeof(phantom));
    // This query phantom has no world. Native caster gets the world from GetWorld1.
    const auto worldSlots = *reinterpret_cast<const std::uintptr_t* const*>(world);
    const bool worldGuard = worldSlots[0x27] == REL::Module::get().base()+0x104b470;
    const unsigned worldBit = worldGuard ? 1u : 2u;
    if (!(runtime->worldGuards & worldBit)) { guardResult("GetWorld1",worldGuard); runtime->worldGuards |= worldBit; }
    if (!worldGuard) { ++runtime->budget.unknown; return {}; }
    const auto havok = world->GetWorld1();
    if (!phantom || !havok) { ++runtime->budget.unknown; return {}; }
    Collector cast,overlap; cast.vtable = overlap.vtable = &collectorTable;
    RE::hkpLinearCastInput input{};
    const float scale = RE::bhkWorld::GetWorldScale();
    input.to = {next.x*scale,next.y*scale,next.z*scale,0};
    input.maxExtraPenetration = input.startPointTolerance = 0x1p-23f;
    using Sweep = void(*)(const RE::hkpWorld*,const RE::hkpCollidable*,const RE::hkpLinearCastInput&,Collector&,Collector*);
    const REL::Relocation<Sweep> sweep{REL::ID(61402)};
    GapDecision decision;
    Gap result;
    RE::BSReadLockGuard lock(world->worldLock);
    sweep(havok,&phantom->collidable,input,cast,&overlap);
    decision.complete = !cast.invalid && !overlap.invalid;
    for (const auto collector : {&cast,&overlap}) {
        for (unsigned i = 0;i < collector->count;++i) {
            const auto& hit = collector->contacts[i];
            const auto collision = hit.rootCollidableB;
            decision.originalSeen = decision.originalSeen || collision == original;
            const auto ref = RE::TESHavokUtilities::FindCollidableRef(*collision);
            const auto object = RE::TESHavokUtilities::FindCollidableObject(*collision);
            const auto layer = static_cast<unsigned>(collision->GetCollisionLayer());
            if (nativeIgnored(layer,!object || object->GetAppCulled(),ref == RE::PlayerCharacter::GetSingleton())) continue;
            // 55784D excludes player; 5578C7..557931 records a Character on
            // these branches then skips its contact except on layer 32.
            if (skipCharacter(layer,ref && ref->Is(RE::FormType::ActorCharacter),ref == RE::PlayerCharacter::GetSingleton())) continue;
            const bool match = listed(ref);
            if (match) {
                const auto fade = remember(ref);
                if (!fadeReady(fade)) { decision.unknown = true; continue; }
            }
            // Native keeps ordinary doors/furniture/clutter too. Layer 17 can
            // legitimately lack scene metadata. All other unresolved roots refuse.
            const bool known = (object && ref && ref->GetBaseObject()) || layer == 17;
            decision.contact(known,match,collector == &overlap,fraction(hit),i);
        }
    }
    result.result = decision.result();
    if (!decision.complete) ++runtime->budget.cap;
    if (decision.unknown || !decision.originalSeen) ++runtime->budget.unknown;
    if (decision.overlap) ++runtime->budget.overlap;
    if (result.result == GapResult::obstruction) {
        result.hit = cast.contacts[decision.index];
        const auto ref = RE::TESHavokUtilities::FindCollidableRef(*result.hit.rootCollidableB);
        runtime->gapReference = ref ? ref->GetFormID() : 0;
    }
    return result;
}
inline RE::TESObjectREFR* hitReference(RE::bhkWorld* world,const RE::hkpRootCdPoint& hit) {
    if (!hit.rootCollidableB) return nullptr;
    RE::BSReadLockGuard lock(world->worldLock);
    return RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB);
}
inline bool measuredCast(void* physics,RE::bhkWorld* world,float* start,float* end,
    RE::hkpRootCdPoint* result,RE::TESObjectREFR** character,float hull) {
    const double begin = runtime->trace ? seconds() : 0;
    const bool hit = runtime->caster(physics,world,start,end,result,character,hull);
    if (begin) runtime->nativeSeconds += seconds()-begin;
    return hit;
}
inline bool hookCaster(void* physics,RE::bhkWorld* world,float* start,float* end,
    RE::hkpRootCdPoint* result,RE::TESObjectREFR** character,float hull) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (!onMain() || !world || !start || !end || !result || !character)
        return runtime->caster(physics,world,start,end,result,character,hull);
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(caller),&memory,sizeof(memory)) || memory.AllocationBase != runtime->smooth)
        return runtime->caster(physics,world,start,end,result,character,hull);
    runtime->smoothCastSeen = true;
    Cost cost;
    const Point startPoint{start[0],start[1],start[2]}, goalPoint{end[0],end[1],end[2]};
    const bool nonzero = finite(startPoint) && finite(goalPoint) && distance(startPoint,goalPoint) > .001f;
    if (nonzero && hull == 15) { runtime->castOrigin = startPoint; runtime->haveOrigin = true; }
    if (!runtime->allowed || !safeNow()) {
        // Ineligible states pass straight through. Empty probes never engage
        // compression; eligible obstruction frames use the filtered path below.
        ++runtime->budget.casts;
        const bool hit = measuredCast(physics,world,start,end,result,character,hull);
        if (countsAsKept(hit,hit && listed(hitReference(world,*result)),nonzero)) {
            runtime->frameKept = true; ++runtime->budget.kept;
        }
        return hit;
    }
    alignas(16) std::array<float,4> origin{},goal{};
    std::memcpy(origin.data(),start,16); std::memcpy(goal.data(),end,16);
    // Native calls are always made, even after our additional-query budget ends.
    ++runtime->budget.casts;
    const bool first = measuredCast(physics,world,start,end,result,character,hull);
    if (!first) return false;
    auto keep = [&]() {
        const auto ref = hitReference(world,*result);
        if (countsAsKept(true,listed(ref),nonzero)) { runtime->frameKept = true; ++runtime->budget.kept; }
        runtime->keptReference = ref ? ref->GetFormID() : 0;
        return true;
    };
    const auto firstContact = *result;
    const auto firstCharacter = *character;
    alignas(16) std::array<float,4> firstEnd{};
    std::memcpy(firstEnd.data(),end,16);
    if (!finite({origin[0],origin[1],origin[2]}) || !finite({goal[0],goal[1],goal[2]}) ||
        distance({origin[0],origin[1],origin[2]},{goal[0],goal[1],goal[2]}) <= .001f) return keep();
    alignas(16) auto from = origin;
    unsigned steps{};
    bool hit = true,valid = true,moved = false;
    while (hit) {
        const auto ref = hitReference(world,*result);
        if (!listed(ref)) break;
        const auto fade = remember(ref);
        if (!fadeReady(fade)) { runtime->refusalReason = 1; valid = false; break; }
        const auto next = advance({from[0],from[1],from[2]},{goal[0],goal[1],goal[2]},
            {end[0],end[1],end[2]},hull,steps);
        // Reserve a reset gateway before each continuation; one shared frame cap.
        if (!next) {
            if (steps >= maxSteps) ++runtime->budget.cap;
            runtime->refusalReason = 2; break; // Current proven contact, never roll back a safe prefix.
        }
        if (runtime->budget.casts+2 > maxCasts || !safeNow()) {
            ++runtime->budget.cap; runtime->refusalReason = 2; valid = false; break;
        }
        const auto guarded = gap(physics,world,*next,result->rootCollidableB);
        if (guarded.result == GapResult::refuse) { runtime->refusalReason = 3; valid = false; break; }
        ++steps;
        if (guarded.result == GapResult::obstruction) {
            const auto p = position(guarded.hit.contact.position);
            const float scale = RE::bhkWorld::GetWorldScaleInverse();
            if (!finite(p)) { runtime->refusalReason = 4; valid = false; break; }
            *result = guarded.hit;
            end[0] = p.x*scale; end[1] = p.y*scale; end[2] = p.z*scale;
            *character = nullptr;
            break;
        }
        from = {next->x,next->y,next->z,origin[3]};
        alignas(16) auto to = goal;
        static_cast<void>(runtime->budget.cast()); moved = true;
        hit = measuredCast(physics,world,from.data(),to.data(),result,character,hull);
        std::memcpy(end,hit ? to.data() : goal.data(),16);
        if (!hit) { *result = {}; *character = nullptr; }
    }
    if (moved) {
        alignas(16) auto resetStart = origin,resetEnd = goal;
        RE::hkpRootCdPoint ignored{}; RE::TESObjectREFR* actor{};
        static_cast<void>(runtime->budget.cast());
        static_cast<void>(measuredCast(physics,world,resetStart.data(),resetEnd.data(),&ignored,&actor,hull));
    }
    if (!valid || !safeNow()) {
        std::memcpy(end,firstEnd.data(),16); *result = firstContact; *character = firstCharacter;
        ++runtime->budget.refused;
        return keep();
    }
    runtime->budget.discarded += steps;
    if (hit) return keep();
    return false;
}
inline std::array<std::uint8_t,14> jump(std::uintptr_t target) {
    std::array<std::uint8_t,14> bytes{0xff,0x25,0,0,0,0};
    std::memcpy(bytes.data()+6,&target,sizeof(target)); return bytes;
}
inline bool guardResult(const char* name,bool passed) {
    char line[160]{};
    std::snprintf(line,sizeof(line),"ThinOccluder guard=%s result=%s memory=live",name,passed ? "PASS" : "FAIL");
    runtime->logger(line); return passed;
}
inline bool guard(std::uint64_t id,std::uintptr_t rva,std::initializer_list<std::uint8_t> bytes) {
    const auto address = REL::ID(id).address();
    char name[32]{}; std::snprintf(name,sizeof(name),"ID%llu",static_cast<unsigned long long>(id));
    return guardResult(name,address == REL::Module::get().base()+rva &&
        std::memcmp(reinterpret_cast<const void*>(address),bytes.begin(),bytes.size()) == 0);
}
inline bool readable(std::uintptr_t address,std::size_t size) {
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) && memory.State == MEM_COMMIT &&
        !(memory.Protect & (PAGE_NOACCESS|PAGE_GUARD)) && address >= reinterpret_cast<std::uintptr_t>(memory.BaseAddress) &&
        size <= memory.RegionSize && address-reinterpret_cast<std::uintptr_t>(memory.BaseAddress) <= memory.RegionSize-size;
}
inline bool ownedBy(std::uintptr_t address,HMODULE module) {
    MEMORY_BASIC_INFORMATION memory{};
    return module && VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) && memory.State == MEM_COMMIT &&
        !(memory.Protect & (PAGE_NOACCESS|PAGE_GUARD)) &&
        (memory.Protect & (PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) && memory.AllocationBase == module;
}
inline bool cameraCallGuard(std::uintptr_t site,std::uintptr_t original) {
    if (!readable(site,5) || *reinterpret_cast<const std::uint8_t*>(site) != 0xe8) return false;
    std::int32_t displacement{}; std::memcpy(&displacement,reinterpret_cast<void*>(site+1),4);
    auto target = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(site+5)+displacement);
    if (target == original) return true;
    const auto tdm = GetModuleHandleW(L"TrueDirectionalMovement.dll");
    if (ownedBy(target,tdm)) return true;
    // SKSE write_call<5> emits an FF25 RIP-relative relay to the provider.
    if (!readable(target,6) || std::memcmp(reinterpret_cast<void*>(target),"\xff\x25",2) != 0) return false;
    std::memcpy(&displacement,reinterpret_cast<void*>(target+2),4);
    const auto slot = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(target+6)+displacement);
    if (!readable(slot,sizeof(target))) return false;
    std::memcpy(&target,reinterpret_cast<void*>(slot),sizeof(target));
    return ownedBy(target,tdm);
}
inline void installHooks(bool hashes) {
    if (!enabled()) return;
    const auto base = REL::Module::get().base();
    constexpr std::array<std::uint8_t,14> prologue{0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55};
    const auto target = REL::ID(33007).address();
    const auto cameraSite = REL::ID(50784).address()+0x1A6;
    const bool addresses = target == base+0x5574f0 && cameraSite == base+0x8f8826 &&
        REL::ID(107719).address() == base+0x1563d10 && REL::ID(403555).address() == base+0x32309d8 &&
        REL::ID(188105).address() == base+0x17fde5c && REL::ID(187407).address() == base+0x17f423c;
    guardResult("addresses",addresses); guardResult("hashes",hashes);
    bool prologues = guardResult("camera-call-native-or-TDM",cameraCallGuard(cameraSite,base+0x558e70));
    prologues &= guardResult("native-camera-update",std::memcmp(reinterpret_cast<const void*>(base+0x558e70),"\x40\x57\x48\x83\xec\x30",6) == 0);
    prologues &= guardResult("caster",target == base+0x5574f0 && std::memcmp(reinterpret_cast<const void*>(target),prologue.data(),prologue.size()) == 0);
    prologues &=
        guard(61402,0xb59d80,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18});
    prologues &= guard(105544,0x14e8000,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x79,0x78});
    prologues &= guard(105545,0x14e8080,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48});
    prologues &= guard(107720,0x1563fc0,{0x40,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30});
    prologues &= guard(70687,0xef2be0,{0x40,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff});
    prologues &= guard(68233,0xce3250,{0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9});
    prologues &= guard(68239,0xce3510,{0xf0,0xff,0x49,0x04,0xc3});
    if (!install(enabled(),hashes,addresses,prologues,runtime->formCount > 0 && runtime->formCount <= maxForms)) {
        refuse("ThinOccluder: exact binary/address/prologue/hook owner check failed; no thin hooks installed.");
        runtime->enabled = false; return;
    }
    runtime->smooth = GetModuleHandleW(L"SmoothCam.dll");
    SKSE::AllocTrampoline(64);
    auto gateway = static_cast<std::uint8_t*>(SKSE::GetTrampoline().allocate(32));
    std::memcpy(gateway,prologue.data(),prologue.size());
    const auto back = jump(target+prologue.size());
    std::memcpy(gateway+prologue.size(),back.data(),back.size());
    FlushInstructionCache(GetCurrentProcess(),gateway,28);
    runtime->caster = reinterpret_cast<Caster>(gateway);
    // ThirdPersonState's existing hook cannot service first-person/paused/load
    // exits. Keep the all-state call, chaining its actual previous target (TDM).
    runtime->update = reinterpret_cast<CameraUpdate>(SKSE::GetTrampoline().write_call<5>(cameraSite,updateCamera));
    const auto detour = jump(reinterpret_cast<std::uintptr_t>(&hookCaster));
    REL::safe_write(target,detour.data(),detour.size());
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(target),detour.size());
    FlushInstructionCache(GetCurrentProcess(),gateway,46); // gateway + 14-byte call relay
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(cameraSite),5);
    runtime->installed = true;
    runtime->logger("ThinOccluder: optional Stage 1 hooks installed; main-thread form resolution pending.");
}
inline std::optional<float> obstructionSample(std::optional<float> measured) {
    // A feature that refused after installing (a later form failed to resolve)
    // keeps its hooks but must not filter compression.
    const bool active = enabled() && runtime->installed && runtime->resolved;
    return compressionSample(active,active && runtime->smoothCastSeen,active && runtime->previousKept,measured);
}
} // namespace skycrpg::camera::thin
