// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace skycrpg::selection::api {
inline constexpr std::uint32_t version = 2;
inline constexpr char exportName[] = "SkyCRPGSelection_GetAPI";
enum class Result : std::uint32_t { ok, unavailable, wrongThread, disabled, invalidTarget, invalidArgument, queued, busy };
struct Snapshot {
    std::uint32_t size = sizeof(Snapshot);
    std::uint32_t apiVersion = version;
    std::uint32_t target = 0, candidateCount = 0, latched = 0, passthrough = 0;
    std::uint64_t refreshes = 0, inspectedTotal = 0, raysTotal = 0, lastCostMicros = 0;
    std::uint64_t pickChecks = 0, lastPickCostMicros = 0;
    std::uint32_t inspectedLast = 0, cappedScans = 0;
    std::uint32_t visitedLast = 0, scanRaysLast = 0;
    double radius = 0;
    std::uint32_t available = 0, validatedLast = 0;
    std::uint32_t externalTarget = 0, externalResult = static_cast<std::uint32_t>(Result::unavailable);
    std::uint64_t cyclesTotal = 0, eventSuppressed = 0, requestDrops = 0;
};
// Coherent snapshot/list publication from Pick; readers never acquire its state gate.
inline constexpr std::uint32_t cursorListCapacity = 16; // CursorFrame::ids and setCursorVisible
struct CursorFrame {
    std::uint32_t size = sizeof(CursorFrame);
    Snapshot snapshot;
    std::uint32_t viable = 0, count = 0, ids[cursorListCapacity]{};
    std::uint64_t completedRefreshes = 0, replies = 0;
    std::uint32_t authorizedTarget = 0; // live atomic authorization, may be newer than snapshot
    std::uint64_t aliveRefreshes = 0; // definite attempts, including exhausted/capped scans
    double refreshSeconds = .1;
    std::uint32_t listReady = 0; // zero with viable=1 withholds the list; count=0 alone is not empty
};
struct Interface {
    std::uint32_t size;
    std::uint32_t apiVersion;
    Result (*getSnapshot)(Snapshot*);
    Result (*copyCandidates)(std::uint32_t* ids, std::uint32_t capacity, std::uint32_t* count);
    Result (*setTarget)(std::uint32_t formID);
    Result (*clearTarget)();
    // Optional append-only v2 extension. Check size before reading this member.
    // 0 exit, 1 enter empty, 2 query active, 3 enter empty with visibility
    // reports: [ ] cycling is refused until the first setCursorVisible of this
    // session arrives. A table without setCursorVisible rejects 3.
    Result (*setCursorMode)(std::uint32_t mode);
    Result (*setCursorTarget)(std::uint32_t formID, std::uint64_t observedCycles);
    Result (*getCursorFrame)(CursorFrame*);
    // Optional append-only cursor visibility member. Check size before reading
    // it. The cursor client reports the listed candidates that have an on-screen
    // box (owner decision 2026-11-08); in a session entered with mode 3, [ ]
    // cycling visits only reported IDs. Reports belong to the session that is
    // active when they are sent; a session entered with mode 1 never restricts.
    Result (*setCursorVisible)(const std::uint32_t* ids, std::uint32_t count);
};
using GetAPI = const Interface* (*)(std::uint32_t requestedVersion);
// Resolve via GetModuleHandleW(L"SkyCRPG_Selection.dll") / GetProcAddress;
// no link dependency. Never LoadLibrary it. Request exactly version 2 after
// kDataLoaded. Version 1 is rejected: it promised synchronous main-thread work.
// All methods now support any thread and never initiate a scan or call engine
// functions. Legacy snapshot/list reads return unavailable on contention.
// wrongThread is a reserved legacy enum value and is never returned by v2.
// Buffers belong to the caller; no engine pointers or STL types cross the ABI.
// Snapshots describe the most recent Pick job. available=0 means no injection
// is currently permitted, even if a retained target/latch is nonzero (e.g. a
// refused ray). Reads do not refresh stale state. Check availability and age by
// observing refreshes; this interface never promises a future native pick.
// copyCandidates requires scanReady and reports the legacy frozen order/count;
// a short buffer receives the leading IDs.
// setTarget/clearTarget post one atomic request. queued means accepted into the
// mailbox, NOT validated/applied. A busy legacy mailbox returns unavailable; callers
// must not spin. Default bAllowExternalTargets=0 returns disabled.
// Pick consumes legacy external requests at most once per refresh interval, checks
// membership in its validated bounded list and fresh eligibility/range/body LOS,
// and only then latches. Invalid/unknown pushes preserve the existing selection.
// externalTarget/externalResult report the last consumed request (zero=legacy clear).
// Cursor extension: require the entire Interface size, including getCursorFrame.
// CursorFrame uses an exact size check; rebuild both cursor consumers together.
// Read once per main-thread frame. viable describes world/session eligibility,
// independently of available (a validated target). listReady=1,count=0 is empty;
// listReady=0 withholds the list and must not revoke hover while viable.
// aliveRefreshes advances on every definite scan attempt, including validation,
// reference/visit and ray budget exhaustion; contention/indeterminate reads do not
// advance it. Cursor-eligible native passthroughs also advance it before entry.
// completedRefreshes remains the usable-scan diagnostic counter. Camera bounds
// age by max(0.5, 3*refreshSeconds), then requires three further engine updates
// without liveness before ending capture; a single hitch is not session loss.
// setCursorMode(1) enters EMPTY exclusive mode; (2) queries session, (0) exits.
// Only query failure or stale alive refreshes imply session loss.
// setCursorTarget(0,cycles) revokes authorization synchronously, without waiting
// for any gate or Pick. Nonzero requests revoke before trying any gate, then post a tagged
// mailbox request. queued is not acceptance; reconcile replies/externalResult.
// New requests replace older pending hover intent. A newer cycle rejects hover.
// Cursor requests are consumed at the next eligible Pick, without the legacy
// mailbox throttle. replies counts consumed cursor requests, including rejection.
// The activation-path handle resolver checks the current cursor token, so an E
// between clear/entry and the next Pick cannot activate an old native handle.
// Matching permits resolve the stored cursor handle while native Pick is in
// flight. After exit the last injected handle is refused until Pick completes.
// Pick also suppresses activation/grab handles on contention or validation failure.
// StartGrabObject reads the target directly: hold-grab revocation awaits next Pick;
// the ActivatePickRef resolver guard does not cover that native path.
// Cursor reads/writes never acquire the Pick state gate. Publication/mailbox
// contention returns busy without spinning; Pick skips that publication/request.
// Cycles retain a frozen list; hover rebinds to the current validated prefix.
// Lifecycle invalidation ends the session. No live DLL unload is supported.
// The mailbox is single-caller coordination, not a multi-client arbitration API.
// Lifecycle invalidation discards pending requests. No request is serialized.
// Cycling is an atomic FIFO; Pick applies each physical edge once, in order.
// Legacy candidate order is a validated nearest prefix (up to eight), frozen
// for a latch. CursorFrame exposes the refreshed prefix plus validated retained
// cycle members (up to sixteen); hover rebinds a new eight-item latch.
// visitedLast counts cheap callbacks; inspectedLast counts nearby geometry;
// validatedLast counts expensive predicate/LOS attempts (<=16 per refresh,
// including <=8 retained latch members). lastCostMicros measures the scan;
// lastPickCostMicros includes due scans and fresh selected/external validation.
// Each validation uses <=3 body LOS rays plus <=3 native-layer ACTI rays.
// Activation text is cached by accepted scan membership; never read per Pick.
// raysTotal includes all paths. SmoothCam's second Pick shares the refresh gate.
// eventSuppressed counts duplicate/budget-suppressed event rows; requestDrops
// counts full/contended cycle mailboxes. No cursor or activation action is built.
} // namespace skycrpg::selection::api
