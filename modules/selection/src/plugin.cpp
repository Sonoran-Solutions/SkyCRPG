// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#define NOMINMAX // before CommonLib's Windows headers; matches skycrpg/skeleton.h
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "selection_policy.h"
#include "cursor_override.h"
#include "skycrpg/SelectionAPI.h"
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace {
using namespace skycrpg::selection;
namespace api = skycrpg::selection::api;
constexpr REL::Version supportedRuntime{1,7,104,0};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(sizeof(RE::BSSpinLock) == 8 && sizeof(RE::BSReadWriteLock) == 8);
static_assert(offsetof(RE::TESBoundObject,boundData) == 0x20);
static_assert(offsetof(RE::TESWorldSpace,skyCell) == 0xf8);
static_assert(static_cast<unsigned>(RE::COL_LAYER::kCustomPick2) == 44);
static_assert(static_cast<unsigned>(RE::COL_LAYER::kLOS) == 41);
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
CursorOverride cursorOverride;
std::atomic<bool> cursorRelease{false};
CursorExchange<api::CursorFrame> publication;
CursorExchange<CursorRequest> cursorRequests;
// Candidates the cursor client can show. Pick keeps its last complete copy, so
// a contended read never lets a cycle reach a hidden candidate.
struct CursorVisible { std::uint32_t count = 0; std::uint32_t ids[16]{}; bool reported = false; };
CursorExchange<CursorVisible> cursorVisible;
CursorVisible visibleSeen;
std::atomic<bool> visibleReset{false};
std::atomic<std::uint64_t> visibleWrites{0};
std::atomic<bool> sessionViable{false};
bool activationGuardReady = false;
std::atomic<std::uint64_t> cursorPickPermit{0};
std::atomic<RE::RefHandle> cursorPickHandle{0};
std::uint64_t completedRefreshes = 0, aliveRefreshes = 0, cursorReplies = 0;
std::array<FreshPress,4> keys;
struct RefIdentity { std::uint32_t id; RE::ObjectRefHandle handle; };
std::vector<RefIdentity> identities;
std::vector<RE::NiPointer<RE::TESObjectREFR>> discovered;
api::Snapshot counters;
std::filesystem::path iniPath;
AccessGate accessGate, inputGate, logGate, flushGate, cursorGate;
std::uint64_t worldContentions = 0, worldRetries = 0, cellContentions = 0, discardedEdges = 0, cycleRefusals = 0;
std::atomic<std::uint64_t> refusedPresses{0};
bool scanContended = false;
CycleRequests cycleRequests;
std::atomic<std::uint64_t> externalRequest{0}; // bit32 marks a request; low32=0 releases
std::atomic<std::uint32_t> menuState{0}; // known / blocking / dialogue
std::atomic<bool> inputAllowed{false}, conflictPosted{false};
std::atomic<std::uint64_t> requestDrops{0};
std::vector<std::uint32_t> scannedIDs;
bool scanReady = false, cursorListReady = false;
std::string_view scanReason = "no_scan";
std::uint32_t mainThreadID = 0;
std::atomic<bool> installed{false}, loading{true}, invalidated{true};
bool badSettings = false, diagnostics = false;
EventLog eventLog;
EventSource latchSource = EventSource::nearest;
// Pick alone mutates policy/handles/counters. API readers use a separate
// publication gate; input owns keys and posts requests, menus post state only.
struct StateAccess {
    bool acquired = false;
    StateAccess() {
        acquired = installed.load() && accessGate.enter();
    }
    ~StateAccess() { if (acquired) accessGate.leave(); }
};
struct CursorAccess {
    bool acquired = cursorGate.enter();
    ~CursorAccess() { if (acquired) cursorGate.leave(); }
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
std::array<std::array<char,768>,256> logRows{};
std::size_t logCount = 0;
std::atomic<std::uint64_t> logDrops{0};
// Process-lifetime path: the nonjoining flush worker must not race its destructor.
std::filesystem::path& logPath = *new std::filesystem::path;
HANDLE flushStop = nullptr;
void log(const char* text) {
    if (!logGate.enter()) { ++logDrops; return; }
    if (logCount < logRows.size()) {
        std::snprintf(logRows[logCount].data(),logRows[logCount].size(),"%s",text);
        ++logCount;
    } else ++logDrops;
    logGate.leave();
}
// Never called by Pick or its API; the diagnostic worker has no engine calls.
void flushLog() {
    if (!flushGate.enter()) return;
    static std::array<std::array<char,768>,256> rows{};
    std::size_t count = 0;
    if (logGate.enter()) {
        count = logCount;
        std::copy_n(logRows.begin(),count,rows.begin()); logCount = 0;
        logGate.leave();
    }
    const auto dropped = logDrops.exchange(0);
    if (count || dropped) {
        FILE* file = nullptr;
        if (_wfopen_s(&file,logPath.c_str(),L"a") == 0 && file) {
            for (std::size_t i = 0; i < count; ++i) std::fprintf(file,"%s\n",rows[i].data());
            if (dropped) std::fprintf(file,"selection log_queue_suppressed=%llu\n",static_cast<unsigned long long>(dropped));
            std::fclose(file);
        } else logDrops.fetch_add(dropped+count);
    }
    flushGate.leave();
}
DWORD WINAPI flushWorker(void*) {
    while (WaitForSingleObject(flushStop,250) == WAIT_TIMEOUT) flushLog();
    return 0;
}
void stopFlushWorker() { if (flushStop) SetEvent(flushStop); }
void startFlushWorker() {
    if (!diagnostics) return;
    flushStop = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!flushStop) { log("Diagnostic flush worker unavailable: event creation failed."); return; }
    if (std::atexit(stopFlushWorker) != 0) {
        CloseHandle(flushStop); flushStop = nullptr;
        log("Diagnostic flush worker unavailable: exit registration failed."); return;
    }
    const auto thread = CreateThread(nullptr,0,flushWorker,nullptr,0,nullptr);
    if (thread) CloseHandle(thread);
    else log("Diagnostic flush worker unavailable: thread creation failed.");
    // No join under process teardown. Windows reclaims the stop event and ends
    // remaining threads on process exit; live DLL unload is unsupported.
}
void threadLog(bool pickThread) {
    static std::atomic<bool> picked{false}, inputSeen{false};
    auto& seen = pickThread ? picked : inputSeen;
    if (seen.exchange(true)) return;
    char line[128]{};
    std::snprintf(line,sizeof(line),"selection first_%s current_thread=%lu main_thread=%u",
        pickThread ? "pick" : "input",GetCurrentThreadId(),mainThreadID);
    log(line);
}
void forwardReason(std::string_view reason) {
    static AccessGate reasonGate;
    static std::array<std::string_view,64> seen{};
    static std::size_t count = 0;
    if (!reasonGate.enter()) return;
    if (std::find(seen.begin(),seen.begin()+count,reason) == seen.begin()+count && count < seen.size()) {
        seen[count++] = reason;
        char line[160]{};
        std::snprintf(line,sizeof(line),"selection forward reason=%.*s current_thread=%lu",
            static_cast<int>(reason.size()),reason.data(),GetCurrentThreadId());
        log(line);
    }
    reasonGate.leave();
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
    s.rayLimit = number("Selection","iMaxScanRays",48u);
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
    diagnosticLimit = (std::min)(number("Debug","iDiagnosticLimit",32u),128u);
    if (badSettings) log("Invalid INI values: safe defaults/bounds applied; invalid bindings refused.");
    if (s.schema != 1) log("Unsupported INI schema: staying native.");
    log("Keyboard [ / ] are UNAPPROVED test bindings; gamepad defaults unbound; native mappings always refuse.");
}
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
Point point(RE::NiPoint3 p) { return {p.x,p.y,p.z}; }
void selectionEvent(EventKind kind, std::uint32_t id, EventSource source, std::string_view reason,
    RE::TESObjectREFR* ref = nullptr);
EventSource selectionSource() { return policy.latched() ? latchSource : EventSource::nearest; }
void publish(bool resetState = false);
void reset() {
    cursorOverride.end(); cursorRelease.store(false); sessionViable.store(false);
    policy.reset(); identities.clear(); discovered.clear(); scannedIDs.clear(); cellID = 0; gate.reset();
    scanReady = false; cursorListReady = false; inputAllowed.store(false); discardedEdges += CycleRequests::count(cycleRequests.take()); externalRequest.store(0);
    counters.target = 0; counters.candidateCount = 0; counters.latched = 0; counters.available = 0;
    counters.externalResult = static_cast<std::uint32_t>(api::Result::unavailable);
    counters.passthrough = static_cast<std::uint32_t>(Pass::invalidWorld);
    if (diagnostics) selectionEvent(EventKind::choose,0,EventSource::nearest,passName(Pass::invalidWorld));
    publish(true);
}
bool menus(RE::UI* ui) {
    if (!ui || ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsModalMenuOpen()) return true;
    for (const auto& menu : ui->menuStack) {
        if (!menu || menu->menuFlags.any(RE::UI_MENU_FLAGS::kUsesCursor,
            RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kModal)) return true;
    }
    return ui->IsMenuOpen("Console") || ui->IsMenuOpen("Loading Menu");
}
void updateMenus() {
    conflictPosted.store(GetModuleHandleW(L"BetterThirdPersonSelection.dll") != nullptr);
    const auto ui = RE::UI::GetSingleton();
    const std::uint32_t next = ui ? 1u | (menus(ui) ? 2u : 0u) | (ui->IsMenuOpen("Dialogue Menu") ? 4u : 0u) : 0u;
    const auto old = menuState.exchange(next);
    if (old != next && (next == 0 || (next & 6u))) {
        inputAllowed.store(false); invalidated.store(true);
    }
}
bool liveActor(RE::TESObjectREFR* ref) {
    return ref && ref->GetFormType() == RE::FormType::ActorCharacter && !ref->IsDead();
}
Context context() {
    Context c;
    c.enabled = settings.enabled; c.runtime = installed.load();
    c.conflict = conflictPosted.load();
    c.loading = loading.load();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto menusSnapshot = menuState.load();
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto map = RE::ControlMap::GetSingleton();
    c.reads = player && camera && camera->currentState && (menusSnapshot & 1u) && controls && map &&
        map->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay] && RE::CrosshairPickData::GetSingleton();
    if (!c.reads) return c;
    const auto parent = player->GetParentCell();
    c.world = parent && parent->IsAttached() && player->Is3DLoaded() &&
        finite(point(player->GetPosition()));
    c.dead = player->IsDead();
    c.firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    c.thirdPerson = camera->currentState->id == RE::CameraState::kThirdPerson;
    c.dialogue = (menusSnapshot & 4u) != 0;
    c.menu = (menusSnapshot & 2u) != 0;
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
    for (const auto& entry : identities) if (entry.id == id) return entry.handle.get();
    return {};
}
bool visibleNode(RE::NiAVObject* node, RE::NiAVObject* root) {
    // Native tests the hit node and ancestors, stopping BEFORE the reference root.
    for (unsigned depth = 0; node && node != root && depth < 128; ++depth, node = node->parent)
        if (node->GetAppCulled()) return false;
    return node == nullptr || node == root;
}
Bounds bounds(RE::TESObjectREFR* ref, bool report = true) {
    Bounds b;
    if (!ref) return b;
    const auto base = ref->GetBaseObject();
    if (!base) return b;
    // Reference pose + immutable OBND avoids thousands of scene-graph reads.
    const auto low = base->boundData.boundMin, high = base->boundData.boundMax;
    b = referenceBounds(point(ref->GetPosition()),point(ref->GetAngle()),ref->GetScale(),
        {static_cast<double>(low.x),static_cast<double>(low.y),static_cast<double>(low.z)},
        {static_cast<double>(high.x),static_cast<double>(high.y),static_cast<double>(high.z)});
    if (!validBounds(b)) {
        b.available = false;
        if (report && boundsLogs < 8) { ++boundsLogs; log("Bound box unavailable/invalid: using reference origin for distance."); }
    }
    return b;
}
Point bodyCentre(RE::PlayerCharacter* player) {
    const auto box = bounds(player,false);
    if (validBounds(box)) return boundsCentre(box);
    forwardReason("body_centre_unavailable");
    return {std::numeric_limits<double>::quiet_NaN(),0,0};
}
Candidate candidate(RE::TESObjectREFR* ref, RE::PlayerCharacter* player) {
    Candidate c;
    if (!ref) { c.loaded = false; return c; }
    c.id = ref->GetFormID(); c.position = point(ref->GetPosition());
    c.player = ref == player; c.disabled = ref->IsDisabled(); c.deleted = ref->IsDeleted();
    const auto base = ref->GetBaseObject();
    const auto parent = ref->GetParentCell();
    c.loaded = parent && parent->IsAttached();
    c.activatable = false;
    if (c.player || c.disabled || c.deleted || !c.loaded || !base) return c;
    const auto type = base->GetFormType();
    const bool carryLight = type == RE::FormType::Light && static_cast<RE::TESObjectLIGH*>(base)->CanBeCarried();
    if (!interactiveType(static_cast<std::uint32_t>(type),carryLight)) return c;
    // Same cheap name predicate as native canPick at 40B1FE, before the cap.
    const auto name = ref->GetDisplayFullName();
    if (!name || !*name) return c;
    c.loaded = ref->Is3DLoaded();
    if (!c.loaded) return c;
    c.liveActor = liveActor(ref);
    if ((ref->GetFormFlags() & 0x800000u) && ref->GetFormType() != RE::FormType::ActorCharacter) return c;
    c.bounds = bounds(ref);
    c.activatable = true;
    return c;
}
bool nativeEligible(RE::TESObjectREFR* ref, RE::PlayerCharacter* player, bool checkText) {
    using CanPick = bool(*)(RE::CrosshairPickData*,RE::TESObjectREFR*);
    static REL::Relocation<CanPick> canPick{REL::ID(26130)};
    if (!canPick(RE::CrosshairPickData::GetSingleton(),ref)) return false;
    float filtered = 0;
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kFilterActivation,player,ref,&filtered);
    if (filtered != 0) return false;
    if (!checkText || liveActor(ref)) return true;
    // Scan-only cache: live NPC text reads process state and is never called here.
    // The prompt supplies R9B=true; CommonLib's declaration omits that argument.
    using ActivateText = bool(*)(RE::TESBoundObject*,RE::TESObjectREFR*,RE::BSString*,bool);
    const auto base = ref->GetBaseObject();
    const auto vtable = *reinterpret_cast<const std::uintptr_t* const*>(base);
    RE::BSString text;
    return reinterpret_cast<ActivateText>(vtable[0x4c])(base,ref,&text,true) && !text.empty();
}
// Hash-pinned layouts. Retry reader churn at most eight times; never wait on a writer.
struct TryWorldRead {
    volatile LONG* word;
    bool acquired;
    explicit TryWorldRead(RE::BSReadWriteLock& lock) :
        word(reinterpret_cast<volatile LONG*>(reinterpret_cast<std::byte*>(&lock)+4)), acquired(false) {
        acquired = tryReadAcquire(static_cast<std::int32_t>(InterlockedCompareExchange(word,0,0)),
            [&](std::int32_t old, std::int32_t next) {
                return static_cast<std::int32_t>(InterlockedCompareExchange(word,next,old));
            },worldRetries);
    }
    ~TryWorldRead() { if (acquired) InterlockedDecrement(word); }
};
struct TryCellLock {
    volatile LONG* words;
    bool acquired;
    explicit TryCellLock(RE::BSSpinLock& lock) :
        words(reinterpret_cast<volatile LONG*>(&lock)), acquired(InterlockedCompareExchange(words+1,1,0) == 0) {
        if (acquired) InterlockedExchange(words,static_cast<LONG>(GetCurrentThreadId()));
    }
    ~TryCellLock() {
        if (acquired) { InterlockedExchange(words,0); InterlockedExchange(words+1,0); }
    }
};
Validation bodyLOS(RE::TESObjectREFR* ref, RE::PlayerCharacter* player, const Candidate& value, ScanBudget& rays) {
    const auto cell = player->GetParentCell();
    const auto world = cell && cell->IsAttached() ? cell->GetbhkWorld() : nullptr;
    const auto controller = player->GetCharController();
    if (!world || !controller) return Validation::unknown;
    const auto origin = bodyCentre(player);
    const float scale = RE::bhkWorld::GetWorldScale();
    if (!finite(origin) || !std::isfinite(scale) || scale <= 0) return Validation::unknown;
    RE::CFilter filter{};
    controller->GetCollisionFilterInfo(filter);
    const bool needsCollision = ref->GetBaseObject()->GetFormType() == RE::FormType::Activator;
    const auto targets = losPoints(value,origin);
    for (const auto target : targets) {
        if (!finite(target)) return Validation::unknown;
        TryWorldRead lock(world->worldLock);
        if (!lock.acquired) {
            ++worldContentions; scanContended = true;
            forwardReason("world_lock_contended"); return Validation::unknown;
        }
        if (!world->GetWorld1() || player->GetParentCell() != cell) return Validation::unknown;
        // Never retain/increment a raw root pointer. Only compare its identity;
        // hit-node ancestry is read under the physics read lock, root excluded.
        const auto root = ref->Get3D();
        if (!root) return Validation::unknown;
        const auto cast = [&](RE::COL_LAYER layer, bool requireHit) {
            if (!rays.inspect()) return Validation::unknown;
            RE::bhkPickData ray{};
            ray.rayInput.from = RE::hkVector4(static_cast<float>(origin.x)*scale,static_cast<float>(origin.y)*scale,static_cast<float>(origin.z)*scale,0);
            ray.rayInput.to = RE::hkVector4(static_cast<float>(target.x)*scale,static_cast<float>(target.y)*scale,static_cast<float>(target.z)*scale,0);
            ray.rayInput.filterInfo.filter = (filter.filter & 0xffff0000u) | static_cast<std::uint32_t>(layer);
            ray.rayInput.enableShapeCollectionFilter = false;
            ray.rayOutput.Reset();
            ++counters.raysTotal;
            world->PickObject(ray);
            if (ray.pickFailed || ref->Get3D() != root) {
                forwardReason("ray_refused_or_root_changed"); return Validation::unknown;
            }
            const auto hit = ray.rayOutput.rootCollidable;
            if (!hit) return requireHit ? Validation::invalid : Validation::valid;
            if (RE::TESHavokUtilities::FindCollidableRef(*hit) != ref) return Validation::invalid;
            const auto node = RE::TESHavokUtilities::FindCollidableObject(*hit);
            const bool visible = node && visibleNode(node,root);
            if (ref->Get3D() != root) return Validation::unknown;
            return visible ? Validation::valid : Validation::invalid;
        };
        const auto los = cast(RE::COL_LAYER::kLOS,false);
        if (los == Validation::unknown) return los;
        if (los != Validation::valid) continue;
        if (!needsCollision) return Validation::valid;
        const auto collision = cast(RE::COL_LAYER::kCustomPick2,true);
        if (collision != Validation::invalid) return collision;
    }
    return Validation::invalid;
}
Validation validateTarget(RE::TESObjectREFR* ref, RE::PlayerCharacter* player,
    const Candidate& value, Point body, double radius, ScanBudget& rays, bool checkText = false) {
    if (!ref || !rankable(value,lastSneaking) || !eligible(value,body,radius) || !nativeEligible(ref,player,checkText))
        return Validation::invalid;
    return bodyLOS(ref,player,value,rays);
}
bool collect(RE::PlayerCharacter* player, double radius, ScanBudget& budget, ScanBudget& visits) {
    const auto tes = RE::TES::GetSingleton();
    if (!tes) return false;
    scannedIDs.clear(); discovered.clear();
    // The native pick is considered even outside the bounded origin horizon.
    const auto native = nativeTarget.get();
    if (native && budget.inspect()) discovered.push_back(native);
    const auto position = bodyCentre(player);
    auto scan = [&](RE::TESObjectCELL* cell) {
        if (!cell || !cell->IsAttached() || !budget.complete || !visits.complete) return;
        auto& data = cell->GetRuntimeData();
        TryCellLock lock(data.spinLock);
        if (!lock.acquired) {
            ++cellContentions; scanContended = true;
            budget.complete = false; forwardReason("cell_lock_contended"); return;
        }
        for (const auto& entry : data.references) {
            const auto ref = entry.get();
            if (!ref) continue;
            if (!visits.inspect()) break;
            if (ref == player || !inBroadPhase(position,point(ref->GetPosition()),radius,settings.boundsPadding))
                continue;
            if (!budget.inspect()) break;
            if (ref != native.get()) discovered.push_back(entry);
        }
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
        const auto ws = tes->GetRuntimeData2().worldSpace;
        if (ws && budget.complete && visits.complete) scan(ws->skyCell);
    }
    for (const auto& ref : discovered) scannedIDs.push_back(ref->GetFormID());
    return budget.complete && visits.complete;
}
void selectionEvent(EventKind kind, std::uint32_t id, EventSource source, std::string_view reason,
    RE::TESObjectREFR* ref) {
    if (!diagnostics) return;
    const auto native = nativeTarget.get();
    const auto nativeID = native ? native->GetFormID() : 0;
    const auto emission = eventLog.observe(kind,id,source,reason,nativeID);
    if (emission == EventEmission::none) return;
    const auto now = std::chrono::steady_clock::now();
    static const auto start = now;
    const auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count());
    SelectionEvent event;
    if (emission == EventEmission::row) {
        event.kind = kind; event.id = id; event.source = source; event.reason = reason;
        event.nativeID = nativeID;
        const auto selected = ref ? RE::NiPointer<RE::TESObjectREFR>{} : reference(id);
        if (!ref) ref = selected.get();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (ref && player) {
            Candidate value;
            value.position = point(ref->GetPosition()); value.bounds = bounds(ref,false);
            event.bounds = validBounds(value.bounds);
            event.distance = std::sqrt(surfaceDistanceSquared(value,bodyCentre(player)));
        }
        const auto order = policy.order();
        event.cycleCount = order.size();
        const auto it = std::find(order.begin(),order.end(),id);
        if (it != order.end()) event.cycleIndex = static_cast<std::int64_t>(it-order.begin());
    }
    log(formatEvent(event,emission,eventLog.sequence(),ms).c_str());
}
void publish(bool resetState) {
    counters.target = policy.current(); counters.latched = policy.latched() ? 1u : 0u;
    counters.candidateCount = static_cast<std::uint32_t>(policy.order().size());
    counters.passthrough = static_cast<std::uint32_t>(resetState ? Pass::invalidWorld : policy.reason());
    counters.available = scanReady ? 1u : 0u;
    counters.eventSuppressed = eventLog.suppressed();
    counters.requestDrops = requestDrops.load();
    if (diagnostics) selectionEvent(EventKind::choose,policy.current(),selectionSource(),passName(policy.reason()));
    api::CursorFrame frame;
    frame.snapshot = counters;
    frame.viable = sessionViable.load() ? 1u : 0u;
    frame.completedRefreshes = completedRefreshes; frame.replies = cursorReplies;
    frame.aliveRefreshes = aliveRefreshes; frame.refreshSeconds = settings.refreshSeconds;
    const auto ids = policy.order(CursorOverride::active(cursorOverride.read()));
    frame.listReady = cursorListReady ? 1u : 0u;
    frame.count = cursorListReady ? static_cast<std::uint32_t>((std::min)(ids.size(),std::size(frame.ids))) : 0u;
    std::copy_n(ids.begin(),frame.count,frame.ids);
    // A reader can defer publication, never a Pick or its native prompt.
    static_cast<void>(publication.write(frame));
}
void diagnostic(double now) {
    if (!diagnostics || diagnosticCount >= diagnosticLimit || now-diagnosticTime < 5) return;
    diagnosticTime = now; ++diagnosticCount;
    char line[768]{};
    std::snprintf(line,sizeof(line),"selection target=%08X candidates=%u latch=%u pass=%u radius=%.2f inspected=%u scans=%llu capped=%u rays=%llu cost_us=%llu visits=%u scan_rays=%u picks=%llu suppressed=%llu validated=%u world_contended=%llu world_retries=%llu cell_contended=%llu refused_presses=%llu discarded_edges=%llu cycle_refusals=%llu pick_cost_us=%llu cycles=%llu request_drops=%llu visible=%u/%llu",
        counters.target,counters.candidateCount,counters.latched,counters.passthrough,counters.radius,counters.inspectedLast,
        static_cast<unsigned long long>(counters.refreshes),counters.cappedScans,
        static_cast<unsigned long long>(counters.raysTotal),static_cast<unsigned long long>(counters.lastCostMicros),counters.visitedLast,counters.scanRaysLast,
        static_cast<unsigned long long>(counters.pickChecks),static_cast<unsigned long long>(eventLog.suppressed()),counters.validatedLast,
        static_cast<unsigned long long>(worldContentions),static_cast<unsigned long long>(worldRetries),
        static_cast<unsigned long long>(cellContentions),static_cast<unsigned long long>(refusedPresses.load()),
        static_cast<unsigned long long>(discardedEdges),static_cast<unsigned long long>(cycleRefusals),
        static_cast<unsigned long long>(counters.lastPickCostMicros),static_cast<unsigned long long>(counters.cyclesTotal),
        static_cast<unsigned long long>(requestDrops.load()),visibleSeen.reported ? visibleSeen.count : 0u,static_cast<unsigned long long>(visibleWrites.load()));
    log(line);
}
bool refresh(double now) {
    if (invalidated.exchange(false)) reset();
    const auto rawContext = context();
    const auto c = cursorContext(rawContext,CursorOverride::active(cursorOverride.read()));
    const auto state = passthrough(c);
    if (state != lastState || c.sneaking != lastSneaking) gate.reset();
    lastState = state; lastSneaking = c.sneaking;
    sessionViable.store(passthrough(cursorContext(rawContext,true)) == Pass::none);
    if (state != Pass::none) {
        cursorOverride.end();
        policy.refresh(c,{}, {},0,now,settings); identities.clear();
        scanReady = false; cursorListReady = false; inputAllowed.store(false);
        // A definite cursor-eligible context keeps entry fresh even when ordinary
        // Selection deliberately passes through combat or the sneak actor.
        if (sessionViable.load()) ++aliveRefreshes;
        discardedEdges += CycleRequests::count(cycleRequests.take()); externalRequest.store(0);
        scanReason = passName(state); publish(); return false;
    }
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto body = bodyCentre(player);
    if (cursorRelease.exchange(false)) { policy.release(); gate.reset(); }
    const auto latchSettings = cursorLatchSettings(settings,CursorOverride::active(cursorOverride.read()));
    const auto expired = policy.expire(body,now,latchSettings);
    if (!expired.empty()) {
        selectionEvent(EventKind::latch,policy.current(),EventSource::nearest,expired);
        scanReady = false; cursorListReady = false; inputAllowed.store(false);
        scanReason = expired == "timeout" ? "latch_timeout" : "latch_movement";
        forwardReason(scanReason);
    }
    const auto parent = player->GetParentCell();
    if (!parent || !parent->IsAttached()) { reset(); scanReason = "invalid_world"; return false; }
    const auto cell = parent->GetFormID();
    if (cellID != cell) { gate.reset(); cellID = cell; }
    if (!gate.due(now,settings.refreshSeconds)) { publish(); return scanReady && policy.reason() == Pass::none; }
    gate.last = now;
    const auto start = std::chrono::steady_clock::now();
    const double radius = nativeRadius();
    ScanBudget budget{settings.inspectLimit}, visits{settings.visitLimit}, rays{settings.rayLimit};
    if (CursorOverride::active(cursorOverride.read()) && latchSource == EventSource::external) policy.release();
    const auto frozen = policy.latched() ? policy.order() : std::vector<std::uint32_t>{};
    // Preserve discovery of a previously native-seeded large/offset latched ref.
    std::vector<RE::ObjectRefHandle> retained;
    for (const auto& entry : identities) {
        if (frozen.empty()) break;
        if (std::find(frozen.begin(),frozen.end(),entry.id) != frozen.end()) retained.push_back(entry.handle);
    }
    scanContended = false;
    bool complete = radius > 0 && collect(player,radius,budget,visits);
    for (const auto handle : retained) {
        const auto ref = handle.get();
        if (ref && std::find(scannedIDs.begin(),scannedIDs.end(),ref->GetFormID()) == scannedIDs.end()) {
            if (!budget.inspect()) { complete = false; break; }
            discovered.push_back(ref);
            scannedIDs.push_back(ref->GetFormID());
        }
    }
    LazyResult checked;
    std::uint32_t validated = 0;
    if (complete) {
        std::vector<Candidate> cheap;
        cheap.reserve(discovered.size());
        for (const auto& ref : discovered) cheap.push_back(candidate(ref.get(),player));
        const auto validateOne = [&](const Candidate& value) {
            ++validated;
            const auto it = std::find_if(discovered.begin(),discovered.end(),
                [&](const auto& ref) { return ref->GetFormID() == value.id; });
            return it == discovered.end() ? Validation::invalid :
                validateTarget(it->get(),player,value,body,radius,rays,true);
        };
        checked = lazyValidate(cheap,body,radius,c.sneaking,8,validateOne);
        // At most eight extra checks preserve a frozen cycle list even if
        // unrelated objects move into the nearest prefix. Never grow that list.
        for (const auto id : frozen) {
            if (checked.unknown) break;
            if (std::find(checked.evaluated.begin(),checked.evaluated.end(),id) != checked.evaluated.end()) continue;
            const auto it = std::find_if(cheap.begin(),cheap.end(),[&](const auto& value) { return value.id == id; });
            if (it == cheap.end()) continue;
            const auto result = validateOne(*it);
            if (result == Validation::unknown) checked.unknown = true;
            else if (result == Validation::valid) checked.accepted.push_back(*it);
        }
    }
    if (scanContended || (checked.unknown && (rays.complete || !CursorOverride::active(cursorOverride.read()))))
        gate.reset(); // preserve legacy retries; definite cursor budgets wait for next refresh
    if (complete && !checked.unknown) {
        identities.clear();
        // Only validated candidates get handles (<=8 prefix + <=8 frozen).
        std::erase_if(checked.accepted,[&](const auto& value) {
            const auto it = std::find_if(discovered.begin(),discovered.end(),
                [&](const auto& ref) { return ref->GetFormID() == value.id; });
            const auto handle = it != discovered.end() ? (*it)->CreateRefHandle() : RE::ObjectRefHandle{};
            if (!handle) return true;
            identities.push_back({value.id,handle});
            return false;
        });
    }
    discovered.clear();
    scanReady = false; cursorListReady = false;
    scanReason = !complete ? "incomplete_scan" : checked.unknown ? "validation_unknown" : "none";
    if (complete && !checked.unknown) {
        scanReady = policy.acceptScan(c,checked,body,radius,now,latchSettings,true);
        cursorListReady = !checked.exhausted || scanReady;
        if (!scanReady) scanReason = checked.exhausted ? "validation_limit" : passName(policy.reason());
        if (!checked.exhausted || scanReady) ++completedRefreshes;
        const auto authorized = CursorOverride::target(cursorOverride.read());
        if (scanReady && authorized && latchSource == EventSource::external)
            static_cast<void>(policy.push(authorized,body,now,settings.allowExternal,true));
    }
    if (finishedScan(complete,checked.unknown,scanContended,!budget.complete || !visits.complete,!rays.complete)) ++aliveRefreshes;
    ++counters.refreshes; counters.inspectedLast = budget.inspected; counters.inspectedTotal += budget.inspected;
    counters.visitedLast = visits.inspected; counters.scanRaysLast = rays.inspected;
    counters.validatedLast = validated;
    if (!complete) {
        ++counters.cappedScans;
        if (capLogs < 8) { ++capLogs; log("Scan incomplete: visit/reference limit or world/lock read; native pick retained."); }
    }
    counters.radius = radius;
    counters.lastCostMicros = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-start).count());
    if (invalidated.load()) { reset(); scanReason = "invalidated"; return false; }
    inputAllowed.store(scanReady);
    publish(); diagnostic(now);
    return scanReady;
}
void consumeRequests(double now, RE::PlayerCharacter* player) {
    CursorAccess access;
    if (!access.acquired) return; // defer requests, never skip normal Pick validation
    const auto body = bodyCentre(player);
    const auto cursorToken = cursorOverride.read();
    if (visibleReset.exchange(false)) visibleSeen = {};
    static_cast<void>(cursorVisible.read(visibleSeen));
    const bool restrictCycle = CursorOverride::active(cursorToken) && visibleSeen.reported;
    const auto eligible = [&](std::uint32_t id) {
        const auto end = visibleSeen.ids+visibleSeen.count;
        return !restrictCycle || std::find(visibleSeen.ids,end,id) != end;
    };
    bool cycled = false;
    auto batch = cycleRequests.take();
    while (batch) {
        const auto direction = CycleRequests::pop(batch);
        if (policy.cycle(direction,body,now,eligible)) {
            ++counters.cyclesTotal;
            cycled = true;
            latchSource = EventSource::cycle;
            selectionEvent(EventKind::cycle,policy.current(),latchSource,direction > 0 ? "next" : "previous");
        } else ++cycleRefusals;
    }
    if (cycled) static_cast<void>(cursorOverride.publish(cursorToken,policy.current()));
    // A physical cycle wins over a hover already waiting in the mailbox.
    const bool cursorCycle = cycled && CursorOverride::active(cursorToken);
    const auto pending = (cursorCycle || externalGate.due(now,settings.refreshSeconds)) ? externalRequest.exchange(0) : 0;
    const auto request = cursorCycle ? 0 : pending;
    if (request) {
        externalGate.last = now;
        const auto id = static_cast<std::uint32_t>(request);
        counters.externalTarget = id;
        if (!id) {
            policy.release();
            if (!CursorOverride::active(cursorOverride.read())) {
                scanReady = false; inputAllowed.store(false); scanReason = "external_release";
            }
            counters.externalResult = static_cast<std::uint32_t>(api::Result::ok);
        }
        else {
            const auto order = policy.order();
            const auto ref = reference(id);
            ScanBudget rays{6};
            const auto result = std::find(order.begin(),order.end(),id) == order.end() ? Validation::invalid :
                validateTarget(ref.get(),player,candidate(ref.get(),player),body,nativeRadius(),rays);
            const bool accepted = result == Validation::valid && !invalidated.load() &&
                policy.push(id,body,now,settings.allowExternal);
            counters.externalResult = static_cast<std::uint32_t>(accepted ? api::Result::ok :
                result == Validation::unknown ? api::Result::unavailable : api::Result::invalidTarget);
            if (accepted) {
                latchSource = EventSource::external;
                static_cast<void>(cursorOverride.publish(cursorToken,id));
            }
        }
    }
    CursorRequest hover;
    if (cursorRequests.take(hover) && hover.token) {
        ++cursorReplies;
        counters.externalTarget = hover.id;
        counters.externalResult = static_cast<std::uint32_t>(api::Result::invalidTarget);
        if (!cursorCycle && hover.token == cursorOverride.read() &&
            cursorRequestCurrent(true,hover.cycles,counters.cyclesTotal)) {
            const auto order = policy.order(true);
            const auto ref = reference(hover.id);
            ScanBudget rays{6};
            const auto result = std::find(order.begin(),order.end(),hover.id) == order.end() ? Validation::invalid :
                validateTarget(ref.get(),player,candidate(ref.get(),player),body,nativeRadius(),rays);
            const bool accepted = result == Validation::valid && !invalidated.load() &&
                policy.push(hover.id,body,now,settings.allowExternal,true);
            counters.externalResult = static_cast<std::uint32_t>(accepted ? api::Result::ok :
                result == Validation::unknown ? api::Result::busy : api::Result::invalidTarget);
            if (accepted) {
                latchSource = EventSource::external;
                static_cast<void>(cursorOverride.publish(hover.token,hover.id));
            }
        }
    }
    publish();
}
void pick(RE::CrosshairPickData* data, RE::bhkWorld* world, const RE::NiPoint3* origin, const RE::NiPoint3* direction) {
    threadLog(true);
    StateAccess access;
    CursorPickScope cursorPick{cursorOverride,cursorPickPermit,cursorPickHandle,[&] {
        if (data) { data->target = {}; data->grabPickRef = {}; data->targetCollider = nullptr; }
    }};
    // Keep the last matching permit while native Pick/validation runs. Revoke
    // on a refused outcome below; token/ID changes already fail the E guard.
    previousPick(data,world,origin,direction);
    if (!access.acquired || !data) { forwardReason(!data ? "null_pick" : "state_busy_or_reentrant"); return; }
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
        forwardReason(reason);
        if (cursorPick.beganExclusive || CursorOverride::active(cursorOverride.read()))
            selectionEvent(EventKind::pick,0,EventSource::external,reason);
        else selectionEvent(EventKind::pick,nativeRef ? nativeRef->GetFormID() : 0,EventSource::native,reason,nativeRef.get());
    };
    const auto now = seconds();
    if (!refresh(now)) { nativeDecision(scanReason); return; }
    const auto player = RE::PlayerCharacter::GetSingleton();
    consumeRequests(now,player);
    if (!scanReady) { nativeDecision(scanReason); return; }
    if (nativeIsNew(nativeRef ? nativeRef->GetFormID() : 0,scannedIDs,policy.latched())) {
        nativeDecision("new_native_since_scan"); return;
    }
    const auto cursorState = cursorOverride.read();
    if (CursorOverride::active(cursorState) && CursorOverride::target(cursorState) != policy.current()) {
        publish(); nativeDecision("cursor_empty"); return;
    }
    const auto target = reference(policy.current());
    ScanBudget rays{6};
    const auto value = candidate(target.get(),player);
    const auto validation = validateTarget(target.get(),player,value,bodyCentre(player),nativeRadius(),rays);
    if (validation != Validation::valid) {
        if (validation == Validation::invalid) policy.invalidate(policy.current());
        // Unknown rays retain latch/list and retry at the next Pick.
        if (validation == Validation::unknown) gate.reset();
        scanReady = false; scanReason = validation == Validation::unknown ? "validation_unknown" : "target_validation";
        cursorListReady = validation == Validation::invalid;
        inputAllowed.store(false); publish(); nativeDecision(scanReason); return;
    }
    RE::ObjectRefHandle handle;
    for (const auto& entry : identities) if (entry.id == policy.current()) { handle = entry.handle; break; }
    if (!handle) {
        policy.invalidate(policy.current()); publish(); nativeDecision("invalid_handle"); return;
    }
    using CanGrab = bool(*)(RE::TESObjectREFR*);
    static REL::Relocation<CanGrab> canGrab{REL::ID(34614)};
    const bool grabbable = canGrab(target.get());
    const bool stale = invalidated.load();
    const auto state = stale ? Pass::invalidWorld : passthrough(cursorContext(context(),CursorOverride::active(cursorOverride.read())));
    if (stale || state != Pass::none) {
        reset(); nativeDecision(stale ? "invalidated" : passName(state)); return;
    }
    const auto change = pickChanges(data->target != handle,grabbable);
    // Store before data->target, so an exit racing this write still refuses it.
    if (CursorOverride::active(cursorState)) cursorPickHandle.store(handle.native_handle());
    if (change.target) {
        data->target = handle;
        data->grabPickRef = change.grab ? handle : RE::ObjectRefHandle{};
    }
    if (change.clearCollider) data->targetCollider = nullptr;
    // targetActor and collisionPoint remain native (aim magnetism / SmoothCam).
    cursorPick.injected = true; cursorPick.id = target->GetFormID();
    cursorPickHandle.store(CursorOverride::active(cursorState) ? handle.native_handle() : 0);
    cursorPickPermit.store(cursorState);
    selectionEvent(EventKind::pick,target->GetFormID(),selectionSource(),"none",target.get());
}
api::Result cursorFrame(api::CursorFrame* out) {
    if (!out || out->size != sizeof(api::CursorFrame)) return api::Result::invalidArgument;
    if (!installed.load()) return api::Result::unavailable;
    if (!publication.read(*out)) return api::Result::busy;
    out->authorizedTarget = CursorOverride::target(cursorOverride.read());
    if (invalidated.load() || loading.load() || menuState.load() != 1u) out->viable = out->snapshot.available = 0;
    return api::Result::ok;
}
api::Result snapshot(api::Snapshot* out) {
    if (!out || out->size != sizeof(api::Snapshot)) return api::Result::invalidArgument;
    StateAccess access;
    const auto result = legacyReadResult(access.acquired);
    if (result != api::Result::ok) return result;
    *out = counters;
    if (invalidated.load() || loading.load() || menuState.load() != 1u) out->available = 0;
    return api::Result::ok;
}
api::Result copyCandidates(std::uint32_t* ids, std::uint32_t capacity, std::uint32_t* count) {
    if (!count || (!ids && capacity)) return api::Result::invalidArgument;
    StateAccess access;
    const auto result = legacyReadResult(access.acquired,access.acquired && scanReady &&
        !invalidated.load() && !loading.load() && menuState.load() == 1u);
    if (result != api::Result::ok) return result;
    const auto order = policy.order();
    *count = static_cast<std::uint32_t>(order.size());
    for (std::uint32_t i = 0; i < capacity && i < *count; ++i) ids[i] = order[i];
    return api::Result::ok;
}
api::Result postTarget(std::uint32_t id) {
    if (!installed.load()) return api::Result::unavailable;
    if (!settings.allowExternal) return api::Result::disabled;
    if (invalidated.load() || loading.load() || !inputAllowed.load() || menuState.load() != 1u) return api::Result::unavailable;
    std::uint64_t empty = 0;
    return externalRequest.compare_exchange_strong(empty,(std::uint64_t{1} << 32) | id) ? api::Result::queued : api::Result::unavailable;
}
api::Result setCursorTarget(std::uint32_t id, std::uint64_t observedCycles) {
    if (!CursorOverride::active(cursorOverride.read())) return api::Result::unavailable;
    if (!id) {
        if (!cursorOverride.clear()) return api::Result::unavailable;
        cursorRelease.store(true);
        return api::Result::ok;
    }
    return replaceCursorTarget(cursorOverride,id,[&](std::uint64_t token, std::uint32_t targetID) {
        CursorAccess access;
        if (!access.acquired) return api::Result::busy;
        if (observedCycles != counters.cyclesTotal) return api::Result::invalidTarget;
        // Generation tags cancel both an older queued request and an in-flight Pick.
        return cursorRequests.write({token,observedCycles,targetID}) ? api::Result::queued : api::Result::busy;
    });
}
api::Result setCursorVisible(const std::uint32_t* ids, std::uint32_t count) {
    if (count > std::size(CursorVisible{}.ids) || (count && !ids)) return api::Result::invalidArgument;
    if (!CursorOverride::active(cursorOverride.read())) return api::Result::unavailable;
    CursorVisible visible; visible.count = count; visible.reported = true;
    if (count) std::copy_n(ids,count,visible.ids);
    if (!cursorVisible.write(visible)) return api::Result::busy;
    ++visibleWrites;
    return api::Result::ok;
}
api::Result setTarget(std::uint32_t id) {
    if (CursorOverride::active(cursorOverride.read())) {
        api::CursorFrame frame;
        const auto result = cursorFrame(&frame);
        return legacyResult(result == api::Result::ok ? setCursorTarget(id,frame.snapshot.cyclesTotal) : result);
    }
    return id ? postTarget(id) : api::Result::invalidArgument;
}
api::Result clearTarget() {
    if (!CursorOverride::active(cursorOverride.read())) return postTarget(0);
    cursorOverride.end(); externalRequest.store(0); cursorRelease.store(true);
    return api::Result::ok;
}
api::Result setCursorMode(std::uint32_t enabled) {
    if (enabled == 2) return CursorOverride::active(cursorOverride.read()) ? api::Result::ok : api::Result::unavailable;
    if (enabled > 2) return api::Result::invalidArgument;
    if (!enabled) {
        if (cursorOverride.end()) { externalRequest.store(0); cursorRelease.store(true); }
        return api::Result::ok;
    }
    if (!installed.load()) return api::Result::unavailable;
    if (!settings.allowExternal) return api::Result::disabled;
    if (!activationGuardReady) return api::Result::unavailable;
    if (invalidated.load() || loading.load() || !sessionViable.load() || menuState.load() != 1u) return api::Result::unavailable;
    externalRequest.store(0); cursorRelease.store(true); visibleReset.store(true); cursorOverride.begin();
    return api::Result::ok;
}
const api::Interface interfaceV2{sizeof(api::Interface),api::version,snapshot,copyCandidates,setTarget,clearTarget,setCursorMode,setCursorTarget,cursorFrame,setCursorVisible};
class Events final : public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        updateMenus(); flushLog(); return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* input,RE::BSTEventSource<RE::InputEvent*>*) override {
        threadLog(false);
        flushLog();
        if (!input || !inputGate.enter()) return RE::BSEventNotifyControl::kContinue;
        const bool safe = inputAllowed.load() && !invalidated.load() && !loading.load() && menuState.load() == 1u;
        if (!safe) for (auto& key : keys) key.suspend();
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
                const bool mapped = !map || !map->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay] ||
                    !map->GetUserEventName(button->GetIDCode(),device).empty() || !button->QUserEvent().empty();
                if (button->IsDown() && (!safe || mapped)) ++refusedPresses;
                if (mapped && button->IsDown() && conflictLogs < 8) { ++conflictLogs; log("Cycle binding refused: mapped native event or unreadable ControlMap."); }
                if (keys[(pad ? 2u : 0u)+i].update(button->value > 0,button->IsDown(),safe && !mapped)) {
                    if (!cycleRequests.post(i == 0 ? 1 : -1)) ++requestDrops;
                }
            }
        }
        inputGate.leave();
        return RE::BSEventNotifyControl::kContinue;
    }
};
Events events;
using ResolveActivation = bool(*)(RE::RefHandle&,RE::NiPointer<RE::TESObjectREFR>&);
ResolveActivation previousResolveActivation = nullptr;
bool resolveActivation(RE::RefHandle& handle, RE::NiPointer<RE::TESObjectREFR>& out) {
    const auto before = cursorOverride.read();
    if (!CursorOverride::active(before)) {
        // Exit cannot write job-owned CrosshairPickData. Refuse its last injected
        // handle until the next Pick completes and clears/replaces the permit.
        if (handle && handle == cursorPickHandle.load()) { out.reset(); return false; }
        return previousResolveActivation(handle,out);
    }
    // Runs at ActivatePickRef's one reference resolution, after native target or
    // command fallback. Never write shared CrosshairPickData from the API thread.
    if (!CursorOverride::target(before) || cursorPickPermit.load() != before ||
        invalidated.load() || loading.load() || menuState.load() != 1u) { out.reset(); return false; }
    // Native Pick overwrites data->target before cursor validation completes.
    // The matching permit belongs to the stored cursor handle, not that incoming
    // centre pick. Publish the handle before its token; check token and FormID
    // again after resolving so concurrent changes cannot borrow authorization.
    auto permittedHandle = cursorPickHandle.load();
    // Substitute only for the crosshair target. Any other incoming handle is the
    // commanded-actor or furniture fallback, which the pointer never chose.
    const auto pick = RE::CrosshairPickData::GetSingleton();
    if (!permittedHandle || (handle != permittedHandle && (!pick || handle != pick->target.native_handle()))) {
        out.reset(); return false;
    }
    const bool result = previousResolveActivation(permittedHandle,out);
    if (!result || !out || cursorPickPermit.load() != before ||
        !CursorOverride::permits(before,cursorOverride.read(),out->GetFormID())) {
        out.reset(); return false;
    }
    // ActivatePickRef re-resolves this same local handle for perk Activate entry
    // points (41003); write it back so perks see the chosen reference.
    handle = permittedHandle;
    return true;
}
bool installActivationGuard() {
    const REL::Relocation<std::uintptr_t> activate{REL::ID(40548)};
    const auto call = activate.address()+0x7e;
    constexpr std::array<unsigned char,5> expected{0xe8,0xdd,0xd5,0xa2,0xff};
    if (activate.offset() != 0x751580 || std::memcmp(reinterpret_cast<const void*>(call),expected.data(),expected.size()) != 0) {
        log("Cursor activation guard refused: ActivatePickRef resolver call changed."); return false;
    }
    const auto allocator = SKSE::GetTrampolineInterface();
    auto stub = allocator ? allocator->AllocateFromBranchPool(14) : nullptr;
    if (!stub) return false;
    const auto distance = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(stub))-static_cast<std::int64_t>(call+5);
    if (distance < (std::numeric_limits<std::int32_t>::min)() || distance > (std::numeric_limits<std::int32_t>::max)()) return false;
    std::array<unsigned char,14> jump{0xff,0x25,0,0,0,0};
    const auto destination = reinterpret_cast<std::uintptr_t>(resolveActivation);
    std::memcpy(jump.data()+6,&destination,sizeof(destination));
    DWORD old = 0, ignored = 0;
    if (!VirtualProtect(stub,jump.size(),PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(stub,jump.data(),jump.size());
    const bool flushed = FlushInstructionCache(GetCurrentProcess(),stub,jump.size()) != 0;
    const bool protectedStub = VirtualProtect(stub,jump.size(),old,&ignored) != 0;
    if (!flushed || !protectedStub) return false;
    std::int32_t original = 0;
    std::memcpy(&original,expected.data()+1,sizeof(original));
    previousResolveActivation = reinterpret_cast<ResolveActivation>(static_cast<std::uintptr_t>(static_cast<std::int64_t>(call+5)+original));
    std::array<unsigned char,5> replacement{0xe8};
    const auto relative = static_cast<std::int32_t>(distance);
    std::memcpy(replacement.data()+1,&relative,sizeof(relative));
    auto site = reinterpret_cast<void*>(call);
    if (std::memcmp(site,expected.data(),expected.size()) != 0 || !VirtualProtect(site,replacement.size(),PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(site,replacement.data(),replacement.size());
    const bool patched = FlushInstructionCache(GetCurrentProcess(),site,replacement.size()) != 0;
    const bool protectedCall = VirtualProtect(site,replacement.size(),old,&ignored) != 0;
    return patched && protectedCall && std::memcmp(site,replacement.data(),replacement.size()) == 0;
}
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
    if (!input || !ui || !activationDistance) {
        log("REFUSED hook: input/menu source or native activation setting unavailable."); return;
    }
    // Reserve explicitly: CommonLib's allocation fallback can terminate the
    // process. Failure here must leave the original five-byte CALL intact.
    const auto allocator = SKSE::GetTrampolineInterface();
    auto stub = allocator ? allocator->AllocateFromBranchPool(14) : nullptr;
    if (!stub) { log("REFUSED hook: SKSE branch pool unavailable."); return; }
    const auto displacement = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(stub)) -
        static_cast<std::int64_t>(call+5);
    if (displacement < (std::numeric_limits<std::int32_t>::min)() || displacement > (std::numeric_limits<std::int32_t>::max)()) {
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
    mainThreadID = main ? main->threadID : 0;
    identities.reserve(16);
    discovered.reserve(settings.inspectLimit);
    scannedIDs.reserve(settings.inspectLimit);
    std::memcpy(site,replacement.data(),replacement.size());
    const bool callFlushed = FlushInstructionCache(GetCurrentProcess(),site,replacement.size()) != 0;
    const bool callProtected = VirtualProtect(site,replacement.size(),oldCall,&ignored) != 0;
    if (!callFlushed || !callProtected || std::memcmp(site,replacement.data(),replacement.size()) != 0) {
        // The wrapper is fully initialized and forwards native with installed=false.
        log("Hook finalization failed: selection inert; any installed wrapper forwards native only."); return;
    }
    input->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&events));
    ui->AddEventSink<RE::MenuOpenCloseEvent>(&events);
    updateMenus();
    if (settings.allowExternal) {
        activationGuardReady = installActivationGuard();
        log(activationGuardReady ? "Cursor activation resolver guard installed: 40548+0x7E." :
            "Cursor unavailable: activation resolver guard not installed; ordinary Selection remains available.");
    }
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
    flushLog();
}
}
extern "C" __declspec(dllexport) const skycrpg::selection::api::Interface* SkyCRPGSelection_GetAPI(std::uint32_t version) {
    return version == api::version ? &interfaceV2 : nullptr;
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
    logPath = std::filesystem::path(skycrpg::LogDirectory())/L"SkyCRPG_Selection.log";
    std::ofstream(logPath,std::ios::trunc).close();
    log("SkyCRPG Selection " SKYCRPG_VERSION " source " SKYCRPG_SOURCE_COMMIT "; original MIT, no Camera/UI/provider dependency.");
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768) ||
        !hashFile(std::filesystem::path(executable),"846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f") ||
        !hashFile(std::filesystem::path(skycrpg::ThisModuleDirectory())/L"versionlib-1-7-104-0.bin","8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842")) {
        log("Executable/Address Library hash mismatch: inert before CommonLib address initialization."); flushLog(); return true;
    }
    SKSE::Init(skse,false); loadSettings();
    flushLog();
    const auto messaging = SKSE::GetMessagingInterface();
    const bool registered = messaging && messaging->RegisterListener(messages);
    if (registered) startFlushWorker();
    flushLog();
    return registered;
}
