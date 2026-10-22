# Validation strategy

## Evidence levels

- **Static/build:** source inspection, compile, archive inspection and focused tests.
- **In-game:** exact candidate installed in Skyrim and exercised on the pinned stack.
- **Owner acceptance:** player assessment of the completed candidate.

These are different claims. Unit tests cannot establish camera visibility or real inventory integrity on their own. An owner liking a screenshot does not replace runtime verification.

Every report records source commit, package hash, game executable, SKSE/Address Library/provider versions, host OS/Proton, input mode, resolution, profile/load order and fixture. State PASS, FAIL, BLOCKED or NOT_RUN, with logs/captures supporting the relevant result.

## Required profiles

| Profile | Purpose |
| --- | --- |
| Baseline | Game plus pinned common prerequisites; measure existing behavior and performance. |
| Camera only | Camera module, its dependencies and ordinary UI. |
| UI only | UI module, its dependencies and ordinary camera. |
| Combined | Both modules with the selected dependency stack. |

A failure of the baseline is not automatically a SkyCRPG regression. Keep the fixtures and graphics/settings comparable.

## Fixtures

| Fixture | Checks |
| --- | --- |
| Exterior road | Follow, yaw, zoom, recenter, sprint, resources and minimap. |
| Tight Nordic corridor/door/stairs | Obstruction, collision, close-view recovery and transitions. |
| Combat room | Melee, bow, aimed/concentration spell, target lock if selected, hotbar and follower status. |
| Interaction cluster | Door, container and small object activation without range/line-of-sight changes. |
| Menu chain | Player grid, quantity dialog, favorites, dialogue, console, crafting, container/barter, map/journal and cancellation. |
| Item set | Plain duplicates, enchanted/tempered instances, stolen copy, quest item, stacked consumable and modded form. |
| Lifecycle | Loading, fast travel, save/reload, death, first person and supported handoffs for mounts/swim/furniture/transformations/killcams. |
| Failure recovery | Missing optional provider, invalid config, unsupported runtime guard, independent disable and restart on copied save. |
| Layout/input | Keyboard/mouse, controller, 720p, 1080p and one increased text/scale case. |

P1 records exact cells/saves/steps available to the owner. The full matrix is a release gate, not the default workload for every PR.

## Validation by change

Documentation only: inspect links, consistency and factual claims. No game launch.

Camera logic: affected state/math tests, camera build and changed camera fixtures. Rerun UI only if a shared/input contract changed.

Inventory logic: focused transaction/identity tests and the relevant real item fixture. Rerun the full item set when identity or serialization changes.

HUD/art/layout: relevant data/input check and representative screenshots. No full combat suite for a color change.

Shared input/lifecycle/dependency/package changes: affected cross-module cases and a short combined smoke.

Phase gate: inspect accumulated evidence, fill gaps and do one integrated route. Reuse evidence for identical code/artifacts; distinguish code changes from evidence-only commits.

## Bounded jobs

Before starting: purpose, exact command/profile, expected result, timeout, log, owner and handle. Maintain one active game job per profile. Quiet output is not evidence of a hang; inspect process/log/CI state before retrying.

Starting budgets: warm build 15 minutes; first dependency build 30 minutes; focused tests 10 minutes; gameplay route 20 minutes; performance/resource sample 10 minutes. Extend only for a measured reason. Use process-group cleanup for owned jobs and confirm exit. Never blanket-kill unrelated game/LLM/user processes.

After failure, preserve evidence and diagnose. One diagnostic retry is allowed; repeat again only with changed input or a new testable hypothesis. A long soak is a separate planned task, not an unbounded loop.

## Performance budget

Initial planning targets on the pinned host: combined p95 frame-time overhead <=10% versus baseline; warmed player-inventory opening <=250 ms at 500 total inventory entries in the scrollable/virtualized dataset; no sustained retained-state growth across a bounded menu/transition cycle. Record median and p95 rather than only average FPS.

P1 establishes an actual baseline and P4 checks the selected budgets. These are proposed engineering targets, not measured results. If baseline hardware makes a target unsuitable, record the revised target and rationale before the release decision rather than quietly moving it after a failure.

Take short comparable warmed captures for each profile, then one 10-minute resource sample. Rerun only a noisy or failing comparison once. Report sample length and noise; do not claim a complete memory-leak proof.

## Acceptance report minimum

Acceptance ID; expected/observed behavior; result; source/package hashes; setup; reproduction; evidence link; defect or limitation; remaining gate. P4 additionally records owner ACCEPT or OWNER_PLAYTEST_PENDING and the exact candidate played.
