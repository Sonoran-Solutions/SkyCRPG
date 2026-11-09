// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "precision_policy.h"
#include "target_lock_policy.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <semaphore>
using namespace skycrpg::camera;

struct Route {
    Context c;
    bool lockEnabled = true;
    bool haveLease = false;
    std::optional<bool> locked = false;
    FreshPress key;
    LockInterrupt interrupt;
    PrecisionReturn intent;
    unsigned returns = 0;
    std::uint32_t mountCount = 0;
    bool mounted = false;
    unsigned mountReads = 0;
    explicit Route(bool enabled = true) : lockEnabled(enabled) { c.enabled = c.prerequisites = c.world = true; }
    void entry() {
        assert(key.update(true,true));
        assert(toggleRequest(c,intent.owned()) == ToggleRequest::firstPerson);
        const auto before = c;
        c.enabled = false; c.firstPerson = true;
        interrupt.request(); // off-main: no provider calls, no later TP update
        assert(intent.claim(before,c,intent.snapshot()));
        assert(!key.update(false,false));
    }
    void claim() {
        if (intent.owned()) return;
        auto before = c; before.firstPerson = false;
        assert(intent.claim(before,c,intent.snapshot()));
    }
    void press(bool edge = true) {
        if (key.update(true,edge)) {
            assert(toggleRequest(c,intent.owned()) == ToggleRequest::thirdPerson);
            if (lockEnabled) {
                assert(intent.request(c,intent.snapshot()));
                interrupt.request();
            } else {
                haveLease = true; // successful existing Phase 4 acquisition
                ++returns; intent.cancel(); c.firstPerson = false; c.enabled = true;
            }
        }
    }
    void service(bool granted = true, bool menuLocks = true) {
        if (!lockEnabled) return;
        if (interrupt.consume() && !haveLease) haveLease = granted;
        if (!c.world || !c.firstPerson) { intent.cancel(); return; }
        if (!PrecisionReturn::requested(intent.snapshot())) return;
        if (!menuLocks) return; // interrupt drain does not depend on menu lock
        bool observedMount = false;
        {
            const PrecisionMountReadLock mountLock(mountCount);
            if (!mountLock) return;
            ++mountReads; observedMount = mounted;
        }
        // The existing ineligible lock policy retries a refused acquisition.
        const auto decision = targetLockLease({true,false,true,true,true,haveLease,locked,false});
        if (decision.action == LockLeaseAction::acquire) haveLease = granted;
        assert(decision.action != LockLeaseAction::release); // first person holds
        auto observed = c;
        observed.special = observed.special || observedMount;
        observed.prerequisites = c.prerequisites && targetLockPrerequisites(haveLease,locked,true,true,true);
        const auto sessionToken = intent.snapshot();
        intent.retain(observed,false,sessionToken);
        if (intent.take(observed,haveLease,locked,sessionToken)) {
            ++returns; intent.cancel(); c.firstPerson = false; c.enabled = true;
        }
    }
};
int main() {
    // Engine-compatible reader accounting, including conservative rejection
    // of writer recursion, reserved flags and count saturation.
    for (const std::uint32_t initial : {0u,2u,0x0ffffffeu,0x0fffffffu,0x10000000u,0x80000001u}) {
        std::uint32_t word = initial;
        {
            const PrecisionMountReadLock mountLock(word);
            const bool allowed = initial < 0x0fffffffu;
            assert(static_cast<bool>(mountLock) == allowed);
            assert(std::atomic_ref<std::uint32_t>(word).load() == initial+(allowed ? 1u : 0u));
        }
        assert(word == initial);
    }
    // A real writer holds the modeled extra-data lock over the callback.
    // No presence read or session mutation is allowed until it releases.
    for (const bool mountAfterWrite : {false,true}) {
        Route r; r.entry(); r.press();
        const auto pending = r.intent.snapshot();
        std::binary_semaphore held(0), release(0);
        std::thread writer([&] {
            std::atomic_ref<std::uint32_t> count(r.mountCount);
            std::uint32_t empty = 0;
            assert(count.compare_exchange_strong(empty,0x80000001u));
            r.mounted = mountAfterWrite;
            held.release(); release.acquire();
            count.store(0);
        });
        held.acquire();
        r.service(); r.service();
        assert(r.intent.snapshot() == pending && r.returns == 0 && r.mountReads == 0);
        assert(r.haveLease); // the independent interrupt drain still ran
        release.release(); writer.join();
        r.service();
        assert(r.mountReads == 1 && r.mountCount == 0);
        assert(r.returns == (mountAfterWrite ? 0u : 1u));
        assert(!r.intent.owned() && !PrecisionReturn::requested(r.intent.snapshot()));
        r.service(); assert(r.mountReads == 1); // consumed/cancelled exactly once
    }
    std::cout << "PASS mount contention: intent unchanged, no presence read; released lock evaluates mounted/unmounted\n";
    std::cout << "PASS mount read lock: shared readers, writer/reserved flags, saturation and balanced release\n";
    // Configuration model: only the target-lock flag controls deferral. Native
    // hook wiring/feature independence is also inspected in plugin.cpp.
    struct Profile { const char* name; bool lock; };
    for (const auto profile : {Profile{"lock-only",true}, Profile{"all-on",true},
        Profile{"fixed-only",false}, Profile{"cursor-only",false}}) {
        for (const bool beforeService : {false,true}) {
            Route r(profile.lock);
            std::thread entry([&] { r.entry(); }); entry.join();
            assert(!r.haveLease && r.intent.owned());
            if (!beforeService) r.service();
            assert(r.intent.owned() && r.returns == 0);
            std::thread input([&] { r.press(); r.press(); r.press(false); }); input.join();
            r.service(); assert(r.returns == 1 && !r.intent.owned() && !r.c.firstPerson);
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
        assert(r.intent.owned() && r.haveLease && r.returns == 0);
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
        case 0: r.intent.cancel(); break; // load callback
        case 1: r.c.world = false; break; // death
        case 2: r.c.prerequisites = false; break; // missing provider
        case 3: r.c.menu = true; break;
        case 4: r.c.special = true; break;
        case 5: r.c.slide = true; break;
        case 6: r.intent.cancel(); break;
        case 7: r.intent.cancel(); break; // native POV
        default: r.intent.cancel(); r.c.prerequisites = false; break; // F8
        }
        r.service(); assert(r.returns == 0);
        r.c = {}; r.c.prerequisites = r.c.world = r.c.firstPerson = true;
        r.claim(); r.service(); assert(r.returns == 0); // no stale replay
    }
    // Death is observed before the UI try-lock, so a busy UI cannot hide it.
    {
        Route r; r.entry(); r.press(); r.c.world = false;
        r.service(true,false); assert(!r.intent.owned() && r.returns == 0);
        r.c.world = true; r.claim(); r.service(); assert(r.returns == 0);
    }
    // Native camera departure without a POV event cancels too.
    {
        Route r; r.entry(); r.press(); r.c.firstPerson = false;
        r.service(); assert(r.returns == 0);
        r.c.firstPerson = true; r.claim(); r.service(); assert(r.returns == 0);
    }
    // Unknown lock observation fails closed and cancels invalid ownership.
    {
        Route r; r.entry(); r.press(); r.locked.reset(); r.service();
        assert(!r.intent.owned() && r.returns == 0);
        r.locked = false; r.claim(); r.service(); assert(r.returns == 0);
    }
    Context fp{}; fp.prerequisites = fp.world = fp.firstPerson = true;
    auto tp = fp; tp.firstPerson = false;
    // Review reproduction 1: the producer samples valid ownership/context while
    // a return is pending, then resumes only AFTER that return was consumed.
    {
        PrecisionReturn intent;
        assert(intent.claim(tp,fp,intent.snapshot()));
        std::binary_semaphore sampled{0}, resume{0};
        bool accepted = true;
        std::thread input([&] {
            const auto oldSession = intent.snapshot();
            const auto oldContext = fp;
            sampled.release(); resume.acquire();
            accepted = intent.request(oldContext,oldSession);
        });
        sampled.acquire();
        assert(intent.request(fp,intent.snapshot()));
        assert(intent.take(fp,true,false,intent.snapshot()));
        resume.release(); input.join();
        assert(!accepted && !intent.owned());
        intent.cancel(); // unconditional third-person departure service
        assert(intent.claim(tp,fp,intent.snapshot()));
        assert(!intent.take(fp,true,false,intent.snapshot()));
        std::cout << "PASS former race reproduction: stale producer rejected after take; no later-session replay\n";
    }
    // Review reproduction 2: a pre-cancel context cannot restore ownership.
    {
        PrecisionReturn intent;
        std::binary_semaphore checked{0}, assign{0};
        bool claimed = true;
        std::thread entry([&] {
            const auto oldSession = intent.snapshot();
            const bool valid = claimPrecisionOwnership(tp,fp);
            checked.release(); assign.acquire();
            claimed = valid && intent.claim(tp,fp,oldSession);
        });
        checked.acquire(); intent.cancel();
        assign.release(); entry.join();
        assert(!claimed && !intent.owned());
        std::cout << "PASS former race reproduction: claim CAS rejects completed cancellation\n";
    }
    // A retention observer that saw false cannot erase a legitimate later claim.
    {
        PrecisionReturn intent;
        std::binary_semaphore sampled{0}, resume{0};
        std::thread observer([&] {
            const auto oldSession = intent.snapshot();
            assert(!PrecisionReturn::owned(oldSession));
            sampled.release(); resume.acquire();
            intent.retain(fp,false,oldSession);
        });
        sampled.acquire(); assert(intent.claim(tp,fp,intent.snapshot()));
        resume.release(); observer.join(); assert(intent.owned());
        const auto oldOwned = intent.snapshot();
        intent.cancel(); assert(intent.claim(tp,fp,intent.snapshot()));
        intent.retain(Context{},true,oldOwned); // stale true observation cannot revoke new session
        assert(intent.owned());
        std::cout << "PASS claim/retention overlap: stale false and stale true observers preserve newer claim\n";
    }
    // Cancellation is one indivisible transition. An old producer loses; a
    // producer sampling the completed focus-cancel can enqueue without a later
    // pending-bit clear erasing it. Full cancellation revokes ownership too.
    {
        PrecisionReturn intent;
        assert(intent.claim(tp,fp,intent.snapshot()));
        const auto before = intent.snapshot();
        assert(intent.request(fp,before));
        intent.cancel(false);
        const auto after = intent.snapshot();
        assert(after != before && intent.owned() && !PrecisionReturn::requested(after));
        assert(!intent.request(fp,before));
        assert(intent.request(fp,after));
        assert(intent.take(fp,true,false,intent.snapshot()));
        assert(!intent.request(fp,before));
        assert(intent.claim(tp,fp,intent.snapshot()));
        const auto claim = intent.snapshot();
        intent.cancel();
        assert(!intent.owned() && !intent.request(fp,claim));
        assert(!intent.request(fp,intent.snapshot()));
        std::cout << "PASS cancel transition: no intermediate epoch/pending clear window\n";
    }
    // A departure observer sampled before a producer queued its request. Its
    // bounded invalidation must retire the newly pending bit in that same epoch.
    {
        PrecisionReturn intent;
        assert(intent.claim(tp,fp,intent.snapshot()));
        const auto departure = intent.snapshot();
        std::binary_semaphore sampled{0}, queued{0};
        std::thread input([&] {
            sampled.acquire();
            assert(intent.request(fp,departure));
            queued.release();
        });
        sampled.release(); queued.acquire();
        assert(intent.invalidate(departure));
        input.join();
        assert(!intent.owned() && !PrecisionReturn::requested(intent.snapshot()));
        assert(intent.claim(tp,fp,intent.snapshot()));
        assert(!intent.take(fp,true,false,intent.snapshot()));
        assert(!intent.invalidate(departure)); // must not revoke a newer claim
        assert(intent.owned());
        std::cout << "PASS departure/request overlap: pending producer retired without revoking a newer session\n";
    }
    // Exercise the terminal boundary directly, not billions of cancellations.
    {
        PrecisionReturn intent{PrecisionReturn::epochLimit-8};
        assert(intent.claim(tp,fp,intent.snapshot()));
        const auto oldSession = intent.snapshot();
        assert(intent.request(fp,oldSession));
        assert(intent.take(fp,true,false,intent.snapshot()));
        assert(intent.snapshot() == PrecisionReturn::epochLimit);
        for (unsigned i = 0; i < 4; ++i) {
            intent.cancel(); intent.cancel(false);
            assert(intent.snapshot() == PrecisionReturn::epochLimit);
            assert(!intent.claim(tp,fp,intent.snapshot()));
            assert(!intent.request(fp,oldSession));
            assert(!intent.take(fp,true,false,intent.snapshot()));
        }
        std::cout << "PASS epoch exhaustion: saturated terminal state cannot wrap or accept stale work\n";
    }
    assert(phase4Prerequisites(true,false,true,false)); // OFF can request lease
    std::cout << "PASS precision return: service ordering, acknowledgement, refusal, input freshness, cancellations, feature combinations\n";
}
