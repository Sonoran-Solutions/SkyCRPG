// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include "target_lock_policy.h"
#include <cassert>
#include <iostream>
#include <semaphore>
#include <thread>
using namespace skycrpg::camera;

// Adapter model: the ordinary SKSE task never runs on Main. Configuration IO
// and all-state Main application must make progress without that task or TP.
struct RefreshRoute {
    LockConfigRefresh refresh;
    LockConfigResult config{true,true};
    PrecisionReturn precision;
    Context c;
    bool lease = true, locked = false, chainReady = false;
    bool loading = false, indicatorStop = false, cursorActive = false;
    unsigned reads = 0, applications = 0, entries = 0, returns = 0;
    RefreshRoute() { c.enabled = c.world = c.prerequisites = true; }
    void menu(std::string_view name, bool opening) {
        if (name == "SkyCRPGCameraWorldPointer") return;
        if (lockMenuRefresh(name,opening)) refresh.request(lockRefreshSource(name,opening));
        if (lockMenuPublishesFocus(name,opening)) precision.cancel(false);
    }
    void ioPass() {
        std::thread worker([&] {
            if (refresh.beginRead()) { ++reads; refresh.finishRead({true,true}); }
        });
        worker.join();
    }
    void mainPass() {
        if (refresh.apply([&](LockConfigResult result) { config = result; })) ++applications;
        // Production checks the hook chain in the same Main service as F6,
        // independently of a starved ordinary task and its stale publication.
        if (PrecisionReturn::entryRequested(precision.snapshot()) || PrecisionReturn::requested(precision.snapshot()))
            chainReady = true;
        if (loading) return;
        c.prerequisites = config.movementSafe && chainReady;
        const auto token = precision.snapshot();
        if (PrecisionReturn::entryRequested(token)) {
            assert(precision.enter(c,token,[&] {
                ++entries; c.enabled = false; c.firstPerson = true; cursorActive = false;
                return c;
            }));
        } else if (PrecisionReturn::requested(token) && !refresh.requested() && precision.take(c,lease,locked,token)) {
            ++returns; c.enabled = true; c.firstPerson = false;
        }
    }
    void f6() {
        if (c.firstPerson) assert(precision.request(c,precision.snapshot()));
        else assert(precision.requestEntry(c,precision.snapshot()));
    }
};

int main() {
    for (const bool combined : {false,true}) {
        for (unsigned sequence = 0; sequence < 3; ++sequence) {
            RefreshRoute r;
            if (sequence == 2) {
                r.loading = true;
                r.menu("Loading Menu",true);
                r.ioPass(); r.mainPass(); // loading is not a refresh gate
                assert(!r.refresh.requested() && r.applications == 1);
                r.menu("Loading Menu",false); r.loading = false;
            } else {
                r.menu("Console",true); r.menu("Console",false);
                if (sequence == 0) {
                    r.cursorActive = true;
                    r.menu("SkyCRPGCameraWorldPointer",true);
                    r.menu("SkyCRPGCameraWorldPointer",false);
                    r.menu("SkyCRPGCameraWorldPointer",true);
                } else {
                    // U / F8 pair changes lock/lease/provider eligibility but
                    // does not request a refresh. The Console debt survives.
                    r.lease = false; r.locked = true;
                    r.c.enabled = false; r.lease = true; r.locked = false;
                    r.indicatorStop = true; r.c.enabled = true;
                }
            }
            assert(r.refresh.requested());
            // With a held acknowledged lease, pending refresh does not itself
            // invalidate entry's input context. Stale chain publication can.
            assert(targetLockPrerequisites(r.lease,r.locked,false,true,false));
            const auto beforeReads = r.reads, beforeApply = r.applications;
            r.f6(); r.mainPass(); // native entry even before IO completion
            assert(r.c.firstPerson && r.entries == 1 && r.precision.owned());
            r.menu("SkyCRPGCameraWorldPointer",false); // F6 cursor exit
            r.f6(); r.mainPass(); // return barrier stays closed
            assert(r.returns == 0 && PrecisionReturn::requested(r.precision.snapshot()));
            r.ioPass();
            assert(r.refresh.requested()); // result must be accepted by Main
            r.mainPass(); // apply in FP, then return in the SAME Main pass
            assert(!r.refresh.requested() && !r.c.firstPerson && r.returns == 1);
            assert(r.reads == beforeReads+1 && r.applications == beforeApply+1);
            r.mainPass(); assert(r.returns == 1);
            std::cout << "PASS " << (combined ? "combined-all" : "camera-only-lock-on") << " sequence=" << sequence
                << ": entry then return; one IO/Main pair; ordinary task withheld\n";
        }
    }
    {
        LockConfigRefresh refresh;
        refresh.request(LockRefreshSource::consoleClose);
        assert(refresh.beginRead() == static_cast<unsigned>(LockRefreshSource::consoleClose));
        assert(refresh.requested() && !refresh.beginRead());
        assert(!refresh.apply([](LockConfigResult) { assert(false); }));
        std::binary_semaphore reading(0), finish(0);
        std::thread worker([&] { reading.release(); finish.acquire(); refresh.finishRead({true,true}); });
        reading.acquire(); refresh.request(LockRefreshSource::loadingClose); finish.release(); worker.join();
        assert(!refresh.beginRead()); // no overwrite of unapplied result
        assert(refresh.apply([&](LockConfigResult result) {
            assert(result.movementSafe && result.lockSafe && refresh.requested());
            refresh.request(LockRefreshSource::journalClose); // concurrent producer during Main application
        }));
        assert(refresh.requested());
        assert(refresh.beginRead() == (static_cast<unsigned>(LockRefreshSource::loadingClose) |
            static_cast<unsigned>(LockRefreshSource::journalClose)));
        refresh.finishRead({false,false});
        assert(refresh.apply([](LockConfigResult result) {
            assert(!result.movementSafe && !result.lockSafe);
            assert(targetLockLease({true,true,result.lockSafe,true,true,true,false,true}).action == LockLeaseAction::keep);
        }));
        assert(!refresh.requested() && !refresh.apply([](LockConfigResult) { assert(false); }));
    }
    std::cout << "PASS requests during IO/application survive; invalid result completes fail-closed; no duplicate read/apply\n";
    for (const bool opening : {false,true}) {
        assert(!lockMenuRefresh("SkyCRPGCameraWorldPointer",opening));
        assert(!lockMenuRefresh("Cursor Menu",opening));
        assert(!lockMenuRefresh("TweenMenu",opening));
        assert(lockMenuRefresh("Console",opening));
        assert(lockMenuRefresh("Loading Menu",opening));
    }
}
