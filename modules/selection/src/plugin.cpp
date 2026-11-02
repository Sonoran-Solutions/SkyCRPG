// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#define NOMINMAX // before CommonLib's Windows headers; matches skycrpg/skeleton.h
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "selection_policy.h"
#include "skycrpg/SelectionAPI.h"
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace {
using namespace skycrpg::selection;
namespace api = skycrpg::selection::api;
constexpr REL::Version supportedRuntime{1,7,104,0};
static_assert(offsetof(RE::CrosshairPickData,target) == 4);
static_assert(offsetof(RE::CrosshairPickData,targetActor) == 8);
static_assert(offsetof(RE::CrosshairPickData,grabPickRef) == 12);
static_assert(offsetof(RE::CrosshairPickData,collisionPoint) == 16);
static_assert(sizeof(RE::CrosshairPickData) == 0x38);
static_assert(offsetof(RE::CrosshairPickData,targetCollider) == 0x20);
// Runtime base is 0x3E8 on the hash-pinned 1.7.104 executable.
static_assert(0x3e8+offsetof(RE::PlayerCharacter::PLAYER_RUNTIME_DATA,actorDoingPlayerCommand) == 0x8a4);
static_assert(0x3e8+offsetof(RE::PlayerCharacter::PLAYER_RUNTIME_DATA,grabData)+
    offsetof(RE::PlayerCharacter::GrabData,grabbedObject) == 0x8d8);
static_assert(0x3e8+offsetof(RE::PlayerCharacter::PLAYER_RUNTIME_DATA,telekinesisDistance) == 0xae4);
Settings settings;
Policy policy;
RefreshGate gate, externalGate;
std::array<FreshPress,4> keys;
std::vector<RE::ObjectRefHandle> handles;
api::Snapshot counters;
std::filesystem::path iniPath;
AccessGate accessGate;
std::atomic<bool> installed{false}, loading{true}, invalidated{true};
bool badSettings = false, diagnostics = false;
EventLog eventLog;
EventSource latchSource = EventSource::nearest;
// Only the engine's main thread may touch policy, handles, keys or counters.
// Other callbacks merely invalidate; reentrant engine/API calls stay native.
struct StateAccess {
    bool acquired = false;
    StateAccess() {
        acquired = installed.load() && accessGate.enter(GetCurrentThreadId());
    }
    ~StateAccess() { if (acquired) accessGate.leave(); }
};
unsigned diagnosticCount = 0, diagnosticLimit = 32, conflictLogs = 0;
double diagnosticTime = -5;
RE::FormID cellID = 0;
RE::Setting* activationDistance = nullptr;
RE::ObjectRefHandle nativeTarget;
Pass lastState = Pass::invalidWorld;
bool lastSneaking = false;
unsigned capLogs = 0, boundsLogs = 0;
using Pick = void(*)(RE::CrosshairPickData*,RE::bhkWorld*,const RE::NiPoint3*,const RE::NiPoint3*);
Pick previousPick = nullptr;
void log(const char* text) {
    FILE* file = nullptr;
    const auto path = skycrpg::LogDirectory()+L"SkyCRPG_Selection.log";
    if (_wfopen_s(&file,path.c_str(),L"a") == 0 && file) { std::fprintf(file,"%s\n",text); std::fclose(file); }
}
bool hashFile(const std::filesystem::path& path, const std::string& expected) {
    std::ifstream file(path,std::ios::binary);
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!file || BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0) < 0) return false;
    bool ok = BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0) >= 0;
    std::array<unsigned char,65536> buffer{};
    while (ok && file) {
        file.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(buffer.size()));
        const auto size = file.gcount();
        if (size > 0) ok = BCryptHashData(hash,buffer.data(),static_cast<ULONG>(size),0) >= 0;
    }
    std::array<unsigned char,32> digest{};
    ok = ok && file.eof() && BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (auto byte : digest) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return ok && result == expected;
}
std::string read(const char* section, const char* key) {
    char value[128]{};
    GetPrivateProfileStringA(section,key,"<missing>",value,128,iniPath.string().c_str());
    return value;
}
template<class T> T number(const char* section, const char* key, T fallback) {
    const auto value = read(section,key);
    if (value == "<missing>") return fallback;
    T result{};
    const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
    bool valid = parsed.ec == std::errc{} && parsed.ptr == value.data()+value.size();
    if constexpr (std::is_floating_point_v<T>) valid = valid && std::isfinite(result);
    if (!valid) { badSettings = true; return fallback; }
    return result;
}
bool flag(const char* section, const char* key, int fallback) {
    const int value = number(section,key,fallback);
    if (value != 0 && value != 1) { badSettings = true; return fallback == 1; }
    return value == 1;
}
void loadSettings() {
    iniPath = std::filesystem::path(skycrpg::ThisModuleDirectory())/L"SkyCRPG_Selection.ini";
    Settings s;
    s.schema = number("Selection","iSchemaVersion",1);
    s.enabled = flag("Selection","bEnabled",1);
    s.allowExternal = flag("Selection","bAllowExternalTargets",0);
    s.radius = number("Selection","fSearchRadius",0.0);
    s.refreshSeconds = number("Selection","fRefreshSeconds",.1);
    s.inspectLimit = number("Selection","iMaxReferences",4096u);
    s.rayLimit = number("Selection","iMaxScanRays",192u);
    s.visitLimit = number("Selection","iMaxVisited",65536u);
    s.boundsPadding = number("Selection","fBoundsSearchPadding",2048.0);
    s.latchSeconds = number("Selection","fLatchSeconds",10.0);
    s.moveThreshold = number("Selection","fLatchMoveUnits",64.0);
    s.nextKey = number("Controls","iCycleNextKey",26);
    s.previousKey = number("Controls","iCyclePreviousKey",27);
    s.nextGamepad = number("Controls","iCycleNextGamepad",-1);
    s.previousGamepad = number("Controls","iCyclePreviousGamepad",-1);
    settings = validate(s);
    badSettings = badSettings || s.radius != settings.radius || s.refreshSeconds != settings.refreshSeconds ||
        s.inspectLimit != settings.inspectLimit || s.rayLimit != settings.rayLimit ||
        s.visitLimit != settings.visitLimit || s.boundsPadding != settings.boundsPadding || s.latchSeconds != settings.latchSeconds ||
        s.moveThreshold != settings.moveThreshold || s.nextKey != settings.nextKey || s.previousKey != settings.previousKey ||
        s.nextGamepad != settings.nextGamepad || s.previousGamepad != settings.previousGamepad;
    diagnostics = flag("Debug","bDiagnostics",0);
    diagnosticLimit = std::min(number("Debug","iDiagnosticLimit",32u),128u);
    if (badSettings) log("Invalid INI values: safe defaults/bounds applied; invalid bindings refused.");
    if (s.schema != 1) log("Unsupported INI schema: staying native.");
    log("Keyboard [ / ] are UNAPPROVED test bindings; gamepad defaults unbound; native mappings always refuse.");
}
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
Point point(RE::NiPoint3 p) { return {p.x,p.y,p.z}; }
void selectionEvent(EventKind kind, std::uint32_t id, EventSource source, std::string_view reason,
    RE::TESObjectREFR* ref = nullptr);
EventSource selectionSource() { return policy.latched() ? latchSource : EventSource::nearest; }
void reset() {
    policy.reset(); handles.clear(); cellID = 0; gate.reset();
    counters.target = 0; counters.candidateCount = 0; counters.latched = 0;
    counters.passthrough = static_cast<std::uint32_t>(Pass::invalidWorld);
    for (auto& key : keys) key.suspend();
    if (diagnostics) selectionEvent(EventKind::choose,0,EventSource::nearest,passName(Pass::invalidWorld));
}
bool menus(RE::UI* ui) {
    if (!ui || ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsModalMenuOpen()) return true;
    for (const auto& menu : ui->menuStack) {
        if (!menu || menu->menuFlags.any(RE::UI_MENU_FLAGS::kUsesCursor,
            RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kModal)) return true;
    }
    return ui->IsMenuOpen("Console") || ui->IsMenuOpen("Loading Menu");
}
bool liveActor(RE::TESObjectREFR* ref) {
    return ref && ref->GetFormType() == RE::FormType::ActorCharacter && !ref->IsDead();
}
Context context() {
    Context c;
    c.enabled = settings.enabled; c.runtime = installed.load();
    c.conflict = GetModuleHandleW(L"BetterThirdPersonSelection.dll") != nullptr;
    c.loading = loading.load();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto ui = RE::UI::GetSingleton();
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto map = RE::ControlMap::GetSingleton();
    c.reads = player && camera && camera->currentState && ui && controls && map &&
        map->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay] && RE::CrosshairPickData::GetSingleton();
    if (!c.reads) return c;
    c.world = player->GetParentCell() && player->GetParentCell()->IsAttached() && player->Is3DLoaded() &&
        finite(point(player->GetPosition()));
    c.dead = player->IsDead();
    c.firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    c.thirdPerson = camera->currentState->id == RE::CameraState::kThirdPerson;
    c.dialogue = ui->IsMenuOpen("Dialogue Menu");
    c.menu = menus(ui);
    const auto state = player->AsActorState();
    c.armed = !state || state->GetWeaponState() != RE::WEAPON_STATE::kSheathed;
    c.combat = player->IsInCombat();
    c.sneaking = state && state->IsSneaking();
    c.nativeLiveActor = c.sneaking && liveActor(nativeTarget.get().get());
    const auto telekinesis = player->GetPlayerRuntimeData().telekinesisDistance;
    c.reads = c.reads && std::isfinite(telekinesis);
    c.telekinesis = telekinesis > 0;
    c.special = !state || player->IsInKillMove() || static_cast<bool>(player->GetPlayerRuntimeData().grabData.grabbedObject) ||
        static_cast<bool>(player->GetPlayerRuntimeData().actorDoingPlayerCommand) ||
        controls->data.povScriptMode || controls->data.povBeastMode || controls->data.fovSlideMode ||
        !map->IsActivateControlsEnabled();
    if (state) c.special = c.special || state->IsSwimming() || state->IsBleedingOut() ||
        state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal;
    if (c.thirdPerson) c.special = c.special ||
        static_cast<const RE::ThirdPersonState*>(camera->currentState.get())->stateNotActive;
    return c;
}
double nativeRadius() {
    const auto value = activationDistance;
    return value && value->GetType() == RE::Setting::Type::kFloat ? searchRadius(value->GetFloat(),settings) : 0;
}
RE::NiPointer<RE::TESObjectREFR> reference(std::uint32_t id) {
    for (const auto& handle : handles) {
        auto ref = handle.get();
        if (ref && ref->GetFormID() == id) return ref;
    }
    return {};
}
bool visibleNode(RE::NiAVObject* node) {
    // Native Pick walks the hit node's parents. A corrupt/cyclic chain refuses.
    for (unsigned depth = 0; node && depth < 128; ++depth, node = node->parent)
        if (node->GetAppCulled()) return false;
    return node == nullptr;
}
Bounds bounds(RE::TESObjectREFR* ref, bool report = true) {
    Bounds b;
    const auto root = ref->Get3D();
    if (!root) return b;
    const auto scale = root->world.scale;
    b.origin = point(root->world.translate);
    b.low = multiply(point(ref->GetBoundMin()),scale);
    b.high = multiply(point(ref->GetBoundMax()),scale);
    for (std::size_t i = 0; i < 3; ++i) b.axes[i] = {
        root->world.rotate.entry[0][i],root->world.rotate.entry[1][i],root->world.rotate.entry[2][i]};
    b.available = std::isfinite(scale) && scale > 0;
    if (!validBounds(b)) {
        b.available = false;
        if (report && boundsLogs < 8) { ++boundsLogs; log("Bound box unavailable/invalid: using reference origin for distance."); }
    }
    return b;
}
Candidate candidate(RE::TESObjectREFR* ref, RE::PlayerCharacter* player) {
    Candidate c;
    if (!ref) { c.loaded = false; return c; }
    c.id = ref->GetFormID(); c.position = point(ref->GetPosition());
    c.player = ref == player; c.disabled = ref->IsDisabled(); c.deleted = ref->IsDeleted();
    const auto root = ref->Get3D();
    c.loaded = root && ref->GetParentCell() && ref->GetParentCell()->IsAttached();
    c.liveActor = liveActor(ref);
    c.activatable = false;
    if (c.player || c.disabled || c.deleted || !c.loaded || !ref->GetBaseObject() || !visibleNode(root)) return c;
    if ((ref->GetFormFlags() & 0x800000u) && ref->GetFormType() != RE::FormType::ActorCharacter) return c;
    c.bounds = bounds(ref);
    using CanPick = bool(*)(RE::CrosshairPickData*,RE::TESObjectREFR*);
    static REL::Relocation<CanPick> canPick{REL::ID(26130)};
    if (!canPick(RE::CrosshairPickData::GetSingleton(),ref)) return c;
    float filtered = 0;
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kFilterActivation,player,ref,&filtered);
    if (filtered != 0) return c;
    // The prompt supplies R9B=true; CommonLib's declaration omits that argument.
    using ActivateText = bool(*)(RE::TESBoundObject*,RE::TESObjectREFR*,RE::BSString*,bool);
    const auto base = ref->GetBaseObject();
    const auto vtable = *reinterpret_cast<const std::uintptr_t* const*>(base);
    RE::BSString text;
    if (!reinterpret_cast<ActivateText>(vtable[0x4c])(base,ref,&text,true) || text.empty()) return c;
    c.activatable = true;
    return c;
}
bool bodyLOS(RE::TESObjectREFR* ref, RE::PlayerCharacter* player, ScanBudget& rays) {
    const auto cell = player->GetParentCell();
    const auto world = cell ? cell->GetbhkWorld() : nullptr;
    const RE::NiPointer<RE::NiAVObject> root{ref->Get3D()};
    const auto controller = player->GetCharController();
    if (!world || !root || !controller || !visibleNode(root.get())) return false;
    const auto low = player->GetBoundMin(), high = player->GetBoundMax();
    auto origin = player->GetPosition();
    origin.z += (high.z+low.z)*.5f;
    const auto targetLow = ref->GetBoundMin(), targetHigh = ref->GetBoundMax();
    const float height = targetHigh.z-targetLow.z;
    const float scale = RE::bhkWorld::GetWorldScale();
    if (!finite(point(origin)) || !std::isfinite(height) || height < 0 || !std::isfinite(scale) || scale <= 0) return false;
    RE::CFilter filter{};
    controller->GetCollisionFilterInfo(filter);
    const bool needsCollision = ref->GetBaseObject()->GetFormType() == RE::FormType::Activator;
    // Native target height fractions, but the source is the body, not camera.
    // For activators require an actual visible collision hit; a miss cannot
    // admit a script marker with no pick collision.
    for (const float fraction : {.75f,.5f,.25f}) {
        if (!rays.inspect()) return false;
        auto target = ref->GetPosition(); target.z += height*fraction;
        if (!finite(point(target))) return false;
        RE::bhkPickData ray{};
        ray.rayInput.from = RE::hkVector4(origin.x*scale,origin.y*scale,origin.z*scale,0);
        ray.rayInput.to = RE::hkVector4(target.x*scale,target.y*scale,target.z*scale,0);
        ray.rayInput.filterInfo.filter = (filter.filter & 0xffff0000u) | 41u;
        ray.rayInput.enableShapeCollectionFilter = false;
        ray.rayOutput.Reset();
        ++counters.raysTotal;
        RE::BSReadLockGuard lock(world->worldLock);
        if (!world->GetWorld1()) return false;
        world->PickObject(ray);
        if (ray.pickFailed) return false;
        const auto hit = ray.rayOutput.rootCollidable;
        if (!hit) { if (!needsCollision) return true; }
        else if (RE::TESHavokUtilities::FindCollidableRef(*hit) == ref) {
            const auto node = RE::TESHavokUtilities::FindCollidableObject(*hit);
            if (node && visibleNode(node)) return true;
        }
    }
    return false;
}
bool collect(RE::PlayerCharacter* player, double radius, ScanBudget& budget, ScanBudget& visits) {
    const auto tes = RE::TES::GetSingleton();
    if (!tes) return false;
    handles.clear();
    // The native pick is considered even outside the bounded origin horizon.
    if (nativeTarget && budget.inspect()) handles.push_back(nativeTarget);
    const auto position = point(player->GetPosition());
    auto scan = [&](RE::TESObjectCELL* cell) {
        if (!cell || !cell->IsAttached() || !budget.complete || !visits.complete) return;
        cell->ForEachReference([&](RE::TESObjectREFR* ref) {
            if (!visits.inspect()) return RE::BSContainer::ForEachResult::kStop;
            if (ref == player || !inBroadPhase(position,point(ref->GetPosition()),radius,settings.boundsPadding))
                return RE::BSContainer::ForEachResult::kContinue;
            if (!budget.inspect()) return RE::BSContainer::ForEachResult::kStop;
            const auto handle = ref->CreateRefHandle();
            if (handle && handle != nativeTarget) handles.push_back(handle);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    };
    if (tes->interiorCell) scan(tes->interiorCell);
    else {
        const auto grid = tes->gridCells;
        if (!grid || !grid->cells || grid->length == 0 || grid->length > 17) return false;
        for (std::uint32_t x = 0; x < grid->length && budget.complete && visits.complete; ++x)
            for (std::uint32_t y = 0; y < grid->length && budget.complete && visits.complete; ++y) {
                const auto cell = grid->GetCell(x,y);
                if (!cell || !cell->IsAttached()) continue;
                const auto coords = cell->GetCoordinates();
                if (!coords) return false;
                const double left = static_cast<double>(coords->cellX)*4096;
                const double bottom = static_cast<double>(coords->cellY)*4096;
                const Point closest{std::clamp(position.x,left,left+4096),
                    std::clamp(position.y,bottom,bottom+4096),position.z};
                if (inBroadPhase(position,closest,radius,settings.boundsPadding)) scan(cell);
            }
    }
    const auto ws = tes->GetRuntimeData2().worldSpace;
    if (ws && budget.complete && visits.complete) scan(ws->GetSkyCell());
    return budget.complete && visits.complete;
}
void selectionEvent(EventKind kind, std::uint32_t id, EventSource source, std::string_view reason,
    RE::TESObjectREFR* ref) {
    if (!diagnostics) return;
    const auto emission = eventLog.observe(kind,id,source,reason);
    if (emission == EventEmission::none) return;
    const auto now = std::chrono::steady_clock::now();
    static const auto start = now;
    const auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count());
    SelectionEvent event;
    if (emission == EventEmission::row) {
        event.kind = kind; event.id = id; event.source = source; event.reason = reason;
        const auto native = nativeTarget.get();
        event.nativeID = native ? native->GetFormID() : 0;
        const auto selected = ref ? RE::NiPointer<RE::TESObjectREFR>{} : reference(id);
        if (!ref) ref = selected.get();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (ref && player) {
            Candidate value;
            value.position = point(ref->GetPosition()); value.bounds = bounds(ref,false);
            event.bounds = validBounds(value.bounds);
            event.distance = std::sqrt(surfaceDistanceSquared(value,point(player->GetPosition())));
        }
        const auto order = policy.order();
        event.cycleCount = order.size();
        const auto it = std::find(order.begin(),order.end(),id);
        if (it != order.end()) event.cycleIndex = static_cast<std::int64_t>(it-order.begin());
    }
    log(formatEvent(event,emission,eventLog.sequence(),ms).c_str());
}
void publish() {
    counters.target = policy.current(); counters.latched = policy.latched() ? 1u : 0u;
    counters.candidateCount = static_cast<std::uint32_t>(policy.order().size());
    counters.passthrough = static_cast<std::uint32_t>(policy.reason());
    if (diagnostics) selectionEvent(EventKind::choose,policy.current(),selectionSource(),passName(policy.reason()));
}
void diagnostic(double now) {
    if (!diagnostics || diagnosticCount >= diagnosticLimit || now-diagnosticTime < 5) return;
    diagnosticTime = now; ++diagnosticCount;
    char line[384]{};
    std::snprintf(line,sizeof(line),"selection target=%08X candidates=%u latch=%u pass=%u radius=%.2f inspected=%u scans=%llu capped=%u rays=%llu cost_us=%llu visits=%u scan_rays=%u picks=%llu",
        counters.target,counters.candidateCount,counters.latched,counters.passthrough,counters.radius,counters.inspectedLast,
        static_cast<unsigned long long>(counters.refreshes),counters.cappedScans,
        static_cast<unsigned long long>(counters.raysTotal),static_cast<unsigned long long>(counters.lastCostMicros),counters.visitedLast,counters.scanRaysLast,
        static_cast<unsigned long long>(counters.pickChecks));
    log(line);
}
bool refresh(double now) {
    if (invalidated.exchange(false)) reset();
    const auto c = context();
    const auto state = passthrough(c);
    if (state != lastState || c.sneaking != lastSneaking) gate.reset();
    lastState = state; lastSneaking = c.sneaking;
    if (state != Pass::none) {
        policy.refresh(c,{}, {},0,now,settings); handles.clear();
        for (auto& key : keys) key.suspend();
        publish(); return false;
    }
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto cell = player->GetParentCell()->GetFormID();
    if (cellID != cell) { gate.reset(); cellID = cell; }
    if (!gate.due(now,settings.refreshSeconds)) return policy.reason() == Pass::none;
    gate.last = now;
    const auto start = std::chrono::steady_clock::now();
    const double radius = nativeRadius();
    ScanBudget budget{settings.inspectLimit}, visits{settings.visitLimit}, rays{settings.rayLimit};
    bool complete = radius > 0 && collect(player,radius,budget,visits);
    std::vector<Candidate> candidates;
    if (complete) {
        for (const auto& handle : handles) {
            const auto ref = handle.get();
            auto value = candidate(ref.get(),player);
            if (!rankable(value,c.sneaking) || !eligible(value,point(player->GetPosition()),radius)) continue;
            value.lineOfSight = bodyLOS(ref.get(),player,rays);
            if (!rays.complete) { complete = false; break; }
            candidates.push_back(value);
        }
    }
    auto checked = c;
    if (invalidated.load()) { reset(); return false; }
    if (radius <= 0) checked.reads = false;
    policy.refresh(checked,candidates,point(player->GetPosition()),radius,now,settings,complete);
    ++counters.refreshes; counters.inspectedLast = budget.inspected; counters.inspectedTotal += budget.inspected;
    counters.visitedLast = visits.inspected; counters.scanRaysLast = rays.inspected;
    if (!complete) {
        ++counters.cappedScans;
        if (capLogs < 8) { ++capLogs; log("Scan incomplete: visit/reference/ray limit or world read; native pick retained."); }
    }
    counters.radius = radius;
    counters.lastCostMicros = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-start).count());
    publish(); diagnostic(now);
    return policy.reason() == Pass::none;
}
void pick(RE::CrosshairPickData* data, RE::bhkWorld* world, const RE::NiPoint3* origin, const RE::NiPoint3* direction) {
    StateAccess access;
    previousPick(data,world,origin,direction);
    if (!access.acquired || !data) return;
    struct PickCost {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        ~PickCost() {
            ++counters.pickChecks;
            counters.lastPickCostMicros = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-start).count());
        }
    } cost;
    nativeTarget = data->target;
    const auto nativeRef = nativeTarget.get();
    const auto nativeDecision = [&](std::string_view reason) {
        if (diagnostics) selectionEvent(EventKind::pick,nativeRef ? nativeRef->GetFormID() : 0,
            EventSource::native,reason,nativeRef.get());
    };
    if (!refresh(seconds())) {
        if (diagnostics) nativeDecision(passName(static_cast<Pass>(counters.passthrough)));
        return;
    }
    // A changed native pick between scans must not disappear from selection.
    if (nativeRef) {
        const auto order = policy.order();
        if (std::find(order.begin(),order.end(),nativeRef->GetFormID()) == order.end()) {
            if (diagnostics) nativeDecision("native_not_in_scan");
            return;
        }
    }
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto target = reference(policy.current());
    ScanBudget rays{3};
    // Revalidate the selected reference and body LOS at the actual pick, so a
    // moving obstruction cannot permit activation using a stale scan's LOS.
    if (!target || !eligible(candidate(target.get(),player),point(player->GetPosition()),nativeRadius()) ||
        !bodyLOS(target.get(),player,rays)) {
        reset();
        if (diagnostics) nativeDecision("target_validation");
        return;
    }
    const auto handle = target->CreateRefHandle();
    if (!handle) {
        reset();
        if (diagnostics) nativeDecision("invalid_handle");
        return;
    }
    using CanGrab = bool(*)(RE::TESObjectREFR*);
    static REL::Relocation<CanGrab> canGrab{REL::ID(34614)};
    const bool grabbable = canGrab(target.get());
    const bool stale = invalidated.load();
    const auto state = stale ? Pass::invalidWorld : passthrough(context());
    if (stale || state != Pass::none) {
        reset();
        if (diagnostics) nativeDecision(stale ? "invalidated" : passName(state));
        return;
    }
    const auto change = pickChanges(data->target != handle,grabbable);
    if (change.target) {
        data->target = handle;
        data->grabPickRef = change.grab ? handle : RE::ObjectRefHandle{};
    }
    if (change.clearCollider) data->targetCollider = nullptr;
    // targetActor and collisionPoint remain native (aim magnetism / SmoothCam).
    if (diagnostics) selectionEvent(EventKind::pick,target->GetFormID(),selectionSource(),"none",target.get());
}
api::Result apiReady() {
    if (!installed.load() || !accessGate.thread()) return api::Result::unavailable;
    if (GetCurrentThreadId() != accessGate.thread()) return api::Result::wrongThread;
    return api::Result::ok;
}
api::Result snapshot(api::Snapshot* out) {
    const auto status = apiReady();
    if (status != api::Result::ok) return status;
    StateAccess access;
    if (!access.acquired) return api::Result::unavailable;
    if (!out || out->size != sizeof(api::Snapshot)) return api::Result::invalidArgument;
    refresh(seconds());
    *out = counters;
    return api::Result::ok;
}
api::Result copyCandidates(std::uint32_t* ids, std::uint32_t capacity, std::uint32_t* count) {
    const auto status = apiReady();
    if (status != api::Result::ok) return status;
    StateAccess access;
    if (!access.acquired) return api::Result::unavailable;
    if (!count || (!ids && capacity)) return api::Result::invalidArgument;
    refresh(seconds());
    const auto order = policy.order();
    *count = static_cast<std::uint32_t>(order.size());
    for (std::uint32_t i = 0; i < capacity && i < *count; ++i) ids[i] = order[i];
    return api::Result::ok;
}
api::Result setTarget(std::uint32_t id) {
    const auto status = apiReady();
    if (status != api::Result::ok) return status;
    StateAccess access;
    if (!access.acquired) return api::Result::unavailable;
    if (!settings.allowExternal) return api::Result::disabled;
    const double now = seconds();
    // Additional API validation is independently limited to the refresh rate.
    if (!externalGate.due(now,settings.refreshSeconds)) return api::Result::unavailable;
    externalGate.last = now;
    if (!refresh(now)) return api::Result::unavailable;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto ref = reference(id);
    ScanBudget rays{3};
    if (!ref || !eligible(candidate(ref.get(),player),point(player->GetPosition()),nativeRadius()) ||
        !bodyLOS(ref.get(),player,rays)) return api::Result::invalidTarget;
    if (invalidated.load() || passthrough(context()) != Pass::none) { reset(); return api::Result::unavailable; }
    if (!policy.push(id,point(player->GetPosition()),now,true)) return api::Result::invalidTarget;
    if (diagnostics) latchSource = EventSource::external;
    publish(); return api::Result::ok;
}
api::Result clearTarget() {
    const auto status = apiReady();
    if (status != api::Result::ok) return status;
    StateAccess access;
    if (!access.acquired) return api::Result::unavailable;
    if (!settings.allowExternal) return api::Result::disabled;
    if (!refresh(seconds())) return api::Result::unavailable;
    policy.release(); publish(); return api::Result::ok;
}
const api::Interface interfaceV1{sizeof(api::Interface),api::version,snapshot,copyCandidates,setTarget,clearTarget};
class Events final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* input,RE::BSTEventSource<RE::InputEvent*>*) override {
        StateAccess access;
        if (!input || !access.acquired) return RE::BSEventNotifyControl::kContinue;
        const double now = seconds();
        const bool safe = refresh(now);
        for (auto event = *input; event; event = event->next) {
            const auto button = event->AsButtonEvent();
            if (!button) continue;
            const auto device = event->GetDevice();
            const bool pad = device == RE::INPUT_DEVICE::kGamepad;
            if (!pad && device != RE::INPUT_DEVICE::kKeyboard) continue;
            const std::array<int,2> bindings{pad ? settings.nextGamepad : settings.nextKey,
                pad ? settings.previousGamepad : settings.previousKey};
            for (std::size_t i = 0; i < bindings.size(); ++i) {
                if (bindings[i] < 0 || static_cast<int>(button->GetIDCode()) != bindings[i]) continue;
                const auto map = RE::ControlMap::GetSingleton();
                const bool mapped = !map || !map->GetUserEventName(button->GetIDCode(),device).empty() || !button->QUserEvent().empty();
                if (mapped && button->IsDown() && conflictLogs < 8) { ++conflictLogs; log("Cycle binding refused: mapped native event or unreadable ControlMap."); }
                if (keys[(pad ? 2u : 0u)+i].update(button->IsPressed(),button->IsDown(),safe && !mapped)) {
                    const auto player = RE::PlayerCharacter::GetSingleton();
                    const bool accepted = policy.cycle(i == 0 ? 1 : -1,point(player->GetPosition()),now);
                    if (diagnostics && accepted) latchSource = EventSource::cycle;
                    publish();
                    if (diagnostics && accepted) selectionEvent(EventKind::cycle,policy.current(),selectionSource(),
                        i == 0 ? "next" : "previous");
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
Events events;
void install() {
    static bool attempted = false;
    if (attempted) return;
    attempted = true;
    if (!settings.enabled) { log("Selection disabled/schema unsupported: no hook installed."); return; }
    if (GetModuleHandleW(L"BetterThirdPersonSelection.dll")) {
        log("REFUSED pick hook: BetterThirdPersonSelection.dll is loaded; only one pick owner is allowed."); return;
    }
    const REL::Relocation<std::uintptr_t> native{REL::ID(26127)}, caller{REL::ID(40620)};
    const auto call = caller.address()+0x1e7;
    constexpr std::array<unsigned char,9> callBytes{0xe8,0xe4,0x26,0xcb,0xff,0x41,0x8b,0x46,0x04};
    std::int32_t originalDisplacement = 0;
    std::memcpy(&originalDisplacement,reinterpret_cast<const void*>(call+1),sizeof(originalDisplacement));
    const auto resolvesTo = static_cast<std::uintptr_t>(static_cast<std::int64_t>(call+5)+originalDisplacement);
    if (native.offset() != 0x409fd0 || caller.offset() != 0x757700 || resolvesTo != native.address() ||
        std::memcmp(reinterpret_cast<const void*>(call),callBytes.data(),callBytes.size()) != 0) {
        log("REFUSED pick hook: call-site bytes/rel32 chain mismatch; staying native."); return;
    }
    log("Chain check PASS: 40620+0x1E7 rel32 -> 26127; Pick entry unexamined; SmoothCam entry/interior patches remain in chain.");
    activationDistance = RE::GetINISetting("fActivatePickLength:Interface");
    const auto main = RE::Main::GetSingleton();
    const auto input = RE::BSInputDeviceManager::GetSingleton();
    const auto ui = RE::UI::GetSingleton();
    if (!main || !main->threadID || !input || !ui || !activationDistance) {
        log("REFUSED hook: required main thread, input source or native activation setting unavailable."); return;
    }
    // Reserve explicitly: CommonLib's allocation fallback can terminate the
    // process. Failure here must leave the original five-byte CALL intact.
    const auto allocator = SKSE::GetTrampolineInterface();
    auto stub = allocator ? allocator->AllocateFromBranchPool(14) : nullptr;
    if (!stub) { log("REFUSED hook: SKSE branch pool unavailable."); return; }
    const auto displacement = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(stub)) -
        static_cast<std::int64_t>(call+5);
    if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()) {
        log("REFUSED hook: branch pool outside rel32 range."); return;
    }
    std::array<unsigned char,14> jump{0xff,0x25,0,0,0,0};
    const auto destination = reinterpret_cast<std::uintptr_t>(pick);
    std::memcpy(jump.data()+6,&destination,sizeof(destination));
    DWORD oldStub = 0;
    if (!VirtualProtect(stub,jump.size(),PAGE_EXECUTE_READWRITE,&oldStub)) {
        log("REFUSED hook: trampoline protection failed."); return;
    }
    std::memcpy(stub,jump.data(),jump.size());
    DWORD ignored = 0;
    const bool stubFlushed = FlushInstructionCache(GetCurrentProcess(),stub,jump.size()) != 0;
    const bool stubProtected = VirtualProtect(stub,jump.size(),oldStub,&ignored) != 0;
    if (!stubFlushed || !stubProtected) { log("REFUSED hook: trampoline finalization failed."); return; }
    std::array<unsigned char,5> replacement{0xe8};
    const auto rel32 = static_cast<std::int32_t>(displacement);
    std::memcpy(replacement.data()+1,&rel32,sizeof(rel32));
    if (GetModuleHandleW(L"BetterThirdPersonSelection.dll") ||
        std::memcmp(reinterpret_cast<const void*>(call),callBytes.data(),callBytes.size()) != 0) {
        log("REFUSED hook: pick ownership changed before patch."); return;
    }
    DWORD oldCall = 0;
    auto site = reinterpret_cast<void*>(call);
    if (!VirtualProtect(site,replacement.size(),PAGE_EXECUTE_READWRITE,&oldCall)) {
        log("REFUSED hook: call-site protection failed."); return;
    }
    previousPick = reinterpret_cast<Pick>(native.address());
    accessGate.bind(main->threadID);
    std::memcpy(site,replacement.data(),replacement.size());
    const bool callFlushed = FlushInstructionCache(GetCurrentProcess(),site,replacement.size()) != 0;
    const bool callProtected = VirtualProtect(site,replacement.size(),oldCall,&ignored) != 0;
    if (!callFlushed || !callProtected || std::memcmp(site,replacement.data(),replacement.size()) != 0) {
        // The wrapper is fully initialized and forwards native with installed=false.
        log("Hook finalization failed: selection inert; any installed wrapper forwards native only."); return;
    }
    input->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&events));
    installed.store(true);
    log("Pick call installed at AE 40620+0x1E7 -> 26127; native first, activation/grab handles and stale collider only; no camera writes.");
}
void messages(SKSE::MessagingInterface::Message* message) {
    if (!message) return;
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded: install(); break;
    case SKSE::MessagingInterface::kPreLoadGame: loading.store(true); invalidated.store(true); break;
    case SKSE::MessagingInterface::kNewGame: loading.store(false); invalidated.store(true); break;
    case SKSE::MessagingInterface::kPostLoadGame:
        loading.store(!message->data); invalidated.store(true); break;
    default: break;
    }
}
}
extern "C" __declspec(dllexport) const skycrpg::selection::api::Interface* SkyCRPGSelection_GetAPI(std::uint32_t version) {
    return version == api::version ? &interfaceV1 : nullptr;
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data;
    data.PluginVersion({SKYCRPG_VERSION_MAJOR,SKYCRPG_VERSION_MINOR,SKYCRPG_VERSION_PATCH,0});
    data.PluginName("SkyCRPG Selection"); data.AuthorName("devils_17");
    data.UsesAddressLibrary(); data.UsesUpdatedStructs();
    data.versionIndependenceEx |= SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5;
    data.CompatibleVersions({supportedRuntime}); data.xseMinimum = REL::Version{2,3,1,0}.pack();
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (!skse || skse->IsEditor() || skse->RuntimeVersion() != supportedRuntime || skse->SKSEVersion() != REL::Version{2,3,1,0}.pack()) return false;
    std::ofstream(std::filesystem::path(skycrpg::LogDirectory())/L"SkyCRPG_Selection.log",std::ios::trunc).close();
    log("SkyCRPG Selection " SKYCRPG_VERSION " source " SKYCRPG_SOURCE_COMMIT "; original MIT, no Camera/UI/provider dependency.");
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768) ||
        !hashFile(std::filesystem::path(executable),"846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f") ||
        !hashFile(std::filesystem::path(skycrpg::ThisModuleDirectory())/L"versionlib-1-7-104-0.bin","8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842")) {
        log("Executable/Address Library hash mismatch: inert before CommonLib address initialization."); return true;
    }
    SKSE::Init(skse,false); loadSettings();
    const auto messaging = SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(messages);
}
