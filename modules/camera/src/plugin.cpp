// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "policy.h"
#include "precision_policy.h"
#include "precision_trace.h"
#include "lifecycle_policy.h"
#include "target_lock_policy.h"
#include "lock_frame_trace.h"
#include "obstruction_policy.h"
#pragma warning(push)
#pragma warning(disable: 4191) // permitted external API FARPROC conversion
#define SMOOTHCAM_API_COMMONLIB
#include "third_party/SmoothCamAPI.h"
#include "third_party/TrueDirectionalMovementAPI.h"
#pragma warning(pop)
#include <bcrypt.h>
#include <array>
#include <charconv>
#include <atomic>
#include <chrono>
#include <cmath>
#include <type_traits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <limits>
#include "thin_occluder.h"

namespace {
using namespace skycrpg::camera;
constexpr REL::Version supportedRuntime{1,7,104,0};
// Exact 1.7.104 member layout audited independently of vtable declarations.
static_assert(offsetof(RE::ThirdPersonState,targetZoomOffset) == 0x74);
static_assert(offsetof(RE::ThirdPersonState,freeRotation) == 0xD4);
static_assert(offsetof(RE::ThirdPersonState,freeRotationEnabled) == 0xDC);
static_assert(offsetof(RE::ThirdPersonState,stateNotActive) == 0xDD);
static_assert(offsetof(RE::PlayerControls,data) == 0x24);
static_assert(offsetof(RE::PlayerControlsData,fovSlideMode) == 0x27);
static_assert(offsetof(RE::PlayerControlsData,povScriptMode) == 0x28);
static_assert(offsetof(RE::PlayerControlsData,povBeastMode) == 0x29);
static_assert(offsetof(RE::ControlMap::RUNTIME_DATA,enabledControls) == 0x30);
static_assert(offsetof(RE::Main,threadID) == 0x28);
static_assert(offsetof(RE::ShaderReferenceEffect,lifetime) == 0x10);
static_assert(offsetof(RE::ShaderReferenceEffect,age) == 0x20);
static_assert(offsetof(RE::ShaderReferenceEffect,effectID) == 0x28);
static_assert(offsetof(RE::ShaderReferenceEffect,target) == 0x38);
static_assert(offsetof(RE::ShaderReferenceEffect,finished) == 0x40);
static_assert(offsetof(RE::ShaderReferenceEffect,effectData) == 0x108);
Settings settings;
ObstructionSettings obstructionSettings;
ObstructionPolicy obstructionPolicy;
ObstructionFrame obstructionFrame;
std::chrono::steady_clock::time_point obstructionSampleTime{};
RE::FormID obstructionCell = 0;
bool adjustedZoomWritten = false;
float lastAdjustedZoom = 0;
bool ready = false, loading = true, ownFirstPerson = false, recenter = false;
SmoothCamAPI::IVSmoothCam3* provider = nullptr;
TDM_API::IVTDM5* movement = nullptr;
bool movementLease = false, applyZoom = true, compatibleSettings = true, movementConfigSafe = false;
bool targetLockEnabled = false, lockConfigSafe = false, lockConfigLogged = false;
bool lockHooksReady = false;
bool lockFrameTraceEnabled = false;
LockTraceWindow lockTraceWindow;
LockFrameWriter lockFrameWriter;
RE::NiPoint2 lockLookInput{};
unsigned lockLookCalls = 0;
LockRetargetWindow lockRetargetWindow;
LockStateChange<TDM_API::APIResult> lockLeaseLog;
bool lockRefusalNotified = false;
LockInterrupt lockInterrupt, lockConfigRefresh;
float lockHoldSeconds = .4f;
RE::FormID indicatorShaderForm = 0xAAEB3;
RE::TESEffectShader* indicatorShader = nullptr;
RE::ObjectRefHandle indicatorTarget;
LockIndicatorAttempt indicatorAttempt;
// Written by the input sink (any thread) and read by the main-thread lease path.
std::atomic<double> lockInputLast{0};
std::atomic<bool> lockInputSeen{false};
// Last main-thread decision, read when context() runs on another thread.
std::atomic<bool> publishedLockPrerequisites{false}, publishedTargetLock{false};
unsigned lockStateLogs = 0;
std::optional<bool> observedLock;
RE::ActorHandle observedTarget;
bool precisionTraceEnabled = false, lifecycleTraceEnabled = false;
bool badSettings = false, prerequisiteFailed = false, failureNotified = false;
EnableIntent enableIntent;
unsigned lifecycleLogCount = 0;
LifecycleTraceDebounce lifecycleTrace;
PrecisionTraceBudget precisionTraceBudget;
FailureDebounce failureDebounce;
std::array<FreshPress,4> keys;
using Look = void(*)(RE::ThirdPersonState*, const RE::NiPoint2&);
Look previousLook = nullptr;
using Update = void(*)(RE::ThirdPersonState*,RE::BSTSmartPointer<RE::TESCameraState>&);
Update previousUpdate = nullptr;
std::filesystem::path iniPath;
bool thinEligible();
RE::NiPoint3 thinGoal();
RE::NiPoint3 thinFocus();
void log(const char* text) {
    FILE* file = nullptr;
    const auto path = skycrpg::LogDirectory() + L"SkyCRPG_Camera.log";
    if (_wfopen_s(&file,path.c_str(),L"a") == 0 && file) { std::fprintf(file,"%s\n",text); std::fclose(file); }
}
// Hash loaded files before any hook/address use. Unknown provider builds fail closed.
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
bool hashMatches(const wchar_t* module, const std::string& expected) {
    wchar_t path[32768]{};
    const auto handle = module ? GetModuleHandleW(module) : GetModuleHandleW(nullptr);
    return handle && GetModuleFileNameW(handle,path,32768) && hashFile(std::filesystem::path(path),expected);
}
std::string read(const char* section, const char* key, const char* fallback) {
    char value[128]{};
    GetPrivateProfileStringA(section,key,fallback,value,128,iniPath.string().c_str());
    return value;
}
template<class T> T number(const char* section,const char* key,T fallback) {
    const auto value = read(section,key,"<missing>");
    if (value == "<missing>") return fallback;
    T result{};
    const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
    bool finite = true;
    if constexpr (std::is_floating_point_v<T>) finite = std::isfinite(result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size() || !finite) {
        badSettings = true; return fallback;
    }
    return result;
}
bool checkMovementConfig() {
    // Match TDM Settings::ReadSettings precedence: provider config, then user MCM.
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768)) return false;
    const auto root = std::filesystem::path(executable).parent_path()/L"Data"/L"MCM";
    int yaw = -1;
    for (const auto& path : {root/L"Config"/L"TrueDirectionalMovement"/L"settings.ini",root/L"Settings"/L"TrueDirectionalMovement.ini"}) {
        char value[32]{};
        GetPrivateProfileStringA("DirectionalMovement","uAdjustCameraYawDuringMovement","",value,32,path.string().c_str());
        if (value[0]) {
            const std::string_view text(value);
            int parsed = -1;
            const auto result = std::from_chars(text.data(),text.data()+text.size(),parsed);
            if (result.ec != std::errc{} || result.ptr != text.data()+text.size()) return false;
            yaw = parsed;
        }
    }
    return yaw == 0;
}
bool checkLockMovementConfig() {
    // Match TDM Settings::ReadSettings precedence: provider config, then user MCM.
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768)) { lockConfigSafe = false; return false; }
    const auto root = std::filesystem::path(executable).parent_path()/L"Data"/L"MCM";
    const std::array paths{root/L"Config"/L"TrueDirectionalMovement"/L"settings.ini",
        root/L"Settings"/L"TrueDirectionalMovement.ini"};
    auto effective = [&](const char* section,const char* key) {
        std::string result;
        for (const auto& path : paths) {
            char value[128]{};
            GetPrivateProfileStringA(section,key,"<missing>",value,128,path.string().c_str());
            if (std::strcmp(value,"<missing>") != 0) result = value;
        }
        return result;
    };
    lockConfigSafe = true;
    for (const auto& setting : lockSettings) {
        if (!lockSettingMatches(setting,effective(setting.section,setting.key),lockHoldSeconds)) {
            lockConfigSafe = false;
            if (targetLockEnabled && !lockConfigLogged) {
                char line[192]{};
                std::snprintf(line,sizeof(line),"Target lock refused: live TDM [%s] %s differs from Camera contract; lease held, file unchanged.",
                    setting.section,setting.key);
                log(line); lockConfigLogged = true;
            }
        }
    }
    // The opt-in yaw guard uses the same strict parser as the lock keys.
    return lockSettingMatches({"DirectionalMovement","uAdjustCameraYawDuringMovement",0},
        effective("DirectionalMovement","uAdjustCameraYawDuringMovement"));
}
bool flag(const char* section, const char* key, int fallback) {
    const int value = number(section,key,fallback);
    if (value != 0 && value != 1) { badSettings = true; return fallback == 1; }
    return value == 1;
}
void loadSettings() {
    iniPath = std::filesystem::path(skycrpg::ThisModuleDirectory())/L"SkyCRPG_Camera.ini";
    Settings s;
    s.schema = number("Camera","iSchemaVersion",1);
    s.enabled = flag("Camera","bEnabled",1);
    enableIntent.wanted = s.enabled;
    s.pitch = number("Camera","fPitchDegrees",55.0f);
    s.pitchSpeed = number("Camera","fPitchSpeedDegrees",90.0f);
    s.recenterSpeed = number("Camera","fRecenterSpeedDegrees",180.0f);
    s.zoom = number("Camera","fZoom",.65f);
    s.minZoom = number("Camera","fMinZoom",.25f);
    s.maxZoom = number("Camera","fMaxZoom",.90f);
    s.toggleKey = number("Controls","iToggleKey",64);
    s.recenterKey = number("Controls","iRecenterKey",65);
    s.toggleGamepad = number("Controls","iToggleGamepad",-1);
    s.recenterGamepad = number("Controls","iRecenterGamepad",-1);
    targetLockEnabled = flag("TargetLock","bEnabled",0);
    if (targetLockEnabled) {
        indicatorShaderForm = number<RE::FormID>("TargetLock","iIndicatorShaderFormID",0xAAEB3);
        lockHoldSeconds = number("TargetLock","fPOVHoldSeconds",.4f);
        lockFrameTraceEnabled = flag("TargetLock","iLockFrameTrace",0);
        if (lockHoldSeconds < .2f || lockHoldSeconds > 1.5f) {
            badSettings = true; lockHoldSeconds = .4f;
        }
    }
    // No allocation, address use or scanning when the optional feature is off.
    if (flag("ThinOccluder","bEnabled",0)) {
        thin::runtime = new thin::Runtime;
        auto& t = *thin::runtime;
        t.enabled = true; t.logger = log; t.eligibleNow = thinEligible; t.goalNow = thinGoal; t.focusNow = thinFocus;
        t.trace = flag("ThinOccluder","iTrace",0);
        t.targetAlpha = thin::validateAlpha(number("ThinOccluder","fTargetAlpha",.35f));
        const auto count = number("ThinOccluder","iFormCount",3u);
        if (!count || count > thin::maxForms) t.enabled = false;
        t.formCount = (std::min)(count,thin::maxForms);
        for (unsigned i = 0;i < t.formCount;++i) {
            char key[24]{},fallback[128]{};
            std::snprintf(key,sizeof(key),"sForm%u",i);
            if (i < thin::defaults.size()) std::snprintf(fallback,sizeof(fallback),"Skyrim.esm|%06X",thin::defaults[i].local);
            t.configured[i] = read("ThinOccluder",key,fallback);
            if (!thin::parseForm(t.configured[i])) t.enabled = false;
        }
        if (!t.enabled) thin::refuse("ThinOccluder: invalid curated form configuration; native collision retained.");
    }
    precisionTraceEnabled = flag("Debug","iPrecisionTrace",0);
    lifecycleTraceEnabled = flag("Debug","iLifecycleTrace",0);
    compatibleSettings = s.schema == 1;
    settings = validate(s);
    ObstructionSettings o;
    o.enabled = flag("Obstruction","bEnabled",0);
    o.enterRatio = number("Obstruction","fEnterRatio",o.enterRatio);
    o.exitRatio = number("Obstruction","fExitRatio",o.exitRatio);
    o.enterSeconds = number("Obstruction","fEnterSeconds",o.enterSeconds);
    o.exitSeconds = number("Obstruction","fExitSeconds",o.exitSeconds);
    o.warmupSeconds = number("Obstruction","fWarmupSeconds",o.warmupSeconds);
    o.tightPitch = number("Obstruction","fTightPitchDegrees",o.tightPitch);
    o.tightZoom = number("Obstruction","fTightZoom",o.tightZoom);
    o.tightenRate = number("Obstruction","fTightenRate",o.tightenRate);
    o.recoverRate = number("Obstruction","fRecoverRate",o.recoverRate);
    obstructionSettings = validateObstruction(o,settings);
    const auto& v = obstructionSettings;
    if (!compatibleSettings || s.pitch != settings.pitch || s.pitchSpeed != settings.pitchSpeed ||
        s.recenterSpeed != settings.recenterSpeed || s.zoom != settings.zoom || s.minZoom != settings.minZoom ||
        s.maxZoom != settings.maxZoom || s.toggleKey != settings.toggleKey || s.recenterKey != settings.recenterKey ||
        s.toggleGamepad != settings.toggleGamepad || s.recenterGamepad != settings.recenterGamepad ||
        o.enterRatio != v.enterRatio || o.exitRatio != v.exitRatio || o.enterSeconds != v.enterSeconds ||
        o.exitSeconds != v.exitSeconds || o.warmupSeconds != v.warmupSeconds || o.tightPitch != v.tightPitch ||
        o.tightZoom != v.tightZoom || o.tightenRate != v.tightenRate || o.recoverRate != v.recoverRate) badSettings = true;
    if (badSettings) log("Invalid Camera INI values: defaults or safe bounds applied; unsupported schema disables policy.");
    if (!s.enabled) log("Camera disabled in INI; native camera controls remain available.");
    if (!compatibleSettings) prerequisiteFailed = true;
}
void savePreferences() {
    if (!compatibleSettings) return;
    // Write only owned preferences, preserving other sections/keys; never touch saves.
    const auto path = iniPath.string();
    const auto zoom = std::to_string(settings.zoom);
    if (!WritePrivateProfileStringA("Camera","iSchemaVersion","1",path.c_str()) ||
        !WritePrivateProfileStringA("Camera","bEnabled",settings.enabled?"1":"0",path.c_str()) ||
        !WritePrivateProfileStringA("Camera","fZoom",zoom.c_str(),path.c_str())) log("Preferences write failed; session remains usable, disk settings unchanged or partial.");
}
bool onTargetLockThread() {
    if (!movement) return false;
    const auto main = RE::Main::GetSingleton();
    const auto thread = GetCurrentThreadId();
    // TDM reports the thread that first requested its API (data load), not its
    // update thread, which is the main thread; only the main thread is checked.
    const auto tdmThread = movement->GetTDMThreadId();
    const bool matches = main && main->threadID == thread;
    static std::atomic_flag reported = ATOMIC_FLAG_INIT;
    if (!matches && !reported.test_and_set()) {
        char line[160]{};
        std::snprintf(line,sizeof(line),"Target lock waiting for the main thread: current=%lu Main=%lu TDM-reported=%lu; lock policy suspended on this thread.",
            static_cast<unsigned long>(thread),static_cast<unsigned long>(main ? main->threadID : 0),tdmThread);
        log(line);
    }
    return matches;
}
bool acquireMovementLease() {
    if (!movement) return false;
    const auto lease = movement->RequestDisableTargetLock(SKSE::GetPluginHandle());
    movementLease = lease == TDM_API::APIResult::OK || lease == TDM_API::APIResult::AlreadyGiven;
    if (!movementLease) prerequisiteFailed = true;
    if (!targetLockEnabled) {
        if (!movementLease) log("TDM target-lock-disable lease refused; controller suspended.");
        else log("TDM target-lock-disable lease acquired.");
    } else if (lockLeaseLog.changed(lease)) {
        if (!movementLease) {
            char line[128]{};
            std::snprintf(line,sizeof(line),"TDM target-lock-disable lease refused (result=%u); controller suspended.",
                static_cast<unsigned>(lease));
            log(line);
        } else log("TDM target-lock-disable lease acquired.");
    }
    return movementLease;
}
void stopIndicator() {
    if (!onTargetLockThread()) { lockInterrupt.request(); return; }
    if (indicatorAttempt.effect) {
        const auto processes = RE::ProcessLists::GetSingleton();
        if (!processes) return; // retry cleanup when process lists return
        processes->ForEachShaderEffect([](RE::ShaderReferenceEffect* effect) {
            if (effect->effectID == *indicatorAttempt.effect && effect->target == indicatorTarget && effect->effectData == indicatorShader)
                effect->finished = true;
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    indicatorAttempt.reset(); indicatorTarget = {};
}
double lockClock() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void noteLockInputActivity() {
    lockInputLast.store(lockClock()); lockInputSeen.store(true);
}
bool lockInputIdle() {
    LockInputIdle idle;
    if (lockInputSeen.load()) idle.activity(lockInputLast.load());
    return idle.ready(lockClock());
}
void consumeLockInterrupt() {
    if (!ready || !lockHooksReady || !onTargetLockThread()) return;
    if (lockInterrupt.consume()) {
        noteLockInputActivity();
        lockRetargetWindow.started.reset();
        stopIndicator();
        if (!movementLease) static_cast<void>(acquireMovementLease());
    }
}
void interruptTargetLock() {
    if (!targetLockEnabled) return;
    lockInterrupt.request();
    if (onTargetLockThread()) consumeLockInterrupt();
}
bool indicatorReady() {
    return indicatorShader && RE::ProcessLists::GetSingleton();
}
void observeTargetLock() {
    observedLock.reset(); observedTarget = {};
    if (!ready || !movement || !onTargetLockThread()) return;
    const auto owner = movement->GetDisableTargetLockOwner();
    movementLease = owner == SKSE::GetPluginHandle();
    if (!movementLease && owner != SKSE::kInvalidPluginHandle) return;
    const bool locked = movement->GetTargetLockState();
    observedTarget = movement->GetCurrentTarget();
    if (locked) {
        const auto actor = observedTarget.get();
        if (!actor || !actor->Is3DLoaded()) return;
        const auto now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (actor->IsDead()) {
            if (movementLease) observedLock = true; // still require an actual disable acknowledgement
            else if (lockRetargetWindow.awaiting(true,now)) observedLock = false;
            return;
        }
    }
    lockRetargetWindow.started.reset();
    observedLock = locked;
}
void checkLockBindings() {
    const auto map = RE::ControlMap::GetSingleton();
    // U is a test default, never a second consumer of a native/Camera binding.
    const bool bindingsSafe = map && map->GetUserEventName(22,RE::INPUT_DEVICE::kKeyboard).empty() &&
        map->GetMappedKey("Toggle POV",RE::INPUT_DEVICE::kGamepad) == 128 &&
        settings.toggleKey != 22 && settings.recenterKey != 22 &&
        settings.toggleGamepad != 128 && settings.recenterGamepad != 128;
    if (!bindingsSafe) {
        lockConfigSafe = false;
        if (!lockConfigLogged) {
            log("Target lock refused: U conflicts or gamepad POV is not R3; lease held, bindings unchanged.");
            lockConfigLogged = true;
        }
    }
}
void updateTargetLockMain(Context& c);
void updateTargetLock(Context& c) {
    if (!ready || !lockHooksReady) {
        interruptTargetLock();
        if (onTargetLockThread() && movement && !movementLease) static_cast<void>(acquireMovementLease());
        c.prerequisites = false; return;
    }
    if (!onTargetLockThread()) {
        // Input and menu callbacks can run on other threads: use the last main-thread
        // decision so F6/F7/F8 keep working, and leave all lease work to the main thread.
        c.prerequisites = c.prerequisites && publishedLockPrerequisites.load();
        c.targetLock = publishedTargetLock.load();
        return;
    }
    updateTargetLockMain(c);
    publishedLockPrerequisites.store(c.prerequisites);
    publishedTargetLock.store(c.targetLock);
}
void updateTargetLockMain(Context& c) {
    observeTargetLock();
    consumeLockInterrupt();
    if (lockConfigRefresh.consume()) { movementConfigSafe = checkLockMovementConfig(); checkLockBindings(); }
    c.prerequisites = c.prerequisites && movementConfigSafe;
    const bool eligible = c.enabled && c.prerequisites && c.world && !c.special && !c.menu &&
        !c.firstPerson && !c.precision && !c.slide;
    const bool safe = lockConfigSafe && indicatorReady();
    if (!eligible || !safe || !observedLock) interruptTargetLock();
    consumeLockInterrupt();
    const auto decision = targetLockLease({targetLockEnabled,eligible,lockConfigSafe,indicatorReady(),
        movement != nullptr,movementLease,observedLock,lockInputIdle()});
    {
        // One bounded line per change, so a refusal can never be silent.
        static std::uint32_t lastState = UINT32_MAX;
        const std::uint32_t state = (eligible?1u:0u)|(lockConfigSafe?2u:0u)|(indicatorReady()?4u:0u)|(observedLock?8u:0u)|
            (observedLock.value_or(false)?16u:0u)|(movementLease?32u:0u)|(lockInputIdle()?64u:0u)|(static_cast<unsigned>(decision.action)<<8);
        if (state != lastState && lockStateLogs < 256) {
            lastState = state; ++lockStateLogs;
            char line[192]{};
            std::snprintf(line,sizeof(line),"Target lock state eligible=%u config=%u indicator=%u observed=%u locked=%u lease=%u input_idle=%u action=%u",
                state&1u,(state>>1)&1u,(state>>2)&1u,(state>>3)&1u,(state>>4)&1u,(state>>5)&1u,(state>>6)&1u,state>>8);
            log(line);
        }
    }
    if (decision.action == LockLeaseAction::acquire) {
        if (movement && !movementLease) static_cast<void>(acquireMovementLease());
    } else if (decision.action == LockLeaseAction::release) {
        // Serialize the final pending check with cross-thread interrupt requests.
        // An interrupt arriving after observation must win before this release.
        if (!lockInterrupt.releaseIfClear([&] {
            const auto result = movement->ReleaseDisableTargetLock(SKSE::GetPluginHandle());
            movementLease = false;
            lockLeaseLog.previous.reset();
            if (result == TDM_API::APIResult::OK)
                log("TDM target-lock-disable lease released after unlocked acknowledgement and idle input.");
            else {
                noteLockInputActivity();
                static_cast<void>(acquireMovementLease());
            }
        })) consumeLockInterrupt();
    }
    c.targetLock = decision.lockStateAllowed && !movementLease;
    if (!c.targetLock) stopIndicator();
    else {
        const auto actor = observedTarget.get();
        if (!actor || actor->IsDead() || !actor->Is3DLoaded()) {
            c.targetLock = c.prerequisites = false; interruptTargetLock(); return;
        }
        const RE::ObjectRefHandle target{observedTarget};
        const auto now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto processes = RE::ProcessLists::GetSingleton();
        if (indicatorTarget != target || !indicatorAttempt.started) {
            stopIndicator();
            std::vector<std::uint32_t> previous;
            processes->ForEachShaderEffect([&](RE::ShaderReferenceEffect* effect) {
                if (effect->target == target && effect->effectData == indicatorShader) previous.push_back(effect->effectID);
                return RE::BSContainer::ForEachResult::kContinue;
            });
            indicatorAttempt.begin(std::move(previous),now);
            indicatorTarget = target;
            // Exact 1.7.104 tail calls do not return an effect pointer.
            static_cast<void>(actor->ApplyEffectShader(indicatorShader,LockIndicatorAttempt::duration));
        }
        bool alive = false;
        processes->ForEachShaderEffect([&](RE::ShaderReferenceEffect* effect) {
            if (effect->finished || effect->target != target || effect->effectData != indicatorShader)
                return RE::BSContainer::ForEachResult::kContinue;
            if (indicatorAttempt.waiting(now) && indicatorAttempt.isNew(effect->effectID)) indicatorAttempt.effect = effect->effectID;
            if (indicatorAttempt.effect && effect->effectID == *indicatorAttempt.effect) {
                alive = true;
                // Save serializes lifetime AND age. A stale instance has at
                // most two seconds of unpaused effect updates left after load.
                effect->lifetime = effect->age+LockIndicatorAttempt::duration;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if (!alive && !indicatorAttempt.waiting(now)) {
            c.targetLock = false; interruptTargetLock();
            log("Target lock refused: owned indicator absent/ended after bounded apply window; retry after a fresh lock.");
        }
    }
    // A held disable lease may still be awaiting TDM's unpaused update.
    c.prerequisites = c.prerequisites && observedLock.has_value() &&
        ((movementLease && (!*observedLock || c.firstPerson)) || (!movementLease && targetLockEnabled && safe));
}
void notifyFailure() {
    if (!failureNotified && enableIntent.failureEligible(prerequisiteFailed)) {
        RE::SendHUDMessage::ShowHUDMessage("SkyCRPG Camera inactive: check SkyCRPG_Camera.log");
        failureNotified = true;
    }
}
void resetObstruction() {
    obstructionPolicy.reset(); obstructionFrame = {};
    obstructionSampleTime = {}; obstructionCell = 0;
    resetObstructionZoom(adjustedZoomWritten,lastAdjustedZoom,applyZoom);
}
void suspend() { resetObstruction(); recenter = false; for (auto& key : keys) key.suspend(); }
bool focused() {
    const auto ui = RE::UI::GetSingleton();
    if (!ui || ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsModalMenuOpen()) return true;
    for (const auto& menu : ui->menuStack) {
        if (menu && menu->menuFlags.any(RE::UI_MENU_FLAGS::kUsesCursor,RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kModal)) return true;
    }
    return ui->IsMenuOpen("Dialogue Menu") || ui->IsMenuOpen("Console") || ui->IsMenuOpen("Loading Menu");
}
bool providerChainReady() {
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_ThirdPersonState[0]};
    const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
    const auto module = GetModuleHandleW(L"SmoothCam.dll");
    for (const auto slot : {3,15}) {
        MEMORY_BASIC_INFORMATION memory{};
        if (!VirtualQuery(reinterpret_cast<const void*>(slots[slot]),&memory,sizeof(memory)) || memory.AllocationBase != module) return false;
    }
    return module != nullptr;
}
Context context(bool* hardSpecial = nullptr, bool allowLeaseRequest = false, bool* failure = nullptr,
    bool readOnlyLock = false) {
    Context c;
    if (hardSpecial) *hardSpecial = true;
    c.enabled = settings.enabled;
    const bool providerEnabled = provider && provider->IsCameraEnabled();
    const bool baseReady = compatibleSettings && movementConfigSafe && ready && movement && providerEnabled &&
        provider->GetCameraOwner() == SKSE::kInvalidPluginHandle && providerChainReady();
    const bool lockThread = targetLockEnabled && onTargetLockThread();
    const bool leaseReady = !targetLockEnabled ? movement && movementLease && !movement->GetTargetLockState() :
        readOnlyLock ? publishedLockPrerequisites.load() :
        lockThread && movement && (movementLease ? !movement->GetTargetLockState() : lockConfigSafe && indicatorReady());
    c.prerequisites = targetLockEnabled ? baseReady : phase4Prerequisites(baseReady,leaseReady,allowLeaseRequest,movementLease);
    if (targetLockEnabled && readOnlyLock) {
        c.prerequisites = c.prerequisites && leaseReady;
        c.targetLock = publishedTargetLock.load();
    }
    // An intentionally disabled SmoothCam (F8) is not a failure.
    if (failure) *failure = prerequisiteFailure(baseReady,leaseReady,
        !provider || providerEnabled || !ready || !compatibleSettings || !movementConfigSafe || !movement);
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    c.world = !loading && player && player->GetParentCell() && player->Is3DLoaded() && !player->IsDead() && camera && camera->currentState;
    if (!c.world) {
        if (targetLockEnabled && !readOnlyLock) updateTargetLock(c);
        return c;
    }
    c.firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    c.tween = camera->currentState->id == RE::CameraState::kTween;
    const bool stateSpecial = !c.firstPerson && camera->currentState->id != RE::CameraState::kThirdPerson;
    const auto actorState = player->AsActorState();
    Hazards hazards{player->IsOnMount(), player->IsInKillMove(), actorState->IsSwimming(),
        actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal, actorState->IsBleedingOut(),
        false, false};
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    hazards.beast = controls && controls->data.povBeastMode;
    hazards.scripted = !controlMap || !controls || controls->data.povScriptMode;
    c.slide = controls && controls->data.fovSlideMode;
    const bool hazard = specialHazard(hazards);
    c.special = stateSpecial || hazard;
    if (!c.firstPerson && camera->currentState->id == RE::CameraState::kThirdPerson &&
        static_cast<RE::ThirdPersonState*>(camera->currentState.get())->stateNotActive) c.special = true;
    // Tween is indeterminate, but real non-camera hazards remain hard.
    if (hardSpecial) *hardSpecial = c.tween ? hazard : c.special;
    if (controlMap && (!controlMap->IsLookingControlsEnabled() || !controlMap->IsPOVSwitchControlsEnabled())) c.special = true;
    c.menu = focused();
    // DYL-158 supplies precision/targeting intervention; select() reserves their priority.
    c.obstruction = obstructionFrame.obstructed;
    if (targetLockEnabled && !readOnlyLock) updateTargetLock(c);
    return c;
}
RE::NiPoint3 thinGoal() {
    RE::NiPoint3 goal{},local{};
    provider->GetGoalPosition(RE::PlayerCharacter::GetSingleton(),goal,local);
    return goal;
}
RE::NiPoint3 thinFocus() {
    RE::NiPoint3 goal{},local{};
    provider->GetGoalPosition(RE::PlayerCharacter::GetSingleton(),goal,local);
    return {goal.x-local.x,goal.y-local.y,goal.z-local.z};
}
bool thinEligible() {
    // The all-state thin tick observes lock decisions; it never advances them.
    const auto c = context(nullptr,false,nullptr,true);
    const auto state = select(c);
    thin::runtime->ineligibleCause = state == State::disabled ? thin::Exit::disable :
        c.firstPerson ? thin::Exit::firstPerson : c.menu ? thin::Exit::menu : thin::Exit::special;
    return thin::eligible(state) && !c.slide;
}
bool noActiveMenus() {
    const auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    const auto hud = ui->GetMenu("HUD Menu");
    const auto cursor = ui->GetMenu("Cursor Menu");
    const auto fader = ui->GetMenu("Fader Menu");
    const auto mist = ui->GetMenu("Mist Menu");
    for (const auto& menu : ui->menuStack) {
        if (!menu || (menu.get() != hud.get() && menu.get() != cursor.get() &&
            menu.get() != fader.get() && menu.get() != mist.get())) return false;
    }
    return true;
}
bool lockBlockingMenu(const RE::MenuOpenCloseEvent* event) {
    if (!event) return false;
    const auto& name = event->menuName;
    if (name == "HUD Menu" || name == "Cursor Menu" || name == "Fader Menu" || name == "Mist Menu") return false;
    if (name == "Dialogue Menu" || name == "Console" || name == "Loading Menu") return true;
    const auto ui = RE::UI::GetSingleton();
    if (!ui) return true;
    const auto menu = ui->GetMenu(name);
    return menu && menu->menuFlags.any(RE::UI_MENU_FLAGS::kPausesGame,
        RE::UI_MENU_FLAGS::kUsesCursor,RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kModal);
}
void notifyLockRefusal(Context c) {
    if (targetLockEnabled && !lockRefusalNotified && !lockConfigSafe &&
        c.world && !c.menu && !loading && noActiveMenus()) {
        RE::SendHUDMessage::ShowHUDMessage("SkyCRPG target lock unavailable: check the TDM overlay and Camera log");
        lockRefusalNotified = true;
    }
}
void observePrerequisiteFailure(Context c, bool failure) {
    if (failureNotified) return;
    const auto now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (failureDebounce.observe(enableIntent.failureEligible(failure) && c.world && !c.menu && !loading && noActiveMenus(),now)) {
        if (!prerequisiteFailed) log("Camera prerequisite or third-person ownership unavailable; policy suspended.");
        prerequisiteFailed = true;
        notifyFailure();
    }
}
PrecisionTraceState traceState(Context c, bool hard, std::size_t key) {
    PrecisionTraceState result{c,hard,ownFirstPerson,keys[key].armed,keys[key].down};
    if (!precisionTraceEnabled) return result;
    if (!c.world) { result.hazardBits = UINT32_MAX; return result; }
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (camera && camera->currentState) result.cameraState = static_cast<std::uint32_t>(camera->currentState->id);
    if (player) {
        result.hazardBits |= std::uint32_t(player->IsOnMount()) << 0;
        result.hazardBits |= std::uint32_t(player->IsInKillMove()) << 1;
        result.hazardBits |= std::uint32_t(player->AsActorState()->IsSwimming()) << 2;
    }
    result.hazardBits |= std::uint32_t(!controls) << 3;
    result.hazardBits |= std::uint32_t(!controlMap) << 4;
    if (controls) {
        result.hazardBits |= std::uint32_t(controls->data.povScriptMode) << 5;
        result.hazardBits |= std::uint32_t(controls->data.povBeastMode) << 6;
        result.hazardBits |= std::uint32_t(controls->data.fovSlideMode) << 7;
    }
    if (camera && camera->currentState && camera->currentState->id == RE::CameraState::kThirdPerson)
        result.hazardBits |= std::uint32_t(static_cast<RE::ThirdPersonState*>(camera->currentState.get())->stateNotActive) << 8;
    return result;
}
void tracePrecision(PrecisionTraceEvent event, PrecisionTraceState before, PrecisionTraceState after,
    unsigned device = 0, bool pressed = false, bool edge = false, unsigned decision = 0) {
    if (!precisionTraceEnabled || !precisionTraceBudget.take()) return;
    char line[192]{};
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (formatPrecisionTrace(line,sizeof(line),{event,before,after,device,pressed,edge,decision,static_cast<std::uint64_t>(now)})) log(line);
}
void lifecycleLog(const char* event, Context c, unsigned stateId) {
    if (!lifecycleTraceEnabled || lifecycleLogCount >= 128) return;
    ++lifecycleLogCount;
    char line[256]{};
    if (!targetLockEnabled) {
        std::snprintf(line,sizeof(line),"Lifecycle event=%s hook=%s state=%u decision=%u owned=%u",event,
            std::strcmp(event,"observe") == 0 ? "update" : "event",stateId,
            static_cast<unsigned>(select(c)),static_cast<unsigned>(ownFirstPerson));
    } else if (onTargetLockThread()) {
        std::snprintf(line,sizeof(line),"Lifecycle event=%s hook=%s state=%u decision=%u owned=%u lease=%u lock=%d target=%08X",event,
            std::strcmp(event,"observe") == 0 ? "update" : "event",stateId,
            static_cast<unsigned>(select(c)),static_cast<unsigned>(ownFirstPerson),static_cast<unsigned>(movementLease),
            observedLock ? (*observedLock ? 1 : 0) : -1,observedTarget.native_handle());
    } else return;
    log(line);
}
void resetLifecycle(const char* event) {
    thin::requestRestore(std::strstr(event,"load") ? thin::Exit::load : thin::Exit::cell);
    if (thin::enabled() && thin::onMain() && std::strcmp(event,"player-detach") == 0)
        thin::restoreAll(thin::Exit::cell);
    interruptTargetLock();
    resetTransient(ownFirstPerson,recenter,applyZoom,keys);
    resetObstruction();
    lifecycleTrace.resetCandidate();
    lifecycleLog(event,Context{},UINT32_MAX);
}
void observeLifecycle(Context c, bool hard) {
    ownFirstPerson = retainPrecisionOwnership(c,ownFirstPerson,hard);
    const auto camera = RE::PlayerCamera::GetSingleton();
    const unsigned id = camera && camera->currentState ? static_cast<unsigned>(camera->currentState->id) : UINT32_MAX;
    if (lifecycleTraceEnabled && lifecycleTrace.observe(select(c),id)) lifecycleLog("observe",c,id);
    // Obstruction is a normal-framing sub-state; suspending on it would reset its latch.
    if (!heldFramingAllowed(c)) suspend();
    else if (c.targetLock) resetObstruction();
}
void sampleObstruction(RE::ThirdPersonState* state) {
    auto c = context();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    // No combat-mode classification is assumed on this branch. Conservatively
    // release automatic framing for ALL drawn weapons/spells pending DYL-158.
    if (player && precisionSuppressesObstruction(player->IsWeaponDrawn())) c.precision = true;
    if (!obstructionSettings.enabled || !obstructionFramingAllowed(c) || !camera || !player ||
        camera->currentState.get() != state || !state->freeRotationEnabled || !camera->cameraRoot ||
        camera->cameraTarget.get().get() != player) {
        resetObstruction(); return;
    }
    const auto cell = player->GetParentCell()->GetFormID();
    if (obstructionCell != cell) resetObstruction();
    obstructionCell = cell;
    // Do not accumulate slide duration into the next obstruction step.
    if (pauseCorrection(c)) { obstructionSampleTime = std::chrono::steady_clock::now(); return; }
    const auto now = std::chrono::steady_clock::now();
    if (obstructionSampleTime == std::chrono::steady_clock::time_point{}) {
        obstructionSampleTime = now; return;
    }
    const double dt = std::chrono::duration<double>(now-obstructionSampleTime).count();
    obstructionSampleTime = now;
    // BEFORE native update: root still contains the last provider-rendered
    // result. GetLastCameraPosition is pre-collision and must not be used here.
    // This is an approximate current-goal signal, not a provider collision flag;
    // interpolation/update ordering still requires exact-binary route validation.
    RE::NiPoint3 goal{}, local{};
    provider->GetGoalPosition(player,goal,local);
    const auto actual = camera->cameraRoot->world.translate;
    const ObstructionPoint focus{static_cast<double>(goal.x)-local.x,
                                 static_cast<double>(goal.y)-local.y,
                                 static_cast<double>(goal.z)-local.z};
    obstructionFrame = obstructionPolicy.step(c,thin::obstructionSample(cameraCompression(
        {actual.x,actual.y,actual.z},{goal.x,goal.y,goal.z},focus)),dt,obstructionSettings);
}
void look(RE::ThirdPersonState* state, const RE::NiPoint2& input) {
    if (targetLockEnabled && lockFrameTraceEnabled && lockTraceWindow.budget.lines < LockFrameTraceBudget::limit && onTargetLockThread()) {
        lockLookInput.x += input.x; lockLookInput.y += input.y; ++lockLookCalls;
    }
    const auto c = context();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!heldFramingAllowed(c) || !camera || camera->currentState.get() != state || !state->freeRotationEnabled) {
        suspend(); previousLook(state,input); return;
    }
    if (pauseCorrection(c)) { previousLook(state,input); return; }
    auto setting = RE::GetINISetting("fFreeRotationSpeed:Camera");
    static REL::Relocation<float*> delta{REL::ID(410200)};
    const float dt = *delta;
    const float gain = setting ? setting->GetFloat() : 0.0f;
    if (!std::isfinite(dt) || dt <= 0 || dt > .1f || !std::isfinite(gain) || gain <= 0) {
        suspend(); previousLook(state,input); return;
    }
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player->IsWeaponDrawn() || c.targetLock) resetObstruction();
    const float currentPitch = player->GetAngleX()-state->freeRotation.y;
    RE::NiPoint2 filtered = input;
    const float pitch = obstructionPitch(settings.pitch,obstructionFrame.blend,obstructionSettings);
    filtered.y = correction(currentPitch-radians(pitch),radians(settings.pitchSpeed),gain,dt);
    if (recenter) {
        const float error = wrap(-state->freeRotation.x);
        filtered.x = correction(error,radians(settings.recenterSpeed),gain,dt);
        if (std::abs(error) < radians(.2f)) recenter = false;
    }
    previousLook(state,filtered); // exactly once, retaining provider input lock + native update
}
void traceLockFrame(Context c);
void update(RE::ThirdPersonState* state,RE::BSTSmartPointer<RE::TESCameraState>& next) {
    if (next && next.get() != state) { interruptTargetLock(); resetObstruction(); }
    else sampleObstruction(state);
    previousUpdate(state,next); // native first; outer SmoothCam wrapper performs transforms AFTER us
    bool hard = true;
    bool failure = false;
    const auto c = context(&hard,false,&failure);
    observeLifecycle(c,hard);
    observePrerequisiteFailure(c,failure);
    if (targetLockEnabled) traceLockFrame(c);
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!heldFramingAllowed(c) || !camera || camera->currentState.get() != state || (next && next.get() != state)) {
        resetObstruction(); return;
    }
    if (RE::PlayerCharacter::GetSingleton()->IsWeaponDrawn()) resetObstruction();
    // Native controller POV-slide zoom bypasses HandleLookInput, so bound the
    // requested scalar after native update and BEFORE the outer provider update.
    // stateNotActive/POV transitions are excluded; current distance is never written.
    auto framing = c; framing.obstruction = false;
    settings.zoom = preferredZoomRequest(state->targetZoomOffset,lastAdjustedZoom,adjustedZoomWritten,settings,
        applyDeferredPreferences(framing,applyZoom,camera->currentState.get() == state));
    state->targetZoomOffset = obstructionZoom(settings.zoom,obstructionFrame.blend,obstructionSettings);
    lastAdjustedZoom = state->targetZoomOffset;
    adjustedZoomWritten = obstructionFrame.blend > 0;
    applyZoom = false;
}
void pollLockInput(RE::InputEvent* input) {
    if (!targetLockEnabled) return;
    static std::atomic_flag reported = ATOMIC_FLAG_INIT;
    if (!reported.test_and_set()) {
        const auto main = RE::Main::GetSingleton();
        char line[96]{};
        std::snprintf(line,sizeof(line),"Target lock input sink thread=%lu main=%lu",
            static_cast<unsigned long>(GetCurrentThreadId()),static_cast<unsigned long>(main ? main->threadID : 0));
        log(line);
    }
    for (auto event = input; event; event = event->next) {
        const auto button = event->AsButtonEvent();
        if (!button) continue;
        if ((event->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == 22) ||
            (event->GetDevice() == RE::INPUT_DEVICE::kGamepad && button->GetIDCode() == 128)) {
            noteLockInputActivity();
        }
    }
}
void traceLockFrame(Context c) {
    if (!targetLockEnabled || !lockFrameTraceEnabled || lockTraceWindow.budget.lines >= LockFrameTraceBudget::limit || !onTargetLockThread()) return;
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    constexpr float unavailable = std::numeric_limits<float>::quiet_NaN();
    float worldPitch = unavailable, worldYaw = unavailable, netPitch = unavailable, netYaw = unavailable;
    // Input dispatch sees the last completed SmoothCam render, outside our
    // inner native-update hook. Trace its world forward axis as well as angles.
    RE::NiPoint3 forward{unavailable,unavailable,unavailable};
    if (camera && camera->cameraRoot) {
        const auto& rotation = camera->cameraRoot->world.rotate;
        forward = {rotation.entry[0][1],rotation.entry[1][1],rotation.entry[2][1]};
        worldPitch = std::atan2(-forward.z,std::hypot(forward.x,forward.y));
        worldYaw = lockWorldYaw(forward.x,forward.y);
    }
    if (player && camera && camera->currentState && camera->currentState->id == RE::CameraState::kThirdPerson) {
        const auto state = static_cast<RE::ThirdPersonState*>(camera->currentState.get());
        netPitch = player->GetAngleX()-state->freeRotation.y;
        netYaw = wrap(player->GetAngleZ()+state->freeRotation.x);
    }
    LockTraceRow line{};
    static std::uint64_t frame = 0;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::snprintf(line.data(),line.size(),"TargetLock event=frame frame=%llu us=%llu decision=%u lease=%u lock=%d target=%08X fresh=%u worldPitch=%.7f worldYaw=%.7f netPitch=%.7f netYaw=%.7f forward=%.7f,%.7f,%.7f look=%.7f,%.7f lookCalls=%u sample=last-rendered",
        static_cast<unsigned long long>(++frame),static_cast<unsigned long long>(now),static_cast<unsigned>(select(c)),
        static_cast<unsigned>(movementLease),observedLock ? (*observedLock ? 1 : 0) : -1,
        observedTarget.native_handle(),static_cast<unsigned>(lockInputIdle()),worldPitch,worldYaw,netPitch,netYaw,
        forward.x,forward.y,forward.z,lockLookInput.x,lockLookInput.y,lockLookCalls);
    lockTraceWindow.sample(line,observedLock.value_or(false),[](const LockTraceRow& row) { lockFrameWriter.append(row); });
    lockLookInput = {}; lockLookCalls = 0;
}
class Events final : public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
    public RE::BSTEventSink<RE::TESLoadGameEvent>, public RE::BSTEventSink<RE::TESCellAttachDetachEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent*, RE::BSTEventSource<RE::TESLoadGameEvent>*) override {
        resetLifecycle("load-game");
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override {
        if (event && event->reference.get() == RE::PlayerCharacter::GetSingleton())
            resetLifecycle(event->attached ? "player-attach" : "player-detach");
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        // Queue before any context() can consider releasing the lease.
        if (targetLockEnabled) {
            lockConfigRefresh.request();
            if (!lockBlockingMenu(event)) return RE::BSEventNotifyControl::kContinue;
            interruptTargetLock();
        }
        bool hard = true;
        const auto c = precisionTraceEnabled ? context(&hard) : Context{};
        const auto before = traceState(c,hard,0);
        if (!targetLockEnabled) movementConfigSafe = checkMovementConfig();
        failureDebounce.observe(false,0);
        suspend();
        if (precisionTraceEnabled && event && (c.world || ownFirstPerson)) {
            bool afterHard = true;
            const auto after = context(&afterHard);
            tracePrecision(event->opening ? PrecisionTraceEvent::menuOpen : PrecisionTraceEvent::menuClose,
                before,traceState(after,afterHard,0));
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        if (thin::enabled()) {
            for (auto event = *events; event; event = event->next) {
                const auto button = event->AsButtonEvent();
                if (button && button->IsDown() && ((event->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                    (button->GetIDCode() == 66 || button->GetIDCode() == 64)) || button->QUserEvent() == "Toggle POV"))
                    thin::requestRestore(button->GetIDCode() == 66 ? thin::Exit::f8 : thin::Exit::firstPerson);
            }
        }
        if (targetLockEnabled) {
            pollLockInput(*events);
            for (auto event = *events; event; event = event->next) {
                const auto button = event->AsButtonEvent();
                if (button && button->IsDown() && event->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == 66)
                    interruptTargetLock();
            }
        }
        bool hardSpecial = true;
        bool failure = false;
        const auto c = context(&hardSpecial,!settings.enabled && !movementLease,&failure);
        observePrerequisiteFailure(c,failure);
        if (targetLockEnabled) notifyLockRefusal(c);
        const bool ownedBefore = ownFirstPerson;
        ownFirstPerson = retainPrecisionOwnership(c,ownFirstPerson,hardSpecial);
        if (precisionTraceEnabled && ownedBefore != ownFirstPerson) {
            const auto after = traceState(c,hardSpecial,0);
            auto before = after; before.owned = ownedBefore;
            tracePrecision(PrecisionTraceEvent::retention,before,after);
        }
        // Deliberate F6 can return from its OWN first-person request; native POV never auto-restores.
        const bool lifecycleSafe = c.prerequisites && c.world && !c.special && !c.menu;
        const bool safe = lifecycleSafe && !c.slide;
        if (!lifecycleSafe) suspend();
        for (auto event = *events; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button) continue;
            if (button->IsDown() && button->QUserEvent() == "Toggle POV") {
                const auto before = traceState(c,hardSpecial,0);
                ownFirstPerson = false;
                tracePrecision(PrecisionTraceEvent::nativePov,before,traceState(c,hardSpecial,0),
                    event->GetDevice() == RE::INPUT_DEVICE::kGamepad ? 2u : 1u,true,true);
            }
            const auto device = event->GetDevice();
            const bool pad = device == RE::INPUT_DEVICE::kGamepad;
            if (!pad && device != RE::INPUT_DEVICE::kKeyboard) continue;
            const int id = static_cast<int>(button->GetIDCode());
            const std::array<int,2> bindings{pad?settings.toggleGamepad:settings.toggleKey,pad?settings.recenterGamepad:settings.recenterKey};
            for (std::size_t i = 0;i < bindings.size();++i) {
                if (bindings[i] < 0 || id != bindings[i]) continue;
                const auto keyIndex = (pad?2u:0u)+i;
                const auto before = traceState(c,hardSpecial,keyIndex);
                if (i == 0 && (button->IsDown() || (!button->IsPressed() && before.down)))
                    tracePrecision(button->IsPressed() ? PrecisionTraceEvent::togglePress : PrecisionTraceEvent::toggleRelease,
                        before,before,pad?2u:1u,button->IsPressed(),button->IsDown());
                if (!safe) {
                    // A refused opt-in policy must still allow the native F6
                    // escape. It cannot re-enable unsafe held framing.
                    if (targetLockEnabled && i == 0 && button->IsDown() && c.world &&
                        !c.menu && !c.special && !c.slide && !c.firstPerson) {
                        const auto map = RE::ControlMap::GetSingleton();
                        if (map && map->GetUserEventName(button->GetIDCode(),device).empty() && button->QUserEvent().empty()) {
                            interruptTargetLock();
                            settings.enabled = false; ownFirstPerson = false;
                            static_cast<void>(RE::PlayerCamera::GetSingleton()->ForceFirstPerson());
                            savePreferences();
                        }
                    }
                    keys[keyIndex].observeSuppressed(button->IsPressed());
                    if (i == 0 && (before.down != keys[keyIndex].down || before.armed != keys[keyIndex].armed))
                        tracePrecision(PrecisionTraceEvent::toggleSuppressed,before,traceState(c,hardSpecial,keyIndex),pad?2u:1u,button->IsPressed(),button->IsDown());
                    continue;
                }
                auto map = RE::ControlMap::GetSingleton();
                // Never dispatch custom action on a mapped native gameplay button.
                if (!map || !map->GetUserEventName(button->GetIDCode(),device).empty() || !button->QUserEvent().empty()) {
                    if (i == 0 && button->IsDown()) tracePrecision(PrecisionTraceEvent::toggleMapped,before,before,pad?2u:1u,button->IsPressed(),button->IsDown());
                    continue;
                }
                if (!keys[keyIndex].update(button->IsPressed(),button->IsDown())) {
                    if (i == 0 && button->IsDown()) tracePrecision(PrecisionTraceEvent::toggleNoEdge,before,traceState(c,hardSpecial,keyIndex),pad?2u:1u,button->IsPressed(),button->IsDown());
                    continue;
                }
                if (i == 0) {
                    auto camera = RE::PlayerCamera::GetSingleton();
                    const auto request = toggleRequest(c,ownFirstPerson);
                    enableIntent.request(request);
                    tracePrecision(PrecisionTraceEvent::toggleDecision,before,traceState(c,hardSpecial,keyIndex),pad?2u:1u,true,true,static_cast<unsigned>(request));
                    if (request == ToggleRequest::firstPerson) {
                        settings.enabled = false; ownFirstPerson = false; interruptTargetLock(); suspend();
                        static_cast<void>(camera->ForceFirstPerson());
                        // Do not claim a late or unrelated first-person transition.
                        bool afterHard = true;
                        const auto after = context(&afterHard);
                        ownFirstPerson = claimPrecisionOwnership(c,after);
                        if (precisionTraceEnabled) {
                            tracePrecision(PrecisionTraceEvent::toggleClaim,before,traceState(after,afterHard,keyIndex),pad?2u:1u,true,true);
                        }
                        savePreferences();
                    } else if (request == ToggleRequest::enableOnly) {
                        if (leaseWanted(false,true,movementLease) && !acquireMovementLease()) continue;
                        if (!enableAfterLease(movementLease)) continue;
                        settings.enabled = true; ownFirstPerson = false; applyZoom = true; suspend(); savePreferences();
                    } else if (request == ToggleRequest::thirdPerson) {
                        if (leaseWanted(false,true,movementLease) && !acquireMovementLease()) continue;
                        if (!enableAfterLease(movementLease)) continue;
                        settings.enabled = true; ownFirstPerson = false; suspend(); applyZoom = true; static_cast<void>(camera->ForceThirdPerson()); savePreferences();
                    }
                } else if (heldFramingAllowed(c)) {
                    recenter = true;
                    if (const auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->currentState)
                        lifecycleLog("recenter",c,static_cast<unsigned>(camera->currentState->id));
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
Events events;
void messages(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
    case SKSE::MessagingInterface::kPostLoad:
        if (!SmoothCamAPI::RegisterInterfaceLoaderCallback(SKSE::GetMessagingInterface(),[](void* api,SmoothCamAPI::InterfaceVersion version) {
            if (version == SmoothCamAPI::InterfaceVersion::V3) provider = static_cast<SmoothCamAPI::IVSmoothCam3*>(api);
        })) log("SmoothCam API listener unavailable; controller suspended.");
        break;
    case SKSE::MessagingInterface::kPostPostLoad:
        if (!SmoothCamAPI::RequestInterface(SKSE::GetMessagingInterface())) log("SmoothCam API unavailable; controller suspended.");
        break;
    case SKSE::MessagingInterface::kDataLoaded: {
        static bool dataLoaded = false;
        if (dataLoaded) break; // no duplicate hook/sinks on repeated notifications
        dataLoaded = true;
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&events));
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
        ready = hashMatches(nullptr,"846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f") &&
            hashMatches(L"SmoothCam.dll","b20bf8a276bf76fbe35277dd9b25c64c57d17bfdcc20daa74a5f941c90f563da") &&
            hashMatches(L"TrueDirectionalMovement.dll","5ba3abd82fe070e2a6e9af7f07eca82b9ddb1a53dcd47bc34af571efab01899f") &&
            hashMatches(L"MCMHelper.dll","d58f433bef7690614d42aaac69cc35f60d179a85429a385e21858a1f276aef38");
        if (!ready) {
            thin::refuseInstall("ThinOccluder: prerequisite hash mismatch; no thin hooks installed.");
            prerequisiteFailed = true; log("Exact executable/provider prerequisite hash mismatch; controller disabled."); break;
        }
        movementConfigSafe = checkMovementConfig();
        if (!movementConfigSafe) { prerequisiteFailed = true; log("TDM camera-yaw config absent/unsafe; custom policy suspended until corrected and a menu closes."); }
        movement = static_cast<TDM_API::IVTDM5*>(TDM_API::RequestPluginAPI());
        if (!movement) {
            thin::refuseInstall("ThinOccluder: TDM V5 API absent; no thin hooks installed.");
            prerequisiteFailed = true; ready = false; log("TDM V5 API absent; controller suspended."); break;
        }
        if (!targetLockEnabled) {
            if (leaseWanted(settings.enabled,false,movementLease)) {
                if (!acquireMovementLease()) {
                    thin::refuseInstall("ThinOccluder: movement lease unavailable; no thin hooks installed.");
                    ready = false; break;
                }
            } else if (!settings.enabled) log("TDM target-lock-disable lease not requested because Camera is disabled.");
        } else {
            // TDM's disable-request API permits startup acquisition without a
            // thread-ID match. Effect mutation and release remain gated.
            static_cast<void>(acquireMovementLease());
            interruptTargetLock();
            lockConfigRefresh.request();
        }
        if (targetLockEnabled) {
            const auto data = RE::TESDataHandler::GetSingleton();
            indicatorShader = data ? data->LookupForm<RE::TESEffectShader>(indicatorShaderForm,"Skyrim.esm") : nullptr;
            if (!indicatorShader) log("Target lock refused: configured Skyrim.esm indicator form is not an EFSH; lease held.");
        }
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_ThirdPersonState[0]};
        const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
        const REL::Relocation<std::uintptr_t> nativeUpdate{REL::ID(50896)};
        const REL::Relocation<std::uintptr_t> nativeLook{REL::ID(50914)};
        if (thin::enabled()) {
            thin::guardResult("third-person-update-before-SmoothCam",slots[3] == nativeUpdate.address());
            thin::guardResult("third-person-look-before-SmoothCam",slots[15] == nativeLook.address());
        }
        if (slots[3] != nativeUpdate.address() || slots[15] != nativeLook.address()) {
            if (targetLockEnabled) {
                if (!movementLease) static_cast<void>(acquireMovementLease());
            } else {
                if (movementLease) static_cast<void>(movement->ReleaseDisableTargetLock(SKSE::GetPluginHandle()));
                movementLease = false;
            }
            ready = false; prerequisiteFailed = true;
            log("Unexpected camera hook order/owner before SmoothCam deferred attach; policy left inert.");
            thin::refuseInstall("ThinOccluder: camera slot owner guard failed; no thin hooks installed.");
            return;
        }
        previousLook = reinterpret_cast<Look>(table.write_vfunc(0xF,look));
        previousUpdate = reinterpret_cast<Update>(table.write_vfunc(3,update));
        lockHooksReady = targetLockEnabled;
        thin::installHooks(ready);
        if (const auto source = RE::ScriptEventSourceHolder::GetSingleton()) {
            source->AddEventSink<RE::TESLoadGameEvent>(&events);
            source->AddEventSink<RE::TESCellAttachDetachEvent>(&events);
        }
        log("Camera input policy installed: one chained native look call; SmoothCam sole transform writer.");
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame: thin::setLoaded(false); loading = true; failureDebounce.observe(false,0); resetLifecycle("pre-load"); break;
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame: thin::setLoaded(true); loading = false; resetLifecycle("post-load-or-new-game"); break;
    case SKSE::MessagingInterface::kSaveGame: interruptTargetLock(); savePreferences(); break;
    default: break;
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data;
    data.PluginVersion({SKYCRPG_VERSION_MAJOR,SKYCRPG_VERSION_MINOR,SKYCRPG_VERSION_PATCH,0});
    data.PluginName("SkyCRPG Camera"); data.AuthorName("devils_17");
    data.UsesAddressLibrary(); data.UsesUpdatedStructs();
    data.versionIndependenceEx |= SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5;
    data.CompatibleVersions({supportedRuntime});
    data.xseMinimum = REL::Version{2,3,1,0}.pack();
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (!skse || skse->IsEditor() || skse->RuntimeVersion() != supportedRuntime || skse->SKSEVersion() != REL::Version{2,3,1,0}.pack()) return false;
    std::ofstream(std::filesystem::path(skycrpg::LogDirectory())/L"SkyCRPG_Camera.log",std::ios::trunc).close();
    log("SkyCRPG Camera " SKYCRPG_VERSION " source " SKYCRPG_SOURCE_COMMIT "; native input policy, no camera transform writes.");
    log("runtime check: PASS pinned 1.7.104; exact SKSE 2.3.1; Address Library format 5");
    if (!hashMatches(nullptr,"846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f") ||
        !hashFile(std::filesystem::path(skycrpg::ThisModuleDirectory())/L"versionlib-1-7-104-0.bin","8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842")) {
        log("Executable/Address Library absent or incompatible; staying inert before CommonLib address initialization.");
        return true;
    }
    SKSE::Init(skse,false);
    loadSettings();
    if (targetLockEnabled && lockFrameTraceEnabled)
        lockFrameWriter.start(std::filesystem::path(skycrpg::LogDirectory())/L"SkyCRPG_Camera_TargetLock.log");
    const auto messaging = SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(messages);
}
