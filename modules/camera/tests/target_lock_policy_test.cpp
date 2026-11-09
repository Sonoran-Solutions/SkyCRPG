// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "target_lock_policy.h"
#include "lifecycle_policy.h"
#include "obstruction_policy.h"
#include <cassert>
#include <fstream>
#include <map>
#include <string>
#include <thread>
using namespace skycrpg::camera;
int main() {
    LockLeaseInput i{true,true,true,true,true,true,true,true};
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.eligible = false; // menu opens while TDM is paused
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.eligible = true; // close before unpaused acknowledgement
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.locked = false; i.inputReleased = false;
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.inputReleased = true;
    assert(targetLockLease(i).action == LockLeaseAction::release);
    i.haveLease = false;
    assert(!targetLockLease(i).lockStateAllowed);
    i.locked = true; assert(targetLockLease(i).lockStateAllowed);
    i.interfaceAvailable = false;
    assert(targetLockLease(i).action == LockLeaseAction::acquire && !targetLockLease(i).lockStateAllowed);
    i.interfaceAvailable = true; i.locked = std::nullopt;
    assert(targetLockLease(i).action == LockLeaseAction::acquire);
    i.locked = false; i.settingsSafe = false;
    assert(targetLockLease(i).action == LockLeaseAction::acquire);
    i.settingsSafe = true; i.indicatorReady = false;
    assert(targetLockLease(i).action == LockLeaseAction::acquire);
    // Feature OFF bypasses the lock policy, using the Phase 4 expression even
    // with an unavailable/unset/off-thread TDM observation.
    for (unsigned bits = 0; bits < 16; ++bits) {
        const bool base = bits&1, lease = bits&2, request = bits&4, have = bits&8;
        assert(phase4Prerequisites(base,lease,request,have) ==
            (base && (lease || (request && !have))));
    }
    for (unsigned bits = 0; bits < 8; ++bits) {
        const bool enabled = bits&1, requested = bits&2, have = bits&4;
        assert(leaseWanted(enabled,requested,have) == ((enabled || requested) && !have));
    }
    assert(!leaseWanted(false,false,false)); // disabled startup does not request
    for (unsigned bits = 0; bits < 32; ++bits) {
        const bool enabled = bits&1, mainThread = bits&2, base = bits&4, lease = bits&8, reportable = bits&16;
        const bool failure = lockFailureReportable(enabled,mainThread) && prerequisiteFailure(base,lease,reportable);
        assert(failure == ((!enabled || mainThread) && !(base && lease) && reportable));
    }
    FailureDebounce failureDebounce;
    for (double now : {10.0,11.0,12.0}) {
        const bool failure = lockFailureReportable(true,false) && prerequisiteFailure(true,false,true);
        assert(!failure && !failureDebounce.observe(failure,now)); // off-main first-person input cannot latch
    }
    assert(!failureDebounce.observe(lockFailureReportable(true,true) && prerequisiteFailure(true,false,true),13));
    assert(failureDebounce.observe(lockFailureReportable(true,true) && prerequisiteFailure(true,false,true),14));
    for (const auto name : {"HUD Menu","Cursor Menu","Fader Menu","Mist Menu","LoadWaitSpinner"}) {
        assert(!lockMenuInterrupt(name,true) && !lockMenuInterrupt(name,false));
        assert(!lockMenuRefresh(name,true) && !lockMenuRefresh(name,false));
    }
    for (const auto name : {"Dialogue Menu","Console","Loading Menu"}) {
        assert(lockMenuInterrupt(name,true) && lockMenuInterrupt(name,false));
        assert(lockMenuRefresh(name,true) && lockMenuRefresh(name,false));
    }
    for (const auto name : {"InventoryMenu","Custom Menu","TDM_BossBar","Tutorial Menu","StatsMenuSkillRing"}) {
        assert(!lockMenuInterrupt(name,true) && lockMenuInterrupt(name,false));
        assert(!lockMenuRefresh(name,true) && !lockMenuRefresh(name,false));
    }
    assert(!lockMenuInterrupt("Journal Menu",true) && lockMenuInterrupt("Journal Menu",false));
    assert(!lockMenuRefresh("Journal Menu",true) && lockMenuRefresh("Journal Menu",false));
    static_assert(static_cast<unsigned>(State::targeting) == 6);
    static_assert(static_cast<unsigned>(State::obstruction) == 7);
    static_assert(static_cast<unsigned>(State::normal) == 8);
    for (unsigned bits = 0; bits < 2048; ++bits) {
        Context off{bool(bits&1),bool(bits&2),bool(bits&4),bool(bits&8),bool(bits&16),
            bool(bits&32),bool(bits&64),bool(bits&128),bool(bits&256),bool(bits&512),bool(bits&1024)};
        const auto phase4 = !off.enabled || !off.prerequisites ? State::disabled :
            !off.world ? State::invalidWorld : off.special ? State::special : off.menu ? State::menu :
            off.firstPerson ? State::firstPerson : off.precision ? State::precision :
            off.targeting ? State::targeting : off.obstruction ? State::obstruction : State::normal;
        assert(!off.targetLock && select(off) == phase4);
        assert(heldFramingAllowed(off) == obstructionFramingAllowed(off));
        assert(applyDeferredPreferences(off,true,true) == (phase4 == State::normal));
    }
    LockFrameTraceBudget trace;
    for (unsigned row = 0; row < LockFrameTraceBudget::limit+10; ++row) {
        assert(!trace.take(false,true) && !trace.take(true,false));
        assert(trace.take(true,true) == (row < LockFrameTraceBudget::limit));
    }
    assert(trace.lines == LockFrameTraceBudget::limit);
    LockInputIdle input;
    assert(!input.ready(100)); // nothing observed yet: the lease stays held
    input.activity(10); assert(!input.ready(10) && !input.ready(10.2) && input.ready(10.25));
    input.activity(10.2); assert(!input.ready(10.3)); // a key held through a menu keeps renewing
    assert(input.ready(10.45));
    input.activity(11); assert(!input.ready(11.1)); // an interruption restarts the window
    assert(input.ready(11.3));
    LockIndicatorAttempt indicator;
    indicator.begin({10,11},5.0);
    assert(!indicator.isNew(10) && !indicator.isNew(11) && indicator.isNew(12));
    assert(indicator.waiting(5.0) && indicator.waiting(5.49) && !indicator.waiting(5.5));
    indicator.effect = 12;
    assert(!indicator.waiting(5.01)); // disappearance after discovery is a failure
    indicator.reset(); assert(!indicator.effect && !indicator.waiting(5.1) && !indicator.isNew(12));
    indicator.begin({10,11,12},6.0); // retarget/retry excludes previous sources
    assert(!indicator.isNew(12) && indicator.isNew(13));
    LockRetargetWindow retarget;
    assert(retarget.awaiting(true,10.0));
    assert(retarget.awaiting(true,10.99));
    assert(!retarget.awaiting(true,11.0) && !retarget.awaiting(true,11.1));
    assert(!retarget.awaiting(false,11.2) && !retarget.started);
    assert(retarget.awaiting(true,12.0));
    i = {true,true,true,true,true,false,false,true}; // dead target awaiting TDM
    assert(targetLockLease(i).action == LockLeaseAction::keep && !targetLockLease(i).lockStateAllowed);
    i.locked = true; assert(targetLockLease(i).lockStateAllowed); // new live target
    i.locked = std::nullopt; assert(targetLockLease(i).action == LockLeaseAction::acquire);
    LockStateChange<unsigned> leaseLog;
    assert(leaseLog.changed(2));
    for (unsigned frame = 0; frame < 10000; ++frame) assert(!leaseLog.changed(2));
    assert(leaseLog.changed(3) && !leaseLog.changed(3) && leaseLog.changed(0));
    for (float yaw : {-3.0f,-1.57f,-.1f,0.0f,.1f,1.57f,3.0f})
        assert(std::abs(lockWorldYaw(std::sin(yaw),std::cos(yaw))-yaw) < .00001f);
    LockInterrupt interrupt;
    unsigned releases = 0;
    auto release = [&] { ++releases; };
    std::thread callback([&] {
        // An unlisted menu opens and closes entirely between main-thread
        // observations. Only its close queues the fresh-input barrier.
        if (lockMenuInterrupt("Custom Menu",true)) interrupt.request();
        if (lockMenuInterrupt("Custom Menu",false)) interrupt.request();
    });
    callback.join();
    assert(!interrupt.releaseIfClear(release) && releases == 0);
    if (interrupt.consume()) input.activity(20);
    i = {true,true,true,true,true,true,false,input.ready(20)};
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.inputReleased = input.ready(20.3);
    assert(targetLockLease(i).action == LockLeaseAction::release);
    // Callback between policy observation and side effect must still win.
    interrupt.request();
    assert(!interrupt.releaseIfClear(release) && releases == 0);
    assert(interrupt.consume()); input.activity(21);
    assert(!input.ready(21) && !interrupt.consume());
    i.inputReleased = input.ready(21.3);
    assert(targetLockLease(i).action == LockLeaseAction::release);
    assert(interrupt.releaseIfClear(release) && releases == 1);
    // A transient indicator refusal uses the same interruption/barrier. The
    // next fresh lock remains possible; there is no session failure latch.
    input.activity(22); i.haveLease = true; i.locked = true; i.inputReleased = false;
    assert(targetLockLease(i).action == LockLeaseAction::keep);
    i.locked = false; i.inputReleased = input.ready(22.3);
    assert(targetLockLease(i).action == LockLeaseAction::release);
    i.haveLease = false; i.locked = true;
    assert(targetLockLease(i).lockStateAllowed);
    Context c{}; c.enabled = c.prerequisites = c.world = c.targetLock = c.obstruction = true;
    assert(select(c) == State::targetLock && !obstructionFramingAllowed(c) && heldFramingAllowed(c));
    assert(applyDeferredPreferences(c,true,true));
    c.precision = true; assert(select(c) == State::precision && !heldFramingAllowed(c));
    c.firstPerson = true; assert(select(c) == State::firstPerson);
    c.menu = true; assert(select(c) == State::menu);
    c.special = true; assert(select(c) == State::special);
    c.world = false; assert(select(c) == State::invalidWorld);
    c.enabled = false; assert(select(c) == State::disabled);
    // Test the opt-in overlay and the runtime guard's shared parser.
    std::ifstream fragment("modules/camera/config/TrueDirectionalMovement-target-lock.fragment.ini");
    assert(fragment);
    std::map<std::string,std::string> values;
    std::string line, section;
    auto trim = [](std::string value) {
        const auto start = value.find_first_not_of(" \t\r\n");
        return start == value.npos ? std::string{} : value.substr(start,value.find_last_not_of(" \t\r\n")-start+1);
    };
    while (std::getline(fragment,line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') { section = line.substr(1,line.size()-2); continue; }
        const auto equal = line.find('='); assert(equal != line.npos);
        values[section+"/"+trim(line.substr(0,equal))] = trim(line.substr(equal+1));
    }
    unsigned guarded = 0;
    for (const auto& setting : lockSettings) {
        const std::string key = std::string(setting.section)+"/"+setting.key;
        assert(values.count(key) && lockSettingMatches(setting,values[key]));
        ++guarded;
        assert(!lockSettingMatches(setting,""));
        assert(!lockSettingMatches(setting,"nan"));
        assert(!lockSettingMatches(setting,"inf"));
        assert(!lockSettingMatches(setting,"0junk"));
        assert(!lockSettingMatches(setting,"7"));
        // Effective user MCM value replaces the provider value, including an
        // explicitly empty/malformed override. Never fall back to safe defaults.
        auto live = values; live[key] = "7";
        assert(!lockSettingMatches(setting,live[key]));
        live[key] = values[key]; assert(lockSettingMatches(setting,live[key]));
        if (setting.expected == -1) assert(lockSettingMatches(setting,"4294967295"));
    }
    for (const auto& [key,value] : values) {
        if (key.starts_with("TargetLock/") || key.starts_with("Keys/") || key.starts_with("HUD/")) {
            bool found = false;
            for (const auto& setting : lockSettings) found |= key == std::string(setting.section)+"/"+setting.key;
            assert(found); // no fragment lock key silently escapes the live guard
        }
    }
    assert(guarded == 19);
    const LockSetting yaw{"DirectionalMovement","uAdjustCameraYawDuringMovement",0};
    assert(lockSettingMatches(yaw,"0"));
    assert(!lockSettingMatches(yaw,"1") && !lockSettingMatches(yaw,""));
    assert(!lockSettingMatches(yaw,"0.0")); // unchanged integer preflight
    const LockSetting hold{"TargetLock","fTargetLockPOVHoldDuration",.4,true};
    assert(lockSettingMatches(hold," 0.4 "));
    assert(!lockSettingMatches(hold,"0.25"));
    assert(lockSettingMatches(hold,"0.6",.6));
}
