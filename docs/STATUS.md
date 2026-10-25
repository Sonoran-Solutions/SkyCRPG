# Current status

Updated: 2026-10-25 (P1-01 progress).

## Project state

Planning initialized. No SkyCRPG implementation, in-game proof, supported runtime, packaged build or owner acceptance exists yet. The four-phase plan and initial tickets are the deliverable of this setup.

**P1-01 / DYL-150 progress (in review, PR [#1](https://github.com/Sonoran-Solutions/SkyCRPG/pull/1)):**

- Environment recorded in [evidence/p1/environment.md](evidence/p1/environment.md): Steam `SkyrimSE.exe` 1.7.104.0 on Ubuntu 26.04 under Proton Experimental 11, RTX 3080, no controller, no mod manager.
- Independent `SkyCRPG_Camera` and `SkyCRPG_UI` load-only SKSE skeletons. They build reproducibly on the Windows x64 CI runner and package into separate archives with disjoint paths, settings and logs.
- Isolated baseline/camera-only/ui-only/combined profiles via `tools/profile.py`.
- In-game load checks PASS (load only) for all four profiles on SKSE 2.3.1. See [BUILD_AND_TEST.md](BUILD_AND_TEST.md) and [runtime-matrix.md](compatibility/runtime-matrix.md).
- Still open: the baseline outdoor/interior route (NOT_RUN, owner to play), and agreement with DYL-151 on the provider stack for runtime 1.7.104. No camera or UI behavior exists yet.

- Repository: https://github.com/Sonoran-Solutions/SkyCRPG
- Linear project: https://linear.app/devils17/project/skycrpg-3bba43f5930f
- MVP: [MVP_SCOPE.md](MVP_SCOPE.md)
- Roadmap: [ROADMAP.md](ROADMAP.md)

## Ready now

| Task | Start condition |
| --- | --- |
| [DYL-150 / P1-01](https://linear.app/devils17/issue/DYL-150/p1-01-establish-reproducible-test-profiles-and-independent-module) — environment, profiles and two module skeletons | In review; baseline route NOT_RUN. |
| [DYL-151 / P1-02](https://linear.app/devils17/issue/DYL-151/p1-02-choose-reusable-mod-components-and-a-compatible-runtime) — reusable components and runtime intersection | Ready; can run alongside P1-01. |

P1-03 and P1-04 become ready after those tasks converge on the provisional stack. Do not start broad implementation before the in-game feasibility proofs and module contracts.

## Phase gates

| Gate | State |
| --- | --- |
| Phase 1 — prototypes and stack | IN_PROGRESS (P1-01 in review) |
| Phase 2 — camera candidate and playable HUD | NOT_STARTED |
| Phase 3 — UI candidate and combined route | NOT_STARTED |
| Phase 4 — packaged MVP acceptance | NOT_STARTED |

## Known open items

Runtime/host selection, exact providers, rendering route, original-work license and the engineering choices in ADR-001 are intentionally pending. The working native-weight/player-inventory/controller scope is documented, not misrepresented as separately approved owner decisions.

## Update discipline

Implementation PRs update this page with completed scope, source/evidence pointers, blockers and the next ready task. Linear carries assignment/status/dependencies. No source file is marked shipped on the basis of a plan or concept screenshot.

## Tracking initialization

26 tickets created: DYL-150 through DYL-175, across four project milestones. DYL-150 and DYL-151 are Todo; subsequent work starts in Backlog with native blocker relations. The component audit may start alongside environment discovery, but final runtime selection waits for the actual environment facts. No issue is assigned to an agent or marked In Progress by the planning setup.
