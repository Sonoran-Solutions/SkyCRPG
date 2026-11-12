// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by plugin.cpp after thin_occluder.h. P5.5-02 wave 1 disposable
// spike adapter (DYL-217): nothing is allocated, installed or evaluated unless
// [FixedCameraSpike] bEnabled=1. SmoothCam stays the only transform writer.
#include "fixed_spike_policy.h"
#include "fixed_spike_fade_policy.h" // base rig: the engine's own player fade (bHoldPlayerFade)
#include "fixed_spike_shader.h" // wave 2 shader: iSeeThroughMode=3
#include <memory>

namespace skycrpg::camera::fixed {
using RoomTest = bool(*)(RE::BSMultiBoundRoom*,const RE::NiPoint3&);
// One code per refusal site, logged once: FixedCameraSpike refused code=FCS-NN.
enum class Refusal : unsigned {
    settings = 1, prerequisites = 2, service = 3,
    casterModule = 10, casterAddress = 11, casterPrologue = 12, casterMemory = 13, casterProtect = 14,
    roomTable = 20, roomSlot = 21, roomPrologue = 22, roomCallers = 23,
    pickSlot = 30, hideExterior = 31,
    fadeSetting = 15,
    snapMapped = 40, snapUnbound = 41,
    traceWriter = 50,
    restoreConflict = 60, offMain = 61
};
struct Hidden {
    RE::ObjectRefHandle handle;
    RE::NiPointer<RE::NiAVObject> root;
    Track track;
    RE::FormID ref{},base{};
    Reason reason{Reason::none};
    float reach{}; // ground-plane reach from the feet at the last scan that saw it
    bool used{};
};
struct Runtime {
    Config config;
    void (*logger)(const char*){};
    RE::NiPoint3 (*goalNow)(){};
    RE::NiPoint3 (*preCollisionNow)(){};
    unsigned (*causeNow)(int){}; // Exit for an ineligible frame; -1 = the update hook did not run
    std::atomic_bool active{};   // look/update hold and the all-state frame bracket are usable
    std::atomic<std::uint64_t> refusals{};
    bool castReady{},castChained{},roomHook{},pickChecked{},pickRefused{},traceReady{};
    // Frame bracket: written by the update hook, read by the caster, main thread only.
    bool frameEligible{},frameSeen{},frameSlide{};
    State frameState{State::disabled};
    unsigned silentFrames{};
    std::uint64_t frame{};
    double now{};
    // Rig.
    SnapTurn turn;
    std::atomic_uint snapLeft{},snapRight{};
    float lastTarget{};
    unsigned suppressed{},wouldHit{},passed{},passedHit{};
    RigWindow window;
    Vec previousCamera{};          // rendered position of the last eligible frame
    std::uint64_t previousFrame{};
    bool previousEligible{};
    thin::Caster caster{};
    HMODULE smooth{};
    // Rooms: the hook runs on culling threads and reads only these.
    std::atomic<std::shared_ptr<const RoomView>> view;
    std::atomic_bool roomActive{};
    RoomCounters counters;
    RoomCallers callers;
    RoomTest originalRoom{};
    // Hide.
    std::array<Hidden,maxHidden> hidden;
    unsigned tracked{};
    HideBudget budget;
    HideCounters totals;
    CutPlane cut;
    std::array<RE::FormID,32> skipLog{};
    unsigned skipNext{};
    float lastCut{},lastFeet{};
    double nextScan{},nextSummary{};
    bool exteriorNoted{};
    // Lifecycle.
    std::atomic_uint requests{};
    RE::FormID cell{};
    // Trace.
    TraceWriter writer;
    LineBudget rigLines{rigTraceLines},roomLines{roomTraceLines},hideLines{hideTraceLines};
    LineBudget shaderLines{shader::traceLines},shaderLookLines{shader::traceLines}; // wave 2 shader: FixedShader, FixedShaderLook
    LineBudget shaderVoidLines{shader::traceLines}; // wave 2 shader, third pass: FixedShaderVoid
    LineBudget shaderStyleLines{shader::traceLines}; // wave 2 shader, fourth pass: FixedShaderStyle (iCutStyle=1)
    LineBudget shaderStyle2Lines{shader::traceLines}; // wave 2 shader, fourth pass W5 fixes: FixedShaderStyle2 (iCutStyle=1)
    LineBudget shaderFrameLines{shader::traceLines}; // wave 2 shader, fifth pass: FixedShaderFrame (bFramePass=1)
    LineBudget shaderFillLines{shader::traceLines}; // Stage 3.1: FixedShaderFill (iCutFill=1)
    // Player fade hold (fixed_spike_fade_policy.h). Main thread only, except the pointer set at install.
    float* fadeValue{};          // the Setting's value; null: not installed
    FadeFeature fadeState{FadeFeature::off};
    bool fadeHeld{};
    float fadeSaved{};
    std::uint32_t playerHandle{};
    FadeWindow fadeWindow;
    double nextFade{};
    LineBudget fadeLines{fadeTraceLines};
    double nextRig{},nextRoom{};
};
// Process lifetime, like the thin runtime: hidden roots and the trace worker
// must never be torn down under the loader lock.
inline Runtime* runtime = nullptr;
inline bool engaged() { return runtime && runtime->active.load(std::memory_order_acquire); }
inline void note(const char* text) {
    char line[320]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike %s",text);
    runtime->logger(line);
}
inline void refuse(Refusal code,const char* text) {
    const std::uint64_t bit = std::uint64_t{1} << static_cast<unsigned>(code);
    if (runtime->refusals.fetch_or(bit,std::memory_order_acq_rel) & bit) return;
    char line[320]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike refused code=FCS-%02u %s",static_cast<unsigned>(code),text);
    runtime->logger(line);
}
inline void emit(LineBudget& lines,const char* text) {
    if (runtime->traceReady && lines.take()) static_cast<void>(runtime->writer.append(text));
}
// Any thread: only flags are posted; the main-thread frame bracket acts on them.
inline void requestRestore(Exit cause) {
    if (runtime) runtime->requests.fetch_or(exitBit(cause),std::memory_order_release);
}
inline void requestSnap(int direction) {
    if (direction > 0) runtime->snapRight.fetch_add(1u,std::memory_order_relaxed);
    else if (direction < 0) runtime->snapLeft.fetch_add(1u,std::memory_order_relaxed);
}
inline void dropSnaps() {
    if (!runtime) return;
    runtime->snapLeft.store(0u,std::memory_order_relaxed); runtime->snapRight.store(0u,std::memory_order_relaxed);
}

// ---- hide -------------------------------------------------------------------
inline const char* modelOf(RE::TESBoundObject* base) {
    const auto model = base ? base->As<RE::TESModel>() : nullptr;
    const auto path = model ? model->GetModel() : nullptr;
    return path ? path : "";
}
inline void traceHide(const char* event,const Hidden& slot,const char* cause,const char* model) {
    if (!runtime->config.hideTrace) return;
    char line[640]{};
    std::snprintf(line,sizeof(line),"FixedHide t=%.3f frame=%llu event=%s ref=%08X base=%08X reason=%s cause=%s hidden=%u cutZ=%.2f feetZ=%.2f model=%.200s",
        runtime->now,static_cast<unsigned long long>(runtime->frame),event,slot.ref,slot.base,reasonName(slot.reason),cause,
        runtime->budget.hidden,static_cast<double>(runtime->lastCut),static_cast<double>(runtime->lastFeet),model);
    emit(runtime->hideLines,line);
}
// Undo only our own write, on the node we wrote it to, then drop the slot.
inline void release(Hidden& slot,Exit cause) {
    if (slot.track.hidden) {
        if (restoreWrite(slot.root->GetAppCulled(),false)) { slot.root->SetAppCulled(false); ++runtime->totals.restores; }
        else {
            ++runtime->totals.conflicts;
            refuse(Refusal::restoreConflict,"a hidden root was already visible at restore; another writer cleared it, nothing rewritten");
        }
        runtime->budget.release();
        ++runtime->totals.exits[static_cast<std::size_t>(cause)];
        traceHide("restore",slot,exitNames[static_cast<std::size_t>(cause)],"");
    }
    slot = {};
    if (runtime->tracked) --runtime->tracked;
}
inline void restoreAll(Exit cause) {
    if (!runtime->tracked) return;
    for (auto& slot : runtime->hidden) if (slot.used) release(slot,cause);
}
// ---- player fade hold (bHoldPlayerFade) -----------------------------------------
// At install (kDataLoaded, any thread: reads only): the Setting by ID, its address and its name.
inline void installFade(std::uintptr_t base) {
    auto& r = *runtime;
    if (!r.config.holdFade) return;
    const std::uintptr_t at = REL::ID(fadeSettingId).address();
    const char* name = at == base+fadeSettingRva ? *reinterpret_cast<const char* const*>(at+settingName) : nullptr;
    if (const unsigned failed = fadeSettingCheck(base,at,name)) {
        r.fadeState = FadeFeature::refused;
        char text[224]{};
        std::snprintf(text,sizeof(text),"check=%u: Setting ID %llu is not fCharControllerCheckHeightOffset:Camera at the audited RVA; "
            "the engine's player fade is left as it is",failed,static_cast<unsigned long long>(fadeSettingId));
        refuse(Refusal::fadeSetting,text); return;
    }
    r.fadeValue = reinterpret_cast<float*>(at+settingValue);
    r.fadeState = FadeFeature::installed;
}
// Main thread: endFrame (eligible: this frame held the view; the next native update reads the
// hold) and every lifecycle exit on the main thread (not eligible).
inline void settleFade(bool eligible) {
    auto& r = *runtime;
    if (!r.fadeValue || !thin::onMain()) return;
    float& value = *r.fadeValue;
    const FadeStep step = fadeStep(true,eligible,r.fadeHeld,value);
    ++r.fadeWindow.steps[static_cast<std::size_t>(step)];
    if (step == FadeStep::hold || step == FadeStep::rehold) { r.fadeSaved = value; value = fadeHoldValue; r.fadeHeld = true; }
    else if (step == FadeStep::release) { value = r.fadeSaved; r.fadeHeld = false; }
    else if (step == FadeStep::leave) r.fadeHeld = false;
}
// Main thread, every camera frame: whether the engine's fade handle names the player.
inline void observeFade() {
    auto& r = *runtime;
    if (r.fadeState != FadeFeature::installed) return;
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!camera || !player) return;
    if (!r.playerHandle) r.playerHandle = player->GetHandle().native_handle();
    std::uint32_t handle{};
    std::memcpy(&handle,reinterpret_cast<const std::uint8_t*>(camera)+playerFadeHandle,sizeof(handle));
    ++r.fadeWindow.frames;
    if (r.playerHandle && handle == r.playerHandle) { ++r.fadeWindow.playerFaded; if (r.fadeHeld) ++r.fadeWindow.heldPlayerFaded; }
}
inline void traceFade() {
    auto& r = *runtime;
    if (r.fadeState == FadeFeature::off || !r.traceReady || r.now < r.nextFade) return;
    r.nextFade = r.now+.1;
    FadeSample s;
    s.seconds = r.now; s.frame = r.frame; s.state = r.fadeState; s.held = r.fadeHeld;
    s.current = r.fadeValue ? *r.fadeValue : std::numeric_limits<float>::quiet_NaN();
    s.saved = r.fadeHeld ? r.fadeSaved : std::numeric_limits<float>::quiet_NaN();
    s.window = r.fadeWindow; r.fadeWindow = {};
    char line[sizeof(TraceRow)]{};
    if (formatFade(line,sizeof(line),s)) emit(r.fadeLines,line);
}
// Lifecycle events can arrive on any thread; restore at once when on Main.
inline void lifecycle(Exit cause) {
    if (!runtime) return;
    requestRestore(cause);
    runtime->roomActive.store(false,std::memory_order_release);
    shader::deactivate(); // wave 2 shader
    if (runtime->active.load(std::memory_order_acquire) && thin::onMain()) { restoreAll(cause); settleFade(false); }
}
// Plain placed references only. Light references are left out on purpose: the
// 3D of a light is the light itself, and hiding a ceiling must not put out the
// lamps under it.
inline Kind kindOf(const RE::TESObjectREFR* ref,const RE::TESBoundObject* base) {
    if (!base || ref->GetFormType() != RE::FormType::Reference) return Kind::excluded;
    switch (base->GetFormType()) {
    case RE::FormType::Static:
    case RE::FormType::MovableStatic:
        return Kind::architecture;
    case RE::FormType::Furniture:
    case RE::FormType::Activator:
    case RE::FormType::TalkingActivator:
    case RE::FormType::Container:
    case RE::FormType::Door:
    case RE::FormType::Tree:
    case RE::FormType::Flora:
    case RE::FormType::Misc:
    case RE::FormType::Weapon:
    case RE::FormType::Armor:
    case RE::FormType::Book:
    case RE::FormType::Ingredient:
    case RE::FormType::AlchemyItem:
    case RE::FormType::Scroll:
    case RE::FormType::Ammo:
    case RE::FormType::KeyMaster:
    case RE::FormType::SoulGem:
        return Kind::object;
    default:
        return Kind::excluded;
    }
}
inline Extent extentOf(const RE::NiAVObject* root,const RE::TESBoundObject* base,bool& sphere) {
    const auto& bounds = base->boundData;
    const auto& transform = root->world;
    const auto& m = transform.rotate.entry;
    Extent box = worldExtent(
        {static_cast<float>(bounds.boundMin.x),static_cast<float>(bounds.boundMin.y),static_cast<float>(bounds.boundMin.z)},
        {static_cast<float>(bounds.boundMax.x),static_cast<float>(bounds.boundMax.y),static_cast<float>(bounds.boundMax.z)},
        {m[2][0],m[2][1],m[2][2]},transform.scale,transform.translate.z);
    sphere = !box.valid;
    if (sphere) box = sphereExtent(root->worldBound.center.z,root->worldBound.radius);
    return box;
}
inline Hidden* findSlot(RE::FormID ref,const RE::NiAVObject* root) {
    for (auto& slot : runtime->hidden) if (slot.used && slot.ref == ref && slot.root.get() == root) return &slot;
    return nullptr;
}
// A full table gives up its farthest entry (restored at once) to a strictly
// nearer candidate, so far pieces can never hold the cap against near ones.
inline Hidden* claimSlot(RE::TESObjectREFR* ref,RE::NiAVObject* root,const RE::TESBoundObject* base,float reach) {
    auto& r = *runtime;
    if (r.tracked >= r.config.hideTotal) {
        const int farthest = farthestSlot(r.hidden,[](const Hidden& slot) { return slot.used; },[](const Hidden& slot) { return slot.reach; });
        if (farthest < 0 || !evicts(reach,r.hidden[static_cast<std::size_t>(farthest)].reach)) { ++r.budget.totalCap; return nullptr; }
        release(r.hidden[static_cast<std::size_t>(farthest)],Exit::evicted);
    }
    for (auto& slot : r.hidden) if (!slot.used) {
        slot.used = true; slot.handle = ref->GetHandle(); slot.root.reset(root);
        slot.ref = ref->GetFormID(); slot.base = base->GetFormID(); slot.reach = reach;
        ++r.tracked;
        return &slot;
    }
    return nullptr;
}
inline void noteSkip(const RE::TESObjectREFR* ref,RE::TESBoundObject* base,Skip skip,const Extent& box) {
    ++runtime->totals.skips[static_cast<std::size_t>(skip)];
    if (!runtime->config.hideTrace) return;
    const auto id = ref->GetFormID();
    for (const auto logged : runtime->skipLog) if (logged == id) return;
    runtime->skipLog[runtime->skipNext] = id;
    runtime->skipNext = (runtime->skipNext+1u)%static_cast<unsigned>(runtime->skipLog.size());
    char line[640]{};
    std::snprintf(line,sizeof(line),"FixedHide t=%.3f frame=%llu event=skip ref=%08X base=%08X reason=on-ray cause=%s minZ=%.2f maxZ=%.2f spanX=%.2f spanY=%.2f cutZ=%.2f feetZ=%.2f model=%.200s",
        runtime->now,static_cast<unsigned long long>(runtime->frame),id,base->GetFormID(),skipName(skip),
        static_cast<double>(box.minZ),static_cast<double>(box.maxZ),static_cast<double>(box.spanX),static_cast<double>(box.spanY),
        static_cast<double>(runtime->lastCut),static_cast<double>(runtime->lastFeet),modelOf(base));
    emit(runtime->hideLines,line);
}
template<std::size_t Size>
bool listed(const std::array<const RE::TESObjectREFR*,Size>& refs,unsigned count,const RE::TESObjectREFR* ref) {
    for (unsigned i = 0;i < count;++i) if (refs[i] == ref) return true;
    return false;
}
// Sightline rays (camera to player, marching past each hit) and floor probes.
// Reference pointers are compared, never dereferenced, by the scan that follows.
inline void castRays(RE::PlayerCharacter* player,RE::TESObjectCELL* parent,Vec feet,Vec eye,
    std::array<const RE::TESObjectREFR*,maxRayHits>& rayRefs,unsigned& rayCount,
    std::array<const RE::TESObjectREFR*,maxFloorProbes>& floorRefs,unsigned& floorCount) {
    auto& r = *runtime;
    if (r.pickRefused) return;
    const auto world = parent->GetbhkWorld();
    const auto controller = player->GetCharController();
    const float scale = RE::bhkWorld::GetWorldScale();
    if (!world || !controller || !std::isfinite(scale) || scale <= 0) return;
    if (!r.pickChecked) {
        // CommonLib slot 0x33 is checked against the audited PickObject once.
        const auto slots = *reinterpret_cast<const std::uintptr_t* const*>(world);
        if (slots[0x33] != REL::Module::get().base()+0x104be80) {
            r.pickRefused = true;
            refuse(Refusal::pickSlot,"bhkWorld slot 0x33 is not the audited PickObject; on-ray hide and floor protection disabled, above-cut only");
            return;
        }
        r.pickChecked = true;
    }
    RE::CFilter filter{};
    controller->GetCollisionFilterInfo(filter);
    const std::uint32_t layer = (filter.filter & 0xffff0000u) | static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
    RE::BSReadLockGuard lock(world->worldLock);
    const auto pick = [&](Vec from,Vec to,float& fraction,bool& hit) -> const RE::TESObjectREFR* {
        RE::bhkPickData ray{};
        ray.rayInput.from = RE::hkVector4(from.x*scale,from.y*scale,from.z*scale,0.0f);
        ray.rayInput.to = RE::hkVector4(to.x*scale,to.y*scale,to.z*scale,0.0f);
        ray.rayInput.filterInfo.filter = layer;
        ray.rayInput.enableShapeCollectionFilter = false;
        ray.rayOutput.Reset();
        ++r.totals.rays;
        world->PickObject(ray);
        if (ray.pickFailed) ++r.totals.rayFailed;
        hit = !ray.pickFailed && ray.rayOutput.rootCollidable != nullptr;
        if (!hit) return nullptr;
        fraction = ray.rayOutput.hitFraction;
        ++r.totals.rayHits;
        return RE::TESHavokUtilities::FindCollidableRef(*ray.rayOutput.rootCollidable);
    };
    for (const auto target : rayTargets(feet)) {
        Vec from = eye;
        for (unsigned segment = 0;segment < maxRaySegments;++segment) {
            float fraction = 1.0f; bool hit = false;
            const auto ref = pick(from,target,fraction,hit);
            if (!hit) break;
            if (ref && !listed(rayRefs,rayCount,ref)) {
                if (rayCount == maxRayHits) { ++r.totals.rayCap; break; }
                rayRefs[rayCount++] = ref;
            }
            const auto next = rayContinue(from,target,fraction);
            if (!next) break;
            from = *next;
        }
    }
    for (const auto probe : floorProbes(feet)) {
        float fraction = 1.0f; bool hit = false;
        const auto ref = pick({probe.x,probe.y,probe.z+48.0f},{probe.x,probe.y,probe.z-160.0f},fraction,hit);
        if (ref && !listed(floorRefs,floorCount,ref)) floorRefs[floorCount++] = ref;
    }
}
inline void traceSummary() {
    auto& r = *runtime;
    if (!r.config.hideTrace || r.now < r.nextSummary) return;
    r.nextSummary = r.now+1.0;
    const auto& t = r.totals;
    char line[768]{};
    std::snprintf(line,sizeof(line),
        "FixedHide t=%.3f frame=%llu event=summary hidden=%u tracked=%u scans=%llu visited=%llu rays=%llu rayHits=%llu hides=%llu restores=%llu "
        "frameCap=%u totalCap=%u visitCap=%llu rayCap=%llu rayFailed=%llu skipNoBounds=%llu skipFloorContact=%llu skipFloorSpan=%llu conflicts=%llu foreign=%llu "
        "cutZ=%.2f feetZ=%.2f exitDisable=%llu exitF8=%llu exitFirstPerson=%llu exitMenu=%llu exitLoad=%llu exitCell=%llu exitUnload=%llu "
        "exitDeath=%llu exitSpecial=%llu exitHookSilent=%llu exitExpired=%llu exitUnderfoot=%llu exitEvicted=%llu sample=cumulative",
        r.now,static_cast<unsigned long long>(r.frame),r.budget.hidden,r.tracked,t.scans,t.visited,t.rays,t.rayHits,t.hides,t.restores,
        r.budget.frameCap,r.budget.totalCap,t.visitCap,t.rayCap,t.rayFailed,t.skips[static_cast<std::size_t>(Skip::noBounds)],
        t.skips[static_cast<std::size_t>(Skip::floorContact)],t.skips[static_cast<std::size_t>(Skip::floorSpan)],t.conflicts,t.foreign,
        static_cast<double>(r.lastCut),static_cast<double>(r.lastFeet),t.exits[0],t.exits[1],t.exits[2],t.exits[3],t.exits[4],t.exits[5],
        t.exits[6],t.exits[7],t.exits[8],t.exits[9],t.exits[10],t.exits[11],t.exits[12]);
    emit(r.hideLines,line);
}
inline void scanHide(RE::PlayerCharacter* player,RE::TESObjectCELL* parent) {
    auto& r = *runtime;
    if (r.now < r.nextScan) return;
    r.nextScan = r.now+hideScanSeconds;
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->cameraRoot) return;
    const auto feetPoint = player->GetPosition();
    const auto eyePoint = camera->cameraRoot->world.translate;
    const Vec feet{feetPoint.x,feetPoint.y,feetPoint.z},eye{eyePoint.x,eyePoint.y,eyePoint.z};
    if (!finite(feet) || !finite(eye)) return;
    r.budget.frame();
    ++r.totals.scans;
    const float cutZ = r.cut.update(feet.z,r.now)+r.config.cutHeight;
    r.lastCut = cutZ; r.lastFeet = feet.z;
    std::array<const RE::TESObjectREFR*,maxRayHits> rayRefs{};
    std::array<const RE::TESObjectREFR*,maxFloorProbes> floorRefs{};
    unsigned rayCount{},floorCount{},visited{};
    castRays(player,parent,feet,eye,rayRefs,rayCount,floorRefs,floorCount);
    parent->ForEachReference([&](RE::TESObjectREFR* ref) {
        if (++visited > maxVisited) { ++r.totals.visitCap; return RE::BSContainer::ForEachResult::kStop; }
        const auto root = ref->Get3D();
        const auto base = ref->GetBaseObject();
        if (!root || ref == player) return RE::BSContainer::ForEachResult::kContinue;
        const Kind kind = kindOf(ref,base);
        if (kind == Kind::excluded) return RE::BSContainer::ForEachResult::kContinue;
        bool sphere = false;
        const Extent box = extentOf(root,base,sphere);
        const bool onRay = listed(rayRefs,rayCount,ref),onFloor = listed(floorRefs,floorCount,ref);
        const auto& bound = root->worldBound;
        const float reach = groundReach(feet,{bound.center.x,bound.center.y,bound.center.z},bound.radius);
        // The looser exit thresholds first; most references fail them without a table lookup.
        Verdict verdict = classify(kind,box,sphere,onRay,onFloor,cutZ,feet.z,reach,r.config.hideRadius,true);
        Hidden* slot = verdict.reason != Reason::none ? findSlot(ref->GetFormID(),root) : nullptr;
        if (verdict.reason != Reason::none && (!slot || !slot->track.hidden))
            verdict = classify(kind,box,sphere,onRay,onFloor,cutZ,feet.z,reach,r.config.hideRadius,false);
        if (verdict.reason == Reason::none) {
            if (verdict.skip != Skip::none) noteSkip(ref,base,verdict.skip,box);
            return RE::BSContainer::ForEachResult::kContinue;
        }
        if (!slot) slot = claimSlot(ref,root,base,reach);
        if (slot) { slot->reason = verdict.reason; slot->reach = reach; slot->track.seen(r.now); }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    r.totals.visited += visited;
    std::array<Near,maxHidden> due{};
    unsigned dueCount{},index{};
    for (auto& slot : r.hidden) {
        const unsigned at = index++;
        if (!slot.used) continue;
        const auto live = slot.handle.get();
        if (!live || live->Get3D() != slot.root.get()) { release(slot,Exit::unload); continue; }
        if (slot.track.hidden && slot.track.last != r.now) {
            // No longer qualifies. If the player now stands on it, or it lies at
            // or below the feet, it comes back in this scan, not after the dwell.
            bool sphere = false;
            const auto liveBase = live->GetBaseObject();
            if (liveBase && restoreAtOnce(extentOf(slot.root.get(),liveBase,sphere),listed(floorRefs,floorCount,live.get()),feet.z,slot.reason == Reason::aboveCut)) {
                release(slot,Exit::underfoot); continue;
            }
        }
        if (slot.track.shouldHide(r.now)) due[dueCount++] = {slot.reach,at};
        else if (slot.track.shouldRestore(r.now) || slot.track.expired(r.now)) release(slot,Exit::expired);
    }
    // Nearest first, so the per-scan cap spends itself on what is over the player.
    nearestFirst(due,dueCount);
    for (unsigned i = 0;i < dueCount;++i) {
        auto& slot = r.hidden[due[i].slot];
        // A root that is already hidden belongs to the engine or another mod.
        if (slot.root->GetAppCulled()) { ++r.totals.foreign; release(slot,Exit::expired); continue; }
        if (!r.budget.take()) break;
        slot.root->SetAppCulled(true);
        slot.track.hidden = true;
        ++r.totals.hides;
        const auto live = slot.handle.get();
        traceHide("hide",slot,"-",live ? modelOf(live->GetBaseObject()) : "");
    }
}
// Exterior cell with hide configured: say so once per entry and do nothing.
inline void noteExterior(RE::FormID cellId) {
    auto& r = *runtime;
    if (r.exteriorNoted) return;
    r.exteriorNoted = true;
    refuse(Refusal::hideExterior,"exterior cell: this wave's hide is interior-only; no scan, no hide rays, nothing hidden outdoors");
    if (!r.config.hideTrace) return;
    char line[256]{};
    std::snprintf(line,sizeof(line),"FixedHide t=%.3f frame=%llu event=inactive cause=exterior code=FCS-31 cell=%08X hidden=%u",
        r.now,static_cast<unsigned long long>(r.frame),cellId,r.budget.hidden);
    emit(r.hideLines,line);
}

// ---- rooms ------------------------------------------------------------------
// A claim needs both: the caller is one of the three culling passes, and the
// queried point is at the rendered camera. Attachment and water queries, and
// any caller this build does not know, always get the vanilla answer.
inline bool hookRoom(RE::BSMultiBoundRoom* room,const RE::NiPoint3& point) {
    const auto caller = roomCaller(runtime->callers,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    auto& r = *runtime;
    const bool vanilla = r.originalRoom(room,point);
    r.counters.count(caller);
    if (!r.roomActive.load(std::memory_order_acquire)) return vanilla;
    const auto view = r.view.load(std::memory_order_acquire);
    const Vec asked{point.x,point.y,point.z};
    if (!view || !viewerQuery(*view,asked)) return vanilla;
    if (caller != RoomCaller::culling) { r.counters.gatedNear.fetch_add(1ull,std::memory_order_relaxed); return vanilla; }
    r.counters.viewerCalls.fetch_add(1ull,std::memory_order_relaxed);
    if (vanilla) { r.counters.vanillaTrue.fetch_add(1ull,std::memory_order_relaxed); return true; }
    const auto& bound = room->worldBound;
    const bool claim = seedClaim(caller,*view,asked,{bound.center.x,bound.center.y,bound.center.z},bound.radius,[&](Vec other) {
        const RE::NiPoint3 native{other.x,other.y,other.z};
        return r.originalRoom(room,native);
    });
    if (claim) r.counters.claims.fetch_add(1ull,std::memory_order_relaxed);
    return claim;
}
inline bool within(RE::BSMultiBoundRoom* room,Vec point) {
    const RE::NiPoint3 native{point.x,point.y,point.z};
    // Always the vanilla answer: the trace must not read our own claims back.
    return runtime->originalRoom ? runtime->originalRoom(room,native) : room->QPointWithin(native);
}
inline void publishView(Vec viewer,Vec eye) {
    auto& r = *runtime;
    if (!r.roomHook || !r.config.roomSeedMode || !finite(viewer) || !finite(eye)) {
        r.roomActive.store(false,std::memory_order_release); return;
    }
    auto next = std::make_shared<RoomView>();
    next->mode = r.config.roomSeedMode; next->viewer = viewer; next->eye = eye;
    next->radius = r.config.roomSeedRadius; next->tolerance = viewerTolerance;
    r.view.store(std::shared_ptr<const RoomView>(std::move(next)),std::memory_order_release);
    r.roomActive.store(true,std::memory_order_release);
}
inline void traceRoom(RE::TESObjectCELL* parent,Vec viewer,Vec eye) {
    auto& r = *runtime;
    if (!r.config.roomTrace || r.now < r.nextRoom) return;
    r.nextRoom = r.now+.1;
    RoomSample s;
    s.seconds = r.now; s.frame = r.frame; s.mode = r.config.roomSeedMode; s.hook = r.roomHook;
    s.active = r.roomActive.load(std::memory_order_acquire);
    s.cell = parent ? parent->GetFormID() : 0u;
    const auto loaded = parent ? parent->GetRuntimeData().loadedData : nullptr;
    const auto graph = loaded ? loaded->portalGraph.get() : nullptr;
    s.graph = graph != nullptr;
    if (graph) {
        s.rooms = graph->rooms.size();
        unsigned index{};
        for (const auto& room : graph->rooms) {
            if (index == maxRooms) { s.capped = true; break; }
            if (room) {
                if (within(room.get(),eye)) { ++s.eyeRooms; if (s.playerRoom < 0) s.playerRoom = static_cast<int>(index); }
                if (within(room.get(),viewer)) { ++s.cameraRooms; if (s.cameraRoom < 0) s.cameraRoom = static_cast<int>(index); }
            }
            ++index;
        }
    }
    s.calls = r.counters.calls.exchange(0ull,std::memory_order_relaxed);
    s.culling = r.counters.culling.exchange(0ull,std::memory_order_relaxed);
    s.attach = r.counters.attach.exchange(0ull,std::memory_order_relaxed);
    s.water = r.counters.water.exchange(0ull,std::memory_order_relaxed);
    s.other = r.counters.other.exchange(0ull,std::memory_order_relaxed);
    s.viewerCalls = r.counters.viewerCalls.exchange(0ull,std::memory_order_relaxed);
    s.gatedNear = r.counters.gatedNear.exchange(0ull,std::memory_order_relaxed);
    s.vanillaTrue = r.counters.vanillaTrue.exchange(0ull,std::memory_order_relaxed);
    s.claims = r.counters.claims.exchange(0ull,std::memory_order_relaxed);
    char line[640]{};
    if (formatRoom(line,sizeof(line),s)) emit(r.roomLines,line);
}

// ---- rig --------------------------------------------------------------------
// Main thread, eligible look call: consume snap presses, return the yaw request.
inline float yawInput(bool slide,float rawX,float playerHeading,float freeRotationX,float gain,float dt,bool home) {
    auto& r = *runtime;
    const double t = thin::seconds();
    if (home) r.turn.home(r.config.heading,t,r.config.snapSeconds);
    for (unsigned presses = (std::min)(r.snapRight.exchange(0u,std::memory_order_relaxed),4u);presses;--presses)
        r.turn.snap(1,r.config.heading,t,r.config.snapSeconds);
    for (unsigned presses = (std::min)(r.snapLeft.exchange(0u,std::memory_order_relaxed),4u);presses;--presses)
        r.turn.snap(-1,r.config.heading,t,r.config.snapSeconds);
    r.lastTarget = r.turn.target(r.config.heading,t,r.config.snapSeconds);
    return rigYawInput(slide,rawX,holdYawInput(r.lastTarget,playerHeading,freeRotationX,gain,dt));
}
// SmoothCam cast on the main thread. True = report "no hit": the caller returns
// false and leaves end/result/character exactly as SmoothCam passed them.
inline bool suppressCast(void* physics,RE::bhkWorld* world,float* start,float* end,float hull) {
    auto& r = *runtime;
    if (castDecision(r.castReady,r.frameEligible,true,true) != Cast::suppress) { ++r.passed; return false; }
    ++r.suppressed;
    if (r.config.castProbe) {
        // The native sweep still runs on private copies, as on the branch base;
        // its answer is only counted. It skips app-culled owners (5577E1-5577F4),
        // so this is "visible geometry would have hit": with the hide on it does
        // not count contacts the hide has removed.
        alignas(16) std::array<float,4> from{},to{};
        std::memcpy(from.data(),start,16); std::memcpy(to.data(),end,16);
        RE::hkpRootCdPoint ignored{}; RE::TESObjectREFR* actor{};
        if (r.caster(physics,world,from.data(),to.data(),&ignored,&actor,hull)) ++r.wouldHit;
    }
    return true;
}
// Own detour, used only when the thin-occluder detour is not installed.
inline bool hookCaster(void* physics,RE::bhkWorld* world,float* start,float* end,
    RE::hkpRootCdPoint* result,RE::TESObjectREFR** character,float hull) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto& r = *runtime;
    if (!thin::onMain() || !world || !start || !end || !result || !character)
        return r.caster(physics,world,start,end,result,character,hull);
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(caller),&memory,sizeof(memory)) || memory.AllocationBase != r.smooth)
        return r.caster(physics,world,start,end,result,character,hull);
    if (suppressCast(physics,world,start,end,hull)) return false;
    const bool hit = r.caster(physics,world,start,end,result,character,hull);
    if (hit) ++r.passedHit;
    return hit;
}
// Every eligible frame while the rig trace is on: pull-in for the previous
// frame (see pullInPair) and this frame's rendered heading against its target.
inline void measureRig(const RE::NiTransform& rendered) {
    auto& r = *runtime;
    if (pullInPair(r.previousEligible,r.previousFrame,r.frame)) {
        const auto before = r.preCollisionNow();
        r.window.pull(span(r.previousCamera,{before.x,before.y,before.z}));
    }
    r.window.heading(headingError(degrees(std::atan2(rendered.rotate.entry[0][1],rendered.rotate.entry[1][1])),degrees(r.lastTarget)));
}
inline void traceRig(bool eligible) {
    auto& r = *runtime;
    if (!r.config.rigTrace || r.now < r.nextRig) return;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!player || !player->GetParentCell() || !player->Is3DLoaded() || !camera || !camera->cameraRoot || !camera->currentState) return;
    r.nextRig = r.now+.1;
    RigSample s;
    s.seconds = r.now; s.frame = r.frame; s.eligible = eligible; s.slide = r.frameSlide;
    s.state = r.frameSeen ? static_cast<unsigned>(r.frameState) : 255u;
    s.index = r.turn.index; s.turning = r.turn.turning;
    const auto& rendered = camera->cameraRoot->world;
    const auto position = player->GetPosition();
    s.camera = {rendered.translate.x,rendered.translate.y,rendered.translate.z};
    s.player = {position.x,position.y,position.z};
    // The provider is asked only in frames it is known to own; otherwise the
    // pre-collision fields read nan and pair=0.
    constexpr float unavailable = std::numeric_limits<float>::quiet_NaN();
    s.previousCamera = s.preCollision = s.goal = {unavailable,unavailable,unavailable};
    if (eligible) {
        const auto before = r.preCollisionNow();
        const auto goal = r.goalNow();
        s.preCollision = {before.x,before.y,before.z};
        s.goal = {goal.x,goal.y,goal.z};
        s.pair = pullInPair(r.previousEligible,r.previousFrame,r.frame);
        if (s.pair) s.previousCamera = r.previousCamera;
    }
    const Vec forward{rendered.rotate.entry[0][1],rendered.rotate.entry[1][1],rendered.rotate.entry[2][1]};
    s.heading = degrees(std::atan2(forward.x,forward.y));
    s.pitch = degrees(std::atan2(-forward.z,std::hypot(forward.x,forward.y)));
    s.target = degrees(r.lastTarget);
    s.heldZoom = r.config.zoom;
    s.zoom = camera->currentState->id == RE::CameraState::kThirdPerson ?
        static_cast<RE::ThirdPersonState*>(camera->currentState.get())->targetZoomOffset : std::numeric_limits<float>::quiet_NaN();
    s.suppressed = r.suppressed; s.wouldHit = r.wouldHit; s.passed = r.passed; s.passedHit = r.passedHit;
    s.window = r.window;
    r.suppressed = r.wouldHit = r.passed = r.passedHit = 0;
    r.window = {};
    char line[768]{};
    if (formatRig(line,sizeof(line),s)) emit(r.rigLines,line);
}

// ---- frame bracket ----------------------------------------------------------
// serviceCameraCall, before the camera update: nothing is eligible until this
// frame's ThirdPersonState update says so.
inline void beginFrame() {
    auto& r = *runtime;
    if (!r.active.load(std::memory_order_acquire) || !thin::onMain()) return;
    r.frameEligible = r.frameSeen = false;
    ++r.frame;
    shader::frameStart(); // wave 2 shader: pending install, a pass left open, an off-thread call
}
// ThirdPersonState update hook entry: a second, unbracketed camera update
// (callers 746A79 and 9969F5) starts from "not eligible" too.
inline void clearFrame() {
    if (runtime->active.load(std::memory_order_acquire)) runtime->frameEligible = false;
}
// Update hook, after the native update and the policy context for this frame.
inline void observeFrame(State state,bool slide) {
    auto& r = *runtime;
    if (!r.active.load(std::memory_order_acquire) || !thin::onMain()) return;
    r.frameSeen = true; r.frameState = state; r.frameSlide = slide;
}
// Update hook, on the path that writes requested zoom: SmoothCam's casts for
// this frame follow, so this is where suppression is switched on.
inline bool holdFrame() {
    auto& r = *runtime;
    if (!r.frameSeen || !thin::onMain()) return false;
    r.frameEligible = rigEligible(r.active.load(std::memory_order_acquire),r.frameState);
    return r.frameEligible;
}
// serviceCameraCall, after the camera update (SmoothCam has written the frame).
inline void endFrame() {
    auto& r = *runtime;
    if (!r.active.load(std::memory_order_acquire)) return;
    if (!thin::onMain()) { refuse(Refusal::offMain,"camera frame service ran off the main thread; that frame was ignored"); return; }
    r.now = thin::seconds();
    const bool eligible = r.frameEligible;
    r.frameEligible = false; // a cast outside the bracket is always native
    if (const unsigned requests = r.requests.exchange(0u,std::memory_order_acq_rel)) restoreAll(firstExit(requests));
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto parent = player ? player->GetParentCell() : nullptr;
    const RE::FormID cellId = parent ? parent->GetFormID() : 0u;
    if (cellId != r.cell) { restoreAll(Exit::cell); r.cell = cellId; r.cut.reset(); r.exteriorNoted = false; }
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!eligible || !player || !parent || !camera || !camera->cameraRoot) {
        const Exit cause = static_cast<Exit>(r.causeNow(r.frameSeen ? static_cast<int>(r.frameState) : -1));
        r.silentFrames = cause == Exit::hookSilent ? r.silentFrames+1u : 0u;
        r.roomActive.store(false,std::memory_order_release);
        shader::deactivate(); shader::trace(r.now); // wave 2 shader
        observeFade(); settleFade(false); traceFade(); // base rig: the engine's player fade back to its own value
        dropSnaps();
        if (restoreNow(cause,r.silentFrames)) restoreAll(cause);
        traceRig(false);
        r.previousEligible = false;
        return;
    }
    r.silentFrames = 0;
    const auto rendered = camera->cameraRoot->world.translate;
    const auto position = player->GetPosition();
    const Vec viewer{rendered.x,rendered.y,rendered.z},eye{position.x,position.y,position.z+eyeLift};
    publishView(viewer,eye);
    if (hideScanned(r.config.seeThroughMode,parent->IsInteriorCell())) { scanHide(player,parent); traceSummary(); }
    else if (r.config.seeThroughMode == 1) { restoreAll(Exit::cell); noteExterior(cellId); }
    if (r.config.rigTrace) measureRig(camera->cameraRoot->world);
    traceRig(true);
    r.previousCamera = viewer; r.previousFrame = r.frame; r.previousEligible = true;
    traceRoom(parent,viewer,eye);
    // wave 2 shader: this frame's cut plane, from the transform SmoothCam has just written.
    const auto& facing = camera->cameraRoot->world.rotate.entry;
    shader::noteCell(parent); // wave 2 shader, fourth pass: the census of a newly entered cell (iCutStyle=1 only)
    shader::publish(viewer,{facing[0][1],facing[1][1],facing[2][1]},{position.x,position.y,position.z},parent->IsInteriorCell(),r.frame,r.now);
    shader::trace(r.now);
    observeFade(); settleFade(true); traceFade(); // base rig: no player fade for the engine's own camera point next frame
}

// ---- install ----------------------------------------------------------------
inline void installCaster(std::uintptr_t base) {
    auto& r = *runtime;
    constexpr std::array<std::uint8_t,14> prologue{0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55};
    r.smooth = GetModuleHandleW(L"SmoothCam.dll");
    if (!r.smooth) { refuse(Refusal::casterModule,"SmoothCam.dll not loaded; casts stay native (pull-in retained)"); return; }
    if (thin::runtime && thin::runtime->installed) {
        // One detour on ID 33007: the thin hook already owns it and asks us first.
        r.caster = thin::runtime->caster;
        thin::runtime->spikeCast = suppressCast;
        r.castChained = r.castReady = true;
        return;
    }
    const auto target = REL::ID(33007).address();
    if (target != base+0x5574f0) { refuse(Refusal::casterAddress,"ID 33007 is not RVA 5574F0; casts stay native (pull-in retained)"); return; }
    if (std::memcmp(reinterpret_cast<const void*>(target),prologue.data(),prologue.size()) != 0) {
        refuse(Refusal::casterPrologue,"caster prologue differs (another patch owns it); casts stay native (pull-in retained)"); return;
    }
    // 14 bytes of whole instructions with no RIP-relative operand, then an
    // absolute jump back: the gateway needs no trampoline and is never freed
    // once in use. Written while read-write, executable only after it is sealed.
    const auto gateway = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!gateway) { refuse(Refusal::casterMemory,"gateway allocation failed; casts stay native (pull-in retained)"); return; }
    std::memcpy(gateway,prologue.data(),prologue.size());
    const auto back = thin::jump(target+prologue.size());
    std::memcpy(gateway+prologue.size(),back.data(),back.size());
    DWORD previous{};
    if (!VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&previous)) {
        VirtualFree(gateway,0,MEM_RELEASE);
        refuse(Refusal::casterProtect,"gateway could not be made execute-read; casts stay native (pull-in retained)"); return;
    }
    FlushInstructionCache(GetCurrentProcess(),gateway,prologue.size()+back.size());
    r.caster = reinterpret_cast<thin::Caster>(gateway);
    const auto detour = thin::jump(reinterpret_cast<std::uintptr_t>(&hookCaster));
    REL::safe_write(target,detour.data(),detour.size());
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(target),detour.size());
    r.castReady = true;
}
// The live unwind table must describe the function exactly as the audited file does.
inline bool functionExtent(std::uintptr_t base,CodeRange rva) {
    DWORD64 image{};
    const auto entry = RtlLookupFunctionEntry(base+rva.begin,&image,nullptr);
    return entry && image == base && entry->BeginAddress == rva.begin && entry->EndAddress == rva.end;
}
// 0 = all three culling functions are where the audit found them. Otherwise
// which check failed: 1x address of function x, 2x its extent, 3x call site x.
inline unsigned cullingCallersGuard(std::uintptr_t base) {
    constexpr std::array<std::uint8_t,6> call{0xff,0x90,0xf8,0x01,0x00,0x00}; // call [rax+1F8]
    for (unsigned i = 0;i < cullingRvas.size();++i) {
        if (REL::ID(cullingIds[i]).address() != base+cullingRvas[i].begin) return 10u+i;
        if (!functionExtent(base,cullingRvas[i])) return 20u+i;
    }
    for (unsigned i = 0;i < cullingReturnRvas.size();++i)
        if (std::memcmp(reinterpret_cast<const void*>(base+cullingReturnRvas[i]-call.size()),call.data(),call.size()) != 0) return 30u+i;
    return 0;
}
inline void installRoomHook(std::uintptr_t base) {
    auto& r = *runtime;
    constexpr std::array<std::uint8_t,16> prologue{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xfa};
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_BSMultiBoundRoom[0]};
    const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
    if (table.address() != base+0x1a69ac8) { refuse(Refusal::roomTable,"BSMultiBoundRoom vtable is not RVA 1A69AC8; room seeding off, vanilla culling"); return; }
    if (slots[0x3f] != base+0xfce0c0) { refuse(Refusal::roomSlot,"vtable slot 0x3F is not RVA FCE0C0 (already hooked or wrong build); room seeding off, vanilla culling"); return; }
    if (std::memcmp(reinterpret_cast<const void*>(slots[0x3f]),prologue.data(),prologue.size()) != 0) {
        refuse(Refusal::roomPrologue,"QPointWithin prologue differs; room seeding off, vanilla culling"); return;
    }
    if (const unsigned failed = cullingCallersGuard(base)) {
        char text[192]{};
        std::snprintf(text,sizeof(text),"culling callers (IDs 13408, 106400, 106401) differ in address, extent or call site (check %u); room seeding off, vanilla culling",failed);
        refuse(Refusal::roomCallers,text); return;
    }
    // Attachment (ID 19163) and water (ID 19002) are only named in the trace;
    // if they do not match they are counted as "other" and stay vanilla anyway.
    r.callers = roomCallersAt(base,REL::ID(19163).address() == base+attachRva.begin && functionExtent(base,attachRva),
        REL::ID(19002).address() == base+waterRva.begin && functionExtent(base,waterRva));
    // Published before the slot changes: a culling thread may enter at once.
    r.originalRoom = reinterpret_cast<RoomTest>(slots[0x3f]);
    static_cast<void>(table.write_vfunc(0x3f,hookRoom));
    r.roomHook = true;
}
// kDataLoaded, after the look/update hooks and the all-state camera service.
inline void install(bool prerequisites,bool service) {
    if (!runtime) return;
    auto& r = *runtime;
    if (!prerequisites) { refuse(Refusal::prerequisites,"hash, provider or camera hook prerequisites not ready; spike inert, native behaviour"); return; }
    if (!service) { refuse(Refusal::service,"all-state camera service not installed; spike inert, native behaviour"); return; }
    const auto base = REL::Module::get().base();
    installCaster(base);
    if (r.config.roomSeedMode) installRoomHook(base);
    shader::install(base); // wave 2 shader: a no-op unless iSeeThroughMode=3
    installFade(base); // base rig: bHoldPlayerFade (FCS-15 on a failed check)
    r.active.store(true,std::memory_order_release);
    char line[256]{};
    std::snprintf(line,sizeof(line),"installed rig=1 caster=%s rooms=%s seeThrough=%u shaderCut=%s heading=%.3f zoom=%.3f snapKeys=%d,%d traces=%u,%u,%u",
        !r.castReady ? "native" : r.castChained ? "thin-chain" : "own-detour",
        !r.config.roomSeedMode ? "vanilla" : r.roomHook ? "hooked" : "refused",r.config.seeThroughMode,shader::status(),
        static_cast<double>(r.config.heading),static_cast<double>(r.config.zoom),r.config.snapLeftKey,r.config.snapRightKey,
        static_cast<unsigned>(r.config.rigTrace),static_cast<unsigned>(r.config.roomTrace),static_cast<unsigned>(r.config.hideTrace));
    note(line);
    std::snprintf(line,sizeof(line),"player fade hold=%s setting=%llu holdValue=%.1f engineValue=%.1f",
        fadeFeatureNames[static_cast<std::size_t>(r.fadeState)],static_cast<unsigned long long>(fadeSettingId),static_cast<double>(fadeHoldValue),
        r.fadeValue ? static_cast<double>(*r.fadeValue) : static_cast<double>(std::numeric_limits<float>::quiet_NaN()));
    note(line);
}
// loadSettings, only when [FixedCameraSpike] bEnabled=1.
inline void configure(const Config& requested,const shader::Config& shaderRequested,const Settings& base,int cursorKey,
    const std::filesystem::path& tracePath,void (*logger)(const char*),RE::NiPoint3 (*goalNow)(),RE::NiPoint3 (*preCollisionNow)(),
    unsigned (*causeNow)(int)) {
    runtime = new Runtime;
    auto& r = *runtime;
    r.logger = logger; r.goalNow = goalNow; r.preCollisionNow = preCollisionNow; r.causeNow = causeNow;
    bool adjusted = false;
    r.config = validate(requested,base,cursorKey,&adjusted);
    r.budget.perFrame = r.config.hidePerFrame; r.budget.total = r.config.hideTotal;
    if (adjusted) refuse(Refusal::settings,"one or more [FixedCameraSpike] values were out of range or reserved; safe values applied");
    if (r.config.snapLeftKey < 0 || r.config.snapRightKey < 0) refuse(Refusal::snapUnbound,"a snap key is unbound or reserved; that turn direction is unavailable");
    // wave 2 shader: its runtime exists only in mode 3; its rows go through the same single-producer writer.
    if (shader::shaderMode(r.config.seeThroughMode))
        shader::configure(shaderRequested,logger,[](const char* text) { emit(runtime->shaderLines,text); },
            [](const char* text) { emit(runtime->shaderLookLines,text); },[](const char* text) { emit(runtime->shaderVoidLines,text); },
            [](const char* text) { emit(runtime->shaderStyleLines,text); },[](const char* text) { emit(runtime->shaderStyle2Lines,text); },
            [](const char* text) { emit(runtime->shaderFrameLines,text); },[](const char* text) { emit(runtime->shaderFillLines,text); });
    if (r.config.rigTrace || r.config.roomTrace || r.config.hideTrace || shader::tracing()) {
        const unsigned rows = (r.config.rigTrace ? rigTraceLines : 0u)+(r.config.roomTrace ? roomTraceLines : 0u)+
            (r.config.hideTrace ? hideTraceLines : 0u)+(shader::tracing() ? shader::traceKinds()*shader::traceLines : 0u)+
            (r.config.holdFade ? fadeTraceLines : 0u);
        r.traceReady = r.writer.start(tracePath,rows);
        if (!r.traceReady) refuse(Refusal::traceWriter,"trace writer did not start; traces off, behaviour unchanged");
    }
    note("enabled: disposable P5.5-02 wave 1 spike; hooks pending data load");
}
} // namespace skycrpg::camera::fixed
