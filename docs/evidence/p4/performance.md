# DYL-172 — performance measurement tooling

**Offline implementation complete; runtime acceptance NOT_RUN.** Base
`c7333c7c13284ad9d228e6f15213bc8465bd6e19`; candidate `<head SHA>` plus this
uncommitted diff; Windows CI `<CI run>`; probe `<DLL sha256>`. No commit, push,
Skyrim/Steam/Proton launch, live runner, module change or public release occurred.

Pinned target: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13,
CommonLibSSE-NG 7.2.0, SkyUI Community 6.11, SmoothCam 1.8.1, TDM 2.3.1,
MCM Helper 1.6.3, Proton. Actual host, graphics settings/resolution, load order,
executable/dependency/source/package/probe/fixture hashes and durations must be
attached to each game result. Portable tests and simulations do not compile the
Windows adapter or establish game behavior.

## Audit and acceptance ledger

| Checklist / blocker 9 item | Existing coverage | Implemented through round 4 / offline status | Runtime |
| --- | --- | --- | --- |
| 1. Bounded frame capture | Round-2 portable ring/summary | Round-4 UI/main ping-pong replaces unsafe self-requeue; two fixed rings, engine anchor, single-chain state and runaway guard; **PASS** portable checks | NOT_RUN |
| 2. Inventory latency | Portable six-frame settle detector | Bound-key input sink, native menu-open QPC endpoint, cold flag, 3-second window and first 30 periods; **PASS** policy checks | NOT_RUN |
| 3. Runner steps/deployment | Existing request writer, probe-confirmed console, restoration journal | perf_capture, inventory_cycle, console, fixture/save copies, refusal and restoration checks; **PASS** fake tests | NOT_RUN |
| 4. Four routes | camera-only already mapped in schema/profile tool | Four matching routes apart from profile; CELL EDIDs verified offline; **PASS** schema and equality checks | NOT_RUN |
| 5. Simulations | No earlier performance routes | All four CLI simulations **PASS**, cleanup verified | NOT_RUN |
| 6. Fixed comparison budgets | Experimental reader and growth mathematics | Complete event reader, both capture comparisons, warm/cold statistics and resource-window verdicts, required [0.9,1.1] anchors for all captures; **PASS** synthetic checks | NOT_RUN |
| 7. Portable/full runner tests | Three probe tests, earlier runner suite | All four portable tests **PASS**; full runner suite **PASS** (119 tests in round 4) | Windows and engine integration NOT_RUN |
| 8. Documentation | Partial evidence, static audit and round history | Evidence, VALIDATION performance section, probe README and external CHANGES updated for the defect/fix and validity rule | Acceptance NOT_RUN |
| 500-entry usability | [P3 grid](../p3/grid-inventory.md): owner-reported end reachable without noticeable lag, untimed | Reused as usability history only; no timing PASS inferred | Current native row count NOT_RUN |
| Static per-frame audit | Round-1 source audit below | Rechecked current unchanged modules at base; no module fixes or new product scope | Measured cost NOT_RUN |

The round-4 orchestrator decision supersedes round 2/3’s unsafe same-queue
requeue design. The earlier present-hook prerequisite remains superseded. No
missing engine API or unresolved design decision blocks the offline implementation. Q2 and DYL-172 runtime acceptance stay open.

## Static audit at base

| Location | Finding |
| --- | --- |
| `modules/camera/src/plugin.cpp:338` | Third-person update calls obstruction sampling and native update; no inventory or actor traversal observed on this path. Trace logging is explicitly capped at 128 lifecycle lines (`:252`) and 64 precision lines (`modules/camera/include/precision_trace.h:33`). |
| `modules/ui/scaleform/hud-init.as.inc:194`, `:258` | Frame callbacks maintain visibility, hotbar samples at 500 ms, resource samples at 100 ms without catch-up. |
| `modules/ui/src/plugin.cpp:289`, `:489` | Hotbar task scans full native inventory at up to 2 Hz, coalescing pending tasks; not each frame, but potentially material with 500 entries. Measure. Input action can separately scan inventory at `:407`. |
| `modules/ui/scaleform/follower-panel.as.inc:36`, `modules/ui/src/follower/adapter.cpp:46`, `:109` | Follower requests at most every 100 ms, one pending task, but each sample traverses all high-process actor handles. Not every frame, and traversal size depends on the world; measure. |
| `modules/ui/scaleform/compass-policy.as.inc:39`, `:75` | Visibility, parent walk and bounds are checked each frame; capability polling at 100 ms. Parent walk depends on clip ancestry. Log capped at 32 per clip (`:32`). |
| `modules/ui/scaleform/inventory-presentation.as.inc:9`, `:196`, `modules/ui/src/inventory/equipment_bridge.cpp:68` | Repaint coalesces into a one-shot; equipment observation coalesces at 100 ms on request and scans armor, not a frame loop. Selection/action snapshot can scan inventory (`modules/ui/src/inventory/inventory_bridge.cpp:217`). |
| `modules/ui/src/plugin.cpp:380`, `:426`, `:643` | Hotkey logging capped at 64; HUD valid/unavailable diagnostics each latch once. |
| `modules/camera/src`, `modules/ui/src`, `modules/ui/scaleform` | Static search for socket, send/recv, curl, WinHTTP/WinINet and URLs found no network calls in the project's runtime sources. This is not a proof about providers or dynamically linked code. |

## Measurement design and API audit

The probe remains independently built, opt-in (`bEnabled=1`), never shipped and
unrelated to either module's runtime. It performs no input emission, item action,
hook or save. Memory sampling from round 1 remains on the fixed/coalesced timer,
using `GetProcessMemoryInfo` private bytes and working set; failure logs once,
and CMake links `psapi`.

### Round-4 defect and replacement

Inspection of the SKSE 2.3.1 copies at
`session-p4/research/Hooks_Threads.cpp` and `Hooks_UI.cpp` confirms both queues
drain until empty, calling Run then Dispose. The previous FrameTask::Dispose
added itself back to the draining main queue: first capture would loop forever
and freeze the game. The main drain is hooked at two sites, so even a
non-hanging main-queue chain would not establish once-per-frame sampling.
The UI drain is hooked once, but self-requeue there is equally invalid.
Round 3's portable/runner passes never established adapter safety; no game
capture with the defective adapter was run by this lane.

The replacement uses one persistent `detail::UIDelegate_v1` and one persistent
`detail::TaskDelegate`, both with no-op Dispose. The UI task reads QPC and the
engine's `BSTimer::realTimeDelta`, pushes scalar periods into two preallocated
16384-double rings, runs the existing inventory-window logic, then queues the
main helper via AddTask. That helper only queues the next UI task via AddUITask.
Neither delegate queues itself or targets its currently draining queue. There
is no per-frame lambda/delegate allocation. SKSE's own queue storage remains
under SKSE's control. Input/stop summaries sort copies; inventory completion
formats its bounded result as before. No engine pointer survives a callback.
The first UI call establishes QPC time; subsequent periods pair with the engine
delta read on that call, converted from seconds to milliseconds.

`FrameChain` is the portable single-reservation state machine used by the
adapter. Its queued hop spans both queues and the reservation-to-enqueue gap.
Stop cancels sampling without releasing a pending reservation; restart resets
capture data/guard and reuses that pending hop, so it cannot create a second
chain. Without restart, one pending hop drains harmlessly then ends. Lifecycle
invalidation uses the same stop path. Queue APIs are called outside perfMutex:
SKSE's UI drain holds its queue lock across Run, so holding perfMutex while
acquiring that lock from the main helper would invert lock order.

The ninth UI task run in any trailing **1 ms** QPC window stops capture, logs
once and emits `perf_capture` with `phase:"abort"`, `available:false`,
`reason:"runaway"`. This is a safety net, not the scheduling mechanism. Enqueue
and callback failures also stop sampling with an unavailable abort. A runaway
produces no successful stop summary. Count means retained periods;
total_count exposes wrapping. Comparison rejects wrapped standing/turn
captures. Resource-summary wrap does not lose streamed memory records.

**This measures UI-drain CPU period, not GPU present.** A main drain between
consecutive UI drains would give one sample per frame, but this remains a
hypothesis to validate in game, including paused inventory. Identical probe,
settings and host are required for relative comparisons. Engine realTimeDelta
can be clamped/smoothed or retained during timer pause; no assumption is made
that it will validate every menu capture. Reject invalid evidence and diagnose.
A painted-grid signal stays an optional refinement, with no UI dependency.

Input resolves the current device's Inventory and Tween Menu mappings, then
timestamps button-down before capture-summary work. MenuOpenCloseEvent timestamps
InventoryMenu opening before probe diagnostic I/O. Scalars alone cross callback
boundaries, protected with the capture mutex; no engine pointer is retained.
The pre-open median is the active ring's retained periods at key-down.
`t_event_ms` = input → menu event. `t_settled_ms` = input → end of the first
post-open period ≤2× that median, followed by five more consecutive qualifying
periods. Confirmation occurs only after all six. A 3-second observation retains
the first 30 raw periods. Missing/stale (>10 seconds) input, missing median,
early close/stop or an unconfirmed settle produces null/partial evidence. The
first inventory open in the process is cold, even if outside a capture.

The request file retains raw checkpoint tags, except reserved commands
`@perf start <label>` / `@perf stop <label>` (no newline). The runner waits for an
exact-tag `perf_capture` acknowledgement with available=true. Stop emits the
frame_summary first. Invalid/overlapping/mismatched requests fail explicitly;
new game/load cancels capture. The resource label emits start/stop markers.

| Engine/API declaration checked in CommonLib 7.2.0 include root | Use / signature |
| --- | --- |
| `SKSE/Interfaces.h:192–195` | AddTask(::TaskDelegate*) and AddUITask(::UIDelegate_v1*) overloads; both pass through without a function-wrapper allocation |
| `SKSE/Impl/Stubs.h:98–110`; `src/SKSE/Interfaces.cpp:184–196` | Both detail interfaces declare Run then Dispose, no virtual destructor. Both opaque pointer overloads forward unchanged; callbacks dispose without deleting the persistent object. Function overloads allocate wrappers and are not used for frame hops. |
| `RE/B/BSTimer.h:8–12,48` | GetSingleton() returns BSTimer*; realTimeDelta is float wall-clock delta before global multiplier. UI task reads it immediately and converts seconds to ms. Update can no-op while pauseCount is nonzero; cadence/paused behavior NOT_RUN. |
| SKSE 2.3.1 `Hooks_Threads.cpp`, `Hooks_UI.cpp` research copies | Main drain has two hooks, UI one; both drain until empty. UI v1 calls Run/Dispose (newer UIDelegate path deletes). Supports pointer ABI inspection, not an in-game cadence proof. |
| `RE/B/BSInputDeviceManager.h:26,49` | BSTEventSource<InputEvent*> and GetSingleton; AddEventSink inherited from BSTEventSource |
| `RE/I/InputEvent.h:49–55` | GetDevice(), AsButtonEvent(), next event chain |
| `RE/B/ButtonEvent.h:33,151` | IsDown(), GetIDCode(); no invented mapping method |
| `RE/C/ControlMap.h:101`; `RE/U/UserEvents.h:111,118` | GetMappedKey(string_view, INPUT_DEVICE, gameplay default), kInvalid; Inventory / Tween Menu event names |
| `RE/M/MenuOpenCloseEvent.h`; existing UI sink | menuName / opening; copied scalar event timing |
| Windows SDK / existing skeleton | QPC/frequency and PROCESS_MEMORY_COUNTERS_EX/GetProcessMemoryInfo; MSVC/SDK build NOT_RUN |

Declaration root: `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0/include`.

Header/source inspection establishes declarations, not a Windows compile or
runtime ABI/cadence guarantee. The retained opaque delegate cast is needed by
CommonLib's incomplete global TaskDelegate/UIDelegate_v1 parameters and complete
detail ABIs; verify this adapter in Windows CI before installing the internal probe.

## Fixed budgets and analysis

`session-p4/autotest/perf_compare.py` reads the four reports' full
`evidence/probe-slice.jsonl`, falling back to embedded events when no slice
exists. It outputs median/p95/p99/max and overhead for standing **and** turning,
10-open event/settled median/max, cold event/settled values, warm maximum,
memory slope and separate PASS/FAIL/NOT_RUN verdicts. A diagnostic anchor table
reports probe median/p95, engine median/p95 and anchor_ratio for **all four**
capture labels, even when a real capture's anchor is invalid.

Before judging any capture budget, require `anchor_ratio = probe median /
engine median` in **[0.9, 1.1] inclusive**. Recompute the ratio and check the
reported value agrees; require positive finite engine deltas/median/p95 and
matching probe/engine retained and total counts. Missing, out-of-range or
malformed anchors and any abort make the corresponding frame, inventory or
resource budget **NOT_RUN**, not PASS/FAIL. A missing/invalid standing or turn
anchor leaves the combined comparison NOT_RUN, even if another capture exceeds
a budget. This validity rule does not relax any performance budget, and a valid
ratio alone cannot prove the full cadence hypothesis.

- Combined p95 ≤1.10× baseline p95 for **both** 60-second captures. Single-module
  overhead is descriptive; the combined budget is evaluated only for combined.
- Each warmed open 2–10 must have t_settled ≤250 ms at 500 native inventory rows.
  Open 1 is cold and reported separately, with no 250 ms cold budget.
- Memory flag = latter-half least-squares private-memory slope >1 MiB/min **and**
  last-quartile mean >105% of first-quartile mean, restricted to the marked
  600-second resource check. Require full-window edge coverage and no sampling
  gap >2 seconds for the routes' 1-second probe cadence. Working set is retained
  as raw context. This finite trend rule cannot prove a complete absence of leaks.

Missing, malformed, non-finite, incomplete, uncorrelated, simulated, failed-run or
wrapped comparison evidence cannot produce a budget PASS. Provenance includes
report/probe SHA-256, actual capture durations, hardware, resolution, load order,
package/probe/save/dependency/executable/settings hashes and fixture/copy records.
Unknown provenance stays null/NOT_RUN and must be filled before owner acceptance.
No budget has been relaxed. The earlier perf_memory.py remains a descriptive
helper and is not the acceptance reader.

## Routes and capture plan

Routes live outside Git in `session-p4/autotest/routes/`:
`perf-baseline.json`, `perf-camera-only.json`, `perf-ui-only.json`, and
`perf-combined.json`. They are identical apart from profile (including the common
name `perf-capture`); use distinct run labels when scheduling real runs. The
existing camera-only profile/plugin-list mapping is preserved.

1. Before prepare, refuse existing deployments/processes, missing/patched fixture
   source, occupied batch destination, or an occupied copied-save destination.
2. Prepare the disposable profile and back up its save directory. Copy
   SkyCRPG_Fixture_CombinedAcceptance to SkyCRPG_Test_Perf172 (.ess and optional
   .skse), load that copy through the existing probe-confirmed console path.
3. Warm up 30 seconds, execute `bat skycrpg-grid-500` once, wait 10 seconds.
4. Capture standing 60 seconds and constant mouse turn 60 seconds (6 relative
   counts per 50 ms, same sensitivity/settings in every profile).
5. A 110-second inventory capture contains ten cycles. Each warms the pre-open
   samples, presses I, confirms open, observes settling for 3.1 seconds, scrolls
   for 2 seconds at 10 wheel ticks/second, closes with TAB, confirms close and
   waits 3 seconds. The remaining window is idle; latency statistics use only
   these ten labelled inventory opens.
6. Five alternating coc transitions between WhiterunBreezehome and WhiterunOrigin.
   Confirm the destination cell through fresh requested player_state observations;
   loading-state observation retries do not replay the coc command.
7. A fixed 600-second resource capture runs an inventory cycle and one alternating
   coc per 60-second slot (ten slots). Missed slots fail instead of lengthening
   the sample. Resource opens carry label resource and do not contaminate the
   ten-open inventory budget.
8. Quit through the existing probe-confirmed console cleanup (`qqq`); never save.
   Restore and verify fixture/probe/copy removal, INI hashes, no active deployment
   and no owned process. Preserve each report, full probe slice and job evidence.

The 1200-second total watchdog includes startup/load and cleanup headroom; the
resource window is 600 seconds within that route. Simulation takes
925.701 seconds (15.43 minutes) of virtual game time; actual startup/load overruns fail at the deadline.
Route paths stage-perf/dist-perf and the probe artifact must be populated by the
orchestrator with the final DYL-170 candidate; no unavailable artifact is assumed
present by this worker.

Batch source:
`/srv/storage/Projects/SkyCRPG/session-p3/fixture-500/out/skycrpg-grid-500.txt`,
SHA-256 `ebd8547e66dd84e7f778d50e05a36f031bcbb396bbbe380d78f7d62768fac0de`.
The runner journals its Data-root destination in preservation.json files/planned
before exclusive creation; existing files/symlinks are never overwritten. Restore
uses the existing runtime_profile journal contract to remove it, with a separate
runner absence check. A lost exclusive-create race withdraws journal ownership;
a foreign file is retained and verification fails. No profile tool was edited.
**Data-root batch lookup is NOT_RUN in game.** This follows the decided deployment
location; the earlier P3 run used the game root.

Both vanilla CELL editor IDs were verified by parsing CELL/EDID records from the
local read-only Skyrim.esm.copy, including compressed records:
WhiterunBreezehome `0x000165A8`, WhiterunOrigin `0x0001A27F`. Record:
`session-p4/logs/perf/cell-verification.json`. No game data was copied into Git.
The fixture adds 500 distinct forms; existing save items may make the total
larger. The owner must verify/report the actual native row count; input-file
length is not proof of the 500-row acceptance dataset. No count/weight change is
made to hide encumbrance.

| Runtime route / required observation | Keyboard/mouse | Controller |
| --- | --- | --- |
| Final hashes, host/settings/load order, disposable copy and native row count | NOT_RUN | NOT_RUN |
| UI/main cadence standing/turn and pause/menu; probe/engine medians/p95 and anchor_ratio in [0.9,1.1]; invalid capture NOT_RUN | NOT_RUN | NOT_RUN |
| Stop/restart during either pending hop: one chain only; stopped hop drains and ends; no runaway log in ordinary captures | NOT_RUN | NOT_RUN |
| Bound input, cold open, warm opens 2–10, raw periods and grid-scroll behavior | NOT_RUN | NOT_RUN |
| Cell destinations and full 10-minute memory window | NOT_RUN | NOT_RUN |
| Normal console quit, no save and verified restoration | NOT_RUN | NOT_RUN |
| Baseline/camera-only/UI-only/combined budget comparison and acceptance | NOT_RUN | NOT_RUN |

Automated routes currently target keyboard/mouse with Inventory=I/Back=TAB and
physical controller off. The input adapter supports mapped devices; the separate
controller/Tween route is a manual capture checklist, not an automated controller
PASS. The owner must state the actual device used. Run one baseline/candidate
capture per profile on the pinned host; repeat only a noisy/failing comparison
once with a recorded hypothesis. Each game job needs its own purpose, command,
profile, owner, handle/log, deadline, expected result and cleanup record.

## Round-4 verification

All job records are `session-p4/logs/perf/round4-*.job.json` and logs have matching
names. Before each job, run_job.py records purpose, exact argv/profile, expected
exit 0/PASS, 600-second timeout/deadline, owner, completion signal and cleanup;
it records the process-group PID and verifies absence on completion. All round-4
jobs completed with no timeouts or surviving owned process groups. No live run
or native Windows build was attempted.

| Current job | Result |
| --- | --- |
| `round4-portable` — four probe C++20 tests, -Wall -Wextra -Werror -pedantic | PASS; chain tests cover repeated drains, start, duplicate start, stop/restart mid-UI and mid-main hop, stopped-hop drain, enqueue failure, ninth call at 1 ms, sliding guard and reset |
| `round4-focused-final` — synthetic analysis tests | PASS, 13 tests; boundaries 0.9/1.1, half/double cadence, missing/invalid/forged anchors, mismatched counts, aborts, baseline invalidation, resource ring wrap |
| `round4-runner-final` — complete offline runner suite | PASS, 119 tests, including all earlier deployment/route/restore safety coverage |
| `round4-sim-{baseline,camera-only,ui-only,combined}` — four CLI routes | PASS, all steps and cleanup; purely virtual reports with engine/anchor schema |
| `round4-compare` — CLI analysis of the four simulated reports | PASS execution; all runtime budgets NOT_RUN |
| `round4-diff-check-final` — complete working-tree whitespace check | PASS |
| Windows/MSVC adapter build and every real cadence/performance measurement | NOT_RUN |

Exact commands (through `python3 session-p4/logs/perf/run_job.py NAME PURPOSE ...`;
full absolute argv are retained in each job JSON):

```sh
bash -c 'set -euo pipefail; for source in tools/testprobe/tests/*_test.cpp; do name=$(basename "$source" .cpp); binary=/srv/storage/Projects/SkyCRPG/session-p4/logs/perf/round4-$name; g++ -std=c++20 -Wall -Wextra -Werror -pedantic "$source" -o "$binary"; "$binary"; echo "$name PASS"; done'
python3 -I -m unittest discover -s /srv/storage/Projects/SkyCRPG/session-p4/autotest/runner/tests -p test_perf_compare.py
python3 -I -m unittest discover -s /srv/storage/Projects/SkyCRPG/session-p4/autotest/runner/tests
# PROFILE expanded once for baseline, camera-only, ui-only, combined
python3 -I /srv/storage/Projects/SkyCRPG/session-p4/autotest/runner/run_route.py /srv/storage/Projects/SkyCRPG/session-p4/autotest/routes/perf-PROFILE.json --simulate --sim-root /srv/storage/Projects/SkyCRPG/session-p4/logs/perf/round4-sim-PROFILE --label perf-PROFILE-round4
# perf_compare.py takes the four reports under each round4-sim-PROFILE/session-p3/autotest/runs/perf-PROFILE-round4/report.json
# and -o /srv/storage/Projects/SkyCRPG/session-p4/logs/perf/round4-simulation-comparison.md
```

## Earlier round-3 commands and results (historical)

Logs/job records are under `session-p4/logs/perf/`. `run_job.py` writes purpose,
exact argv, expected result, owner, completion signal, timeout/deadline and log
before spawning, records the PID/process group, and verifies group absence after
completion. All jobs have a 600-second limit. There were no timeouts or surviving
owned children. No live backend was used.

| Job | Result |
| --- | --- |
| All four probe portable C++20 contracts (`round3-portable.log`) | PASS: json_line, snapshot_policy, probe_config, perf_policy; -Wall -Wextra -Werror -pedantic |
| Focused performance tests (`round3-focused-fixed.log`) | PASS, 20 tests at that stage |
| Full runner suite (`round3-runner-complete.log`) | PASS, 114 tests, including synthetic pass/fail/missing budgets and deployment/request/restore failures |
| Four CLI simulations (`round3-sim-baseline-verified.log; round3-sim-{camera-only,ui-only,combined}.log`) | PASS, all steps and cleanup; simulated measurement budgets NOT_RUN |
| Comparison CLI on four simulations (`round3-compare.log`, simulation-comparison.md) | PASS tool execution; every runtime budget NOT_RUN |
| Windows/MSVC /W4 /WX /permissive- engine adapter, SDK/link, paused-menu cadence and all real measurements | NOT_RUN |

Exact core commands (each executed through run_job.py with the named purpose):

```sh
# worktree cwd, all portable contracts; binaries/logs external
bash -c 'set -euo pipefail; for source in tools/testprobe/tests/*_test.cpp; do name=$(basename "$source" .cpp); binary=/srv/storage/Projects/SkyCRPG/session-p4/logs/perf/round3-$name; g++ -std=c++20 -Wall -Wextra -Werror -pedantic "$source" -o "$binary"; "$binary"; echo "$name PASS"; done'
python3 -I -m unittest discover -s /srv/storage/Projects/SkyCRPG/session-p4/autotest/runner/tests
# profile expanded once for each of baseline, camera-only, ui-only, combined
python3 -I /srv/storage/Projects/SkyCRPG/session-p4/autotest/runner/run_route.py /srv/storage/Projects/SkyCRPG/session-p4/autotest/routes/perf-PROFILE.json --simulate --sim-root /srv/storage/Projects/SkyCRPG/session-p4/logs/perf/sim-PROFILE --label perf-PROFILE-round3
# final baseline uses sim-baseline-verified (same route); exact argv are in each .job.json
```

Initial regression FAILs are preserved: a legacy assertion matched explanatory
PASS prose, and applying new save restoration to older fake scenarios changed
an existing contract. The focused test then caught an extra floating-point
scroll tick and a test mutating the already-start resource marker. Changed-input
fixes use absolute input schedules, constrain save restoration to copied-save
scenarios and correct the synthetic marker. Final verification passes; no
unchanged failing retry loop was used.

## Remaining gates and decisions

No owner decision or module integration patch is needed for this implementation.
The selected UI-drain CPU period, engine anchor and settle design are implemented
without a new painted-grid dependency. Re-audit changed module sources after
DYL-170, build the probe in Windows CI, stage the final artifacts, and let the orchestrator run the routes.
Owner runtime acceptance and any explicit budget tradeoff remain pending; no
performance acceptance is claimed here. No SWFs were built. Post-MVP candidates:
broader hardware/settings coverage, a separately bounded multi-hour soak and an
optional native painted-grid endpoint.
