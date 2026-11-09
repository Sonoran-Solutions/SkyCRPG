// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include "precision_trace.h"
#include "target_lock_policy.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <semaphore>
#include <thread>
using namespace skycrpg::camera;

struct EntryRoute {
    Context camera;
    PrecisionReturn intent;
    PrecisionCameraObservation observation;
    PrecisionClaimTrace claimTrace;
    PrecisionTraceRecord lastClaim{};
    const std::thread::id mainThread = std::this_thread::get_id();
    unsigned forces = 0, claims = 0;
    explicit EntryRoute() { camera.enabled = camera.prerequisites = camera.world = true; }
    bool publish(Context sampled, PrecisionReturn::Token token,
        PrecisionClaimInput input = {1,true,true,{true,true}}) {
        return claimTrace.publish(intent,sampled,token,input);
    }
    bool service(bool contextAvailable = true, bool cancelDuringForce = false, bool refuseForce = false) {
        if (std::this_thread::get_id() != mainThread) return false;
        const auto token = intent.snapshot();
        if (observation.retire(camera.firstPerson,token)) {
            static_cast<void>(intent.invalidate(token)); return false;
        }
        if (!contextAvailable) return false;
        const auto input = std::optional<PrecisionClaimInput>{claimTrace.read(token).value_or(PrecisionClaimInput{})};
        const PrecisionTraceState before{camera,false,false,input->key.armed,input->key.down,9};
        const bool claimed = intent.enter(camera,token,[&] {
            ++forces;
            camera.enabled = false;
            camera.firstPerson = !refuseForce;
            if (cancelDuringForce) intent.cancel();
            return camera; // immediate observation in the same Main operation
        });
        if (claimed) {
            auto key = input->key;
            key.suspend();
            const PrecisionTraceState after{camera,false,true,key.armed,key.down,0};
            lastClaim = {PrecisionTraceEvent::toggleClaim,before,after,input->device,input->pressed,input->edge};
            ++claims;
        }
        return claimed;
    }
};

int main() {
    // Input metadata belongs to the accepted token, not to the latest button
    // seen by Main. Rejected duplicates and stale producers cannot replace it.
    for (const bool lockEnabled : {false,true}) {
        for (const unsigned device : {1u,2u}) {
            for (const bool pressed : {false,true}) {
                for (const bool edge : {false,true}) {
                    EntryRoute r;
                    r.camera.targetLock = lockEnabled;
                    const auto initial = r.intent.snapshot();
                    std::thread producer([&] {
                        assert(r.publish(r.camera,initial,{device,pressed,edge,{true,true}}));
                    });
                    producer.join();
                    assert(!r.publish(r.camera,initial,{0,!pressed,!edge,{}}));
                    assert(!r.service(false) && r.claims == 0); // UI/mount contention
                    assert(r.service() && r.claims == 1);
                    assert(!r.service() && r.claims == 1); // same token, no replay
                    const auto& record = r.lastClaim;
                    assert(record.event == PrecisionTraceEvent::toggleClaim);
                    assert(record.device == device && record.pressed == pressed && record.edge == edge);
                    assert(record.before.context.enabled && !record.before.context.firstPerson && !record.before.owned);
                    assert(!record.after.context.enabled && record.after.context.firstPerson && record.after.owned);
                    assert(record.before.armed && record.before.down && !record.after.armed && record.after.down);
                    char line[192]{};
                    assert(formatPrecisionTrace(line,sizeof(line),record));
                    assert(std::strstr(line,"event=11 ") && std::strstr(line,"camera=9/0 "));
                    // A cancelled epoch must not leak its metadata into a new entry.
                    r.intent.cancel(); r.camera.firstPerson = false; r.camera.enabled = true;
                    assert(r.publish(r.camera,r.intent.snapshot(),{3-device,true,true,{true,true}}));
                    assert(!r.publish(r.camera,initial,{device,pressed,edge,{}}));
                    assert(r.service() && r.claims == 2 && r.lastClaim.device == 3-device);
                }
            }
        }
    }
    // The debug sample never gates entry: an intent published without one
    // (or while the sample flag is busy) still claims once, tracing device 0.
    for (const bool lockEnabled : {false,true}) {
        EntryRoute r;
        r.camera.targetLock = lockEnabled;
        assert(r.intent.requestEntry(r.camera,r.intent.snapshot()));
        assert(r.service() && r.claims == 1 && r.lastClaim.device == 0);
        assert(!r.service() && r.claims == 1);
    }
    std::cout << "PASS claim trace: lock on/off; stored device/press/edge; event=11 camera=9/0; contention emits none; one per token; stale/duplicate publication preserves metadata\n";
    // Lock-off entry must progress from the camera frame even if every SKSE
    // ordinary task runs off Main. Configuration/optional hooks are not a gate.
    for (const bool menuContention : {false,true}) {
        EntryRoute r;
        PrecisionDiagnostics diagnostic;
        const auto initial = r.intent.snapshot();
        std::thread input([&] {
            assert(r.publish(r.camera,initial));
            diagnostic.note(PrecisionReason::entryAccepted,r.camera,initial);
        });
        input.join();
        const auto pending = r.intent.snapshot();
        std::thread taskDrain([&] {
            for (unsigned pass = 0; pass < 120; ++pass) {
                assert(!r.service());
                diagnostic.note(PrecisionReason::serviceOther);
            }
        });
        taskDrain.join();
        assert(r.intent.snapshot() == pending && r.forces == 0);
        // Model either UI or mount contention in the before-context reader.
        assert(!r.service(false));
        diagnostic.note(menuContention ? PrecisionReason::menuBusy : PrecisionReason::mountBusy);
        assert(r.intent.snapshot() == pending && r.forces == 0);
        // First eligible camera frame, without another task-drain invocation.
        assert(r.service() && r.intent.owned() && r.camera.firstPerson && r.forces == 1);
        diagnostic.note(PrecisionReason::entryClaimed,r.camera,r.intent.snapshot(),false,true);
        assert(!r.service() && r.forces == 1);
        unsigned emitted = 0;
        diagnostic.drain([&](unsigned, std::uint32_t) { ++emitted; });
        assert(emitted == 4); // 120 off-Main samples produce one reason
        diagnostic.note(PrecisionReason::serviceOther);
        diagnostic.drain([&](unsigned, std::uint32_t) { ++emitted; });
        assert(emitted == 4);
    }
    std::cout << "PASS lock-off entry: worker intent; 120 off-Main drains preserve intent; UI/mount contention preserves intent; first eligible Main frame claims once; diagnostics bounded\n";
    // Deferred adapter model: IO is gated separately from native entry. The
    // source audit checks this wiring against plugin.cpp; these calls exercise
    // the real intent, menu and lock-lease policies for both feature settings.
    for (const bool lockEnabled : {false,true}) {
        for (const bool outsideMenuLocks : {false,true}) {
            for (unsigned route = 0; route < 3; ++route) {
                EntryRoute r;
                LockInterrupt refresh, interrupt;
                bool recenterRequested = false, haveLease = !lockEnabled;
                if (route == 0) refresh.request();
                else {
                    // Console close is a refresh source in both profiles;
                    // Inventory/Magic close refreshes only with lock disabled.
                    r.camera.menu = true;
                    if (lockMenuPublishesFocus("Console",true)) r.intent.cancel(false);
                    r.camera.menu = false;
                    if (!lockEnabled || lockMenuRefresh("Console",false)) refresh.request();
                    if (route == 2) recenterRequested = heldFramingAllowed(r.camera); // F7
                }
                assert(refresh.requested());
                assert(recenterRequested == (route == 2));
                assert(r.publish(r.camera,r.intent.snapshot())); // F6 after menu/F7
                interrupt.request();
                const auto pending = r.intent.snapshot();
                // Reproduce candidate 4's entry gate without consuming intent.
                const bool oldWouldService = !refresh.requested();
                assert(!oldWouldService && r.intent.snapshot() == pending && r.forces == 0);
                unsigned passes = 0, reads = 0;
                const auto deferredPass = [&] {
                    ++passes;
                    if (outsideMenuLocks && refresh.consume()) ++reads;
                    if (lockEnabled && interrupt.consume()) haveLease = true;
                    return r.service(); // independent of refresh/IO-gate state
                };
                assert(deferredPass());
                assert(passes == 1 && r.forces == 1 && r.intent.owned() && haveLease);
                assert(reads == (outsideMenuLocks ? 1u : 0u));
                assert(refresh.requested() == !outsideMenuLocks);
                // Pending refresh still closes lock release; native entry did
                // not clear it or weaken the unlocked acknowledgement barrier.
                if (refresh.requested()) {
                    const auto lease = targetLockLease({lockEnabled,true,false,true,true,haveLease,false,true});
                    assert(lease.action == LockLeaseAction::keep && !lease.lockStateAllowed);
                }
                assert(!deferredPass() && r.forces == 1);
            }
        }
        std::cout << "PASS refresh entry lock-" << (lockEnabled ? "on" : "off")
            << ": pending refresh, Console close, F7 then F6; one deferred Main pass with IO allowed or deferred\n";
    }

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

    // Adapted from sr47b menu_repro.cpp: the whole menu interval occurs
    // between input publication and service, including lock-off deferred entry.
    for (const bool lockEnabled : {false,true}) {
        EntryRoute r;
        FreshPress key;
        LockInterrupt refresh;
        const auto menu = [&](bool opening) {
            r.camera.menu = opening;
            // Mirror the menu sink: precision cancellation precedes the lock gate.
            if (lockMenuPublishesFocus("InventoryMenu",opening)) r.intent.cancel(false);
            if (lockEnabled) {
                if (lockMenuRefresh("InventoryMenu",opening)) refresh.request();
                if (!lockMenuInterrupt("InventoryMenu",opening)) return;
            }
            if (!lockEnabled) refresh.request();
            key.suspend();
        };
        assert(key.update(true,true));
        assert(r.publish(r.camera,r.intent.snapshot()));
        const auto pending = r.intent.snapshot();
        menu(true); menu(false); // no input/context/service observation inside
        static_cast<void>(refresh.consume()); // task refresh precedes entry service
        assert(!r.service() && r.forces == 0 && !r.intent.owned());
        assert(r.intent.snapshot() != pending && !PrecisionReturn::entryRequested(r.intent.snapshot()));
        assert(!r.publish(r.camera,pending)); // stale producer cannot republish
        assert(!key.update(true,true) && !r.service()); // held F6 cannot replay
        assert(!key.update(false,true) && key.update(true,true));
        assert(r.publish(r.camera,r.intent.snapshot()));
        assert(r.service() && r.forces == 1 && r.intent.owned());
        // Preserve the existing active-session retention, clearing return intent.
        assert(r.intent.request(r.camera,r.intent.snapshot()));
        menu(true); menu(false);
        static_cast<void>(refresh.consume());
        assert(r.intent.owned() && !PrecisionReturn::requested(r.intent.snapshot()));
        assert(!r.service() && r.forces == 1 && r.intent.owned());
        std::cout << "PASS sr47b menu reproduction: lock-" << (lockEnabled ? "on" : "off")
            << " cancels entry across full menu interval; fresh F6 required; owned FP retained\n";
    }
    {
        EntryRoute r;
        assert(r.publish(r.camera,r.intent.snapshot()));
        PrecisionDiagnostics diagnostic;
        diagnostic.note(PrecisionReason::entryToken,r.camera,r.intent.snapshot(),false,true);
        assert(r.service() && r.intent.request(r.camera,r.intent.snapshot()));
        diagnostic.note(PrecisionReason::requestAccepted,r.camera,r.intent.snapshot());
        unsigned emitted = 0;
        diagnostic.drain([&](unsigned reason, std::uint32_t bits) {
            const bool entry = reason == static_cast<unsigned>(PrecisionReason::entryToken);
            assert(((bits & 256u) != 0) == !entry);
            assert(((bits & 2048u) != 0) == entry);
            ++emitted;
        });
        assert(emitted == 2);
        std::cout << "PASS sr47b diagnostics: bit 8 encodes return only; bit 11 carries the caller's Main marker\n";
    }

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
        assert(!r.service() && !r.intent.owned() && r.forces == 0 && r.claims == 0);
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
        assert(r.forces == 1 && !r.service() && r.claims == 0);
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
