// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "policy.h"
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
std::array<FreshPress,4> keys;
using Look = void(*)(RE::ThirdPersonState*, const RE::NiPoint2&);
Look previousLook = nullptr;
using Update = void(*)(RE::ThirdPersonState*,RE::BSTSmartPointer<RE::TESCameraState>&);
Update previousUpdate = nullptr;
std::filesystem::path iniPath;
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
    const auto value = read(section,key,"");
    T result{};
    const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data()+value.size() ? result : fallback;
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
void loadSettings() {
    iniPath = std::filesystem::path(skycrpg::ThisModuleDirectory())/L"SkyCRPG_Camera.ini";
    Settings s;
    s.schema = number("Camera","iSchemaVersion",1);
    s.enabled = number("Camera","bEnabled",1) == 1;
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
    compatibleSettings = s.schema == 1;
    settings = validate(s);
    ObstructionSettings o;
    o.enabled = number("Obstruction","bEnabled",0) == 1;
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
void resetObstruction() {
    obstructionPolicy.reset(); obstructionFrame = {};
    obstructionSampleTime = {}; obstructionCell = 0;
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
Context context(bool allowZoomSlide = false) {
    Context c;
    c.enabled = settings.enabled;
    c.prerequisites = compatibleSettings && movementConfigSafe && ready && movement && movementLease && !movement->GetTargetLockState() && provider && provider->IsCameraEnabled() && provider->GetCameraOwner() == SKSE::kInvalidPluginHandle && providerChainReady();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    c.world = !loading && player && player->GetParentCell() && player->Is3DLoaded() && !player->IsDead() && camera && camera->currentState;
    if (!c.world) return c;
    c.firstPerson = camera->currentState->id == RE::CameraState::kFirstPerson;
    c.special = (!c.firstPerson && camera->currentState->id != RE::CameraState::kThirdPerson) ||
        player->IsOnMount() || player->IsInKillMove() || player->AsActorState()->IsSwimming();
    const auto controls = RE::PlayerControls::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (!controlMap || !controlMap->IsLookingControlsEnabled() || !controlMap->IsPOVSwitchControlsEnabled()) c.special = true;
    if (!controls || controls->data.povScriptMode || controls->data.povBeastMode || (controls->data.fovSlideMode && !allowZoomSlide)) c.special = true;
    if (!c.firstPerson && camera->currentState->id == RE::CameraState::kThirdPerson &&
        static_cast<RE::ThirdPersonState*>(camera->currentState.get())->stateNotActive) c.special = true;
    c.menu = focused();
    // DYL-158 supplies precision/targeting intervention; select() reserves their priority.
    c.obstruction = obstructionFrame.obstructed;
    return c;
}
void sampleObstruction(RE::ThirdPersonState* state) {
    auto c = context();
    const auto camera = RE::PlayerCamera::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    // No combat-mode classification is assumed on this branch. Conservatively
    // release automatic framing for ALL drawn weapons/spells pending DYL-158.
    if (player && player->IsWeaponDrawn()) c.precision = true;
    if (!obstructionSettings.enabled || !obstructionFramingAllowed(c) || !camera || !player ||
        camera->currentState.get() != state || !state->freeRotationEnabled || !camera->cameraRoot ||
        camera->cameraTarget.get().get() != player) {
        resetObstruction(); return;
    }
    const auto cell = player->GetParentCell()->GetFormID();
    if (obstructionCell != cell) resetObstruction();
    obstructionCell = cell;
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
    obstructionFrame = obstructionPolicy.step(c,cameraCompression(
        {actual.x,actual.y,actual.z},{goal.x,goal.y,goal.z},focus),dt,obstructionSettings);
}
void look(RE::ThirdPersonState* state, const RE::NiPoint2& input) {
    const auto c = context();
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!obstructionFramingAllowed(c) || !camera || camera->currentState.get() != state || !state->freeRotationEnabled) {
        suspend(); previousLook(state,input); return;
    }
    auto setting = RE::GetINISetting("fFreeRotationSpeed:Camera");
    static REL::Relocation<float*> delta{REL::ID(410200)};
    const float dt = *delta;
    const float gain = setting ? setting->GetFloat() : 0.0f;
    if (!std::isfinite(dt) || dt <= 0 || dt > .1f || !std::isfinite(gain) || gain <= 0) {
        suspend(); previousLook(state,input); return;
    }
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player->IsWeaponDrawn()) resetObstruction();
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
void update(RE::ThirdPersonState* state,RE::BSTSmartPointer<RE::TESCameraState>& next) {
    if (next && next.get() != state) resetObstruction();
    else sampleObstruction(state);
    previousUpdate(state,next); // native first; outer SmoothCam wrapper performs transforms AFTER us
    const auto c = context(true);
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!obstructionFramingAllowed(c) || !camera || camera->currentState.get() != state || (next && next.get() != state)) {
        resetObstruction(); return;
    }
    if (RE::PlayerCharacter::GetSingleton()->IsWeaponDrawn()) resetObstruction();
    // Native controller POV-slide zoom bypasses HandleLookInput, so bound the
    // requested scalar after native update and BEFORE the outer provider update.
    // stateNotActive/POV transitions are excluded; current distance is never written.
    settings.zoom = preferredZoomRequest(state->targetZoomOffset,lastAdjustedZoom,adjustedZoomWritten,settings,applyZoom);
    state->targetZoomOffset = obstructionZoom(settings.zoom,obstructionFrame.blend,obstructionSettings);
    lastAdjustedZoom = state->targetZoomOffset;
    adjustedZoomWritten = obstructionFrame.blend > 0;
    applyZoom = false;
}
class Events final : public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        movementConfigSafe = checkMovementConfig();
        suspend(); return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        const auto c = context();
        // Deliberate F6 can return from its OWN first-person request; native POV never auto-restores.
        const bool safe = c.prerequisites && c.world && !c.special && !c.menu;
        if (!safe) suspend();
        for (auto event = *events; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button) continue;
            if (button->IsDown() && button->QUserEvent() == "Toggle POV") ownFirstPerson = false;
            const auto device = event->GetDevice();
            const bool pad = device == RE::INPUT_DEVICE::kGamepad;
            if (!pad && device != RE::INPUT_DEVICE::kKeyboard) continue;
            const int id = static_cast<int>(button->GetIDCode());
            const std::array<int,2> bindings{pad?settings.toggleGamepad:settings.toggleKey,pad?settings.recenterGamepad:settings.recenterKey};
            for (std::size_t i = 0;i < bindings.size();++i) {
                if (bindings[i] < 0 || id != bindings[i]) continue;
                if (!safe) { keys[(pad?2:0)+i].observeSuppressed(button->IsPressed()); continue; }
                auto map = RE::ControlMap::GetSingleton();
                // Never dispatch custom action on a mapped native gameplay button.
                if (!map || !map->GetUserEventName(button->GetIDCode(),device).empty() || !button->QUserEvent().empty()) continue;
                if (!keys[(pad?2:0)+i].update(button->IsPressed(),button->IsDown())) continue;
                if (i == 0) {
                    auto camera = RE::PlayerCamera::GetSingleton();
                    const auto request = toggleRequest(c,ownFirstPerson);
                    if (request == ToggleRequest::firstPerson) {
                        settings.enabled = false; ownFirstPerson = true; suspend(); static_cast<void>(camera->ForceFirstPerson()); savePreferences();
                    } else if (request == ToggleRequest::enableOnly) {
                        settings.enabled = true; ownFirstPerson = false; applyZoom = true; suspend(); savePreferences();
                    } else if (request == ToggleRequest::thirdPerson) {
                        settings.enabled = true; ownFirstPerson = false; suspend(); applyZoom = true; static_cast<void>(camera->ForceThirdPerson()); savePreferences();
                    }
                } else if (obstructionFramingAllowed(c)) recenter = true;
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
        if (previousLook) break; // no duplicate hook/sinks on repeated notifications
        ready = hashMatches(nullptr,"846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f") &&
            hashMatches(L"SmoothCam.dll","b20bf8a276bf76fbe35277dd9b25c64c57d17bfdcc20daa74a5f941c90f563da") &&
            hashMatches(L"TrueDirectionalMovement.dll","5ba3abd82fe070e2a6e9af7f07eca82b9ddb1a53dcd47bc34af571efab01899f") &&
            hashMatches(L"MCMHelper.dll","d58f433bef7690614d42aaac69cc35f60d179a85429a385e21858a1f276aef38");
        if (!ready) { log("Exact executable/provider prerequisite hash mismatch; controller disabled."); break; }
        movementConfigSafe = checkMovementConfig();
        if (!movementConfigSafe) log("TDM camera-yaw config absent/unsafe; custom policy suspended until corrected and a menu closes.");
        movement = static_cast<TDM_API::IVTDM5*>(TDM_API::RequestPluginAPI());
        if (!movement) { ready = false; log("TDM V5 API absent; controller suspended."); break; }
        const auto lease = movement->RequestDisableTargetLock(SKSE::GetPluginHandle());
        movementLease = lease == TDM_API::APIResult::OK || lease == TDM_API::APIResult::AlreadyGiven;
        if (!movementLease) { ready = false; log("TDM target-lock-disable lease refused; controller suspended."); break; }
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_ThirdPersonState[0]};
        const auto slots = reinterpret_cast<const std::uintptr_t*>(table.address());
        const REL::Relocation<std::uintptr_t> nativeUpdate{REL::ID(50896)};
        const REL::Relocation<std::uintptr_t> nativeLook{REL::ID(50914)};
        if (slots[3] != nativeUpdate.address() || slots[15] != nativeLook.address()) {
            static_cast<void>(movement->ReleaseDisableTargetLock(SKSE::GetPluginHandle()));
            movementLease = false; ready = false;
            log("Unexpected camera hook order/owner before SmoothCam deferred attach; policy left inert.");
            return;
        }
        previousLook = reinterpret_cast<Look>(table.write_vfunc(0xF,look));
        previousUpdate = reinterpret_cast<Update>(table.write_vfunc(3,update));
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&events));
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
        log("Camera input policy installed: one chained native look call; SmoothCam sole transform writer.");
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame: loading = true; ownFirstPerson = false; suspend(); break;
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame: loading = false; ownFirstPerson = false; applyZoom = true; suspend(); break;
    case SKSE::MessagingInterface::kSaveGame: savePreferences(); break;
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
    const auto messaging = SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(messages);
}
