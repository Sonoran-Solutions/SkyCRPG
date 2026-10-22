# Build and test setup

Status: no implementation/build system is selected or present yet. P1-01 and ADR-001 will replace the pending entries with real commands verified on a clean checkout.

## Required environment record

| Field | Current value |
| --- | --- |
| Game storefront and executable version | Pending P1-01 |
| SKSE and Address Library | Pending runtime selection |
| Camera/movement providers | Pending P1-02/P1-03 |
| UI/minimap/follower providers | Pending P1-02/P1-04 |
| CommonLib source fork/commit, if used | Pending ADR-001 |
| Compiler, SDK, generator, dependency locks | Pending P1-01/ADR-001 |
| Game host / Proton version | Pending actual installation check |
| Mod manager and disposable profiles | Pending P1-01 |
| Build, focused-test and package commands | Must be verified by P1-01, then maintained |
| Fixture saves/cells and captured baseline | Pending P1-01 |

## Setup plan

1. Inspect the owner's available installation; choose a legally available runtime and matching prerequisites. Do not upgrade/downgrade the normal install as an unreviewed convenience.
2. Create disposable baseline, camera-only, UI-only and combined profiles using copied saves.
3. Choose one Windows x64 build path appropriate to the selected stack. A Windows CI runner is valid for a Linux workstation; a cross-build path needs its own proof.
4. Pin dependencies and verify both minimal packages load separately.
5. Record exact successful commands, outputs, hashes and the short test route.

The official [SKSE download page](https://skse.silverlock.org/) and selected provider releases are starting points. Do not infer executable support from a store's Special/Anniversary product label.

## Automation scope

Add targeted build/test jobs when actual code exists. Keep art/docs-only changes from starting expensive game or unrelated native tests. Cache only versioned build dependencies. Never upload owned Skyrim binaries, personal saves or third-party mod archives to public CI artifacts.

A developer without a game host can implement and compile appropriate work, but in-game gates remain NOT_RUN until the exact artifact is tested. Prepare a short reproducible handoff instead of manufacturing an acceptance result.
