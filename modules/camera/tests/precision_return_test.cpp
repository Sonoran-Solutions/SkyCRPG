// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include "target_lock_policy.h"
#include <cassert>
#include <iostream>
#include <thread>
using namespace skycrpg::camera;

struct Route {
    Context c;
    bool lockEnabled = true;
    bool owned = false, haveLease = false;
    std::optional<bool> locked = false;
    FreshPress key;
    LockInterrupt interrupt;
    PrecisionReturn intent;
    unsigned returns = 0;
    explicit Route(bool enabled = true) : lockEnabled(enabled) { c.enabled = c.prerequisites = c.world = true; }
    void entry() {
        assert(key.update(true,true));
        assert(toggleRequest(c,owned) == ToggleRequest::firstPerson);
        const auto before = c;
        c.enabled = false; c.firstPerson = true;
        interrupt.request(); // off-main: no provider calls, no later TP update
        owned = claimPrecisionOwnership(before,c);
        assert(owned);
        assert(!key.update(false,false));
    }
    void press(bool edge = true) {
        if (key.update(true,edge)) {
            assert(toggleRequest(c,owned) == ToggleRequest::thirdPerson);
            if (lockEnabled) {
                assert(intent.request(c,owned,intent.generation()));
                interrupt.request();
            } else {
                haveLease = true; // successful existing Phase 4 acquisition
                ++returns; owned = false; c.firstPerson = false; c.enabled = true;
            }
        }
    }
    void service(bool granted = true, bool menuLocks = true) {
        if (!lockEnabled) return;
        if (interrupt.consume() && !haveLease) haveLease = granted;
        if (!c.world) { intent.cancel(); owned = false; return; }
        if (!menuLocks) return; // interrupt drain does not depend on menu lock
        // The existing ineligible lock policy retries a refused acquisition.
        const auto decision = targetLockLease({true,false,true,true,true,haveLease,locked,false});
        if (decision.action == LockLeaseAction::acquire) haveLease = granted;
        assert(decision.action != LockLeaseAction::release); // first person holds
        auto observed = c;
        observed.prerequisites = c.prerequisites && targetLockPrerequisites(haveLease,locked,true,true,true);
        owned = retainPrecisionOwnership(observed,owned,false);
        if (intent.take(observed,owned,haveLease,locked)) {
            ++returns; owned = false; c.firstPerson = false; c.enabled = true;
        }
    }
};
int main() {
    // Configuration model: only the target-lock flag controls deferral. Native
    // hook wiring/feature independence is also inspected in plugin.cpp.
    struct Profile { const char* name; bool lock; };
    for (const auto profile : {Profile{"lock-only",true}, Profile{"all-on",true},
        Profile{"fixed-only",false}, Profile{"cursor-only",false}}) {
        for (const bool beforeService : {false,true}) {
            Route r(profile.lock);
            std::thread entry([&] { r.entry(); }); entry.join();
            assert(!r.haveLease && r.owned);
            if (!beforeService) r.service();
            assert(r.owned && r.returns == 0);
            std::thread input([&] { r.press(); r.press(); r.press(false); }); input.join();
            r.service(); assert(r.returns == 1 && !r.owned && !r.c.firstPerson);
            r.service(); assert(r.returns == 1);
        }
        std::cout << "PASS configuration model: " << profile.name << '\n';
    }
    // Lease refusal retains one press, but cannot return until acquisition.
    {
        Route r; r.entry(); r.press();
        r.service(false); assert(!r.haveLease && r.returns == 0);
        r.service(false); assert(r.returns == 0);
        r.service(); assert(r.haveLease && r.returns == 1);
    }
    // Paused/delayed TDM acknowledgement must not revoke precision ownership.
    {
        Route r; r.locked = true; r.entry(); r.press();
        r.service(); r.service();
        assert(r.owned && r.haveLease && r.returns == 0);
        assert(targetLockPrerequisites(true,true,true,true,true));
        assert(!targetLockPrerequisites(true,true,false,true,true));
        r.locked = false; r.service(); assert(r.returns == 1);
        // Subsequent third-person lease release still needs unlocked + idle.
        LockLeaseInput lease{true,true,true,true,true,true,true,true};
        assert(targetLockLease(lease).action == LockLeaseAction::keep);
        lease.locked = false; lease.inputReleased = false;
        assert(targetLockLease(lease).action == LockLeaseAction::keep);
        lease.inputReleased = true;
        assert(targetLockLease(lease).action == LockLeaseAction::release);
    }
    // UI contention skips validation, not the interrupt; one intent survives.
    {
        Route r; r.entry(); r.press(); r.service(true,false);
        assert(r.haveLease && r.returns == 0); r.service(); assert(r.returns == 1);
    }
    // Load, death, provider loss, focus, hazards, slide and ownership loss.
    for (unsigned cause = 0; cause < 9; ++cause) {
        Route r; r.entry(); r.press();
        switch (cause) {
        case 0: r.intent.cancel(); r.owned = false; break; // load callback
        case 1: r.c.world = false; break; // death
        case 2: r.c.prerequisites = false; break; // missing provider
        case 3: r.c.menu = true; break;
        case 4: r.c.special = true; break;
        case 5: r.c.slide = true; break;
        case 6: r.owned = false; break;
        case 7: r.intent.cancel(); r.owned = false; break; // native POV
        default: r.intent.cancel(); r.c.prerequisites = false; break; // F8
        }
        r.service(); assert(r.returns == 0);
        r.c = {}; r.c.prerequisites = r.c.world = r.c.firstPerson = true;
        r.owned = true; r.service(); assert(r.returns == 0); // no stale replay
    }
    // Death is observed before the UI try-lock, so a busy UI cannot hide it.
    {
        Route r; r.entry(); r.press(); r.c.world = false;
        r.service(true,false); assert(!r.owned && r.returns == 0);
        r.c.world = true; r.owned = true; r.service(); assert(r.returns == 0);
    }
    // Native camera departure without a POV event cancels too.
    {
        Route r; r.entry(); r.press(); r.c.firstPerson = false;
        r.service(); assert(r.returns == 0);
        r.c.firstPerson = true; r.owned = true; r.service(); assert(r.returns == 0);
    }
    // Unknown lock observation fails closed and cancels invalid ownership.
    {
        Route r; r.entry(); r.press(); r.locked.reset(); r.service();
        assert(!r.owned && r.returns == 0);
        r.locked = false; r.owned = true; r.service(); assert(r.returns == 0);
    }
    // An in-flight input cannot enqueue after lifecycle cancellation, nor can
    // two independent fresh presses leave two returns queued.
    {
        PrecisionReturn intent;
        Context c{}; c.prerequisites = c.world = c.firstPerson = true;
        const auto stale = intent.generation();
        std::thread cancel([&] { intent.cancel(); }); cancel.join();
        assert(!intent.request(c,true,stale));
        assert(!intent.request(c,false,intent.generation()));
        assert(intent.request(c,true,intent.generation()));
        assert(!intent.request(c,true,intent.generation()));
        assert(intent.take(c,true,true,false));
        assert(!intent.take(c,true,true,false));
    }
    assert(phase4Prerequisites(true,false,true,false)); // OFF can request lease
    std::cout << "PASS precision return: service ordering, acknowledgement, refusal, input freshness, cancellations, feature combinations\n";
}
