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
using CameraDestroy = void*(*)(RE::PlayerCamera*,unsigned);
using Material = RE::BSLightingShaderMaterialBase;
struct Part {
    RE::NiPointer<RE::BSGeometry> geometry;
    RE::NiPointer<RE::BSShaderProperty> shader;
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
};
struct Runtime {
    std::atomic_bool enabled{};
    bool installed{},resolved{},refused{},trace{},allowed{},frameKept{},previousKept{};
    bool (*eligibleNow)(){};
    RE::NiPoint3 (*goalNow)(){};
    void (*logger)(const char*){};
    std::atomic_uint requests{};
    std::atomic_bool loaded{};
    std::array<std::string,maxForms> configured;
    std::array<std::uint32_t,maxForms> forms{};
    unsigned formCount{3},poolSize{},traceLines{};
    float targetAlpha{.35f};
    std::array<Part,maxPool> pool;
    std::array<Reference,maxReferences> refs;
    Budget budget;
    double now{},last{},nextPrepare{},nextTrace{},nextScan{};
    std::uint64_t frame{};
    RE::FormID cell{},keptReference{},gapReference{};
    unsigned refusalReason{};
    Caster caster{};
    CameraUpdate update{};
    CameraDestroy destroy{};
    HMODULE smooth{};
};
// Retain the bounded private allocations through process exit: renderer consumers
// may outlive swap-back. Recycle idle slots only for the exact same source
// material/blend flags after later updates; never change their texture/layout
// data or free a clone. No scene destructor runs under loader lock.
inline Runtime* runtime = nullptr;
inline bool enabled() { return runtime && runtime->enabled; }
inline bool onMain() {
    const auto main = RE::Main::GetSingleton();
    return main && main->threadID == GetCurrentThreadId();
}
inline double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline void refuse(const char* reason) {
    if (!runtime->refused) { runtime->logger(reason); runtime->refused = true; }
}
inline void requestRestore() {
    if (enabled()) runtime->requests.fetch_add(1,std::memory_order_release);
}
inline void setLoaded(bool value) {
    if (enabled()) { runtime->loaded.store(value,std::memory_order_release); requestRestore(); }
}
inline void attach(RE::BSShaderProperty* shader,RE::BSShaderMaterial* material) {
    using Function = void(*)(RE::BSShaderProperty*,RE::BSShaderMaterial*);
    const REL::Relocation<Function> function{REL::ID(105545)};
    function(shader,material);
}
inline RestoreCheck check(const Part& part) {
    const auto& data = part.geometry->GetGeometryRuntimeData();
    return {data.shaderProperty.get() == part.shader.get(),part.shader->material == part.clone,
        data.alphaProperty == part.privateProperty,part.clone && part.clone->materialAlpha == part.writtenAlpha,
        part.privateProperty && part.privateProperty->alphaFlags == part.flags && part.privateProperty->alphaThreshold == 0};
}
inline bool fadeReady(const Reference* ref) {
    if (!ref || !ref->prepared || !ref->fade.admitted) return false;
    for (unsigned i = 0;i < ref->count;++i) {
        const auto& part = *ref->parts[i];
        const auto state = check(part);
        if (!part.active || !state.material() || !state.property() || part.shader->GetRefCount() != 2) return false;
    }
    return true;
}
inline void restore(Reference& ref) {
    for (unsigned i = 0;i < ref.count;++i) {
        auto& part = *ref.parts[i];
        if (!part.active) continue;
        const auto state = check(part);
        if (state.material()) attach(part.shader.get(),part.original);
        else part.conflict = true;
        if (state.property()) part.geometry->GetGeometryRuntimeData().alphaProperty = part.originalProperty;
        else part.conflict = true;
        part.active = false;
        part.retiredFrame = runtime->frame;
        if (part.conflict) refuse("ThinOccluder: restore conflict; foreign state preserved, affected geometry refused.");
    }
    ref = {};
}
inline void restoreAll() {
    for (auto& ref : runtime->refs) if (ref.used) restore(ref);
    runtime->allowed = false;
}
inline bool listed(RE::TESObjectREFR* ref) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto base = ref ? ref->GetBaseObject() : nullptr;
    return player && ref && ref != player && base && base->Is(RE::FormType::Static) &&
        matches(base->GetFormID(),runtime->forms) && ref->GetParentCell() == player->GetParentCell() && ref->Get3D();
}
inline Reference* remember(RE::TESObjectREFR* ref) {
    if (!listed(ref)) return nullptr;
    const auto handle = ref->GetHandle();
    Reference* empty = nullptr;
    for (auto& entry : runtime->refs) {
        if (entry.used && entry.handle == handle && entry.root.get() == ref->Get3D()) {
            entry.fade.touch(runtime->now); return &entry;
        }
        if (!entry.used && !empty) empty = &entry;
    }
    if (!empty) return nullptr;
    empty->used = true; empty->handle = handle; empty->root.reset(ref->Get3D());
    empty->fade.touch(runtime->now);
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
    const auto flags = translucentFlags(property ? property->alphaFlags : 0);
    for (unsigned i = 0;i < runtime->poolSize;++i) {
        auto& part = runtime->pool[i];
        if (!part.clone || !recycle(part.active,part.conflict,part.original == shader->material,part.flags == flags,
            part.originalAlpha == part.original->materialAlpha,
            std::find(ref.parts.begin(),ref.parts.end(),&part) != ref.parts.end(),part.retiredFrame,runtime->frame)) continue;
        return &part;
    }
    return nullptr;
}
inline bool prepare(Reference& ref) {
    std::array<RE::BSGeometry*,maxParts> geometries{};
    unsigned count{},visited{};
    bool valid = true;
    ++runtime->budget.scans;
    RE::BSVisit::TraverseScenegraphObjects(ref.root.get(),[&](RE::NiAVObject* object) {
        if (++visited > 256) { valid = false; return RE::BSVisit::BSVisitControl::kStop; }
        const auto geometry = object->AsGeometry();
        if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
        const auto shader = geometry->lightingShaderProp_cast();
        const auto old = cached(geometry);
        // Each geometry owns its shader through NiPointer. Reject other owners,
        // even outside WorldRootNode; a private material on a shared shader is unsafe.
        if (count == maxParts || !shader || !shader->material ||
            shader->material->GetType() != RE::BSShaderMaterial::Type::kLighting ||
            (old ? old->conflict || old->active || shader != old->shader.get() || shader->material != old->original ||
                geometry->GetGeometryRuntimeData().alphaProperty != old->originalProperty || old->original->materialAlpha != old->originalAlpha || shader->GetRefCount() != 2 : shader->GetRefCount() != 1)) {
            valid = false; return RE::BSVisit::BSVisitControl::kStop;
        }
        geometries[count++] = geometry;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    if (!valid || !count) return false;
    for (unsigned i = 0;i < count;++i) {
        const auto geometry = geometries[i];
        auto part = cached(geometry);
        if (!part) {
            part = reusable(geometry,ref);
            if (part) {
                part->geometry.reset(geometry);
                part->shader.reset(geometry->lightingShaderProp_cast());
                part->originalProperty = geometry->GetGeometryRuntimeData().alphaProperty;
                part->retiredFrame = 0;
            }
        }
        if (!part) {
            if (runtime->poolSize == maxPool) return false;
            part = &runtime->pool[runtime->poolSize++];
            part->geometry.reset(geometry); part->shader.reset(geometry->lightingShaderProp_cast());
            part->original = static_cast<Material*>(part->shader->material);
            static_cast<void>(part->original->IncRef());
            part->originalAlpha = part->original->materialAlpha;
            part->originalProperty = geometry->GetGeometryRuntimeData().alphaProperty;
            part->flags = translucentFlags(part->originalProperty ? part->originalProperty->alphaFlags : 0);
            using Factory = RE::NiAlphaProperty*(*)();
            const REL::Relocation<Factory> factory{REL::ID(70687)};
            part->privateProperty.reset(factory());
            if (!part->privateProperty || !std::isfinite(part->originalAlpha) || part->originalAlpha <= 0 || part->originalAlpha > 1) {
                part->conflict = true; return false;
            }
            part->privateProperty->alphaFlags = part->flags;
            part->privateProperty->alphaThreshold = 0;
            part->shader->SetMaterial(part->original,true);
            const auto material = part->shader->material;
            if (!material || material == part->original || material->GetType() != RE::BSShaderMaterial::Type::kLighting) {
                attach(part->shader.get(),part->original); part->conflict = true; return false;
            }
            part->clone = static_cast<Material*>(material);
            static_cast<void>(part->clone->IncRef());
            // Keep all clones alive, including this temporary install/swap-back.
            attach(part->shader.get(),part->original);
        }
        ref.parts[ref.count++] = part;
    }
    for (unsigned i = 0;i < ref.count;++i) {
        auto& part = *ref.parts[i];
        part.writtenAlpha = part.originalAlpha;
        part.clone->materialAlpha = part.writtenAlpha;
        ++runtime->budget.fades;
        attach(part.shader.get(),part.clone);
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
        runtime->requests.load(std::memory_order_acquire) == 0 && runtime->eligibleNow();
}
inline void scanSightline() {
    if (runtime->now < runtime->nextScan) return;
    runtime->nextScan = runtime->now+.1;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!player || !player->GetParentCell() || !camera || !camera->cameraRoot) return;
    auto from = player->GetPosition(); from.z += 80;
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
                static_cast<void>(remember(ref));
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    ++runtime->budget.scans;
}
inline void tick() {
    if (!onMain()) return;
    runtime->now = seconds();
    const double dt = runtime->last ? runtime->now-runtime->last : 0;
    runtime->last = runtime->now;
    ++runtime->frame;
    runtime->previousKept = runtime->frameKept;
    runtime->frameKept = false;
    if (runtime->trace && runtime->traceLines < maxTraceLines && runtime->now >= runtime->nextTrace) {
        const auto camera = RE::PlayerCamera::GetSingleton();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (camera && camera->cameraRoot && player) {
            char line[400]{};
            const auto p = camera->cameraRoot->world.translate;
            std::snprintf(line,sizeof(line),"ThinOccluder frame=%llu seconds=%.3f z=%.3f relativeZ=%.3f casts=%u gaps=%u fades=%u scans=%u discarded=%u kept=%u refused=%u pool=%u keptRef=%08X gapRef=%08X reason=%u sample=previous-frame",
                static_cast<unsigned long long>(runtime->frame),runtime->now,p.z,p.z-player->GetPositionZ(),
                runtime->budget.casts,runtime->budget.gaps,runtime->budget.fades,runtime->budget.scans,
                runtime->budget.discarded,runtime->budget.kept,runtime->budget.refused,runtime->poolSize,runtime->keptReference,runtime->gapReference,runtime->refusalReason);
            runtime->logger(line); ++runtime->traceLines;
        }
        runtime->nextTrace = runtime->now+.1;
    }
    runtime->budget = {};
    runtime->keptReference = runtime->gapReference = runtime->refusalReason = 0;
    if (runtime->requests.exchange(0,std::memory_order_acq_rel)) restoreAll();
    if (!runtime->enabled || !runtime->loaded.load(std::memory_order_acquire)) { restoreAll(); return; }
    if (!runtime->resolved) resolveForms();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto cell = player && player->GetParentCell() ? player->GetParentCell()->GetFormID() : 0;
    if (cell != runtime->cell) { restoreAll(); runtime->cell = cell; }
    runtime->allowed = safeNow();
    if (!runtime->allowed) { restoreAll(); return; }
    scanSightline();
    for (auto& ref : runtime->refs) {
        if (!ref.used) continue;
        const auto live = ref.handle.get();
        if (!live || !listed(live.get()) || live->Get3D() != ref.root.get()) { restore(ref); continue; }
        if (!ref.prepared && runtime->now >= runtime->nextPrepare) {
            runtime->nextPrepare = runtime->now+.1;
            if (!prepare(ref)) { restore(ref); ++runtime->budget.refused; continue; }
        }
        if (!ref.fade.step(runtime->now,dt,runtime->targetAlpha,true)) { restore(ref); continue; }
        if (!ref.prepared) continue;
        bool valid = true;
        for (unsigned i = 0;i < ref.count;++i) {
            auto& part = *ref.parts[i];
            const auto state = check(part);
            if (!state.material() || !state.property() || part.shader->GetRefCount() != 2) { valid = false; break; }
        }
        if (!valid) { restore(ref); ++runtime->budget.refused; continue; }
        for (unsigned i = 0;i < ref.count;++i) {
            auto& part = *ref.parts[i];
            part.writtenAlpha = part.originalAlpha*ref.fade.alpha;
            part.clone->materialAlpha = part.writtenAlpha;
            ++runtime->budget.fades;
        }
    }
}
inline void updateCamera(RE::PlayerCamera* camera) {
    if (onMain()) tick();
    runtime->update(camera);
    if (onMain() && runtime->allowed && !safeNow()) restoreAll();
}
inline void* destroyCamera(RE::PlayerCamera* camera,unsigned flags) {
    if (onMain()) restoreAll(); else requestRestore();
    return runtime->destroy(camera,flags);
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
    if (!runtime->budget.gap() || !context || !world) return {};
    void* wrapper{}; RE::hkpWorldObject* phantom{};
    std::memcpy(&wrapper,context,sizeof(wrapper));
    if (wrapper) std::memcpy(&phantom,static_cast<const std::byte*>(wrapper)+0x10,sizeof(phantom));
    // This query phantom has no world. Native caster gets the world from GetWorld1.
    const auto worldSlots = *reinterpret_cast<const std::uintptr_t* const*>(world);
    if (worldSlots[0x27] != REL::Module::get().base()+0x104b470) return {};
    const auto havok = world->GetWorld1();
    if (!phantom || !havok) return {};
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
            const bool match = listed(ref);
            if (match) {
                const auto fade = remember(ref);
                if (!fadeReady(fade)) { decision.unknown = true; continue; }
            }
            // Actor branches have extra native endpoint/character rules. Preserve
            // the original result rather than inventing an actor hit from a sweep.
            const bool known = object && ref && ref->GetBaseObject() && ref->GetBaseObject()->Is(RE::FormType::Static);
            decision.contact(known,match,collector == &overlap,fraction(hit),i);
        }
    }
    result.result = decision.result();
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
inline bool hookCaster(void* physics,RE::bhkWorld* world,float* start,float* end,
    RE::hkpRootCdPoint* result,RE::TESObjectREFR** character,float hull) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (!onMain() || !world || !start || !end || !result || !character)
        return runtime->caster(physics,world,start,end,result,character,hull);
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(caller),&memory,sizeof(memory)) || memory.AllocationBase != runtime->smooth)
        return runtime->caster(physics,world,start,end,result,character,hull);
    if (!runtime->allowed || !safeNow()) {
        // Compression can continue observing real native hits while its higher
        // state suspends filtering. No scene lookup or fade work on this path.
        ++runtime->budget.casts;
        const bool hit = runtime->caster(physics,world,start,end,result,character,hull);
        runtime->frameKept = runtime->frameKept || hit;
        if (hit) ++runtime->budget.kept;
        return hit;
    }
    alignas(16) std::array<float,4> origin{},goal{};
    std::memcpy(origin.data(),start,16); std::memcpy(goal.data(),end,16);
    // Native calls are always made, even after our additional-query budget ends.
    ++runtime->budget.casts;
    const bool first = runtime->caster(physics,world,start,end,result,character,hull);
    if (!first) return false;
    auto keep = [&]() {
        runtime->frameKept = true; ++runtime->budget.kept;
        const auto ref = hitReference(world,*result);
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
        if (!next || runtime->budget.casts+2 > maxCasts || !safeNow()) { runtime->refusalReason = 2; valid = false; break; }
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
        hit = runtime->caster(physics,world,from.data(),to.data(),result,character,hull);
        std::memcpy(end,hit ? to.data() : goal.data(),16);
        if (!hit) { *result = {}; *character = nullptr; }
    }
    if (moved) {
        alignas(16) auto resetStart = origin,resetEnd = goal;
        RE::hkpRootCdPoint ignored{}; RE::TESObjectREFR* actor{};
        static_cast<void>(runtime->budget.cast());
        static_cast<void>(runtime->caster(physics,world,resetStart.data(),resetEnd.data(),&ignored,&actor,hull));
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
inline bool guard(std::uint64_t id,std::uintptr_t rva,std::initializer_list<std::uint8_t> bytes) {
    const auto address = REL::ID(id).address();
    return address == REL::Module::get().base()+rva && std::memcmp(reinterpret_cast<const void*>(address),bytes.begin(),bytes.size()) == 0;
}
inline void installHooks(bool hashes) {
    if (!enabled()) return;
    const auto base = REL::Module::get().base();
    constexpr std::array<std::uint8_t,14> prologue{0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55};
    const auto target = REL::ID(33007).address();
    const auto cameraSite = REL::ID(50784).address()+0x1A6;
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCamera[0]};
    const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
    const bool addresses = target == base+0x5574f0 && cameraSite == base+0x8f8826 &&
        REL::ID(107719).address() == base+0x1563d10 && REL::ID(403555).address() == base+0x32309d8 &&
        REL::ID(188105).address() == base+0x17fde5c && REL::ID(187407).address() == base+0x17f423c &&
        table.address() == base+0x196d980 && slots[0] == base+0x8fc670 && slots[2] == base+0x558e70;
    const bool prologues = addresses &&
        std::memcmp(reinterpret_cast<const void*>(cameraSite),"\xe8\x45\x06\xc6\xff",5) == 0 &&
        std::memcmp(reinterpret_cast<const void*>(target),prologue.data(),prologue.size()) == 0 &&
        guard(61402,0xb59d80,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18}) &&
        guard(105544,0x14e8000,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x79,0x78}) &&
        guard(105545,0x14e8080,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48}) &&
        guard(107720,0x1563fc0,{0x40,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30}) &&
        guard(70687,0xef2be0,{0x40,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff}) &&
        guard(68233,0xce3250,{0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9}) &&
        guard(68239,0xce3510,{0xf0,0xff,0x49,0x04,0xc3}) &&
        std::memcmp(reinterpret_cast<const void*>(base+0x558e70),"\x40\x57\x48\x83\xec\x30",6) == 0 &&
        std::memcmp(reinterpret_cast<const void*>(base+0x8fc670),"\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20",10) == 0;
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
    runtime->update = reinterpret_cast<CameraUpdate>(slots[2]);
    runtime->destroy = reinterpret_cast<CameraDestroy>(slots[0]);
    // PlayerCamera calls TESCamera::Update directly here, bypassing slot 2.
    static_cast<void>(SKSE::GetTrampoline().write_call<5>(cameraSite,updateCamera));
    static_cast<void>(table.write_vfunc(0,destroyCamera));
    const auto detour = jump(reinterpret_cast<std::uintptr_t>(&hookCaster));
    REL::safe_write(target,detour.data(),detour.size());
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(target),detour.size());
    FlushInstructionCache(GetCurrentProcess(),gateway,46); // gateway + 14-byte call relay
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(cameraSite),5);
    runtime->installed = true;
    runtime->logger("ThinOccluder: optional Stage 1 hooks installed; main-thread form resolution pending.");
}
inline bool keptNativeHit() { return compressionAllowed(runtime && runtime->installed,runtime && runtime->previousKept); }
} // namespace skycrpg::camera::thin
