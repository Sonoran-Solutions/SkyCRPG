// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
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
constexpr bool lockMenuPassive(std::string_view name) {
    return name == "HUD Menu" || name == "Cursor Menu" || name == "Fader Menu" || name == "Mist Menu" ||
        name == "LoadWaitSpinner";
}
constexpr bool lockMenuPublishesFocus(std::string_view name, bool opening) {
    return opening && !lockMenuPassive(name);
}
constexpr bool menuFocusNeedsReassert(unsigned epochBefore, unsigned epochAfterStore) {
    return epochBefore != epochAfterStore;
}
constexpr bool publishedMenuFocus(bool sampled, unsigned epochBefore, unsigned epochAfter) {
    return sampled || menuFocusNeedsReassert(epochBefore,epochAfter);
}
constexpr bool lockMenuInterrupt(std::string_view name, bool opening) {
    if (lockMenuPassive(name)) return false;
    if (name == "Dialogue Menu" || name == "Console" || name == "Loading Menu") return true;
    // Openings publish conservative focus until main-thread focused() runs.
    // The close barrier catches menus between two main-thread observations.
    // Never read UI::GetMenu or menu flags on the event thread.
    return !opening;
}
// Callbacks only publish interrupts. The main-thread consumer resets freshness
// and cleans up before release. The CAS linearizes release against requests;
// a request after it stays pending for the next service, without blocking input.
class LockInterrupt {
public:
    bool requested() const { return (state.load() & pending) != 0; }
    void request() { state.fetch_or(pending); }
    bool consume() { return (state.fetch_and(~pending) & pending) != 0; }
    template<class F> bool releaseIfClear(F release) {
        unsigned clear = 0;
        if (!state.compare_exchange_strong(clear,releasing)) return false;
        release();
        state.fetch_and(~releasing);
        return true;
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned pending = 1, releasing = 2;
    std::atomic<unsigned> state{0};
};
enum class LockRefreshSource : unsigned {
    startup = 1, consoleOpen = 2, consoleClose = 4, dialogue = 8,
    loadingOpen = 16, loadingClose = 32, journalClose = 64, lockOffMenu = 128
};
constexpr LockRefreshSource lockRefreshSource(std::string_view name, bool opening) {
    if (name == "Console") return opening ? LockRefreshSource::consoleOpen : LockRefreshSource::consoleClose;
    if (name == "Loading Menu") return opening ? LockRefreshSource::loadingOpen : LockRefreshSource::loadingClose;
    return name == "Dialogue Menu" ? LockRefreshSource::dialogue : LockRefreshSource::journalClose;
}
struct LockConfigResult { bool movementSafe = false, lockSafe = false; };
// Producers only OR source bits. One IO worker reads files; Main applies the
// scalar result. Pending stays set through BOTH stages, including application.
// A request during either stage survives for the next worker/Main pair.
class LockConfigRefresh {
public:
    void request(LockRefreshSource source) { state.fetch_or(static_cast<unsigned>(source)); }
    bool requested() const { return state.load() != 0; }
    unsigned snapshot() const { return state.load(); }
    unsigned beginRead() {
        auto value = state.load();
        if (!(value & sources) || (value & (reading | complete))) return 0;
        return state.compare_exchange_strong(value,reading) ? value & sources : 0;
    }
    void finishRead(LockConfigResult result) {
        state.fetch_or(complete | (result.movementSafe ? movementSafe : 0u) | (result.lockSafe ? lockSafe : 0u));
        state.fetch_and(~reading);
    }
    template<class Apply> bool apply(Apply applyResult) {
        const auto value = state.load();
        if (!(value & complete) || (value & reading)) return false;
        applyResult(LockConfigResult{(value & movementSafe) != 0,(value & lockSafe) != 0});
        state.fetch_and(~(complete | movementSafe | lockSafe));
        return true;
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned sources = 255, reading = 256, complete = 512, movementSafe = 1024, lockSafe = 2048;
    std::atomic<unsigned> state{0};
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
constexpr bool targetLockPrerequisites(bool haveLease, std::optional<bool> locked,
    bool firstPerson, bool enabled, bool safe) {
    return locked.has_value() &&
        ((haveLease && (!*locked || firstPerson)) || (!haveLease && enabled && safe));
}
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
