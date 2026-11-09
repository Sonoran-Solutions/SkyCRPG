// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include <cassert>
#include <iostream>
#include <semaphore>
#include <thread>
using namespace skycrpg::camera;

struct EntryRoute {
    Context camera;
    PrecisionReturn intent;
    PrecisionCameraObservation observation;
    const std::thread::id mainThread = std::this_thread::get_id();
    unsigned forces = 0;
    explicit EntryRoute() { camera.enabled = camera.prerequisites = camera.world = true; }
    bool publish(Context sampled, PrecisionReturn::Token token) { return intent.requestEntry(sampled,token); }
    bool service(bool contextAvailable = true, bool cancelDuringForce = false, bool refuseForce = false) {
        if (std::this_thread::get_id() != mainThread) return false;
        const auto token = intent.snapshot();
        if (observation.retire(camera.firstPerson,token)) {
            static_cast<void>(intent.invalidate(token)); return false;
        }
        if (!contextAvailable) return false;
        return intent.enter(camera,token,[&] {
            ++forces;
            camera.enabled = false;
            camera.firstPerson = !refuseForce;
            if (cancelDuringForce) intent.cancel();
            return camera; // immediate observation in the same Main operation
        });
    }
};

int main() {
    // Adapted from sr47 retire_repro.cpp, case 1: a cancelled FP observation
    // must not retire a new token captured after native POV and re-enable.
    for (const bool publishBeforeObservation : {false,true}) {
        EntryRoute r;
        assert(r.publish(r.camera,r.intent.snapshot())); assert(r.service());
        assert(!r.observation.retire(true,r.intent.snapshot()));
        r.intent.cancel(); // native POV
        r.camera.firstPerson = false;
        r.intent.cancel(); // deliberate enableOnly
        r.camera.enabled = true;
        const auto fresh = r.intent.snapshot();
        if (publishBeforeObservation) assert(r.publish(r.camera,fresh));
        assert(!r.observation.retire(false,r.intent.snapshot()));
        if (!publishBeforeObservation) assert(r.publish(r.camera,fresh));
        assert(r.service() && r.intent.owned() && r.forces == 2);
    }
    std::cout << "PASS sr47 stale-observation reproduction: cancelled epoch cannot retire fresh entry\n";

    // Adapted from sr47 case 2: input can pause only around publication. It
    // cannot force FP, carry an after-context, or claim after a hidden departure.
    {
        EntryRoute r;
        std::binary_semaphore sampled(0), resume(0);
        bool accepted = false, offMain = true;
        std::thread input([&] {
            const auto token = r.intent.snapshot();
            const auto before = r.camera;
            sampled.release(); resume.acquire();
            accepted = r.publish(before,token);
            offMain = r.service();
        });
        sampled.acquire();
        assert(!r.observation.retire(false,r.intent.snapshot()));
        r.camera.firstPerson = true; // unrelated native interval, unseen by service
        r.camera.firstPerson = false;
        assert(!r.service());
        resume.release(); input.join();
        assert(accepted && !offMain && !r.intent.owned() && r.forces == 0);
        // A new synchronous Main transition is required before any claim.
        assert(r.service() && r.forces == 1 && r.intent.owned());
        r.camera.firstPerson = false;
        assert(!r.service() && !r.intent.owned()); // owned departure before an FP frame sample
        r.camera.firstPerson = true;
        assert(!r.service() && !r.intent.owned() && r.forces == 1);
    }
    std::cout << "PASS sr47 hidden-departure reproduction: no delayed input claim; only a fresh Main transition owns FP\n";

    for (unsigned cause = 0; cause < 8; ++cause) {
        EntryRoute r;
        assert(r.publish(r.camera,r.intent.snapshot()));
        switch (cause) {
        case 0: r.camera.firstPerson = true; break;
        case 1: r.camera.world = false; break;
        case 2: r.camera.menu = true; break;
        case 3: r.camera.special = true; break;
        case 4: r.camera.slide = true; break;
        case 5: r.camera.prerequisites = false; break;
        case 6: r.camera.enabled = false; break;
        default: r.intent.cancel(); break;
        }
        assert(!r.service() && !r.intent.owned() && r.forces == 0);
        r.camera = EntryRoute{}.camera;
        assert(!r.service() && r.forces == 0); // no replay after recovery
    }
    {
        EntryRoute r;
        const auto old = r.intent.snapshot(); r.intent.cancel();
        assert(!r.publish(r.camera,old));
        assert(r.publish(r.camera,r.intent.snapshot()));
        const auto pending = r.intent.snapshot();
        assert(!r.publish(r.camera,pending));
        assert(!PrecisionReturn::requested(pending)); // entry is not a return
        assert(!r.service(false) && r.intent.snapshot() == pending && r.forces == 0);
        assert(r.service() && r.forces == 1);
        assert(!r.service() && r.forces == 1);
    }
    for (const bool cancelDuringForce : {false,true}) {
        EntryRoute r;
        assert(r.publish(r.camera,r.intent.snapshot()));
        assert(!r.service(true,cancelDuringForce,!cancelDuringForce));
        assert(!r.intent.owned() && !PrecisionReturn::entryRequested(r.intent.snapshot()));
        assert(r.forces == 1 && !r.service());
    }
    {
        EntryRoute r;
        const auto third = r.camera;
        assert(r.publish(third,r.intent.snapshot()));
        assert(r.intent.enter(third,r.intent.snapshot(),[&] {
            // A producer sampling during the native call must not steal the
            // new epoch's pending bit and make the immediate claim fail.
            assert(!r.publish(third,r.intent.snapshot()));
            r.camera.enabled = false; r.camera.firstPerson = true;
            const auto inFlight = r.intent.snapshot();
            assert(!PrecisionReturn::owned(inFlight));
            r.intent.retain(r.camera,false,inFlight); // concurrent safe FP input observation
            assert(r.intent.snapshot() == inFlight);
            return r.camera;
        }));
        assert(r.intent.owned());
    }
    // Same-epoch unowned departure still retires a pending producer; the
    // cancelled-epoch correction must not simply disable departure tracking.
    {
        EntryRoute r;
        const auto before = r.intent.snapshot();
        assert(!r.observation.retire(true,before));
        assert(r.publish(r.camera,before));
        assert(!r.service() && !r.intent.owned() && r.forces == 0);
        assert(!r.publish(r.camera,before));
    }
    std::cout << "PASS entry validation: fresh context, Main gate, contention, duplicate intent, cancellation, refusal and no replay\n";
}
