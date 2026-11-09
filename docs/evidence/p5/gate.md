# DYL-202 / P5-06 — Phase 5 gate

**REVISE — owner decision 2026-11-08 ("REVISE, do R1–R11").** Written 2026-11-08 from the evidence notes and pull requests listed below as the orchestrator's recommendation, then ruled on by the owner; see [owner decision](#owner-decision). Nothing in this record turns a NOT_RUN or FAIL row into a pass, and no Phase 5 pull request is merged.

What REVISE means here: each of the four features has in-game evidence and an owner hands-on acceptance of its feel on keyboard and mouse, the spike has its owner ruling (D028), and the owner decisions D5-2 through D5-6, D026, D027 and D028 are recorded. What Phase 5 does not yet have is a **combined candidate**: no build or package contains Camera, Selection and UI together, the Phase 4 regression and the MVP acceptance IDs C1–C5 have not been rerun on any Phase 5 build, nothing has run on a controller, the binding table is not written into `docs/CONTROLS.md`, and the Phase 4 ACCEPT record (#33) is still unmerged. Those are finite and listed in [REVISE items](#revise-items). A HOLD is not warranted: no feature is blocked by an unresolved engine or provider problem, and the one feature-level FAIL that remains ("no pull-in while walking", DYL-201) was accepted by the owner as felt.

## Candidate identity

**Main `20e215d`** (merge of #32, Phase 4 final docs; [CI 41462292724](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41462292724) PASS). Main has not moved during Phase 5. Every Phase 5 branch starts from it with zero drift.

| PR | Ticket | Branch → base | Head | Last code commit | Branch CI | Draft |
| --- | --- | --- | --- | --- | --- | --- |
| [#33](https://github.com/Sonoran-Solutions/SkyCRPG/pull/33) | DYL-175 record | `phase5/baseline-decisions` → `main` | `720f40b` | docs only | — | no (ready, unmerged) |
| [#34](https://github.com/Sonoran-Solutions/SkyCRPG/pull/34) | DYL-197 P5-01 target lock | `phase5/dyl-197-target-lock` → `main` | `151a2c6` | `63e992e` | [41523913654](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41523913654) PASS | yes |
| [#35](https://github.com/Sonoran-Solutions/SkyCRPG/pull/35) | DYL-199 P5-04 occlusion spike | `phase5/dyl-199-occlusion-spike` → `main` | `a2d766c` | `6667bf5` | [41537644253](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41537644253) PASS | yes (evidence only; never merge hooks) |
| [#36](https://github.com/Sonoran-Solutions/SkyCRPG/pull/36) | DYL-198 P5-02 selection | `phase5/dyl-198-selection` → `main` | `f3c09a9` | `1506e6d` | [41513733275](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41513733275) PASS | yes |
| [#37](https://github.com/Sonoran-Solutions/SkyCRPG/pull/37) | DYL-201 P5-05 fixed height | `phase5/dyl-201-fixed-height` → #34 | `821361c` | `c10e6bc` | [41759225743](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41759225743) PASS | yes |
| [#38](https://github.com/Sonoran-Solutions/SkyCRPG/pull/38) | DYL-200 P5-03 cursor mode | `phase5/dyl-200-cursor-mode` → #37, with #36 merged in | `ed9692c` | `4946c92` | [42537097857](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/42537097857) PASS | yes |
| [#39](https://github.com/Sonoran-Solutions/SkyCRPG/pull/39) | vinput tooling spike | `spike/vinput-proton` | `4813f72` | — | — | **closed unmerged** (dropped) |

Merge order proposed by the PR bodies: #33, then #34, #36, #37, #38; #35 stays unmerged as evidence. #34, #35 and #36 each edit `.github/workflows/build.yml`, `tools/ci_scope.py` and its test; #38 already carries a resolution for #36 against #37. The integrator resolves the rest; nothing auto-merges.

**Tested binaries (last staged build per feature).** No archive contains more than one of these; there is no combined Phase 5 candidate archive, and whether Selection ships in its own package or inside Camera's is an open owner decision (P5-06 question from #36 and #34's overlay delivery).

| Feature | Build | DLL SHA-256 (prefix) | Provenance |
| --- | --- | --- | --- |
| Target lock (Camera) | `63e992e` | Camera `8be698a5…` | owner runs lock-1/2/3; earlier feature-off routes on `6b42f35` |
| Selection | `1506e6d` | Selection `eba23c2e…` | route `j-p5`; the cursor routes later ran Selection `675db0a7…` (`00129fd`) and `6d8b12e2…` (`4946c92`) |
| Fixed height (Camera) | `c10e6bc` | Camera `5409d1bd…` | owner run fixed-1; harness routes on `50a3dd2` (`28de1800…`) |
| Cursor mode (Camera + Selection) | `4946c92` | Camera `8ca54149…`, Selection `6d8b12e2…` | route-cursor-8 and owner run cursor-1 |
| Occlusion spike (never shipped) | `6667bf5` | Spike `abff6b40…` | final2-2204 |

The Camera DLL that contains all three Camera features at once is the cursor-mode build (`8ca54149…`, branch stacked on #37 on #34), but it has only been run with its cursor routes and the owner cursor session: target lock and fixed height were off in those profiles. **No run has had two Phase 5 features enabled together.**

## Host and runtime

Unchanged from Phase 4: Steam `SkyrimSE.exe` 1.7.104.0 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`, SKSE 2.3.1, Address Library `versionlib-1-7-104-0.bin` `8aab3dd2…`, Proton Experimental `experimental-11.0-20261027b-x86_64`, Ubuntu 26.04.1, RTX 3080, 1920×1080 windowed, MO2 2.5.2 portable. Providers: CommonLibSSE-NG 7.2.0, SmoothCam 1.8.1 (`b20bf8a2…`), TDM 2.3.1, MCM Helper 1.6.3; see the [runtime matrix](../../compatibility/runtime-matrix.md) and the [dependency lock](../../compatibility/dependency-lock.md). Devices: keyboard and mouse for every automated route and every owner session except owner-lock-3 (controller, R3 only). Other hosts, Proton builds, resolutions and managers: NOT_RUN.

## Start conditions

| Condition | Status |
| --- | --- |
| Phase 4 actual owner ACCEPT | **Recorded, unmerged.** D025: the owner confirmed the DYL-175 comment as their ACCEPT after playing on keyboard/mouse and controller; per-area observations were not recorded. The record lives in #33, which is open and mergeable; on `main` the Phase 4 docs still read OWNER_PLAYTEST_PENDING. The gate treats the ACCEPT as given and the docs as owed. |
| Phase 4 regression resolution | Phase 4 technical rows unchanged (DYL-172 memory FAIL accepted as external-provider limitation, D024). Not rerun on Phase 5 builds. |
| D5-1 … D5-6 rulings | D5-1 superseded by **D026** (original selection plugin replaces BTPS). **D5-2** bindings: R3 short press lock / hold POV at 0.4 s (**D027**, after owner-lock-3); cursor **B**, no pad default (2026-11-08); `[` `]` cycle and **U** lock key remain unapproved test defaults. **D5-3** toggleable cursor mode. **D5-4** original Camera-owned lock indicator (TrueHUD not used). **D5-5** / **D028** Stage 1 only with a curated base-form list. **D5-6** DYL-157 compression optional, default OFF. |
| P5-04 GO/REVISE/HOLD | **GO for Stage 1** under D028. The spike PR body still says "Awaiting the owner decision" and the note's header says "pending"; the DYL-201 note records D028. #35 needs its header corrected. |

## Feature outcomes

Counts follow the evidence notes; "PASS, owner" rows are the owner's word on feel, recorded as such, not harness proof.

| Ticket | Harness | Owner | Open | Disposition for this gate |
| --- | --- | --- | --- | --- |
| **DYL-197 target lock** ([note](target-lock.md), #34) | 8 PASS (feature-off routes; lock, lease release/retake, stable pitch and yaw in owner sessions); 2 FAIL on earlier builds (`6b42f35`, `82b2121`: lease never released), fixed in `63e992e`; one supplied Windows candidate failed to link (CI 41475236195), later builds green. NOT_RUN: mouse look and wheel zoom while locked (no look input in the trace, so lock-2's "all passed" is unresolved for that row), target switching (no binding), auto-retarget, dialogue/load/F8 interrupting a held lock. | 9 PASS, owner, 2026-11-02: "The lock worked, I'm satisfied with the implementation"; "All checks passed except the last one because there was only one bandit"; "It worked and felt right, I approve" (R3, controller → D027). | Keyboard lock key (U is a test default); controller target switching; overlay delivery; fixes after the independent review not re-reviewed. | Feature accepted as felt on both devices for the lock itself. REVISE items R3, R5, R7. |
| **DYL-198 selection** ([note](proximity-activation.md), #36) | 4 PASS on `1506e6d` unattended (hook installs with SmoothCam, Bannered Mare bench prompt, cycling 0,1,2,0, scan limits); log check 57 substitutions in 11 353 picks. Later cursor routes exercised the same plugin at `675db0a7…`/`6d8b12e2…` (hover, activation E on the bench, empty-floor E suppressed). NOT_RUN: real activation across doors/containers/items/NPCs, LOS through walls, combat/sneak/telekinesis/grab, controller, DLL-absent on the final build, BTPS-present refusal in game. | None on its own; the cursor owner session used it. | Combat and sneak rules; owned-clutter Steal prompt; `[` `]` keys; controller bindings; packaging; the note's acceptance table still shows the Windows build and co-load as NOT_RUN while the PR body says PASS. | Works as the activation path the owner used on 2026-11-08. REVISE items R3, R5, R6, R8. |
| **DYL-199 occlusion spike** ([note on its branch](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase5/dyl-199-occlusion-spike/docs/evidence/p5/occlusion-spike.md), #35) | final2-2204 on `6667bf5`: 5 PASS (height held 60 s, hook ≈0.10 ms/frame, fade 0.0 % p95 at the frame cap, filter inert without a match, observe routes), 1 PARTIAL (boundary walk, fail-closed drop at 14.8 s), 1 FAIL (automatic classifier discards real enclosure), 2 NOT_RUN (culling above a real ceiling; Camera Studio/Disocclusion binaries not acquired). | The m-* see-through look approved (2026-11-02). | Header says "pending" while D028 is recorded; the 14.8 s drop is attributed differently by the spike note and the DYL-201 note. | Closed by **D028**; evidence stays on its branch. REVISE item R9 (record hygiene only). |
| **DYL-201 fixed height** ([note](fixed-height-camera.md), #37) | `50a3dd2`: 7 PASS (hooks, standing 60 s at z≈540, transparent listed beam, enclosure kept, F8/POV/menu restores, feature off, frame budget +1.04 % p95 at the cap); 2 FAIL ("character stays in view" at corridor and rock ceiling, "no pull-in while walking" as written: z 288→552, jumps to 236 units). `c10e6bc`: 5 PASS, walking FAIL unchanged, F8 return one-sample drop (defect, not fixed), beam stays faded during the paused console (limitation). NOT_RUN: low-space framing, load/cell/unload restores as a distinct test, target lock with fixed height, controller, frame budget on `c10e6bc`. | 4 PASS, owner, 2026-11-03: "everything passed and I approve"; walking jumps accepted as felt; paused console and F8 drop accepted. Low-space step skipped. | Whether the list stays at three beams; the note's proposed rewording of the height criterion is unanswered; review fixes not re-reviewed. | Accepted as felt; the literal criterion stays FAIL and must be reworded or re-accepted at the owner decision. REVISE items R2, R4, R5. |
| **DYL-200 cursor mode** ([note](cursor-mode.md), #38) | route-cursor-8 on `4946c92`: 6 PASS (first `[` visits only on-screen candidates, drift keeps the latch, latch survives the 10 s timeout, reverse cycle skips hidden candidates, no refused cycle, hover regression; pick cost median 6 µs, max 65 µs); 1 NOT_RUN (latch release by pointer movement: fixture inconclusive). Seven earlier route sets found and fixed five defects (UI job thread, missing Boolean, point hit-test, dropped latch, entry handshake); three independent review rounds on the last pass, last one no blocker. | 7 PASS, owner, 2026-11-08: "Finished, everything passed", "All of A–G". The session lasted 4 minutes; logs corroborate A, B, one cycle of D and the pick cost; F.6 quickload is contradicted by the lifecycle log (one load) and stays NOT_RUN. Decisions: B, no pad default, 65/48/12 provisional, on-screen-only cycling, Selection ships `bAllowExternalTargets=0`. | Controller reticle and binding; other resolutions; controlled frame-cost sample; quickload while active; latch release as harness evidence. | Accepted as felt on keyboard/mouse. REVISE items R3, R5. |

## P5-06 acceptance criteria

| Criterion | Status | Evidence and gap |
| --- | --- | --- |
| 1. Every enabled feature's scoped acceptance routes PASS, or each non-PASS carries an owner-ratified disposition | **REVISE** | Per-feature routes PASS or are owner-accepted as above. Non-PASS rows carrying an owner disposition: fixed-height walking (accepted as felt), paused console, F8 one-sample drop, cursor latch kept off-screen. Non-PASS rows **without** one: fixed-height "character stays in view" at corridor/rock ceiling; lock mouse-look/zoom unresolved; cursor F.6; every controller row. |
| 2. Every new feature independently disable-able; Camera-only and UI-only work without the other; no required provider masquerades as optional | **PARTIAL** | Feature-off routes PASS per feature (`p5-cam-off-*` for lock, `p5-cursor-off`, fixed-off variant). Selection absent: PASS on an earlier stage per #36, NOT_RUN on the final build. Cursor mode refuses cleanly without Selection or with `bAllowExternalTargets=0` (logged). UI-only and combined starts on a Phase 5 build: **NOT_RUN**. Selection is a third DLL whose packaging is undecided. |
| 3. Final binding table works on both devices; menus/F6/F7/F8/POV win in priority; no duplicate native action or stranded state | **NOT_RUN as a table** | Per-feature interrupt rows PASS (lock: menu and F6; fixed: console, Tab, dialogue, F6, F8; cursor: B, Tab, console, F6, F8, dialogue, weapon draw, lock). No run has had all three enabled to check the table as a whole; `docs/CONTROLS.md` has no Phase 5 rows; keyboard lock key U and `[` `]` are unapproved; controller cursor and switching are unbound. See [binding table](#binding-table-as-tested). |
| 4. C1–C5 and Phase 4 functional gates remain PASS; missing-provider and disabled-feature regressions safe | **NOT_RUN** | The Phase 4 packaged regression (22/22 on `6b4c0fc`) has not been rerun on any Phase 5 build. C1–C5 carry their Phase 2–4 statuses (C1 and C4 FAIL on pitch persistence, C2/C3/C5 partial) and were not retested. Feature-disabled profiles PASS per feature only. |
| 5. Bounded performance matrix and screenshots reflect tested runtime, without claiming unrun capture routes | **PARTIAL** | See [performance](#performance-as-measured). Each number is from a different build and profile; no matched comparison on one candidate. Screenshots exist per route and are cited in the notes; none is claimed as route proof. |
| 6. Owner explicitly records ACCEPT or itemized REVISE/HOLD | **PENDING** | Three per-feature owner sessions recorded with quotes. No combined route played; no Phase 5 ACCEPT/REVISE/HOLD recorded. |

## Binding table as tested

| Action | Keyboard/mouse | Controller | Status |
| --- | --- | --- | --- |
| Target lock toggle | **U** (test default, unapproved) | **R3 short press**, hold 0.4 s = native POV (D027, approved) | Lock PASS on both; U needs an owner choice or confirmation |
| Target switch | none | none | NOT_RUN; no binding (open D5-2 item) |
| Selection cycle | **[** / **]** (test defaults, unapproved) | none | PASS keyboard in routes; controller unbound |
| Cursor mode toggle | **B** (approved 2026-11-08) | none by default (owner decision; D-pad Left would need a Hotkey1 remap) | PASS keyboard; controller NOT_RUN |
| Cursor pointer | mouse look deltas (65) | right stick (same path; speed reduced ≈14×) | Keyboard PASS; controller NOT_RUN |
| Activate | native E only | native A only | Single dispatch through Selection's Pick; PASS keyboard |
| Precision / recenter / emergency | F6 / F7 / F8 | unbound by default (unchanged) | Each exits cursor mode and lock; PASS in per-feature rows |
| Native POV | F key / R3 hold | R3 hold | PASS lock-3 |
| Menus, dialogue, load, death | native | native | Exit every feature; PASS per feature; combined NOT_RUN |

One input consumer per event was reviewed in source for each feature (lock's input tracker, Selection's edge FIFO, cursor's captured look deltas) and not contradicted in any run; it has not been tested with all three enabled.

## Performance as measured

| Measure | Value | Build / profile | Caveat |
| --- | --- | --- | --- |
| Phase 4 baseline frame cost | combined p95 +0.58 % standing, +0.72 % turning | `6b4c0fc` rc1 | Reference only |
| Fixed-height frame budget | +1.04 % p95 at the frame cap | `50a3dd2` fixed-height profile | Not remeasured on `c10e6bc`; owner run saw 104/6013 frames over budget incl. a 2212 ms load hitch (not a measurement) |
| Thin-occluder fade path | 0.0 % p95 at the cap; hook ≈0.10 ms/frame (0.59–0.64 %) | spike `6667bf5` | Spike plugin, not the shipped Camera path |
| Cursor pick cost | median 6–7 µs, p99 13 µs, max 65 µs per frame | `4946c92` cursor profile | Trace capped at 512 samples per process |
| Selection scan cost | `cost_us` 230–430 per refresh at 0.1 s, pick 17–20 µs | `6d8b12e2…` in the Bannered Mare | Dense exterior cells NOT_RUN |
| Target lock cost | not measured as frame time | — | NOT_RUN |
| Memory | not measured on any Phase 5 build | — | Phase 4 D024 limitation stands |

## Owner play so far

| Session | Date | Build | Device | Result |
| --- | --- | --- | --- | --- |
| owner-lock-1 / -2 / -3 | 2026-11-02 | `63e992e` | keyboard/mouse; -3 controller | Lock, lease, arrows, Firebolt, menu and F6 interrupts, R3 press/hold; quotes above; D027 |
| owner-fixed-1 | 2026-11-03 | `c10e6bc` | keyboard/mouse | "everything passed and I approve"; low-space step skipped |
| owner-cursor-1 | 2026-11-08 | `4946c92` | keyboard/mouse | "Finished, everything passed", "All of A–G"; 4-minute session, logs corroborate the first minutes, F.6 NOT_RUN |
| Combined representative route | — | — | — | **NOT_RUN** |

## Tooling and process notes

- The vinput in-process automation spike was dropped: with focus-faking and isolation off, Skyrim still crashed at `SkyrimSE.exe+0xE128B2` one second after real window activation (DirectInput buffered-read interposition). [Record](https://github.com/Sonoran-Solutions/SkyCRPG/blob/spike/vinput-proton/docs/evidence/p5/vinput-spike.md) on `spike/vinput-proton`; #39 closed unmerged. Routes keep the uinput/xwd harness.
- `session-p4/runtime_watchdog.py` does not restore the profile after a normal quit; owner sessions need a manual restore (done each time; recorded in the ledger).
- Trace caps (`iTrace`, `iDiagnosticLimit`, Selection event budget) cut owner sessions short in the logs; raise them for owner runs.

## Known limitations carried to the owner decision

Short form; each note has the full list.

- Target lock: a competing API owner or MustKeep cannot be overridden; a held lease can swallow a short R3 press; the lock does not fix unlocked free aim (F6 remains the fallback); no target-switch binding.
- Selection: candidates limited to the validated list (≤8 within ≈180 units); prompt text can lag one scan; OBND reference pose is a hypothesis; no live DLL unload.
- Fixed height: only the three curated beams (`0602A6`, `0602AF`, `0616EA`); real enclosure still pulls the camera in; beam stays faded during a paused console; one-sample drop on F8 return.
- Cursor mode: point box for degenerate OBND; centre-behind-camera objects unpickable; near-camera boxes inflated; a hidden candidate can be Selection's target for one Pick tick; a cycled latch is kept after the object leaves the screen (owner-accepted); unlocked `focused()`/`noActiveMenus()` walks pre-date Phase 5.

## REVISE items

Ordered by what unblocks the most. Each names who closes it and what evidence closes it.

| # | Item | Owner of the work | Closes when |
| --- | --- | --- | --- |
| R1 | Merge #33 so `main` carries the Phase 4 ACCEPT (D025) and the D5 rulings; then merge #34, #36, #37, #38 in order with the integrator resolving the `build.yml` / `ci_scope.py` overlaps. | Owner (merge approval), integrator | `main` CI green with all four features; branch evidence notes unchanged. |
| R2 | Build **one combined candidate** from merged `main` (Camera with lock, fixed height, cursor; Selection; UI) and decide Selection's packaging (own archive or inside Camera's) and the lock overlay's delivery. | Owner (packaging decision), orchestrator | Archives with hashes in `packaging.md`-style provenance; `tools/package.py` covers them. |
| R3 | Write the Phase 5 rows into `docs/CONTROLS.md` and decide the open bindings: keyboard lock key (U or other), `[` `]` cycle, controller cycle/switch/cursor. | Owner (choices), orchestrator (docs) | Table merged; each chosen key verified unmapped in the stock all-context map. |
| R4 | Reword or re-accept the fixed-height criterion: "no additional beam-induced pull-in, subject to enclosure" versus the literal "stays at configured height". Decide whether the curated list stays at three beams. | Owner | Decision recorded in `DECISIONS.md`; the FAIL row either closes under the new wording or stays an accepted limitation. |
| R5 | **Controller pass** on the combined candidate: lock R3 (re-check), cycle and cursor reticle once bound, F6/F7/F8 and native POV priority, no stuck stick cursor or stranded state. | Owner (hands-on), orchestrator (profile, logs) | Owner sheet with per-step results and log corroboration, as for owner-cursor-1. |
| R6 | **Regression on the combined candidate**: the Phase 4 packaged route set (22 routes) on Camera-only, UI-only and combined profiles, plus the disable-every-new-feature profile and Selection-absent start. Retest C1–C5 to their current scoped definitions and record their status honestly (C1/C4 pitch FAIL may remain). | Orchestrator | Route reports with PASS/FAIL per route; `acceptance-matrix.md` rows updated from them. |
| R7 | Close the lock NOT_RUN rows that a fixture can reach: mouse look and wheel zoom while locked (with Deskflow off), dialogue/load/F8 interrupting a held lock, auto-retarget with two hostiles. | Orchestrator | Route or owner rows with trace evidence of look input while locked. |
| R8 | Selection activation breadth: one bounded route over a door, a container, a loose item, an NPC and an owned item (prompt only), with LOS through a wall; reconcile the note's acceptance table with the PR body (Windows build, co-load, DLL absent). | Orchestrator | Route report; note and PR body agree. |
| R9 | Record hygiene: #35 header and body to say D028 GO; #34 and #37 bodies to name their actual last code commits and DLL hashes; cursor PR body's opening line and results row to match its own later sections; fixed-height pre-run table marked superseded. | Orchestrator | Bodies and notes consistent with this gate. |
| R10 | Matched performance sample on the combined candidate: standing and turning p95 with every feature off, each on alone, and all on; cursor pick cost and Selection scan cost in one dense exterior cell; memory over 10 minutes. | Orchestrator | Table in `performance.md` form, one build, one host, same route. |
| R11 | Owner plays one bounded **combined representative route** on the candidate from R2 (lock a hostile, cycle to a chest, cursor-activate a door, walk under a listed beam, F6/F7/F8 and a menu in between) and records ACCEPT or itemized REVISE/HOLD. | Owner | Quote recorded in this file's [owner decision](#owner-decision). |

R5, R6, R10 and R11 need the candidate from R2; R2 needs R1. R3, R4, R7, R8 and R9 can proceed now. A reasonable sequence is R1 → R3/R4/R9 (docs and decisions, no game time) → R2 → R6 → R7/R8/R10 → R5 → R11.

## Documentation obligations

This record adds Phase 5 sections to [STATUS](../../STATUS.md), the [release checklist](../../RELEASE_CHECKLIST.md) and the [runtime matrix](../../compatibility/runtime-matrix.md). It does not change the Phase 4 rows there; #33 does, and this PR merges after it. `docs/CONTROLS.md` (R3), `docs/compatibility/acceptance-matrix.md` (R6) and `docs/DECISIONS.md` (R4, plus the 2026-11-08 cursor decisions) are owed by the REVISE items, not by this record.

## Out of scope, unchanged

Automatic merges and public release (D023), Stage 2 walls and ceilings, cursor-aimed projectiles, provider forks, click-to-move, turn-based or party commands, a new camera controller, and any expansion of the four-phase MVP claim.

## Owner decision

**REVISE — owner decision 2026-11-08.** After reading this record the owner ruled, verbatim: "REVISE, do R1–R11". The ruling adopts the full list: all eleven items are required before the Phase 5 ACCEPT question is put again. It is a ruling on the record, not a play session; the owner's hands-on sessions remain the three per-feature ones above (keyboard/mouse; controller for R3 lock only). The owner choices embedded in R2, R3 and R4 are still to be made and will be recorded here and in `DECISIONS.md` once #33 is merged. DYL-202 stays In Progress; Phase 5 is not accepted, and nothing in R1 merges without the owner's explicit go on the merge order and the review state of each pull request.
