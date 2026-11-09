// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace skycrpg::camera {
constexpr bool phase4Prerequisites(bool baseReady, bool leaseReady, bool allowLeaseRequest, bool haveLease) {
    return baseReady && (leaseReady || (allowLeaseRequest && !haveLease));
}
constexpr bool lockFailureReportable(bool enabled, bool mainThread) {
    return !enabled || mainThread;
}
constexpr bool lockMenuRefresh(std::string_view name, bool opening) {
    return name == "Dialogue Menu" || name == "Console" || name == "Loading Menu" ||
        (name == "Journal Menu" && !opening);
}
constexpr bool lockMenuInterrupt(std::string_view name, bool opening) {
    if (name == "HUD Menu" || name == "Cursor Menu" || name == "Fader Menu" || name == "Mist Menu" ||
        name == "LoadWaitSpinner") return false;
    if (name == "Dialogue Menu" || name == "Console" || name == "Loading Menu") return true;
    // Main-thread focused() handles unlisted openings. The close barrier also
    // catches menus that open and close between two main-thread observations.
    // Never read UI::GetMenu or menu flags on the event thread.
    return !opening;
}
// Callbacks only publish interrupts. The main-thread consumer resets freshness
// and cleans up before release; the mutex closes the check/release race.
class LockInterrupt {
public:
    void request() { const std::lock_guard guard(mutex); pending = true; }
    bool consume() {
        const std::lock_guard guard(mutex);
        const bool result = pending;
        pending = false;
        return result;
    }
    template<class F> bool releaseIfClear(F release) {
        const std::lock_guard guard(mutex);
        if (pending) return false;
        release();
        return true;
    }
private:
    std::mutex mutex;
    bool pending = false;
};
struct LockFrameTraceBudget {
    static constexpr unsigned limit = 2048;
    unsigned lines = 0;
    bool take(bool enabled, bool trace) {
        if (!enabled || !trace || lines >= limit) return false;
        ++lines;
        return true;
    }
};
enum class LockLeaseAction { keep, acquire, release };
struct LockLeaseInput {
    bool enabled = false, eligible = false, settingsSafe = false, indicatorReady = false;
    bool interfaceAvailable = false, haveLease = false;
    std::optional<bool> locked;
    bool inputReleased = false;
};
struct LockLeaseDecision {
    LockLeaseAction action = LockLeaseAction::keep;
    bool lockStateAllowed = false;
};
constexpr LockLeaseDecision targetLockLease(LockLeaseInput i) {
    const bool eligible = i.enabled && i.eligible && i.settingsSafe && i.indicatorReady &&
        i.interfaceAvailable && i.locked.has_value();
    if (!eligible) return {i.haveLease ? LockLeaseAction::keep : LockLeaseAction::acquire,false};
    if (i.haveLease) return {*i.locked || !i.inputReleased ? LockLeaseAction::keep : LockLeaseAction::release,false};
    return {LockLeaseAction::keep,*i.locked};
}
// A held key emits an event on every input poll, so a quiet window means it is
// up. Time-based because the input sink may not run on the main thread.
struct LockInputIdle {
    static constexpr double window = 0.25;
    double lastActivity = 0;
    bool started = false;
    void activity(double now) { lastActivity = now; started = true; }
    bool ready(double now) const { return started && now-lastActivity >= window; }
};
struct LockRetargetWindow {
    std::optional<double> started;
    bool awaiting(bool dead, double now) {
        if (!dead) { started.reset(); return false; }
        if (!started) started = now;
        return now-*started < 1.0;
    }
};
struct LockIndicatorAttempt {
    static constexpr float duration = 2.0f;
    std::vector<std::uint32_t> previous;
    std::optional<std::uint32_t> effect;
    std::optional<double> started;
    void begin(std::vector<std::uint32_t> ids, double now) {
        previous = std::move(ids); effect.reset(); started = now;
    }
    bool isNew(std::uint32_t id) const {
        return started && std::find(previous.begin(),previous.end(),id) == previous.end();
    }
    bool waiting(double now) const { return started && !effect && now-*started < .5; }
    void reset() { previous.clear(); effect.reset(); started.reset(); }
};
template<class T> struct LockStateChange {
    std::optional<T> previous;
    bool changed(T value) {
        if (previous && *previous == value) return false;
        previous = value; return true;
    }
};
inline float lockWorldYaw(float forwardX, float forwardY) {
    return std::atan2(forwardX,forwardY);
}
struct LockSetting {
    const char* section;
    const char* key;
    double expected;
    bool real = false;
};
// Read-only contract, not provider implementation. Hold duration is a route
// parameter; all other values must match the declared merge fragment.
inline constexpr std::array lockSettings{
    LockSetting{"TargetLock","uTargetLockMode",1},
    LockSetting{"TargetLock","fTargetLockYawAdjustSpeed",0,true},
    LockSetting{"TargetLock","fTargetLockPitchAdjustSpeed",0,true},
    LockSetting{"TargetLock","bResetCameraWithTargetLock",0},
    LockSetting{"TargetLock","bResetCameraPitch",0},
    LockSetting{"TargetLock","bTargetLockEnableLockBehindTarget",0},
    LockSetting{"TargetLock","uTargetLockArrowAimType",1},
    LockSetting{"TargetLock","uTargetLockMissileAimType",1},
    LockSetting{"TargetLock","bTargetLockUsePOVSwitchGamepad",1},
    LockSetting{"TargetLock","bTargetLockUsePOVSwitchKeyboard",0},
    LockSetting{"TargetLock","fTargetLockPOVHoldDuration",0.4,true},
    LockSetting{"TargetLock","bTargetLockUseMouse",0},
    LockSetting{"TargetLock","bTargetLockUseScrollWheel",0},
    LockSetting{"TargetLock","bTargetLockUseRightThumbstick",0},
    LockSetting{"HUD","bEnableTargetLockReticle",0},
    LockSetting{"Keys","uTargetLockKey",22},
    LockSetting{"Keys","uSwitchTargetLeftKey",-1},
    LockSetting{"Keys","uSwitchTargetRightKey",-1},
    LockSetting{"Keys","uTargetLockBehindTargetKey",-1}
};
inline bool lockSettingMatches(LockSetting setting, std::string_view text, double holdSeconds = 0.4) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == text.npos) return false;
    text = text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
    const double expected = std::string_view(setting.key) == "fTargetLockPOVHoldDuration" ? holdSeconds : setting.expected;
    if (setting.real) {
        double value = 0;
        const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value);
        return parsed.ec == std::errc{} && parsed.ptr == text.data()+text.size() && std::isfinite(value) &&
            std::abs(value-expected) < 0.00001;
    }
    std::int64_t value = 0;
    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data()+text.size() &&
        (value == static_cast<std::int64_t>(expected) || (expected == -1 && value == 4294967295LL));
}
} // namespace skycrpg::camera
