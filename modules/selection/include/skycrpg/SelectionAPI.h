// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace skycrpg::selection::api {
inline constexpr std::uint32_t version = 2;
inline constexpr char exportName[] = "SkyCRPGSelection_GetAPI";
enum class Result : std::uint32_t { ok, unavailable, wrongThread, disabled, invalidTarget, invalidArgument, queued };
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
struct Interface {
    std::uint32_t size;
    std::uint32_t apiVersion;
    Result (*getSnapshot)(Snapshot*);
    Result (*copyCandidates)(std::uint32_t* ids, std::uint32_t capacity, std::uint32_t* count);
    Result (*setTarget)(std::uint32_t formID);
    Result (*clearTarget)();
};
using GetAPI = const Interface* (*)(std::uint32_t requestedVersion);
// Resolve via GetModuleHandleW(L"SkyCRPG_Selection.dll") / GetProcAddress;
// no link dependency. Never LoadLibrary it. Request exactly version 2 after
// kDataLoaded. Version 1 is rejected: it promised synchronous main-thread work.
// All methods now support any thread and never initiate a scan or call engine
// functions. Snapshot/list reads return unavailable on contention/reentrancy.
// Legacy readers share Pick's nonblocking state gate: an overlapping read can
// make that Pick forward the native target, briefly interrupting substitution.
// Poll sparingly; do not spin on unavailable. Reads do not guarantee injection.
// wrongThread is a reserved legacy enum value and is never returned by v2.
// Buffers belong to the caller; no engine pointers or STL types cross the ABI.
// Snapshots describe the most recent Pick job. available=0 means no injection
// is currently permitted, even if a retained target/latch is nonzero (e.g. a
// refused ray). Reads do not refresh stale state. Check availability and age by
// observing refreshes; this interface never promises a future native pick.
// copyCandidates reports total count; a short buffer receives the leading IDs.
// setTarget/clearTarget post one atomic request. queued means accepted into the
// mailbox, NOT validated/applied. A busy mailbox returns unavailable; callers
// must not spin. Default bAllowExternalTargets=0 returns disabled.
// Pick consumes external requests at most once per refresh interval, checks
// membership in its validated bounded list and fresh eligibility/range/body LOS,
// and only then latches. Invalid/unknown pushes preserve the existing selection.
// externalTarget/externalResult report the last consumed request (zero=clear).
// The mailbox is single-caller coordination, not a multi-client arbitration API.
// Lifecycle invalidation discards pending requests. No request is serialized.
// Cycling is an atomic FIFO; Pick applies each physical edge once, in order.
// Candidate order is a validated nearest prefix (up to eight), frozen for a latch.
// visitedLast counts cheap callbacks; inspectedLast counts nearby geometry;
// validatedLast counts expensive predicate/LOS attempts (<=16 per refresh,
// including <=8 retained latch members). lastCostMicros measures the scan;
// lastPickCostMicros includes due scans and fresh selected/external validation.
// Each validation uses <=3 body LOS rays plus <=3 native-layer ACTI rays.
// Activation text is not queried by selection; native main-thread prompt code
// owns it. Text-empty objects can pass selection's native eligibility checks.
// raysTotal includes all paths. SmoothCam's second Pick shares the refresh gate.
// eventSuppressed counts duplicate/budget-suppressed event rows; requestDrops
// counts full/contended cycle mailboxes. No cursor or activation action is built.
} // namespace skycrpg::selection::api
