# ADR-001: MVP stack and ownership

Status: **PROPOSED / pending Phase 1 evidence**. Owner tasks: P1-02 through P1-05.

## Context

We need independent camera and UI packages with a shared tested runtime. Existing movement, HUD, grid-inventory and minimap projects cover significant pieces, but feature names do not guarantee compatible behavior, APIs or permissions.

## Questions to resolve

1. Which executable/storefront and Windows or Proton host can the owner actually test?
2. Which one component owns camera transforms?
3. Does a preset/adapter meet C1–C5, or is a small custom controller required?
4. Which UI rendering path supports real native item actions and controller focus?
5. Which provider versions produce a working minimap and safe compass handoff?
6. Which follower/hotbar pieces can be configured or adapted without importing unwanted mechanics?
7. What source/art obligations apply to the exact chosen material?

## Selection criteria

Prefer the smallest maintainable route that passes the spikes, preserves scope, supports the pinned runtime and has a viable distribution path. Weight game evidence above theoretical flexibility. Reject a route that requires rebuilding broad systems to ship a small widget.

## Preliminary recommendation — 2026-10-25

Desk-audit candidate: Steam Skyrim **1.7.104**, SKSE **2.3.1**, Address Library **13**,
SmoothCam **1.8.1** (AE 1.7.99+ installer branch), TDM **2.3.1**, and SkyUI Community
**6.11**. This is a documented candidate, not an observed installation or supported stack.
Final executable/storefront/host selection is **BLOCKED on DYL-150's environment snapshot**.

Use SmoothCam as the sole transform writer, with original configuration and a bounded
held-pitch/input adapter only if needed. Reuse native inventory actions through a
player-only SkyUI/Scaleform grid patch; retain weight, instances, quantity dialogs and
other menus. Grid Inventory's Tetris capacity mechanics are rejected for this baseline.
Party Sheet **3.7** is an external configured HUD candidate, not source to copy.

First minimap candidate: Dragon's Eye settings fork **1.7.5**, source tag at
`7c3ce18f54bd6c8dff3239db9522cb75d822a31f`, with matching 1.7 builds of Infinity UI
(ApocryphaRealm **2.1.3**) and Local Map Upgrade (ApocryphaRealm **1.3.4** candidate).
Do not combine the 1.7 minimap with upstream Infinity UI 2.0.3 / Local Map Upgrade 3.1.0.
The fork's current GPL notices supersede stale MIT language in its port notes;
keep providers external and inspect asset permissions separately.

Minimal native skeleton dependencies: matched SKSE plus Address Library at runtime;
no TDM, camera, HUD or minimap provider is needed to establish two empty DLL loads.
If native code is required, investigate alandtse/CommonLibSSE-NG **7.2.0** at
`7a60f4de794095d7b0f8928d1b930a52e9a7da83` with Windows x64 MSVC/CMake/vcpkg.
Its documented Linux clang-cl/xwin route is an alternative build-host route, not
proof of ABI correctness or a reason to use a native Linux plugin.

Provisional: installed runtime, toolchain availability, held pitch, native menu grid,
follower/controller integration, compass readiness/failure recovery and all game behavior.
The full audit and proof requirements will be recorded in REUSE_RESEARCH and dependency-lock.

Primary starting evidence: [SKSE](https://skse.silverlock.org/),
[SmoothCam release](https://github.com/mwilsnd/SkyrimSE-SmoothCam/releases/tag/Beta1.8.1),
[minimap source](https://github.com/ApocryphaRealm/DragonsEyeMinimap/tree/v1.7.5),
[Infinity UI changelog](https://github.com/ApocryphaRealm/InfinityUI/blob/43daa2e55607c6805a714085634184741575f9ca/CHANGELOG.md),
[Local Map Upgrade changelog](https://github.com/ApocryphaRealm/LocalMapUpgrade/blob/8a9cf24c30860e4ce5fb49b45ba62a8cc0badcb7/CHANGELOG.md).

## Consequences and follow-up

Pending. Update ARCHITECTURE, dependency-lock, runtime-matrix, phase tickets and any affected product assumption after the decision. Material changes to weight mechanics or required MVP features need a recorded owner choice.
