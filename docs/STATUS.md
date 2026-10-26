# Current status

Updated2026-10-26 for DYL-154. GitHub and Linear were checked directly; old spike comments describing unmerged PRs are historical.

PR1/2 (foundation/research) and PR3/4 (Camera/UI spikes) are **merged**. Main baseline is `87d91626815dca145c6a876664f1ac7d0ffeafdf`. Senior code review passed the bounded spikes with explicit gameplay limits. DYL-150/151 remain Done; DYL-152/153 currently remain **In Progress**, not automatically closed by merge. DYL-154 contract work is complete and submitted for senior review; DYL-155 remains Backlog. No Phase 2 implementation is started.

| Ticket / source | Current evidence and remaining obligation |
| --- | --- |
| DYL-152 / [merged PR3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3) | **PASS** qualified exploration/melee, first-person precision fallback, activation, ten F8 pairs and transition usability. **FAIL** top-down ranged aim → DYL-158. **FAIL** load-pitch persistence (zoom persists), explicitly owner-deferred → DYL-159. Detailed tight-interior obstruction recovery **NOT_RUN** → DYL-157; missing Phase 1 proof still evaluated by DYL-155, no waiver recorded. |
| DYL-153 / [merged PR4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4) | **PASS** UI-only ordinary camera, live small grid/native equip/unequip/counts/weight, one action per activation, final controller grid/category routing, minimap inside/outside and compass usable when map hidden. Missing-provider clean startup **NOT_RUN**; automatic compass replacement unimplemented → DYL-166. 19+ row alignment concern → DYL-164. Full instance/modal/layout/follower/hotbar acceptance remains open. |
| Combined075556 | **PASS** bounded startup/all8 DLLs, owner checkpoint and captures, clean exit/no leftovers. Pitch persistence still **FAIL**. Not complete combined quality/performance acceptance. |
| DYL-154 / P1-05 | Evidence-backed architecture, accepted technical direction, runtime classification, independent packaging and all C/U/M/Q traceability submitted for senior review. Documentation only; no gameplay/build rerun or new adapter. |
| DYL-155 / P1-06 | **NOT_RUN**, Backlog. May begin after DYL-154 senior review/acceptance; must evaluate remaining critical feasibility proof and fallback/defer dispositions before passing Phase 1. |

## Implementation route and open decisions

[ARCHITECTURE](ARCHITECTURE.md) defines the state/input/inventory/provider/package contracts; [ADR-001](decisions/ADR-001-stack.md) records the chosen route and rejected alternatives; the [acceptance matrix](compatibility/acceptance-matrix.md) maps every MVP ID to evidence, implementation and validation gates.

SmoothCam1.8.1 owns normal third-person transforms, TDM2.3.1 movement only; competing yaw/lock remain disabled. First-person precision is proven. The small pitch/restoration/exclusive-handoff adapter is planned, **not implemented**. UI uses SkyUI6.11 native InventoryMenu/Scaleform and native authoritative actions/weight. Minimap uses the pinned existing trio; readiness-driven compass replacement is still required, not inferred from clip presence. Party Sheet remains optional/unverified, and the eight-slot bar remains future native-action presentation.

Only the [single recorded Steam1.7.104/Proton host](compatibility/runtime-matrix.md) has evidence. D015 original-work source/art license remains an owner decision before distribution (DYL-174/171). No owner decision to remove grid/minimap, change native weight, expand runtime support or replace the provider with a large custom system is needed or assumed. Pitch defer does not waive other missing tests.

## Foundation and preservation

- Rewritten profile tool real-host deploy/load/undeploy **PASS**; copied clean Helgen interior/exterior route **PASS** with owner captures. Old heavily modded2021 save **FAIL** to load. DYL-150 is not reopened.
- Controller is available and final UI navigation/action checks **PASS**. Full baseline/controller/modal/layout and supported-profile matrices remain **NOT_RUN**.
- Last recorded session restoration **PASS**: all146 pre-existing save/copy hashes unchanged, original INIs and Plugins.txt restored, no active deployment/game process at that checkpoint. DYL-154 does not modify the game installation.
- First camera attempt lost the desktop under memory pressure; root contributor unresolved. Later bounded retry passed. No global crash-free or performance claim.

Evidence: [session](evidence/p1/session-2026-10-26.md), [camera](evidence/p1/camera-spike.md), [UI](evidence/p1/ui-spike.md), [exact artifacts](evidence/p1/tested-artifacts.json), [dependency hashes](compatibility/dependency-lock.md). DYL-154 focused validation and review provenance are in [contract-freeze](evidence/p1/contract-freeze.md).
