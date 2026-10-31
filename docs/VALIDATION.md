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

DYL-172's [Phase 4 performance evidence](evidence/p4/performance.md) defines the
implemented offline tooling and the remaining runtime gates. Its opt-in,
never-shipped probe records QPC periods at the UI drain, with persistent UI/main
queue delegates handing off to each other. Same-queue self-requeue is forbidden:
SKSE drains to empty and the earlier design would hang. This measures UI-drain
CPU period, not GPU presentation. One sample per frame is a runtime hypothesis.
A second fixed ring records BSTimer real-time deltas; every capture must have
probe median / engine median (`anchor_ratio`) in **[0.9, 1.1] inclusive** with
matching valid sample counts. Invalid/missing anchors or runaway aborts make
that capture's frame/inventory/resource budget **NOT_RUN**, never PASS/FAIL.
The ninth UI run within 1 ms aborts with available=false/reason=runaway; the guard
is a safety net. Validate cadence standing, turning and in paused inventory.
Compare identical standing and scripted-turn 60-second captures on the same host with the same
observer. Combined p95 must be ≤1.10× baseline p95 in **both** captures.

Inventory timing uses the bound Inventory/Tween button-down and native
InventoryMenu open event on the probe's QPC clock. `t_event_ms` measures the open
event; `t_settled_ms` ends at the first post-open period ≤2× pre-open capture
median followed by five more qualifying periods. Observe 3 seconds, retain the
first 30 raw periods, report cold open 1 separately, and require **every** warm
open 2–10 to settle within 250 ms at 500 native rows. A painted-grid signal is an
optional refinement; this metric does not claim GPU present or painted pixels.

Four matching `session-p4/autotest/routes/perf-{baseline,camera-only,ui-only,combined}.json`
routes use a copied acceptance save, a SHA-256 pinned deployment-owned fixture,
30-second warm-up, standing/turn captures, ten open/scroll/close cycles, five
verified cell transitions and a 600-second resource window. The runner journals
fixture ownership before copying and verifies its removal on restore. Only
simulation was run in this lane. Windows compilation, cadence/paused-menu
behavior, Data-root batch lookup, 500 actual rows and **all runtime budgets
remain NOT_RUN**.

`session-p4/autotest/perf_compare.py` reads full probe slices (or embedded report
events), records report/probe hashes and provenance, and emits PASS/FAIL/NOT_RUN
per fixed budget. The memory flag requires **both** latter-half least-squares
private-memory slope >1 MiB/min and last-quartile mean >105% of first-quartile
mean, restricted to the marked resource window. Missing/short/sparse/non-finite
measurements, simulated reports and truncated comparison captures cannot pass.
The slope test describes a bounded trend, not a complete memory-leak proof.
Confirm hardware, resolution/settings, load order, executable, dependency,
source, package, probe and fixture hashes before runtime acceptance.

## Acceptance report minimum

Acceptance ID; expected/observed behavior; result; source/package hashes; setup; reproduction; evidence link; defect or limitation; remaining gate. P4 additionally records owner ACCEPT or OWNER_PLAYTEST_PENDING and the exact candidate played.
