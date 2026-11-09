# DYL-202 / P5-06 — Phase 5 gate

**REVISE — owner decision 2026-11-08 ("REVISE, do R1–R11", D030).** The orchestrator first wrote the recommendation on 2026-11-08; the owner then ruled REVISE on that date. The dated chronology and subsequent owner decisions are recorded below and in D030–D035. Nothing in this record turns a NOT_RUN or FAIL row into a pass.

What REVISE means here: each feature has bounded in-game evidence and owner feedback as detailed below, the spike has its owner ruling (D028), and the owner decisions D026–D035 are recorded. The Phase 5 combined candidate, Phase 4 regression, C1–C5 rerun and combined owner route remain outstanding. Controller evidence exists for target lock R3 in owner-lock-3 (D027); cursor reticle and the combined controller pass remain NOT_RUN. The Phase 5 controls table is present in `docs/CONTROLS.md`; its entries and remaining unrun coverage are listed below. The Phase 4 ACCEPT is recorded by D025 and merged in #33. These remaining work items are listed in [REVISE items](#revise-items). A HOLD is not warranted: no feature is blocked by an unresolved engine or provider problem, and the walking result is accepted under the owner's reworded fixed-height criterion in D033.

## Candidate identity

**Recommendation-time base:** main `20e215d` (merge of #32, Phase 4 final docs; [CI 41462292724](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41462292724) PASS). This identity table records the gate draft snapshot; later merge/review progress is in the dated [REVISE progress](#revise-progress) section.

| PR | Ticket | Branch → base | Head | Last code commit | Branch CI | Draft |
| --- | --- | --- | --- | --- | --- | --- |
| [#33](https://github.com/Sonoran-Solutions/SkyCRPG/pull/33) | DYL-175 record | `phase5/baseline-decisions` → `main` | `720f40b` | docs only | — | recommendation-time snapshot; merged `4142ea3` |
| [#34](https://github.com/Sonoran-Solutions/SkyCRPG/pull/34) | DYL-197 P5-01 target lock | `phase5/dyl-197-target-lock` → `main` | `151a2c6` | `63e992e` | [41523913654](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41523913654) PASS | yes |
| [#35](https://github.com/Sonoran-Solutions/SkyCRPG/pull/35) | DYL-199 P5-04 occlusion spike | `phase5/dyl-199-occlusion-spike` → `main` | `a2d766c` | `6667bf5` | [41537644253](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41537644253) PASS | yes (evidence only; never merge hooks) |
| [#36](https://github.com/Sonoran-Solutions/SkyCRPG/pull/36) | DYL-198 P5-02 selection | `phase5/dyl-198-selection` → `main` | `f3c09a9` | `1506e6d` | [41513733275](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41513733275) PASS | yes |
| [#37](https://github.com/Sonoran-Solutions/SkyCRPG/pull/37) | DYL-201 P5-05 fixed height | `phase5/dyl-201-fixed-height` → #34 | `821361c` | `c10e6bc` | [41759225743](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41759225743) PASS | yes |
| [#38](https://github.com/Sonoran-Solutions/SkyCRPG/pull/38) | DYL-200 P5-03 cursor mode | `phase5/dyl-200-cursor-mode` → #37, with #36 merged in | `ed9692c` | `4946c92` | [42537097857](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/42537097857) PASS | yes |
| [#39](https://github.com/Sonoran-Solutions/SkyCRPG/pull/39) | vinput tooling spike | `spike/vinput-proton` | `4813f72` | — | — | **closed unmerged** (dropped) |

Merge order proposed by the PR bodies: #33, then #34, #36, #37, #38; #35 stays unmerged as evidence. #34, #35 and #36 each edit `.github/workflows/build.yml`, `tools/ci_scope.py` and its test; #38 already carries a resolution for #36 against #37. The integrator resolves the rest; nothing auto-merges.

**Tested binaries (last staged build per feature).** No archive contains more than one of these; there is no combined Phase 5 candidate archive. Packaging is decided by D031: Selection ships in its own archive, and the lock overlay ships inside Camera's archive.

| Feature | Build | DLL SHA-256 (prefix) | Provenance |
| --- | --- | --- | --- |
| Target lock (Camera) | `63e992e` | Camera `8be698a5…` | owner runs lock-1/2/3; earlier feature-off routes on `6b42f35` |
| Selection | `1506e6d` | Selection `eba23c2e…` | route `j-p5`; the cursor routes later ran Selection `675db0a7…` (`00129fd`) and `6d8b12e2…` (`4946c92`) |
| Fixed height (Camera) | `c10e6bc` | Camera `5409d1bd…` | owner run fixed-1; harness routes on `50a3dd2` (`28de1800…`) |
| Cursor mode (Camera + Selection) | `4946c92` | Camera `8ca54149…`, Selection `6d8b12e2…` | route-cursor-8 and owner run cursor-1 |
| Occlusion spike (never shipped) | `6667bf5` | Spike `abff6b40…` | final2-2204 |

The Camera DLL containing target lock, fixed height and cursor code was staged for the cursor work. The repository Camera INI default is TargetLock `bEnabled=0`, while the owner-cursor-1 stage shipped TargetLock enabled. The route override enabled CursorMode and left TargetLock unchanged; its comments state that this let the owner check the lock key as a cursor exit (sheet step F.8). Fixed height was off. This is the one bounded two-feature combination (cursor plus target lock), owner-only, with capped logs; it is not a combined candidate or harness-route result. No harness route and no combined candidate has run two Phase 5 features together.

## Host and runtime

Unchanged from Phase 4: Steam `SkyrimSE.exe` 1.7.104.0 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`, SKSE 2.3.1, Address Library `versionlib-1-7-104-0.bin` `8aab3dd2…`, Proton Experimental `experimental-11.0-20261027b-x86_64`, Ubuntu 26.04.1, RTX 3080, 1920×1080 windowed, MO2 2.5.2 portable. Providers: CommonLibSSE-NG 7.2.0, SmoothCam 1.8.1 (`b20bf8a2…`), TDM 2.3.1, MCM Helper 1.6.3; see the [runtime matrix](../../compatibility/runtime-matrix.md) and the [dependency lock](../../compatibility/dependency-lock.md). Devices: keyboard and mouse for every automated route and every owner session except owner-lock-3 (controller, R3 only). Other hosts, Proton builds, resolutions and managers: NOT_RUN.

## Start conditions

| Condition | Status |
| --- | --- |
| Phase 4 actual owner ACCEPT | **Recorded and merged.** D025 records the owner's ACCEPT after keyboard/mouse and controller play; per-area observations were not recorded. PR #33 merged as `4142ea3`. |
| Phase 4 regression resolution | Phase 4 technical rows unchanged (DYL-172 memory FAIL accepted as external-provider limitation, D024). Not rerun on Phase 5 builds. |
| Owner rulings | D026 replaces BTPS with original Selection. D027 approves R3 short lock / 0.4 s hold for native POV. D029 records cursor defaults (B, no gamepad default, 65/48/12 and on-screen-only cycle). D030 records REVISE, do R1–R11; ACCEPT is not recorded. D031 selects Selection's own archive and the Camera overlay folder. D032 approves U and `[` / `]` keyboard defaults and leaves pad cycle/switch/cursor unbound. D033 rewords fixed height to “no additional beam-induced pull-in, subject to enclosure” and retains three listed beams; the prior owner acceptance closes it under that wording. D034 selects the trimmed regression scope. D035 sets the per-PR merge/review process. D5-3–D5-6 remain as previously ruled; D028 is Stage 1 GO with a curated list. |
| P5-04 GO/REVISE/HOLD | **GO for Stage 1** under D028. The owner decision is recorded; the spike is evidence only and remains unmerged. |

## Feature outcomes

Counts follow the evidence notes; "PASS, owner" rows are the owner's word on feel, recorded as such, not harness proof.

| Ticket | Harness | Owner | Open | Disposition for this gate |
| --- | --- | --- | --- | --- |
| **DYL-197 target lock** ([note](target-lock.md), #34) | 8 PASS (feature-off routes; lock, lease release/retake, stable pitch and yaw in owner sessions); 2 FAIL on earlier builds (`6b42f35`, `82b2121`: lease never released), fixed in `63e992e`; later builds green. NOT_RUN: mouse look and wheel zoom while locked, target switching, auto-retarget, dialogue/load/F8 interrupting a held lock. Menu and F6 interruption PASS owner in lock-2; F7 recenters and does not interrupt target lock. | Owner sessions on 2026-11-02: lock, lease and R3 results recorded; D027 records controller R3 short/hold approval. | Mouse look/zoom, target switching, auto-retarget, dialogue/load/F8 interruption remain NOT_RUN; review fixes' runtime behavior remains unverified. | Lock accepted as felt; D032 sets U and D031 sets overlay delivery. REVISE items R5 and R7. |
| **DYL-198 selection** ([note](proximity-activation.md), #36) | 4 PASS on `1506e6d` unattended (hook installs with SmoothCam, Bannered Mare bench prompt, cycling 0,1,2,0, scan limits); log check 57 substitutions in 11 353 picks. Later cursor routes exercised the same plugin at `675db0a7…`/`6d8b12e2…` (hover, activation E on the bench, empty-floor E suppressed). NOT_RUN: activation breadth, LOS through walls, combat/sneak/telekinesis/grab, controller, DLL-absent on final build, BTPS-present refusal in game. | None on its own; cursor owner session used it. | Activation breadth, LOS, combat/sneak and controller coverage remain NOT_RUN; packaging (D031) and keyboard bindings (D032) are decided. | Works as the activation path used in cursor routes and owner session. REVISE items R5, R6, R8. |
| **DYL-199 occlusion spike** ([note on its branch](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase5/dyl-199-occlusion-spike/docs/evidence/p5/occlusion-spike.md), #35) | final2-2204 on `6667bf5`: 5 PASS (height held 60 s, hook ≈0.10 ms/frame, fade 0.0 % p95 at the frame cap, filter inert without a match, observe routes), 1 PARTIAL (boundary walk, fail-closed drop at 14.8 s), 1 FAIL (automatic classifier discards real enclosure), 2 NOT_RUN (culling above a real ceiling; Camera Studio/Disocclusion binaries not acquired). | The m-* see-through look approved (2026-11-02). | Boundary-walk PARTIAL, classifier FAIL and two NOT_RUN rows remain as above; the spike is evidence-only. | Closed by **D028**; evidence remains on its branch. No R9 decision remains open. |
| **DYL-201 fixed height** ([note](fixed-height-camera.md), #37) | `50a3dd2`: 7 PASS (hooks, standing, transparent listed beam, enclosure kept, F8/POV/menu restore, feature off, budget); 2 FAIL (character framing at corridor/rock ceiling, and walking pull-in under the old wording). `c10e6bc`: 5 PASS; walking result unchanged, F8 return one-sample drop and beam remains faded during paused console. NOT_RUN: low-space framing, load/cell/unload restores as a distinct test, target lock with fixed height, controller, frame budget on `c10e6bc`. Console close, Tab close and F8 restore/resume behavior follow the evidence note; fixed height resumes automatically after menu/F8 by design. | 4 PASS, owner, 2026-11-03: “everything passed and I approve”; walking jumps accepted as felt; paused console and F8 drop accepted. Low-space step skipped. | Remaining NOT_RUN rows are low-space framing, lock with fixed height, controller, separate load/cell/unload route and frame budget on `c10e6bc`. D033's owner wording is recorded. | Closed under the D033 wording “no additional beam-induced pull-in, subject to enclosure.” REVISE items R5 and R6. |
| **DYL-200 cursor mode** ([note](cursor-mode.md), #38) | route-cursor-8 on `4946c92`: 6 PASS (on-screen cycling, latch drift/timeout, reverse cycle skipping hidden candidates, no refused cycle, hover regression; cost median 6 µs, max 65 µs); 1 NOT_RUN (pointer-movement latch release fixture inconclusive). Owner-cursor-1 used the staged Camera INI with TargetLock enabled while CursorMode was active; it was owner-only with capped logs. Seven earlier route sets found and fixed five defects; code passed review round 2 at `d94a91c`, record confirmation running. | Owner, 2026-11-08: “Finished, everything passed”, “All of A–G”. Four-minute session; logs corroborate A, B, one cycle of D and pick cost; F.6 quickload remains NOT_RUN. D029 records B/no pad/65/48/12/on-screen-only cycling and Selection's shipped external-target default. | Controller reticle, other resolutions, controlled frame-cost sample, quickload while active, pointer-movement fixture. | Accepted as felt on keyboard/mouse. D029 records defaults. |

## P5-06 acceptance criteria

| Criterion | Status | Evidence and gap |
| --- | --- | --- |
| 1. Every enabled feature's scoped acceptance routes PASS, or each non-PASS carries an owner-ratified disposition | **REVISE** | Per-feature routes PASS or are owner-accepted as above. Non-PASS rows carrying an owner disposition: fixed-height walking (accepted as felt), paused console, F8 one-sample drop, cursor latch kept off-screen. Non-PASS rows **without** one: fixed-height "character stays in view" at corridor/rock ceiling; lock mouse-look/zoom unresolved; cursor F.6; every controller row. |
| 2. Every new feature independently disable-able; Camera-only and UI-only work without the other; no required provider masquerades as optional | **PARTIAL** | Feature-off routes PASS per feature (`p5-cam-off-*` for lock, `p5-cursor-off`, fixed-off variant). Selection absent: PASS on an earlier stage per #36, NOT_RUN on the final build. Cursor mode refuses cleanly without Selection or with `bAllowExternalTargets=0` (logged). UI-only and combined starts on a Phase 5 build: **NOT_RUN**. D031 decides Selection ships in its own archive. |
| 3. Final binding table works on both devices; menus/F6/F8/native POV take precedence; no duplicate native action or stranded state | **NOT_RUN as a table** | D032 approves U and `[` next / `]` previous; pad cycle/switch/cursor remain unbound. Per-feature evidence: lock menu/F6 PASS owner, dialogue/load/F8 NOT_RUN, and F7 does not interrupt lock; cursor exits on toggle, Tab/menus, console, F6/F8, weapon draw, lock, dialogue and load (F.6 quickload NOT_RUN); fixed height restores and resumes after menu/F8 by design. The table exists in `docs/CONTROLS.md`, but a combined-device check remains NOT_RUN. See [binding table](#binding-table-as-tested). |
| 4. C1–C5 and Phase 4 functional gates remain PASS; missing-provider and disabled-feature regressions safe | **NOT_RUN** | The Phase 4 packaged regression (22/22 on `6b4c0fc`) has not been rerun on any Phase 5 build. C1–C5 carry their Phase 2–4 statuses (C1 and C4 FAIL on pitch persistence, C2/C3/C5 partial) and were not retested. Feature-disabled profiles PASS per feature only. |
| 5. Bounded performance matrix and screenshots reflect tested runtime, without claiming unrun capture routes | **PARTIAL** | See [performance](#performance-as-measured). Each number is from a different build and profile; no matched comparison on one candidate. Screenshots exist per route and are cited in the notes; none is claimed as route proof. |
| 6. Owner explicitly records ACCEPT or itemized REVISE/HOLD | **REVISE** | Owner recorded itemized REVISE on 2026-11-08 (D030), requiring R1–R11 before the ACCEPT question is put again. ACCEPT is not recorded. |

## Binding table as tested

| Action | Keyboard/mouse | Controller | Status |
| --- | --- | --- | --- |
| Target lock toggle | **U** (D032) | **R3 short press**, hold 0.4 s = native POV (D027/D032) | Owner-approved; lock route PASS on both |
| Target switch | none | none | NOT_RUN; unbound for Phase 5 (D032) |
| Selection cycle | **[** next / **]** previous (D032) | none | PASS keyboard in routes; controller unbound for Phase 5 |
| Cursor mode toggle | **B** (approved 2026-11-08) | none by default (owner decision; D-pad Left would need a Hotkey1 remap) | PASS keyboard; controller NOT_RUN |
| Cursor pointer | mouse look deltas (65) | right stick (same path; speed reduced ≈14×) | Keyboard PASS; controller NOT_RUN |
| Activate | native E only | native A only | Single dispatch through Selection's Pick; PASS keyboard |
| Precision / recenter / emergency | F6 / F7 / F8 | unbound by default (unchanged) | Cursor exits on F6/F8; fixed height restores and resumes after F6/F8; lock exits on F6. F7 recenters and does not interrupt lock. |
| Native POV | F key / R3 hold | R3 hold | PASS lock-3 |
| Menus, dialogue, load, death | native | native | Cursor exits on menu/dialogue/load (death per cursor route contract); lock menu PASS, dialogue/load NOT_RUN; fixed-height restores on menu/load. Combined NOT_RUN. |

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
| R1 | Complete the D035 per-PR merge process for #34, #36, #37, #38 after merged #33; resolve shared workflow overlaps and preserve evidence notes. | Owner (merge authorization), integrator | Ordered merges with green CI and Astra review per D035; all four features in main. |
| R2 | Build **one combined candidate** from merged `main` (Camera with lock, fixed height, cursor; Selection; UI). D031 has decided Selection's own archive and Camera's optional lock-overlay folder. | Integrator | Archives with hashes in `packaging.md`-style provenance; `tools/package.py` covers them. [Packaging](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase5/dyl-202-packaging/docs/evidence/p5/packaging.md) (on its branch until merged). |
| R3 | Phase 5 controls and binding decisions recorded in `docs/CONTROLS.md`; D032 approves U, `[` / `]`, and leaves pad cycle/switch/cursor unbound. | Orchestrator | **Done in this PR**; remaining combined checks are R5. |
| R4 | Fixed-height criterion and curated beam list. | Owner | **Decided by D033**: “no additional beam-induced pull-in, subject to enclosure”; three beams retained. |
| R5 | **Controller pass** on the combined candidate: recheck lock R3 short press / 0.4 s native-POV hold, F6/F7/F8 and native POV priority, and confirm no stranded state. D032 leaves pad cycle, target switch and cursor unbound for Phase 5. | Owner (hands-on), orchestrator (profile, logs) | Owner sheet with per-step results and log corroboration, as for owner-cursor-1. |
| R6 | **Regression on the combined candidate**: the Phase 4 packaged route set (22 routes) on Camera-only, UI-only and combined profiles, plus the disable-every-new-feature profile and Selection-absent start. Retest C1–C5 to their current scoped definitions and record their status honestly (C1/C4 pitch FAIL may remain). | Orchestrator | Route reports with PASS/FAIL per route; `acceptance-matrix.md` rows updated from them. |
| R7 | Close the lock NOT_RUN rows that a fixture can reach: mouse look and wheel zoom while locked (with Deskflow off), dialogue/load/F8 interrupting a held lock, auto-retarget with two hostiles. | Orchestrator | Route or owner rows with trace evidence of look input while locked. |
| R8 | Selection activation breadth: one bounded route over a door, a container, a loose item, an NPC and an owned item (prompt only), with LOS through a wall; reconcile the note's acceptance table with the PR body (Windows build, co-load, DLL absent). | Orchestrator | Route report; note and PR body agree. |
| R9 | Record hygiene: #35 header and body to say D028 GO; #34 and #37 bodies to name their actual last code commits and DLL hashes; cursor PR body's opening line and results row to match its own later sections; fixed-height pre-run table marked superseded. | Orchestrator | Bodies and notes consistent with this gate. |
| R10 | Matched performance sample on the combined candidate: standing and turning p95 with every feature off, each on alone, and all on; cursor pick cost and Selection scan cost in one dense exterior cell; memory over 10 minutes. | Orchestrator | Table in `performance.md` form, one build, one host, same route. |
| R11 | Owner plays one bounded **combined representative route** on the candidate from R2 (lock a hostile, cycle to a chest, cursor-activate a door, walk under a listed beam, F6/F7/F8 and a menu in between) and records ACCEPT or itemized REVISE/HOLD. | Owner | Quote recorded in this file's [owner decision](#owner-decision). |

R5, R6, R10 and R11 need the candidate from R2; R2 needs R1. R3, R4, R7, R8 and R9 can proceed now. A reasonable sequence is R1 → R3/R4/R9 (docs and decisions, no game time) → R2 → R6 → R7/R8/R10 → R5 → R11.

## REVISE progress

Kept current as items close. "Done" means the closing evidence named in the table above exists.

| # | State | Record |
| --- | --- | --- |
| R1 | In progress under D035: #33 merged `4142ea3`; #36 merged `3ed2951`; #42 merged `116f9b7` (D029–D035). #34 reviewed PASS through round 7 at `2947baa`, merging on green CI; #37 reviewed PASS through rounds 2–5 with green CI at `c950d8c`, merges after #34 and its own main-merge; #38 code passed round 2 at `d94a91c`, record confirmation running; #41 and #40 under review. | `session-p5/ledger.md`; review logs `session-p5/logs/sr*/` |
| R2 | D031 decided Selection ships in its own archive and the lock overlay in Camera. Packaging implementation is on PR #41 (`cbccd24`): `package.py native --module selection`, three-module assembly, overlay folder with README; CI/review status tracked in the ledger. Combined candidate still needs R1. | [Packaging](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase5/dyl-202-packaging/docs/evidence/p5/packaging.md) (on its branch until merged) |
| R3 | **Done 2026-11-08** | D032 records U, `[` / `]`, B; pad R3 only for lock. Phase 5 section is in `docs/CONTROLS.md`; feature notes record stock-map scans. |
| R4 | **Done 2026-11-08** | D033 records the reworded criterion and three beams. |
| R5 | Open — needs R2 | — |
| R6 | Plan drafted (Sol worker): `session-p5/routes/regression-plan.md`, 16 stage families, all 22 Phase 4 routes mapped, C1–C5 mapping, 69 draft routes (64 200 s of watchdog) in `routes/gen/regression/`. Owner scope decision 2026-11-08 ("Trim to ~39 runs"): skip the UI-only reruns of the byte-identical UI module except the `ui-main` startup sentinel, and skip the 19 union cases; 41 routes kept (≈10.8 h of watchdog), 28 recorded NOT_RUN (owner-waived) — lists in `session-p5/routes/regression-trim.txt` / `regression-skipped.txt`. Runs need R2. | — |
| R7 | Routes drafted (Luna worker): `p5-lock-look`, `p5-lock-interrupts`, `p5-lock-retarget` (two-hostile fixture is a hypothesis) in `routes/gen/revise/`; orchestrator reconciliation remains, and the stage must be built from the merged head (the proposed `46f3072` lock stage predates later lock fixes). | `session-p5/routes/revise-routes.md` |
| R8 | Route drafted (Luna worker): `p5-selection-breadth` (door, wardrobe, owned tankard prompt-only, Hulda, bench, wall LOS); fixture coordinates to reconcile against `vanilla-scan.json`. | `session-p5/routes/revise-routes.md` |
| R9 | **Done 2026-11-08** | #35 body and spike note header record D028 (`6f9834c`); #34 and #37 bodies name last code commits and DLL prefixes; #36 body and selection note reconciled (build PASS, co-load PARTIAL, DLL-absent by construction; `d133dae`); #38 body opening line and owner row corrected; fixed-height pre-run table marked superseded (`4e9d8e7`). |
| R10 | Protocol drafted in the R6 plan (frame matrix ×7, dense cell ×3, resource ×3). Runs need R2. | — |
| R11 | Open — needs R2, R5, R6 | — |

## Documentation obligations

This PR carries the Phase 5 sections in [STATUS](../../STATUS.md), the [release checklist](../../RELEASE_CHECKLIST.md), the [runtime matrix](../../compatibility/runtime-matrix.md) and the Phase 5 controls in [CONTROLS](../../CONTROLS.md). D025 and D029–D035 are already recorded on main; the acceptance matrix update remains part of R6.

## Out of scope, unchanged

Automatic merges and public release (D023), Stage 2 walls and ceilings, cursor-aimed projectiles, provider forks, click-to-move, turn-based or party commands, a new camera controller, and any expansion of the four-phase MVP claim.

## Owner decision

**REVISE — owner decision 2026-11-08 (D030).** The owner ruled, verbatim: “REVISE, do R1–R11”. All eleven items are required before the Phase 5 ACCEPT question is put again. This is a ruling on the gate, not a combined play session; hands-on evidence remains the per-feature sessions above (keyboard/mouse; controller for target-lock R3 only). D031–D034 record the packaging, bindings, fixed-height criterion and regression-scope choices; D035 records the merge process:

| Item | Owner choice (2026-11-08) |
| --- | --- |
| R2 packaging | **Selection ships in its own archive** (`SkyCRPG_Selection`); the lock indicator overlay ships inside Camera's archive. Camera and UI remain independent; cursor mode needs Camera and Selection. (D031) |
| R3 keyboard | **U** toggles target lock; **[** cycles next and **]** previous. (D032) |
| R3 controller | **R3 short press lock / 0.4 s hold native POV only** (D027/D032). Pad cycle, target switch and cursor stay unbound in Phase 5. |
| R4 fixed height | Criterion reworded to **“no additional beam-induced pull-in, subject to enclosure”**; 2026-11-03 owner acceptance closes it under this wording. Three beams stay listed (`0602A6`, `0602AF`, `0616EA`). (D033) |
| R6 regression scope | Keep 41 routes; skip byte-identical UI-only reruns except the `ui-main` sentinel and skip 19 union cases; record 28 as NOT_RUN, owner-waived. (D034) |
| R1 merge process | Per-PR green CI and Astra review; PASS permits merge, CHANGES loops through fixes, review, CI and re-review. (D035) |

D029 records cursor defaults. DYL-202 stays In Progress; Phase 5 is not accepted. R1 merges follow D035 and its recorded owner authorization.
