// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace skycrpg::selection::api {
inline constexpr std::uint32_t version = 1;
inline constexpr char exportName[] = "SkyCRPGSelection_GetAPI";
enum class Result : std::uint32_t { ok, unavailable, wrongThread, disabled, invalidTarget, invalidArgument };
struct Snapshot {
    std::uint32_t size = sizeof(Snapshot);
    std::uint32_t apiVersion = version;
    std::uint32_t target = 0, candidateCount = 0, latched = 0, passthrough = 0;
    std::uint64_t refreshes = 0, inspectedTotal = 0, raysTotal = 0, lastCostMicros = 0;
    std::uint64_t pickChecks = 0, lastPickCostMicros = 0;
    std::uint32_t inspectedLast = 0, cappedScans = 0;
    std::uint32_t visitedLast = 0, scanRaysLast = 0;
    double radius = 0;
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
// no link dependency. Never LoadLibrary it. Request exactly version 1 after
// kDataLoaded. All methods require the game thread (wrongThread otherwise).
// Buffers belong to the caller; no engine pointers or STL types cross the ABI.
// getSnapshot/copyCandidates refresh when due (100ms default); genuine state
// or cell changes force revalidation. Native passthrough clears the snapshot.
// Check target before acting; it is the validated policy target, not a promise
// that a subsequent native pick will be substituted.
// copyCandidates reports total count; a short buffer receives the leading IDs.
// setTarget uses a complete bounded scan (<=100ms old with defaults), then
// rechecks the pushed reference's native eligibility/range/body LOS immediately,
// then latches. Default [Selection] bAllowExternalTargets=0 refuses mutations.
// Pushes are rate limited to fRefreshSeconds (unavailable: retry later).
// Invalid pushes preserve the existing selection. clearTarget returns nearest.
// Latches clear on unsafe states or invalidation; incidental menus/cells preserve them.
// Nothing is saved. No cursor is built.
// Reentrant methods return unavailable. lastCostMicros measures a full scan;
// lastPickCostMicros measures the adapter after native Pick (including any due
// scan and the selected target's immediate LOS, up to three rays). raysTotal includes both paths.
// SmoothCam use3DPicker can call twice per frame: each pick is counted and
// freshly validated; scans share one refresh gate. visitedLast is cheap broad
// phase work; inspectedLast counts nearby references; scanRaysLast is capped.
} // namespace skycrpg::selection::api
