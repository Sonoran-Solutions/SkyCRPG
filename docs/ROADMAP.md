# Four-phase MVP roadmap

**Goal:** independently usable Camera and UI MVPs accepted by the end of Phase 4. The product is feature-complete as an internal candidate in Phase 3; Phase 4 is stabilization, packaging and playtest.

[Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f) · [Scope and acceptance](MVP_SCOPE.md) · [Current status](STATUS.md)

## Phases

| Phase | Deliverable | Required exit | Detailed plan |
| --- | --- | --- | --- |
| 1 — Foundation and feasibility | Two small in-game prototypes and a selected stack. | Camera obstruction/aim proof, real inventory action, working minimap, independent loads and pinned runtime. | [Phase 1](phases/PHASE_1.md) |
| 2 — Camera MVP and playable HUD | Functional camera candidate plus player HUD/hotbar. | Camera-only criteria and UI-only/combined HUD route pass. | [Phase 2](phases/PHASE_2.md) |
| 3 — UI MVP and combined gameplay | Grid, follower status, minimap, menu/input integration. | UI-only and combined feature criteria pass in a playable vertical slice. | [Phase 3](phases/PHASE_3.md) |
| 4 — Stabilization and acceptance | Two reproducible independent candidate archives. | Supported-profile matrix, integrity/performance evidence, docs and owner playtest. | [Phase 4](phases/PHASE_4.md) |

There are 26 implementation/gate tickets. Milestones group the phases; gate tickets track phase exit. A milestone's progress percentage alone does not establish acceptance. No speculative calendar dates or hours are assigned before the Phase 1 measurements.

## Sequencing and parallel work

At kickoff, **P1-01 environment/skeletons** and **P1-02 reuse/runtime audit** can start in parallel. The audit can begin immediately, but its final runtime selection waits for P1-01's environment facts. They must converge on a provisional stack before P1-03 camera and P1-04 UI spikes run in parallel. P1-05 freezes contracts and P1-06 verifies the phase evidence.

After the Phase 1 gate, the camera lane (P2-01 through P2-04) and UI lane (P2-05/P2-06) can proceed independently. Camera obstruction and aiming work share a controller: assign disjoint interfaces/files after the base controller lands. Lifecycle integration (P2-04) follows both to reconcile shared transitions.

Phase 3 has independent inventory-model, follower and minimap work. Grid presentation follows the authoritative item adapter; menu/lifecycle and input/layout integration follow those features. The gate runs the combined route.

Phase 4 fixes integration blockers before candidate packaging. Performance and packaging can proceed in parallel after that baseline. Final docs follow the actual archives; regression consumes both package and performance evidence. Owner playtest is last.

## Critical path

Environment and compatible dependencies → camera/UI proof → ownership contracts → camera and HUD candidate → real inventory/minimap/follower UI → packaged independent candidates → technical and owner acceptance.

The item-action path and camera obstruction/aiming path are the major uncertainties. The plan deliberately tests them before investing in polish.

## Gate discipline

Each phase ends with a concrete source/package and an evidence table. Missing game access remains NOT_RUN, not an automatic pass. Failed critical spikes produce a bounded alternative or a recorded blocker; they cannot silently remove grid inventory, minimap, real-time combat or module independence.

P4 acceptance requires no fifth feature phase. If a required feature has slipped beyond P4, the MVP has not been delivered and the roadmap must say so.

## Later roadmap — intentionally unticketed

After acceptance, prioritize actual player feedback: additional runtimes/providers, richer portraits, grid container/barter screens, character sheet polish, event-log display, themes and optional advanced occlusion. Click-to-move, party commands and a Tetris inventory are separate product decisions. Turn-based combat is excluded from the current direction.

A post-MVP **Phase 5 proposal** for the owner's targeting and camera requests (proximity activation and cycling, pointer/stick cursor mode, TDM-based lock-on, fixed-height camera with see-through overhead occluders) is drafted in [Phase 5](phases/PHASE_5.md) with its [research and feasibility record](planning/phase5-targeting-camera-research.md). It has no tickets and no evidence yet and does not alter the four-phase MVP exit.

## Ticket index

The phase documents below include goals, scope, dependencies, acceptance, validation and evidence. Linear is the live execution tracker; update the source plan when scope changes.

| Plan ID | Linear | Work item | Prerequisites |
| --- | --- | --- | --- |
| P1-01 | [DYL-150](https://linear.app/devils17/issue/DYL-150/p1-01-establish-reproducible-test-profiles-and-independent-module) | Establish reproducible test profiles and independent module skeletons | Ready at kickoff |
| P1-02 | [DYL-151](https://linear.app/devils17/issue/DYL-151/p1-02-choose-reusable-mod-components-and-a-compatible-runtime) | Choose reusable mod components and a compatible runtime intersection | Ready at kickoff |
| P1-03 | [DYL-152](https://linear.app/devils17/issue/DYL-152/p1-03-prove-overhead-camera-aiming-and-obstruction-behavior-in-game) | Prove overhead camera, aiming and obstruction behavior in game | P1-01, P1-02 |
| P1-04 | [DYL-153](https://linear.app/devils17/issue/DYL-153/p1-04-prove-the-ui-native-inventory-action-and-minimap-integration-in) | Prove the UI, native inventory action and minimap integration in game | P1-01, P1-02 |
| P1-05 | [DYL-154](https://linear.app/devils17/issue/DYL-154/p1-05-freeze-module-boundaries-control-contracts-and-the-tested) | Freeze module boundaries, control contracts and the tested support baseline | P1-03, P1-04 |
| P1-06 | [DYL-155](https://linear.app/devils17/issue/DYL-155/p1-06-phase-1-gate-verify-two-playable-prototypes-and-commit-the) | Phase 1 gate — verify two playable prototypes and commit the implementation route | P1-05 |
| P2-01 | [DYL-156](https://linear.app/devils17/issue/DYL-156/p2-01-implement-the-elevated-camera-controller-and-player-controls) | Implement the elevated camera controller and player controls | P1-06 |
| P2-02 | [DYL-157](https://linear.app/devils17/issue/DYL-157/p2-02-handle-camera-collision-and-tight-interior-fallback) | Handle camera collision and tight-interior fallback | P2-01 |
| P2-03 | [DYL-158](https://linear.app/devils17/issue/DYL-158/p2-03-integrate-combat-aiming-target-lock-and-world-interaction) | Integrate combat aiming, target lock and world interaction | P2-01 |
| P2-04 | [DYL-159](https://linear.app/devils17/issue/DYL-159/p2-04-implement-camera-lifecycle-handoff-and-safe-recovery) | Implement camera lifecycle handoff and safe recovery | P2-02, P2-03 |
| P2-05 | [DYL-160](https://linear.app/devils17/issue/DYL-160/p2-05-build-the-core-crpg-hud-and-player-status-layout) | Build the core CRPG HUD and player status layout | P1-06 |
| P2-06 | [DYL-161](https://linear.app/devils17/issue/DYL-161/p2-06-connect-an-eight-slot-hotbar-to-native-favorites-and-equipment) | Connect an eight-slot hotbar to native favorites and equipment actions | P2-05 |
| P2-07 | [DYL-162](https://linear.app/devils17/issue/DYL-162/p2-07-phase-2-gate-accept-the-camera-mvp-candidate-and-playable-hud) | Phase 2 gate — accept the camera MVP candidate and playable HUD | P2-02, P2-03, P2-04, P2-06 |
| P3-01 | [DYL-163](https://linear.app/devils17/issue/DYL-163/p3-01-implement-authoritative-inventory-snapshots-and-item-actions) | Implement authoritative inventory snapshots and item actions | P2-07 |
| P3-02 | [DYL-164](https://linear.app/devils17/issue/DYL-164/p3-02-build-the-grid-inventory-and-equipment-presentation) | Build the grid inventory and equipment presentation | P3-01 |
| P3-03 | [DYL-165](https://linear.app/devils17/issue/DYL-165/p3-03-add-follower-status-portraits-through-the-selected-provider) | Add follower status portraits through the selected provider | P2-07 |
| P3-04 | [DYL-166](https://linear.app/devils17/issue/DYL-166/p3-04-integrate-the-minimap-and-replace-the-skyrim-compass-safely) | Integrate the minimap and replace the Skyrim compass safely | P2-07 |
| P3-05 | [DYL-167](https://linear.app/devils17/issue/DYL-167/p3-05-handle-ui-menu-focus-pause-semantics-and-saveload-lifecycle) | Handle UI menu focus, pause semantics and save/load lifecycle | P3-02, P3-03, P3-04 |
| P3-06 | [DYL-168](https://linear.app/devils17/issue/DYL-168/p3-06-finish-controller-navigation-ui-scaling-and-readable-layouts) | Finish controller navigation, UI scaling and readable layouts | P3-02, P3-03, P3-04 |
| P3-07 | [DYL-169](https://linear.app/devils17/issue/DYL-169/p3-07-phase-3-gate-accept-the-ui-mvp-candidate-and-combined-vertical) | Phase 3 gate — accept the UI MVP candidate and combined vertical slice | P3-05, P3-06 |
| P4-01 | [DYL-170](https://linear.app/devils17/issue/DYL-170/p4-01-resolve-integration-blockers-and-verify-failuredisable-behavior) | Resolve integration blockers and verify failure/disable behavior | P3-07 |
| P4-02 | [DYL-171](https://linear.app/devils17/issue/DYL-171/p4-02-package-reproducible-independent-camera-and-ui-release) | Package reproducible independent camera and UI release candidates | P4-01 |
| P4-03 | [DYL-172](https://linear.app/devils17/issue/DYL-172/p4-03-measure-frame-cost-inventory-latency-and-resource-stability) | Measure frame cost, inventory latency and resource stability | P4-01 |
| P4-04 | [DYL-173](https://linear.app/devils17/issue/DYL-173/p4-04-run-the-final-supported-profile-regression-and-inventory) | Run the final supported-profile regression and inventory integrity matrix | P4-02, P4-03 |
| P4-05 | [DYL-174](https://linear.app/devils17/issue/DYL-174/p4-05-finalize-installation-controls-limitations-and-third-party) | Finalize installation, controls, limitations and third-party attribution | P4-02 |
| P4-06 | [DYL-175](https://linear.app/devils17/issue/DYL-175/p4-06-phase-4-gate-owner-playtest-and-cameraui-mvp-acceptance) | Phase 4 gate — owner playtest and camera/UI MVP acceptance | P4-04, P4-05 |
