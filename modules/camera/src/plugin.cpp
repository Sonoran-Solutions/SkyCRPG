// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "policy.h"
#include "precision_policy.h"
#include "cursor_policy.h"
#include "../../selection/include/skycrpg/SelectionAPI.h"
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
#include <bit>
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
#include "fixed_spike.h"

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
static_assert(offsetof(RE::ProcessLists,magicEffects) == 0x108);
static_assert(offsetof(RE::ProcessLists,magicEffectsLock) == 0x120);
Settings settings;
cursor::Settings cursorSettings;
namespace cursorMode {
bool ownMenu(RE::IMenu* menu);
void requestExit(cursor::Exit reason);
void input(RE::InputEvent* events);
}
ObstructionSettings obstructionSettings;
ObstructionPolicy obstructionPolicy;
ObstructionFrame obstructionFrame;
std::chrono::steady_clock::time_point obstructionSampleTime{};
RE::FormID obstructionCell = 0;
bool adjustedZoomWritten = false;
float lastAdjustedZoom = 0;
std::atomic<bool> ready{false};
bool loading = true, recenter = false;
PrecisionReturn precisionReturn;
PrecisionCameraObservation precisionCameraObservation;
PrecisionDiagnostics precisionDiagnostics;
std::atomic<std::uint64_t> preferencesToSave{0};
std::atomic<bool> deferredIndicatorStop{false};
std::atomic<bool> deferredCameraQueued{false};
HANDLE deferredCameraTimer = nullptr;
const SKSE::TaskInterface* deferredCameraTasks = nullptr;
std::atomic<bool> publishedReturnChainReady{false};
SmoothCamAPI::IVSmoothCam3* provider = nullptr;
TDM_API::IVTDM5* movement = nullptr;
bool movementLease = false, applyZoom = true, compatibleSettings = true, movementConfigSafe = false;
bool targetLockEnabled = false, lockConfigSafe = false;
std::atomic<bool> lockHooksReady{false};
bool lockFrameTraceEnabled = false;
LockTraceWindow lockTraceWindow;
LockFrameWriter lockFrameWriter;
RE::NiPoint2 lockLookInput{};
unsigned lockLookCalls = 0;
LockRetargetWindow lockRetargetWindow;
LockStateChange<TDM_API::APIResult> lockLeaseLog;
bool lockRefusalNotified = false;
std::atomic<bool> lockRefusalRequested{false};
LockInterrupt lockInterrupt;
LockConfigRefresh lockConfigRefresh;
std::atomic_flag deferredConfigBusy = ATOMIC_FLAG_INIT;
std::atomic<bool> lockBindingRefused{false};
std::atomic<unsigned> lockRefreshApplied{0}, deferredTaskRuns{0}, deferredTaskOther{0}, deferredTaskMenuBusy{0};
std::atomic<unsigned> deferredTaskQueuedCount{0}, lockRefreshMainSample{0};
std::optional<RE::BSFixedString> lockPOVEvent; // prepared before callbacks; no interning in Main apply
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
std::atomic<bool> publishedLockMenu{true}, publishedLockMovementConfigSafe{false};
std::atomic<unsigned> lockMenuOpenEpoch{0};
// Lease changes also occur at data load; opt-in off-main input reads only this copy.
std::atomic<bool> publishedMovementLease{false};
unsigned lockStateLogs = 0;
std::optional<bool> observedLock;
RE::ActorHandle observedTarget;
bool precisionTraceEnabled = false, lifecycleTraceEnabled = false;
bool badSettings = false, prerequisiteFailed = false, failureNotified = false;
EnableIntent enableIntent;
unsigned lifecycleLogCount = 0;
LifecycleTraceDebounce lifecycleTrace;
PrecisionTraceBudget precisionTraceBudget;
PrecisionClaimTrace precisionClaimTrace;
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
RE::NiPoint3 fixedPreCollision();
unsigned fixedCause(int state);
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
LockConfigResult readLockMovementConfig() {
    // Match TDM Settings::ReadSettings precedence: provider config, then user MCM.
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768)) return {};
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
    bool contractSafe = true;
    static bool mismatchLogged = false; // serialized IO worker only
    for (const auto& setting : lockSettings) {
        if (!lockSettingMatches(setting,effective(setting.section,setting.key),lockHoldSeconds)) {
            contractSafe = false;
            if (!mismatchLogged) {
                char line[192]{};
                std::snprintf(line,sizeof(line),"Target lock refused: live TDM [%s] %s differs from Camera contract; lease held, file unchanged.",
                    setting.section,setting.key);
                log(line); mismatchLogged = true;
            }
        }
    }
    // The opt-in yaw guard uses the same strict parser as the lock keys.
    return {lockSettingMatches({"DirectionalMovement","uAdjustCameraYawDuringMovement",0},
        effective("DirectionalMovement","uAdjustCameraYawDuringMovement")),contractSafe};
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
    cursorSettings.enabled = flag("CursorMode","bEnabled",0);
    if (cursorSettings.enabled) {
        cursorSettings.toggleKey = number("CursorMode","iToggleKey",48);
        cursorSettings.toggleGamepad = number("CursorMode","iToggleGamepad",-1);
        cursorSettings.sensitivity = number("CursorMode","fSensitivity",65.0f);
        cursorSettings.radius = number("CursorMode","fPickRadiusPixels",48.0f);
        cursorSettings.latchPixels = number("CursorMode","fLatchMovePixels",12.0f);
        cursorSettings.trace = number("CursorMode","iTrace",0u);
        cursorSettings = cursor::validate(cursorSettings,settings);
    }
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
    // P5.5-02 disposable spike: no allocation, hook or evaluation when off.
    if (flag("FixedCameraSpike","bEnabled",0)) {
        fixed::Config f;
        f.enabled = true;
        f.heading = number("FixedCameraSpike","fHeadingDegrees",f.heading);
        f.snapSeconds = number("FixedCameraSpike","fSnapSeconds",f.snapSeconds);
        f.zoom = number("FixedCameraSpike","fZoom",f.zoom);
        f.snapLeftKey = number("FixedCameraSpike","iSnapLeftKey",f.snapLeftKey);
        f.snapRightKey = number("FixedCameraSpike","iSnapRightKey",f.snapRightKey);
        f.castProbe = flag("FixedCameraSpike","iCastProbe",1);
        f.rigTrace = flag("FixedCameraSpike","iRigTrace",0);
        f.roomSeedMode = number("FixedCameraSpike","iRoomSeedMode",f.roomSeedMode);
        f.roomSeedRadius = number("FixedCameraSpike","fRoomSeedRadius",f.roomSeedRadius);
        f.roomTrace = flag("FixedCameraSpike","iRoomTrace",0);
        f.seeThroughMode = number("FixedCameraSpike","iSeeThroughMode",f.seeThroughMode);
        f.cutHeight = number("FixedCameraSpike","fCutHeight",f.cutHeight);
        f.hideRadius = number("FixedCameraSpike","fHideRadius",f.hideRadius);
        f.hidePerFrame = number("FixedCameraSpike","iHideMaxPerFrame",f.hidePerFrame);
        f.hideTotal = number("FixedCameraSpike","iHideMaxTotal",f.hideTotal);
        f.hideTrace = flag("FixedCameraSpike","iHideTrace",0);
        f.holdFade = flag("FixedCameraSpike","bHoldPlayerFade",1);
        // Wave 2 shader cut (iSeeThroughMode=3); unused in every other mode.
        fixed::shader::Config shaderCut;
        shaderCut.height = number("FixedCameraSpike","fShaderCutHeight",shaderCut.height);
        shaderCut.radius = number("FixedCameraSpike","fShaderCutRadius",shaderCut.radius);
        shaderCut.maxPerFrame = number("FixedCameraSpike","iShaderCutMaxPerFrame",shaderCut.maxPerFrame);
        shaderCut.shaders = number("FixedCameraSpike","iShaderCutShaders",shaderCut.shaders);
        shaderCut.trace = flag("FixedCameraSpike","iShaderCutTrace",0);
        // Second pass (owner decision 2026-11-10): on by default in mode 3.
        shaderCut.effect = flag("FixedCameraSpike","bShaderCutEffect",1);
        shaderCut.voidColor = flag("FixedCameraSpike","bVoidColor",1);
        shaderCut.colorText = read("FixedCameraSpike","sVoidColor","0,0,0");
        // Third pass: on by default in mode 3.
        shaderCut.hideSky = flag("FixedCameraSpike","bVoidHideSky",1);
        shaderCut.effectWhole = flag("FixedCameraSpike","bShaderCutEffectWhole",1);
        // Fourth pass (owner decision 2026-11-10, Stage 1 seamless cutaway): Stage 1 by default in mode 3.
        shaderCut.style = number("FixedCameraSpike","iCutStyle",shaderCut.style);
        shaderCut.keepFarGallery = flag("FixedCameraSpike","bCutKeepFarGallery",0);
        shaderCut.styleShadows = flag("FixedCameraSpike","bCutStyleShadows",1);
        // Stage 3 step 3.0: column stacks with an open top at the cut (iCutStyle=1 only).
        shaderCut.stacks = number("FixedCameraSpike","iCutStacks",shaderCut.stacks);
        // Fifth pass (owner go 2026-11-11, Stage 2 of the seamless cutaway): the frame pass, on by default in mode 3.
        shaderCut.frame.enabled = flag("FixedCameraSpike","bFramePass",1);
        shaderCut.frame.band = number("FixedCameraSpike","fFrameBand",shaderCut.frame.band);
        shaderCut.frame.headroom = number("FixedCameraSpike","fFrameBandHeadroom",shaderCut.frame.headroom);
        shaderCut.frame.radiusInner = number("FixedCameraSpike","fFrameRadiusInner",shaderCut.frame.radiusInner);
        shaderCut.frame.radiusOuter = number("FixedCameraSpike","fFrameRadiusOuter",shaderCut.frame.radiusOuter);
        shaderCut.frame.vignette = number("FixedCameraSpike","fFrameVignette",shaderCut.frame.vignette);
        fixed::configure(f,shaderCut,settings,cursorSettings.enabled ? cursorSettings.toggleKey : -1,
            std::filesystem::path(skycrpg::LogDirectory())/L"SkyCRPG_Camera_FixedSpike.log",log,thinGoal,fixedPreCollision,fixedCause);
    }
    if (badSettings) log("Invalid Camera INI values: defaults or safe bounds applied; unsupported schema disables policy.");
    if (!s.enabled) log("Camera disabled in INI; native camera controls remain available.");
    if (!compatibleSettings) prerequisiteFailed = true;
}
void requestPreferences() {
    // One preallocated snapshot: the task never reads input-owned preferences.
    const auto zoomBits = std::bit_cast<std::uint32_t>(settings.zoom);
    preferencesToSave.store((std::uint64_t{1} << 33) |
        (static_cast<std::uint64_t>(settings.enabled) << 32) | zoomBits);
}
void savePreferences(std::uint64_t snapshot) {
    if (!compatibleSettings) return;
    // Write only owned preferences, preserving other sections/keys; never touch saves.
    const auto path = iniPath.string();
    const auto zoom = std::to_string(std::bit_cast<float>(static_cast<std::uint32_t>(snapshot)));
    const bool enabled = (snapshot & (std::uint64_t{1} << 32)) != 0;
    if (!WritePrivateProfileStringA("Camera","iSchemaVersion","1",path.c_str()) ||
        !WritePrivateProfileStringA("Camera","bEnabled",enabled?"1":"0",path.c_str()) ||
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
bool acquireMovementLease(bool report = true) {
    if (!movement) return false;
    const auto lease = movement->RequestDisableTargetLock(SKSE::GetPluginHandle());
    movementLease = lease == TDM_API::APIResult::OK || lease == TDM_API::APIResult::AlreadyGiven;
    publishedMovementLease.store(movementLease);
    if (!movementLease) prerequisiteFailed = true;
    if (!report) return movementLease;
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
    deferredIndicatorStop.store(false); // this main-thread cleanup also satisfies a deferred stop
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
    publishedMovementLease.store(movementLease);
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
bool lockBindingsSafe() {
    const auto map = RE::ControlMap::GetSingleton();
    if (!map || !lockPOVEvent || !map->GetUserEventName(22,RE::INPUT_DEVICE::kKeyboard).empty() ||
        settings.toggleKey == 22 || settings.recenterKey == 22 ||
        settings.toggleGamepad == 128 || settings.recenterGamepad == 128) return false;
    const auto gameplay = map->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
    if (!gameplay) return false;
    // GetMappedKey interns its string_view even if the caller retains a key.
    // Compare the prepared key directly, with no string-table lock in a callback.
    for (const auto& mapping : gameplay->deviceMappings[RE::INPUT_DEVICE::kGamepad])
        if (mapping.eventID == *lockPOVEvent) return mapping.inputKey == 128;
    return false;
}
void applyLockConfigRefresh() {
    const auto main = RE::Main::GetSingleton();
    if (!main || main->threadID != GetCurrentThreadId()) return;
    if (lockConfigRefresh.apply([](LockConfigResult result) {
        movementConfigSafe = result.movementSafe;
        lockConfigSafe = result.lockSafe;
        if (targetLockEnabled && !lockBindingsSafe()) {
            lockConfigSafe = false;
            lockBindingRefused.store(true);
        }
        publishedLockMovementConfigSafe.store(movementConfigSafe);
    })) {
        lockRefreshApplied.fetch_add(1u);
        const auto camera = RE::PlayerCamera::GetSingleton();
        const bool firstPerson = camera && camera->currentState && camera->currentState->id == RE::CameraState::kFirstPerson;
        lockRefreshMainSample.store((firstPerson?1u:0u)|(loading?2u:0u)|(movementLease?4u:0u)|
            (deferredIndicatorStop.load()?8u:0u)|(movementConfigSafe?16u:0u)|(lockConfigSafe?32u:0u));
    }
}
void updateTargetLockMain(Context& c);
void updateTargetLock(Context& c, unsigned sampledMenuEpoch) {
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
    publishedLockMenu.store(publishedMenuFocus(c.menu,sampledMenuEpoch,lockMenuOpenEpoch.load()));
    // An opening can land between the preceding epoch load and focus store.
    if (menuFocusNeedsReassert(sampledMenuEpoch,lockMenuOpenEpoch.load())) publishedLockMenu.store(true);
    publishedLockMovementConfigSafe.store(movementConfigSafe);
}
void updateTargetLockMain(Context& c) {
    observeTargetLock();
    consumeLockInterrupt();
    c.prerequisites = c.prerequisites && movementConfigSafe;
    const bool eligible = c.enabled && c.prerequisites && c.world && !c.special && !c.menu &&
        !c.firstPerson && !c.precision && !c.slide;
    const bool safe = lockConfigSafe && !lockConfigRefresh.requested() && !deferredIndicatorStop.load() && indicatorReady();
    if (!eligible || !safe || !observedLock) interruptTargetLock();
    consumeLockInterrupt();
    const auto decision = targetLockLease({targetLockEnabled,eligible,safe,indicatorReady(),
        movement != nullptr,movementLease,observedLock,lockInputIdle()});
    {
        // One bounded line per state change; idle input alone does not spend the budget.
        static std::uint32_t lastState = UINT32_MAX;
        const std::uint32_t state = (eligible?1u:0u)|(lockConfigSafe?2u:0u)|(indicatorReady()?4u:0u)|(observedLock?8u:0u)|
            (observedLock.value_or(false)?16u:0u)|(movementLease?32u:0u)|(static_cast<unsigned>(decision.action)<<8);
        if (state != lastState && lockStateLogs < 256) {
            lastState = state; ++lockStateLogs;
            char line[192]{};
            std::snprintf(line,sizeof(line),"Target lock state eligible=%u config=%u indicator=%u observed=%u locked=%u lease=%u input_idle=%u action=%u",
                state&1u,(state>>1)&1u,(state>>2)&1u,(state>>3)&1u,(state>>4)&1u,(state>>5)&1u,static_cast<unsigned>(lockInputIdle()),state>>8);
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
            publishedMovementLease.store(movementLease);
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
    c.prerequisites = c.prerequisites && targetLockPrerequisites(movementLease,observedLock,c.firstPerson,targetLockEnabled,safe);
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
        if (cursorSettings.enabled && cursorMode::ownMenu(menu.get())) continue;
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
void readCameraWorld(Context& c, bool* hardSpecial, std::optional<bool> mounted = std::nullopt) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    c.world = !loading && player && player->GetParentCell() && player->Is3DLoaded() && !player->IsDead() && camera && camera->currentState;
    if (!c.world) return;
    c.firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    c.tween = camera->currentState->id == RE::CameraState::kTween;
    const bool stateSpecial = !c.firstPerson && camera->currentState->id != RE::CameraState::kThirdPerson;
    const auto actorState = player->AsActorState();
    Hazards hazards{mounted ? *mounted : player->IsOnMount(), player->IsInKillMove(), actorState->IsSwimming(),
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
}
Context context(bool* hardSpecial = nullptr, bool allowLeaseRequest = false, bool* failure = nullptr,
    bool readOnlyLock = false) {
    if (!readOnlyLock) applyLockConfigRefresh();
    Context c;
    if (hardSpecial) *hardSpecial = true;
    c.enabled = settings.enabled;
    const bool providerEnabled = provider && provider->IsCameraEnabled();
    const bool lockThread = targetLockEnabled && onTargetLockThread();
    // Capture before focused() so an intervening opening keeps published focus.
    const unsigned sampledMenuEpoch = lockThread ? lockMenuOpenEpoch.load() : 0u;
    const bool haveLease = targetLockEnabled && !lockThread ? publishedMovementLease.load() : movementLease;
    const bool configSafe = targetLockEnabled && !lockThread ? publishedLockMovementConfigSafe.load() : movementConfigSafe;
    const bool baseReady = compatibleSettings && configSafe && ready && movement && providerEnabled &&
        provider->GetCameraOwner() == SKSE::kInvalidPluginHandle && providerChainReady();
    const bool leaseReady = !targetLockEnabled ? movement && haveLease && !movement->GetTargetLockState() :
        readOnlyLock ? publishedLockPrerequisites.load() :
        lockThread && lockHooksReady && movement && (haveLease ? !movement->GetTargetLockState() : lockConfigSafe && indicatorReady());
    c.prerequisites = targetLockEnabled ? baseReady : phase4Prerequisites(baseReady,leaseReady,allowLeaseRequest,haveLease);
    if (targetLockEnabled && readOnlyLock) {
        c.prerequisites = c.prerequisites && leaseReady;
        c.targetLock = publishedTargetLock.load();
    }
    // An intentionally disabled SmoothCam (F8) is not a failure.
    // With lock enabled, off-main observations cannot diagnose lease failure.
    if (failure) *failure = lockFailureReportable(targetLockEnabled,lockThread) && prerequisiteFailure(baseReady,leaseReady,
        !provider || providerEnabled || !ready || !compatibleSettings || !configSafe || !movement);
    readCameraWorld(c,hardSpecial);
    if (!c.world) {
        if (targetLockEnabled && !readOnlyLock) updateTargetLock(c,sampledMenuEpoch);
        return c;
    }
    c.menu = targetLockEnabled && !lockThread ? publishedLockMenu.load() : focused();
    // DYL-158 supplies precision/targeting intervention; select() reserves their priority.
    c.obstruction = obstructionFrame.obstructed;
    if (targetLockEnabled && !readOnlyLock) updateTargetLock(c,sampledMenuEpoch);
    return c;
}
#include "cursor_mode.h"
void servicePrecisionEntry();
void serviceFirstPersonReturn();
void serviceLockOffPrecisionEntry();

// SKSE's ordinary task drain can run on workers or Main. A process-lifetime
// timer bridges remaining engine work into that drain; callbacks never
// allocate a task or acquire SKSE's queue lock. At most one task is outstanding.
void serviceDeferredCameraWork() {
    deferredTaskRuns.fetch_add(1u);
    const bool taskMain = cursorMode::mainThread();
    const bool taskOutside = cursorMode::outsideMenuLocks();
    if (!taskMain) deferredTaskOther.fetch_add(1u);
    if (!taskOutside) deferredTaskMenuBusy.fetch_add(1u);
    if (taskMain && taskOutside) {
        applyLockConfigRefresh();
        publishedReturnChainReady.store(providerChainReady());
        if (deferredIndicatorStop.load()) stopIndicator();
        if (const auto snapshot = preferencesToSave.exchange(0)) savePreferences(snapshot);
    }
    // Native entry is independent of the task's IO gate. Opt-in claim tracing
    // checks inherited UI locks in its service. Lock-on keeps lease ordering.
    if (PrecisionReturn::entryRequested(precisionReturn.snapshot())) {
        if (!taskMain) precisionDiagnostics.note(PrecisionReason::serviceOther);
        else if (targetLockEnabled) serviceFirstPersonReturn();
        else serviceLockOffPrecisionEntry();
    }
    deferredCameraQueued.store(false);
}
// File validation has no engine/UI access and must not depend on SKSE's task
// drain ever running on Main. The timer serializes IO; Main only accepts scalars.
void serviceLockConfigIO() {
    if (const auto sources = lockConfigRefresh.beginRead()) {
        static unsigned readLines = 0;
        const bool report = readLines++ < 64;
        if (report) {
            char line[160]{};
            std::snprintf(line,sizeof(line),"LockRefresh read-start sources=0x%X seconds=%.3f",sources,lockClock());
            log(line);
        }
        LockConfigResult result;
        try {
            result = targetLockEnabled ? readLockMovementConfig() : LockConfigResult{checkMovementConfig(),false};
        } catch (...) {
            static bool readFailureLogged = false; // serialized IO worker only
            if (!readFailureLogged) { readFailureLogged = true; log("LockRefresh read failed; applying unsafe result, lease retained."); }
        }
        lockConfigRefresh.finishRead(result);
        if (report) log("LockRefresh read-complete; awaiting Main apply.");
    }
    static bool bindingLogged = false;
    if (lockBindingRefused.exchange(false) && !bindingLogged) {
        bindingLogged = true;
        log("Target lock refused: U conflicts or gamepad POV is not R3; lease held, bindings unchanged.");
    }
    // Bounded state-change diagnostics distinguish queued/no-task, off-Main and UI-lock
    // rejection. Samples are atomic; no callback formats, allocates or logs.
    static unsigned ticks = 0, traceLines = 0;
    static unsigned lastRefreshState = 0, lastApplied = 0, lastTaskFlags = 0;
    const auto refreshState = lockConfigRefresh.snapshot();
    const auto applied = lockRefreshApplied.load();
    const unsigned taskFlags = (deferredTaskQueuedCount.load()?1u:0u)|(deferredTaskRuns.load()?2u:0u)|
        (deferredTaskOther.load()?4u:0u)|(deferredTaskMenuBusy.load()?8u:0u);
    if (++ticks % 10 == 0 && traceLines < 128 &&
        (refreshState != lastRefreshState || applied != lastApplied || taskFlags != lastTaskFlags)) {
        lastRefreshState = refreshState; lastApplied = applied; lastTaskFlags = taskFlags;
        ++traceLines;
        char line[256]{};
        std::snprintf(line,sizeof(line),"LockRefresh state=0x%X applied=%u main_sample=0x%X task_queued=%u task_runs=%u task_other=%u task_menu_busy=%u seconds=%.3f",
            lockConfigRefresh.snapshot(),lockRefreshApplied.load(),lockRefreshMainSample.load(),
            deferredTaskQueuedCount.load(),deferredTaskRuns.load(),deferredTaskOther.load(),deferredTaskMenuBusy.load(),lockClock());
        log(line);
    }
    precisionDiagnostics.drain([](unsigned reason, std::uint32_t sample) {
        constexpr std::array names{"service-main","service-other","entry-claimed","entry-context","entry-token",
            "input-unsafe","input-unowned","input-mapped","input-no-edge","ownership-lost",
            "request-accepted","request-rejected","service-pending","refresh-pending",
            "menu-busy","mount-busy","invalid-context","lease-missing","unlock-pending","session-changed","returned","return-refused","entry-before","entry-accepted","entry-rejected"};
        static_assert(names.size() == static_cast<unsigned>(PrecisionReason::count));
        char line[160]{};
        std::snprintf(line,sizeof(line),"PrecisionReturn reason=%s context=0x%X",names[reason],sample);
        log(line);
    });
}
void CALLBACK queueDeferredCameraWork(PVOID, BOOLEAN) {
    if (deferredConfigBusy.test_and_set()) return;
    serviceLockConfigIO();
    deferredConfigBusy.clear();
    if (!preferencesToSave.load() && !lockConfigRefresh.requested() && !deferredIndicatorStop.load() &&
        !precisionDiagnostics.requested() &&
        !PrecisionReturn::entryRequested(precisionReturn.snapshot())) return;
    if (deferredCameraQueued.exchange(true)) return;
    deferredTaskQueuedCount.fetch_add(1u);
    deferredCameraTasks->AddTask(serviceDeferredCameraWork);
}
bool prepareDeferredCameraWork() {
    deferredCameraTasks = SKSE::GetTaskInterface();
    // No live DLL unload is supported; Windows tears down this timer at exit.
    if (!deferredCameraTasks || !CreateTimerQueueTimer(&deferredCameraTimer,nullptr,
        queueDeferredCameraWork,nullptr,100,100,WT_EXECUTEDEFAULT)) {
        log("Camera deferred service unavailable; camera hooks left inert.");
        return false;
    }
    log("LockRefresh IO timer prepared: 100 ms; Main applies scalar results in all camera states.");
    return true;
}

using CameraCall = void(*)(RE::PlayerCamera*);
CameraCall previousCameraCall = nullptr;
std::optional<bool> callbackMount(RE::PlayerCharacter& player) {
    // 1.7.104 / CommonLib 7.2.0: presence +0x10, RW lock +0x18.
    // AL 68233/68239 (CE3250/CE3510): owner +0, count +4; read unlock
    // is one atomic decrement. Never enter the native read-lock Sleep loop.
    static_assert(sizeof(RE::ExtraDataList) == 0x20);
    static_assert(sizeof(RE::BSReadWriteLock) == 8);
    const auto extra = reinterpret_cast<std::byte*>(&player.extraList);
    const PrecisionMountReadLock mountLock(*reinterpret_cast<std::uint32_t*>(extra+0x1c));
    if (!mountLock) return std::nullopt;
    const RE::BaseExtraList::PresenceBitfield* presence = nullptr;
    std::memcpy(&presence,extra+0x10,sizeof(presence));
    constexpr auto interaction = static_cast<std::uint32_t>(RE::ExtraDataType::kInteraction);
    static_assert(interaction == 0xa9);
    return !player.IsAMount() && presence && (presence->bits[interaction >> 3] & (1u << (interaction % 8))) != 0;
}
// This reader never advances lock policy, refreshes configuration or diagnoses
// failures. UI keys/relocations are prepared at DataLoaded; menu objects are borrowed.
Context returnContext(bool& hardSpecial, bool mounted) {
    Context c;
    c.enabled = settings.enabled;
    c.prerequisites = compatibleSettings && ready && lockHooksReady &&
        publishedLockMovementConfigSafe.load() && publishedReturnChainReady.load() && movement && provider &&
        provider->IsCameraEnabled() && provider->GetCameraOwner() == SKSE::kInvalidPluginHandle;
    readCameraWorld(c,&hardSpecial,mounted);
    c.menu = cursorMode::callbackMenu(false);
    return c;
}
PrecisionTraceState traceState(Context c, bool hard, FreshPress key, bool owned, std::optional<bool> mounted);
void tracePrecision(PrecisionTraceEvent event, PrecisionTraceState before, PrecisionTraceState after,
    unsigned device = 0, bool pressed = false, bool edge = false, unsigned decision = 0);
void servicePrecisionEntry() {
    const auto sessionToken = precisionReturn.snapshot();
    if (!PrecisionReturn::entryRequested(sessionToken)) return;
    if (!cursorMode::mainThread()) { precisionDiagnostics.note(PrecisionReason::serviceOther); return; }
    precisionDiagnostics.note(PrecisionReason::serviceMain,{},sessionToken,false,true);
    if (!ready) { precisionDiagnostics.note(PrecisionReason::entryContext); return; }
    // Debug-only synchronous logging is permitted after the native setter,
    // but never under inherited UI locks (including a deferred Main task).
    if (precisionTraceEnabled && !cursorMode::outsideMenuLocks()) {
        precisionDiagnostics.note(PrecisionReason::menuBusy); return;
    }
    // The trace sample never gates entry; without it the claim line reads device 0.
    const auto claimInput = precisionClaimTrace.read(sessionToken).value_or(PrecisionClaimInput{});
    auto claimKey = claimInput.key;
    // A refresh gates held framing/lock release, not escape into native FP.
    // Keep the last validated prerequisites and all fresh world/focus checks;
    // leave refresh pending for timer IO/Main apply and the return-to-TP barrier.
    if (lockConfigRefresh.requested()) precisionDiagnostics.note(PrecisionReason::refreshPending);
    // Two bounded reads, with no UI/mount lock held across the native setter.
    // All context and camera observations used to claim belong to this Main call.
    const auto readEntry = [&]() -> std::optional<PrecisionTraceState> {
        const cursorMode::UITryLock menuLock;
        if (!menuLock) { precisionDiagnostics.note(PrecisionReason::menuBusy); return std::nullopt; }
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return PrecisionTraceState{};
        const auto mounted = callbackMount(*player);
        if (!mounted) { precisionDiagnostics.note(PrecisionReason::mountBusy); return std::nullopt; }
        bool hard = true;
        auto c = returnContext(hard,*mounted);
        if (!targetLockEnabled) {
            c.prerequisites = compatibleSettings && ready && publishedLockMovementConfigSafe.load() &&
                publishedReturnChainReady.load() && movement && provider && provider->IsCameraEnabled() &&
                provider->GetCameraOwner() == SKSE::kInvalidPluginHandle && movementLease &&
                !movement->GetTargetLockState();
        }
        return traceState(c,hard,claimKey,false,*mounted);
    };
    const auto before = readEntry();
    if (!before) return; // contention before consumption preserves the intent
    precisionDiagnostics.note(PrecisionReason::entryBefore,before->context,sessionToken,false,true);
    PrecisionTraceState after;
    const bool claimed = precisionReturn.enter(before->context,sessionToken,[&] {
        settings.enabled = false; suspend();
        claimKey.suspend();
        static_cast<void>(RE::PlayerCamera::GetSingleton()->ForceFirstPerson());
        // A refused transition or unavailable fresh context cannot own FP.
        after = readEntry().value_or(PrecisionTraceState{});
        requestPreferences();
        return after.context;
    });
    if (claimed) {
        after.owned = true;
        // Both readers released UI/mount locks; ForceFirstPerson has returned.
        // tracePrecision alone gates formatting/IO by debug opt-in and budget.
        tracePrecision(PrecisionTraceEvent::toggleClaim,*before,after,
            claimInput.device,claimInput.pressed,claimInput.edge);
    }
    precisionDiagnostics.note(claimed ? PrecisionReason::entryClaimed :
        claimPrecisionOwnership(before->context,after.context) ? PrecisionReason::entryToken : PrecisionReason::entryContext,
        after.context,sessionToken,!claimed && precisionReturn.snapshot() != sessionToken,true);
}
void serviceFirstPersonReturn() {
    if (!ready || !lockHooksReady) return;
    if (!cursorMode::mainThread()) { precisionDiagnostics.note(PrecisionReason::serviceOther); return; }
    precisionDiagnostics.note(PrecisionReason::serviceMain);
    applyLockConfigRefresh(); // before loading/first-person exits; no IO or effect traversal
    const auto pendingPrecision = precisionReturn.snapshot();
    if (PrecisionReturn::entryRequested(pendingPrecision) || PrecisionReturn::requested(pendingPrecision))
        publishedReturnChainReady.store(providerChainReady());
    movementLease = movement && movement->GetDisableTargetLockOwner() == SKSE::GetPluginHandle();
    publishedMovementLease.store(movementLease);
    // No indicator traversal (engine lock) or routine diagnostic IO here.
    // Entry's opt-in bounded claim trace is the exception. Acquisition is the
    // TDM API's atomic lease request; cleanup stays deferred.
    if (lockInterrupt.consume()) {
        noteLockInputActivity();
        lockRetargetWindow.started.reset();
        deferredIndicatorStop = true;
        if (!movementLease) static_cast<void>(acquireMovementLease(false));
    }
    const auto sessionToken = precisionReturn.snapshot();
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (loading || !player || !player->GetParentCell() || !player->Is3DLoaded() || player->IsDead()) {
        static_cast<void>(precisionCameraObservation.retire(false,sessionToken));
        static_cast<void>(precisionReturn.invalidate(sessionToken));
        return;
    }
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->currentState) {
        static_cast<void>(precisionCameraObservation.retire(false,sessionToken));
        static_cast<void>(precisionReturn.invalidate(sessionToken));
        return;
    }
    const bool firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    if (precisionCameraObservation.retire(firstPerson,sessionToken)) {
        // Only same-epoch FP history can retire an unowned session. A
        // cancelled observation cannot invalidate a fresh entry intent.
        static_cast<void>(precisionReturn.invalidate(sessionToken));
        return;
    }
    servicePrecisionEntry();
    if (!firstPerson || !PrecisionReturn::requested(sessionToken)) return;
    precisionDiagnostics.note(PrecisionReason::servicePending,{},sessionToken);
    if (lockConfigRefresh.requested()) { precisionDiagnostics.note(PrecisionReason::refreshPending); return; }
    {
        const cursorMode::UITryLock menuLock; // one attempt, global then UI+0x158
        if (!menuLock) { precisionDiagnostics.note(PrecisionReason::menuBusy); return; }
        const auto mounted = callbackMount(*player);
        if (!mounted) { precisionDiagnostics.note(PrecisionReason::mountBusy); return; }
        bool hard = true;
        const auto c = returnContext(hard,*mounted);
        precisionReturn.retain(c,hard,sessionToken);
        // Retry a refused acquisition without running configuration/indicator policy.
        if (c.prerequisites && !movementLease) static_cast<void>(acquireMovementLease(false));
        const bool haveLease = movement && movement->GetDisableTargetLockOwner() == SKSE::GetPluginHandle();
        const auto locked = movement ? std::optional<bool>{movement->GetTargetLockState()} : std::nullopt;
        if (!precisionReturn.take(c,haveLease,locked,sessionToken)) {
            const auto reason = !c.firstPerson || toggleRequest(c,PrecisionReturn::owned(sessionToken)) != ToggleRequest::thirdPerson ?
                PrecisionReason::invalidContext : !haveLease ? PrecisionReason::leaseMissing :
                !locked || *locked ? PrecisionReason::unlockPending : PrecisionReason::sessionChanged;
            precisionDiagnostics.note(reason,c,sessionToken,precisionReturn.snapshot() != sessionToken);
            return;
        }
    }
    settings.enabled = true; suspend(); applyZoom = true;
    static_cast<void>(camera->ForceThirdPerson());
    // take() already retired this transition; do not retire a fresh entry's
    // token on the next idle frame after our own successful return.
    static_cast<void>(precisionCameraObservation.retire(camera->currentState &&
        camera->currentState->id == RE::CameraState::kFirstPerson,precisionReturn.snapshot()));
    precisionDiagnostics.note(camera->currentState && camera->currentState->id == RE::CameraState::kThirdPerson ?
        PrecisionReason::returned : PrecisionReason::returnRefused,{},sessionToken);
    requestPreferences(); // drained on the SKSE main task path, outside UI locks
}
void serviceLockOffPrecisionEntry() {
    if (!PrecisionReturn::entryRequested(precisionReturn.snapshot())) return;
    if (!cursorMode::mainThread()) { precisionDiagnostics.note(PrecisionReason::serviceOther); return; }
    if (ready) {
        applyLockConfigRefresh();
        publishedReturnChainReady.store(providerChainReady());
    }
    servicePrecisionEntry();
}
void serviceCameraCall(RE::PlayerCamera* camera) {
    // Every all-state camera frame services entry, independently of optional
    // lock/cursor/fixed-height hooks. Keep lock-on interrupt ordering unchanged.
    if (targetLockEnabled) serviceFirstPersonReturn();
    else serviceLockOffPrecisionEntry();
    if (fixed::runtime) fixed::beginFrame();
    previousCameraCall(camera); // exactly once, including TDM's pitch/update chain
    if (fixed::runtime) fixed::endFrame();
}
bool installLockCameraService() {
    // Reuse the already guarded all-state call when thin hooks are installed.
    // The thin observer stays read-only; its previous provider target is retained.
    if (thin::runtime && thin::runtime->installed) {
        previousCameraCall = thin::runtime->update;
        thin::runtime->update = serviceCameraCall;
        log("Camera precision service path=thin-chain.");
    } else {
        const auto base = REL::Module::get().base();
        const auto site = REL::ID(50784).address()+0x1A6;
        if (site != base+0x8f8826 || !thin::cameraCallGuard(site,base+0x558e70) ||
            std::memcmp(reinterpret_cast<const void*>(base+0x558e70),"\x40\x57\x48\x83\xec\x30",6) != 0) {
            log("Camera precision service refused: all-state camera call guard failed; lease held.");
            return false;
        }
        SKSE::AllocTrampoline(32);
        previousCameraCall = reinterpret_cast<CameraCall>(SKSE::GetTrampoline().write_call<5>(site,serviceCameraCall));
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);
        log("Camera precision service path=trampoline.");
    }
    log("Camera all-state precision service installed (both target-lock settings).");
    return true;
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
RE::NiPoint3 fixedPreCollision() { return provider->GetLastCameraPosition(); }
// Why a frame was not eligible, from state the frame service can read without
// advancing any policy. state is the update hook's selected State, -1 if silent.
unsigned fixedCause(int state) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    auto cause = state < 0 ? fixed::Exit::hookSilent : fixed::Exit::special;
    if (loading) cause = fixed::Exit::load;
    else if (player && player->IsDead()) cause = fixed::Exit::death;
    else if (!settings.enabled) cause = fixed::Exit::disable;
    else if (provider && !provider->IsCameraEnabled()) cause = fixed::Exit::f8;
    else if (camera && camera->currentState && camera->currentState->id == RE::CameraState::kFirstPerson) cause = fixed::Exit::firstPerson;
    else if (state == static_cast<int>(State::menu)) cause = fixed::Exit::menu;
    return static_cast<unsigned>(cause);
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
void notifyLockRefusal(Context c) {
    if (targetLockEnabled && ready && lockHooksReady && !lockRefusalNotified && (!lockConfigSafe || !indicatorReady()) &&
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
PrecisionTraceState traceState(Context c, bool hard, FreshPress key, bool owned, std::optional<bool> mounted) {
    PrecisionTraceState result{c,hard,owned,key.armed,key.down};
    if (!precisionTraceEnabled) return result;
    if (!c.world) { result.hazardBits = UINT32_MAX; return result; }
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (camera && camera->currentState) result.cameraState = static_cast<std::uint32_t>(camera->currentState->id);
    if (player) {
        result.hazardBits |= std::uint32_t(mounted ? *mounted : player->IsOnMount()) << 0;
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
PrecisionTraceState traceState(Context c, bool hard, std::size_t key) {
    return traceState(c,hard,keys[key],precisionReturn.owned(),std::nullopt);
}
void tracePrecision(PrecisionTraceEvent event, PrecisionTraceState before, PrecisionTraceState after,
    unsigned device, bool pressed, bool edge, unsigned decision) {
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
            static_cast<unsigned>(select(c)),static_cast<unsigned>(precisionReturn.owned()));
    } else if (onTargetLockThread()) {
        std::snprintf(line,sizeof(line),"Lifecycle event=%s hook=%s state=%u decision=%u owned=%u lease=%u lock=%d target=%08X",event,
            std::strcmp(event,"observe") == 0 ? "update" : "event",stateId,
            static_cast<unsigned>(select(c)),static_cast<unsigned>(precisionReturn.owned()),static_cast<unsigned>(movementLease),
            observedLock ? (*observedLock ? 1 : 0) : -1,observedTarget.native_handle());
    } else return;
    log(line);
}
void resetLifecycle(const char* event) {
    precisionReturn.cancel();
    cursorMode::requestExit(std::strstr(event,"load") ? cursor::Exit::load : cursor::Exit::cell);
    thin::requestRestore(std::strstr(event,"load") ? thin::Exit::load : thin::Exit::cell);
    if (thin::enabled() && thin::onMain() && std::strcmp(event,"player-detach") == 0)
        thin::restoreAll(thin::Exit::cell);
    fixed::lifecycle(std::strstr(event,"load") ? fixed::Exit::load : fixed::Exit::cell);
    interruptTargetLock();
    bool owned = false;
    resetTransient(owned,recenter,applyZoom,keys);
    resetObstruction();
    lifecycleTrace.resetCandidate();
    lifecycleLog(event,Context{},UINT32_MAX);
}
void observeLifecycle(Context c, bool hard, PrecisionReturn::Token sessionToken) {
    precisionReturn.retain(c,hard,sessionToken);
    const auto camera = RE::PlayerCamera::GetSingleton();
    const unsigned id = camera && camera->currentState ? static_cast<unsigned>(camera->currentState->id) : UINT32_MAX;
    if (lifecycleTraceEnabled && lifecycleTrace.observe(select(c),id)) lifecycleLog("observe",c,id);
    // Obstruction is a normal-framing sub-state; suspending on it would reset its latch.
    if (!heldFramingAllowed(c)) suspend();
    else if (c.targetLock) resetObstruction();
}
void sampleObstruction(RE::ThirdPersonState* state) {
    // The spike removes the pull-in this policy measures (DYL-157 suppressed).
    if (!fixed::obstructionSampled(fixed::engaged())) { resetObstruction(); return; }
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
    if (cursorSettings.enabled && cursorMode::look(input)) { fixed::dropSnaps(); previousLook(state,RE::NiPoint2{}); return; }
    if (targetLockEnabled && lockFrameTraceEnabled && lockTraceWindow.budget.lines < LockFrameTraceBudget::limit && onTargetLockThread()) {
        lockLookInput.x += input.x; lockLookInput.y += input.y; ++lockLookCalls;
    }
    const auto c = context();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!heldFramingAllowed(c) || !camera || camera->currentState.get() != state || !state->freeRotationEnabled) {
        suspend(); previousLook(state,input); return;
    }
    // Spike rig: yaw and pitch stay held through a native POV slide as well.
    const bool rig = fixed::engaged() && fixed::rigEligible(true,select(c));
    if (pauseCorrection(c) && !rig) { previousLook(state,input); return; }
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
    if (rig) {
        // Player yaw is discarded; the request holds world heading (player
        // heading + freeRotation.x) on the current snap heading. F7 = home.
        filtered.x = fixed::yawInput(c.slide,input.x,player->GetAngleZ(),state->freeRotation.x,gain,dt,recenter);
        recenter = false;
    } else if (recenter) {
        const float error = wrap(-state->freeRotation.x);
        filtered.x = correction(error,radians(settings.recenterSpeed),gain,dt);
        if (std::abs(error) < radians(.2f)) recenter = false;
    }
    previousLook(state,filtered); // exactly once, retaining provider input lock + native update
}
void traceLockFrame(Context c);
void update(RE::ThirdPersonState* state,RE::BSTSmartPointer<RE::TESCameraState>& next) {
    if (fixed::runtime) fixed::clearFrame();
    if (cursorSettings.enabled) {
        if (next && next.get() != state) cursorMode::requestExit(cursor::Exit::special);
        cursorMode::service();
        cursorMode::traceFrame();
    }
    if (next && next.get() != state) { interruptTargetLock(); resetObstruction(); }
    else sampleObstruction(state);
    previousUpdate(state,next); // native first; outer SmoothCam wrapper performs transforms AFTER us
    const auto sessionToken = precisionReturn.snapshot();
    bool hard = true;
    bool failure = false;
    const auto c = context(&hard,false,&failure);
    if (fixed::runtime) fixed::observeFrame(select(c),c.slide);
    observeLifecycle(c,hard,sessionToken);
    observePrerequisiteFailure(c,failure);
    if (targetLockEnabled && onTargetLockThread() && lockRefusalRequested.exchange(false)) notifyLockRefusal(c);
    if (targetLockEnabled) traceLockFrame(c);
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!heldFramingAllowed(c) || !camera || camera->currentState.get() != state || (next && next.get() != state)) {
        resetObstruction(); return;
    }
    if (RE::PlayerCharacter::GetSingleton()->IsWeaponDrawn()) resetObstruction();
    // Spike rig: one requested distance. Wheel and POV-slide requests are
    // overwritten here, before SmoothCam reads the scalar; the stored
    // preference is not touched. This also arms cast suppression for the frame.
    if (fixed::runtime && fixed::holdFrame()) {
        state->targetZoomOffset = fixed::heldZoom(true,fixed::runtime->config.zoom,state->targetZoomOffset);
        return;
    }
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
        if (event && event->menuName == cursorMode::menuName) return RE::BSEventNotifyControl::kContinue;
        if (cursorSettings.enabled && event && event->opening &&
            event->menuName != "HUD Menu" && event->menuName != "Fader Menu" && event->menuName != "Mist Menu")
            cursorMode::requestExit(cursor::Exit::menu);
        // Cancel entry even when lock-off service waits for the deferred task.
        // An active first-person session keeps its ownership across menu focus.
        if (event && lockMenuPublishesFocus(event->menuName.c_str(),event->opening)) {
            precisionReturn.cancel(false);
            fixed::lifecycle(fixed::Exit::menu);
        }
        // Queue before any context() can consider releasing the lease.
        if (targetLockEnabled) {
            if (!event) return RE::BSEventNotifyControl::kContinue;
            const std::string_view name{event->menuName.c_str()};
            if (lockMenuPublishesFocus(name,event->opening)) {
                lockMenuOpenEpoch.fetch_add(1u);
                publishedLockMenu.store(true);
            }
            if (lockMenuRefresh(name,event->opening)) lockConfigRefresh.request(lockRefreshSource(name,event->opening));
            if (!lockMenuInterrupt(name,event->opening)) return RE::BSEventNotifyControl::kContinue;
            interruptTargetLock();
        }
        bool hard = true;
        const auto c = precisionTraceEnabled ? context(&hard) : Context{};
        const auto before = traceState(c,hard,0);
        if (!targetLockEnabled) lockConfigRefresh.request(LockRefreshSource::lockOffMenu);
        failureDebounce.observe(false,0);
        suspend();
        if (precisionTraceEnabled && event && (c.world || precisionReturn.owned())) {
            bool afterHard = true;
            const auto after = context(&afterHard);
            tracePrecision(event->opening ? PrecisionTraceEvent::menuOpen : PrecisionTraceEvent::menuClose,
                before,traceState(after,afterHard,0));
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        const auto returnSession = precisionReturn.snapshot();
        cursorMode::input(*events);
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
                if (button && button->IsDown() && event->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == 66) {
                    precisionReturn.cancel();
                    interruptTargetLock();
                }
            }
        }
        bool hardSpecial = true;
        bool failure = false;
        const bool lockThread = targetLockEnabled && onTargetLockThread();
        const bool precisionInputMain = cursorMode::mainThread();
        const bool haveLease = targetLockEnabled && !lockThread ? publishedMovementLease.load() : movementLease;
        const auto c = context(&hardSpecial,!settings.enabled && !haveLease,&failure);
        observePrerequisiteFailure(c,failure);
        if (targetLockEnabled) lockRefusalRequested.store(true);
        const bool ownedBefore = precisionReturn.owned();
        precisionReturn.retain(c,hardSpecial,returnSession);
        if (ownedBefore && !precisionReturn.owned())
            precisionDiagnostics.note(PrecisionReason::ownershipLost,c,returnSession,precisionReturn.snapshot() != returnSession,precisionInputMain);
        if (precisionTraceEnabled && ownedBefore != precisionReturn.owned()) {
            const auto after = traceState(c,hardSpecial,0);
            auto before = after; before.owned = ownedBefore;
            tracePrecision(PrecisionTraceEvent::retention,before,after);
        }
        if (fixed::engaged()) {
            // Spike snap turns: fresh keyboard presses only, never in a menu or
            // a handed-off state, never on a key the game has mapped.
            for (auto event = *events; event; event = event->next) {
                const auto button = event->AsButtonEvent();
                if (!button || !button->IsDown() || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
                const int direction = fixed::snapDirection(fixed::runtime->config,static_cast<int>(button->GetIDCode()));
                if (!direction || !fixed::rigEligible(true,select(c))) continue;
                const auto map = RE::ControlMap::GetSingleton();
                if (!map || !map->GetUserEventName(button->GetIDCode(),RE::INPUT_DEVICE::kKeyboard).empty() || !button->QUserEvent().empty()) {
                    fixed::refuse(fixed::Refusal::snapMapped,"a snap key is mapped to a native control; the press was ignored");
                    continue;
                }
                fixed::requestSnap(direction);
            }
        }
        // Deliberate F6 can return from its OWN first-person request; native POV never auto-restores.
        const bool lifecycleSafe = c.prerequisites && c.world && !c.special && !c.menu;
        const bool safe = lifecycleSafe && !c.slide;
        // Safe unowned FP may be Main's in-flight entry. Only Main validates
        // that transition; an input observation must not cancel its claim.
        if (!safe) static_cast<void>(precisionReturn.invalidate(returnSession,false));
        if (!lifecycleSafe) suspend();
        for (auto event = *events; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button) continue;
            if (button->IsDown() && button->QUserEvent() == "Toggle POV") {
                const auto before = traceState(c,hardSpecial,0);
                precisionReturn.cancel();
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
                    if (i == 0 && button->IsDown())
                        precisionDiagnostics.note(PrecisionReason::inputUnsafe,c,returnSession,precisionReturn.snapshot() != returnSession,precisionInputMain);
                    // A refused opt-in policy must still allow the native F6
                    // escape. It cannot re-enable unsafe held framing.
                    if (targetLockEnabled && i == 0 && button->IsDown() && c.world &&
                        !c.menu && !c.special && !c.slide && !c.firstPerson) {
                        const auto map = RE::ControlMap::GetSingleton();
                        if (map && map->GetUserEventName(button->GetIDCode(),device).empty() && button->QUserEvent().empty()) {
                            interruptTargetLock();
                            settings.enabled = false; precisionReturn.cancel();
                            static_cast<void>(RE::PlayerCamera::GetSingleton()->ForceFirstPerson());
                            requestPreferences();
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
                    if (i == 0 && button->IsDown())
                        precisionDiagnostics.note(PrecisionReason::inputMapped,c,returnSession,false,precisionInputMain);
                    if (i == 0 && button->IsDown()) tracePrecision(PrecisionTraceEvent::toggleMapped,before,before,pad?2u:1u,button->IsPressed(),button->IsDown());
                    continue;
                }
                if (!keys[keyIndex].update(button->IsPressed(),button->IsDown())) {
                    if (i == 0 && button->IsDown())
                        precisionDiagnostics.note(PrecisionReason::inputNoEdge,c,returnSession,false,precisionInputMain);
                    if (i == 0 && button->IsDown()) tracePrecision(PrecisionTraceEvent::toggleNoEdge,before,traceState(c,hardSpecial,keyIndex),pad?2u:1u,button->IsPressed(),button->IsDown());
                    continue;
                }
                if (i == 0) {
                    auto camera = RE::PlayerCamera::GetSingleton();
                    const auto request = toggleRequest(c,precisionReturn.owned());
                    if (request == ToggleRequest::none)
                        precisionDiagnostics.note(PrecisionReason::inputUnowned,c,returnSession,precisionReturn.snapshot() != returnSession,precisionInputMain);
                    enableIntent.request(request);
                    tracePrecision(PrecisionTraceEvent::toggleDecision,before,traceState(c,hardSpecial,keyIndex),pad?2u:1u,true,true,static_cast<unsigned>(request));
                    if (request == ToggleRequest::firstPerson) {
                        // Publish only an epoch-bound intent, even on Main.
                        // The service alone samples/forces/claims the FP interval.
                        const bool entryAccepted = precisionClaimTrace.publish(precisionReturn,c,returnSession,
                            {pad?2u:1u,button->IsPressed(),button->IsDown(),keys[keyIndex]});
                        precisionDiagnostics.note(entryAccepted ? PrecisionReason::entryAccepted : PrecisionReason::entryRejected,
                            c,returnSession,!entryAccepted && precisionReturn.snapshot() != returnSession,precisionInputMain);
                        if (entryAccepted) lockInterrupt.request();
                    } else if (request == ToggleRequest::enableOnly) {
                        if (targetLockEnabled && !lockThread) {
                            if (!publishedMovementLease.load()) { interruptTargetLock(); continue; }
                        } else {
                            if (leaseWanted(false,true,movementLease) && !acquireMovementLease()) continue;
                            if (!enableAfterLease(movementLease)) continue;
                        }
                        settings.enabled = true; precisionReturn.cancel(); applyZoom = true; suspend(); requestPreferences();
                    } else if (request == ToggleRequest::thirdPerson) {
                        if (targetLockEnabled) {
                            // Both threads post one intent. Main revalidates the
                            // session/focus/provider and TDM's unlocked reply.
                            const bool accepted = precisionReturn.request(c,returnSession);
                            precisionDiagnostics.note(accepted ? PrecisionReason::requestAccepted : PrecisionReason::requestRejected,
                                c,returnSession,!accepted && precisionReturn.snapshot() != returnSession,precisionInputMain);
                            if (accepted) lockInterrupt.request();
                            continue;
                        }
                        if (leaseWanted(false,true,movementLease) && !acquireMovementLease()) continue;
                        if (!enableAfterLease(movementLease)) continue;
                        settings.enabled = true; precisionReturn.cancel(); suspend(); applyZoom = true; static_cast<void>(camera->ForceThirdPerson()); requestPreferences();
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
            lockConfigRefresh.request(LockRefreshSource::startup);
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
                publishedMovementLease.store(movementLease);
            }
            ready = false; prerequisiteFailed = true;
            log("Unexpected camera hook order/owner before SmoothCam deferred attach; policy left inert.");
            thin::refuseInstall("ThinOccluder: camera slot owner guard failed; no thin hooks installed.");
            return;
        }
        lockPOVEvent.emplace("Toggle POV");
        cursorMode::prepareInputMenuKeys();
        static_cast<void>(cursorMode::outsideMenuLocks());
        if (!prepareDeferredCameraWork()) { ready = false; return; }
        publishedLockMovementConfigSafe.store(movementConfigSafe);
        previousLook = reinterpret_cast<Look>(table.write_vfunc(0xF,look));
        previousUpdate = reinterpret_cast<Update>(table.write_vfunc(3,update));
        lockHooksReady = targetLockEnabled;
        thin::installHooks(ready);
        if (!installLockCameraService()) {
            lockHooksReady = false;
            if (!targetLockEnabled) ready = false;
        }
        fixed::install(ready && provider,previousCameraCall != nullptr);
        if (const auto source = RE::ScriptEventSourceHolder::GetSingleton()) {
            source->AddEventSink<RE::TESLoadGameEvent>(&events);
            source->AddEventSink<RE::TESCellAttachDetachEvent>(&events);
        }
        if (cursorSettings.enabled) {
            static_cast<void>(cursorMode::registration.prepare(false,true,false,false,cursorMode::registerOverlay));
            cursorMode::dataReady.store(true);
        }
        log("Camera input policy installed: one chained native look call; SmoothCam sole transform writer.");
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
        if (cursorSettings.enabled) cursorMode::worldLoaded.store(false);
        thin::setLoaded(false); loading = true; failureDebounce.observe(false,0); resetLifecycle("pre-load"); break;
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame:
        if (cursorSettings.enabled) cursorMode::worldLoaded.store(message->type == SKSE::MessagingInterface::kNewGame || message->data != nullptr);
        thin::setLoaded(true); loading = false; resetLifecycle("post-load-or-new-game"); break;
    case SKSE::MessagingInterface::kSaveGame:
        interruptTargetLock();
        requestPreferences();
        break;
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
