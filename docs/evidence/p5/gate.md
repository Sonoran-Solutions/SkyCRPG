# DYL-202 / P5-06 — Phase 5 gate

**REVISE — owner decision 2026-11-08 ("REVISE, do R1–R11", D030).** The orchestrator first wrote the recommendation on 2026-11-08; the owner then ruled REVISE on that date. The dated chronology and subsequent owner decisions are recorded below and in D030–D035. Nothing in this record turns a NOT_RUN or FAIL row into a pass.

What REVISE means here: each feature has bounded in-game evidence and owner feedback as detailed below, the spike has its owner ruling (D028), and the owner decisions D026–D040 are recorded. The Phase 5 combined candidate is recorded under R2; the Phase 4 regression and C1–C5 rerun remain outstanding. Controller evidence exists for target lock R3 in owner-lock-3 (D027); the controller cursor reticle remains NOT_RUN. R5, the combined controller pass on candidate 8, has the owner's PASS with per-step log limitations ([R5 owner record](owner-r5.md)). The hands-on R11 route was waived and R11 ruled PASS under D040. Phase 5 remains REVISE. The Phase 5 controls table is present in `docs/CONTROLS.md`; its entries and remaining unrun coverage are listed below. The Phase 4 ACCEPT is recorded by D025 and merged in #33. These remaining work items are listed in [REVISE items](#revise-items). A HOLD is not warranted: no feature is blocked by an unresolved engine or provider problem, and the walking result is accepted under the owner's reworded fixed-height criterion in D033.

## Candidate identity

**Recommendation-time base (snapshot 2026-11-08):** main `20e215d` (merge of #32, Phase 4 final docs; [CI 41462292724](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41462292724) PASS). This identity table and the sections from here through [Performance as measured](#performance-as-measured) record the gate draft as written at recommendation time; their statements, such as NOT_RUN combined starts or missing Phase 5 frame and memory measurements, describe that date. Current evidence is the dated [REVISE progress](#revise-progress) section and the R6/R10 [regression record](regression.md).

**Current R2 candidate (2026-11-09):** c8, main `53410cdc365999bc154fd5897fac3d6a0be33bcd`, all-module CI `42679051460` PASS. Its Camera archive SHA-256 is `fc392483591e1469755ada847031c1264a98f6e78ee450436d34135d9a188877`; Camera DLL prefix `36638453`. Candidate 8 Selection archive SHA-256 is `d2481d281c579841e8f3e1e135068ffe0a0ba9bf66764c5c16aac92e2d1232c5`; reused UI rc1 archive SHA-256 is `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb` (UI/shared unchanged). Candidate c1 `890a5da` remains the stage-run candidate; the candidate sequence and hashes are in [regression.md](regression.md). The earlier recommendation-time base was main `20e215d` (merge of #32, Phase 4 final docs; CI `41462292724` PASS).

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

The Camera DLL containing target lock, fixed height and cursor code was staged for the cursor work, but the staged INI kept the repository defaults: the configuration preserved from owner-cursor-1 (`session-p4/owner-cursor-1/after-owned-files/33`, SHA-256 `8600e14a…`) shows `[TargetLock] bEnabled=0`, `[ThinOccluder] bEnabled=0` and `[CursorMode] bEnabled=1`. The override script enabled only cursor mode; the sheet's step F.8 ("target lock key while on") therefore exercised no lock and is NOT_RUN as a lock interaction. **No run — harness route or owner session — has had two Phase 5 features enabled together.**

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
| **DYL-197 target lock** ([note](target-lock.md), #34) | 8 PASS (feature-off routes; lock, lease release/retake, stable pitch and yaw in owner sessions); 2 FAIL on earlier builds (`6b42f35`, `82b2121`: lease never released), fixed in `63e992e`; later builds green. R7 `p5-r7-c` measured mouse yaw, held pitch and screenshot-based wheel zoom; `p5-r7-d` measured menu/F8 interruption. Auto-retarget, dialogue/load interruption remain NOT_RUN; target switching remains unbound. Menu and F6 interruption PASS owner in lock-2; F7 recenters and does not interrupt target lock. | Owner sessions on 2026-11-02: lock, lease and R3 results recorded; D027 records controller R3 short/hold approval. | Owner framing judgment on R7 mouse look/zoom, auto-retarget, target switching, dialogue/load interruption and review fixes' runtime behavior remain unverified. | Lock accepted as felt; D032 sets U and D031 sets overlay delivery. REVISE items R5 and R7. |
| **DYL-198 selection** ([note](proximity-activation.md), #36) | 4 PASS on `1506e6d` unattended (hook installs with SmoothCam, Bannered Mare bench prompt, cycling 0,1,2,0, scan limits); log check 57 substitutions in 11 353 picks. Later cursor routes exercised the same plugin at `675db0a7…`/`6d8b12e2…` (hover, activation E on the bench, empty-floor E suppressed). NOT_RUN: activation breadth, LOS through walls, combat/sneak/telekinesis/grab, controller, DLL-absent on final build, BTPS-present refusal in game. | None on its own; cursor owner session used it. | Activation breadth, LOS, combat/sneak and controller coverage remain NOT_RUN; packaging (D031) and keyboard bindings (D032) are decided. | Works as the activation path used in cursor routes and owner session. REVISE items R5, R6, R8. |
| **DYL-199 occlusion spike** ([note on its branch](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase5/dyl-199-occlusion-spike/docs/evidence/p5/occlusion-spike.md), #35) | final2-2204 on `6667bf5`: 5 PASS (height held 60 s, hook ≈0.10 ms/frame, fade 0.0 % p95 at the frame cap, filter inert without a match, observe routes), 1 PARTIAL (boundary walk, fail-closed drop at 14.8 s), 1 FAIL (automatic classifier discards real enclosure), 2 NOT_RUN (culling above a real ceiling; Camera Studio/Disocclusion binaries not acquired). | The m-* see-through look approved (2026-11-02). | Boundary-walk PARTIAL, classifier FAIL and two NOT_RUN rows remain as above; the spike is evidence-only. | Closed by **D028**; evidence remains on its branch. No R9 decision remains open. |
| **DYL-201 fixed height** ([note](fixed-height-camera.md), #37) | `50a3dd2`: 7 PASS (hooks, standing, transparent listed beam, enclosure kept, F8/POV/menu restore, feature off, budget); 2 FAIL (character framing at corridor/rock ceiling, and walking pull-in under the old wording). `c10e6bc`: 5 PASS; walking result unchanged, F8 return one-sample drop and beam remains faded during paused console. NOT_RUN: low-space framing, load/cell/unload restores as a distinct test, target lock with fixed height, controller, frame budget on `c10e6bc`. Console close, Tab close and F8 restore/resume behavior follow the evidence note; fixed height resumes automatically after menu/F8 by design. | 4 PASS, owner, 2026-11-03: “everything passed and I approve”; walking jumps accepted as felt; paused console and F8 drop accepted. Low-space step skipped. | Remaining NOT_RUN rows are low-space framing, lock with fixed height, controller, separate load/cell/unload route and frame budget on `c10e6bc`. D033's owner wording is recorded. | Closed under the D033 wording “no additional beam-induced pull-in, subject to enclosure.” REVISE items R5 and R6. |
| **DYL-200 cursor mode** ([note](cursor-mode.md), #38) | route-cursor-8 on `4946c92`: 6 PASS (on-screen cycling, latch drift/timeout, reverse cycle skipping hidden candidates, no refused cycle, hover regression; cost median 6 µs, max 65 µs); 1 NOT_RUN (pointer-movement latch release fixture inconclusive). Owner-cursor-1 ran with CursorMode on and TargetLock and ThinOccluder off (preserved INI `8600e14a…`); it was owner-only with capped logs, so its F.8 step is NOT_RUN as a lock interaction. Seven earlier route sets found and fixed five defects; senior merge review: code PASS round 2 at `d94a91c`, record confirmed PASS round 3. | Owner, 2026-11-08: “Finished, everything passed”, “All of A–G”. Four-minute session; logs corroborate A, B, one cycle of D and pick cost; F.6 quickload remains NOT_RUN. D029 records B/no pad/65/48/12/on-screen-only cycling and Selection's shipped external-target default. | Controller reticle, other resolutions, controlled frame-cost sample, quickload while active, pointer-movement fixture. | Accepted as felt on keyboard/mouse. D029 records defaults. |

## P5-06 acceptance criteria

*At recommendation time (2026-11-08). Current R6/R10 evidence: [regression record](regression.md) and [REVISE progress](#revise-progress).*

| Criterion | Status | Evidence and gap |
| --- | --- | --- |
| 1. Every enabled feature's scoped acceptance routes PASS, or each non-PASS carries an owner-ratified disposition | **REVISE** | Per-feature routes PASS or are owner-accepted as above. Non-PASS rows carrying an owner disposition: fixed-height walking (accepted as felt), paused console, F8 one-sample drop, cursor latch kept off-screen. Non-PASS rows **without** one: fixed-height "character stays in view" at corridor/rock ceiling; lock mouse-look/zoom unresolved; cursor F.6; the controller rows other than R3 lock/hold (owner-lock-3 PASS, D027): pad cycle, cursor reticle, switching and a combined controller pass. |
| 2. Every new feature independently disable-able; Camera-only and UI-only work without the other; no required provider masquerades as optional | **PARTIAL** | Feature-off routes PASS per feature (`p5-cam-off-*` for lock, `p5-cursor-off`, fixed-off variant). Selection absent: PASS on an earlier stage per #36, NOT_RUN on the final build. Cursor mode refuses cleanly without Selection or with `bAllowExternalTargets=0` (logged). UI-only and combined starts on a Phase 5 build: **NOT_RUN**. D031 decides Selection ships in its own archive. |
| 3. Final binding table works on both devices; menus/F6/F8/native POV take precedence; no duplicate native action or stranded state | **NOT_RUN as a table** | D032 approves U and `[` next / `]` previous; pad cycle/switch/cursor remain unbound. Per-feature evidence: lock menu/F6 PASS owner, dialogue/load/F8 NOT_RUN, and F7 does not interrupt lock; cursor exits on toggle, Tab/menus, console, F6/F8, weapon draw, dialogue and load per the routes and owner sheet (F.6 quickload NOT_RUN); the designed exit on target lock is **NOT_RUN** (no session has had both features enabled); fixed height restores and resumes after menu/F8 by design. The table exists in `docs/CONTROLS.md`, but a combined-device check remains NOT_RUN. See [binding table](#binding-table-as-tested). |
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
| Native POV | F key | R3 hold (0.4 s) | Controller R3 hold: PASS, owner (lock-3, controller-only session). Keyboard F with the lock features on: NOT_RUN. |
| Menus, dialogue, load, death | native | native | Cursor exits on menu/dialogue/load (death per cursor route contract); lock menu PASS, dialogue/load NOT_RUN; fixed-height restores on menu/load. Combined NOT_RUN. |

One input consumer per event was reviewed in source for each feature (lock's input tracker, Selection's edge FIFO, cursor's captured look deltas) and not contradicted in any run; it has not been tested with all three enabled.

## Performance as measured

*At recommendation time (2026-11-08). Current R6/R10 evidence: [regression record](regression.md) and [REVISE progress](#revise-progress).*

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
| owner-p5-r5 (R5 combined controller pass) | 2026-11-09 | `53410cd` (candidate 8) | 8BitDo controller | "everything passed"; F7 reaffirmed "the F7 recent passed"; per-step log limitations in [owner-r5.md](owner-r5.md) |
| Combined representative route (R11) | — | — | — | Hands-on R11 waived; R11 ruled PASS by the owner under D040. Phase 5 remains REVISE. |

## Tooling and process notes

- The vinput in-process automation spike was dropped: with focus-faking and isolation off, Skyrim still crashed at `SkyrimSE.exe+0xE128B2` one second after real window activation (DirectInput buffered-read interposition). [Record](https://github.com/Sonoran-Solutions/SkyCRPG/blob/spike/vinput-proton/docs/evidence/p5/vinput-spike.md) on `spike/vinput-proton`; #39 closed unmerged. Routes keep the uinput/xwd harness.
- `session-p4/runtime_watchdog.py` does not restore the profile after a normal quit; owner sessions need a manual restore (done each time; recorded in the ledger).
- Trace caps (`iTrace`, `iDiagnosticLimit`, Selection event budget) cut owner sessions short in the logs; raise them for owner runs.
- A fix scoped by a feature flag needs routes in the other flag state; the lock-off F6 regression was not covered by lock-enabled-only reruns.
- Probe assertions, not step completion, establish F6 cycles; assert camera states 0 then 9.

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
| R2 | D031: Selection ships in its own archive, the lock overlay inside Camera's. Packaging merged in #41 (`890a5da`): `package.py native --module selection`, three-module `assemble`, overlay folder with README — [packaging note](packaging.md). The current all-module main build and local three-module candidate assembly are complete on candidate 8; hashes and run are recorded below. | `docs/evidence/p5/packaging.md` |
| R3 | Phase 5 controls and binding decisions recorded in `docs/CONTROLS.md`; D032 approves U, `[` / `]`, and leaves pad cycle/switch/cursor unbound. | Orchestrator | **Done in this PR**; R5 combined controller results are recorded in [owner-r5.md](owner-r5.md). |
| R4 | Fixed-height criterion and curated beam list. | Owner | **Decided by D033**: “no additional beam-induced pull-in, subject to enclosure”; three beams retained. |
| R5 | Original R5 requirement: **Controller pass** on the combined candidate: recheck lock R3 short press / 0.4 s native-POV hold, F6/F7/F8 and native POV priority, and confirm no stranded state. D032 leaves pad cycle, target switch and cursor unbound for Phase 5. | Owner (hands-on), orchestrator (profile, logs) | Owner PASS recorded in [owner-r5.md](owner-r5.md), with per-step log limitations. |
| R6 | **Regression on the combined candidate**: 41 owner-approved routes across Camera/UI/combined and performance profiles, as listed in [the R6/R10 regression record](regression.md). Retest C1–C5 to their current scoped definitions and record status honestly (C1/C4 pitch FAIL may remain). | Orchestrator | Route reports and feature evidence; `acceptance-matrix.md` rows updated from them. |
| R7 | Close the lock rows that a fixture can reach: owner framing judgment on mouse look/zoom, dialogue/load interruption, and auto-retarget with two verified hostiles. | Orchestrator | R7 unattended routes measured mouse yaw, held pitch, screenshot-based zoom, menu/F8/console interrupts and target-death lock release; remaining rows are listed in the progress table and target-lock note. |
| R8 | Selection activation breadth: one bounded route over a door, a container, a loose item, an NPC and an owned item (prompt only), with LOS through a wall; reconcile the note's acceptance table with the PR body (Windows build, co-load, DLL absent). | Orchestrator | Route report; note and PR body agree. |
| R9 | Record hygiene: #35 header and body to say D028 GO; #34 and #37 bodies to name their actual last code commits and DLL hashes; cursor PR body's opening line and results row to match its own later sections; fixed-height pre-run table marked superseded. | Orchestrator | Bodies and notes consistent with this gate. |
| R10 | Matched performance sample for R2: standing and turning p95 with features off and individually enabled, plus cursor/all rows; dense Selection/Cursor/All and resource Baseline/Off/All. Figures are recorded with each source candidate in [regression.md](regression.md); the display-cap caveat remains. | Orchestrator | Candidate measurements and pending rows in [the R6/R10 regression record](regression.md). |
| R11 | Owner plays one bounded **combined representative route** on the candidate from R2 (lock a hostile, cycle to a chest, cursor-activate a door, walk under a listed beam, F6/F7/F8 and a menu in between) and records ACCEPT or itemized REVISE/HOLD. | Owner | Original hands-on requirement waived under D040; R11 ruled PASS by the owner. Phase 5 ACCEPT remains a separate ruling. |

Original planning sequence (2026-11-08): R5, R6, R10 and R11 need the candidate from R2; R2 needs R1. R3, R4, R7, R8 and R9 can proceed now. A reasonable sequence is R1 → R3/R4/R9 (docs and decisions, no game time) → R2 → R6 → R7/R8/R10 → R5 → R11. Current status: R5 owner PASS recorded; R11 hands-on route waived and R11 ruled PASS under D040.

## REVISE progress

Kept current as items close. "Done" means the closing evidence named in the table above exists.

| # | State | Record |
| --- | --- | --- |
| R1 | **Feature PRs #34/#36–#38 and packaging #41** remain merged as previously recorded. Additional F6 fix chain merged 2026-11-09 under D035, each after Astra senior-review PASS and green Windows CI: **#44 `8dbc5f9f72567166064579a4c3e8e6a51c9266f4`** (cursor menu-edge fix; merge run `42575458147`), **#46 `e4230c654cd0ecb57957bc024e477ed49ed9260f`** (F6 return serviced under lock; `42593738041`), **#47 `d405a0643fb1a6d415cca94f286f16c44c42f384`** (idle-token, Main entry and lock-independent menu cancellation; `42608605103`), **#48 `b2c79609dfb0ac47e2115406dcae1cf618f4df14`** (entry with pending refresh; `42621542425`), **#49 `d4ac5dcd79f3a918c08d5e148f71057e79bc85cb`** (serialized refresh drain; runs `42636833068` and `42637840878`; the latter includes the wiring audit). #49's two Astra MINORs were fixed in `3cdf2d0`. Each later candidate's all-module build is recorded in [regression.md](regression.md). PR #49 merged as `d4ac5dc` after review PASS and green builds. PR #50 merged as `ecfd90c` after review PASS and green Windows CI (runs `42656449958`, `42657380868`). PR #51 merged as `53410cdc365999bc154fd5897fac3d6a0be33bcd`: Astra PASS at `2bb323b`, documentation MINOR fixed in `1552f97`, then Windows build run `42672432631` green at `1552f97` after the GitHub Actions outage (18:12–18:45Z) ended; the `2bb323b` run `42671603448` never started a job during the outage. These post-owner-session fixes have route evidence as recorded; no owner acceptance is claimed. | `session-p5/ledger.md`; review logs `session-p5/logs/sr*/` |
| R2 | **Candidate 8 recorded.** Main `53410cdc365999bc154fd5897fac3d6a0be33bcd`, GitHub Actions run `42679051460` (`module=all`) green; Camera archive `fc392483591e1469755ada847031c1264a98f6e78ee450436d34135d9a188877`, Camera DLL prefix `36638453`; Selection archive `d2481d281c579841e8f3e1e135068ffe0a0ba9bf66764c5c16aac92e2d1232c5`; reused UI rc1 archive `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb`. Candidate c1 `890a5da` remains the original stage-run candidate. | `session-p5/dist-p5-candidate-8/candidate.json`; see [regression record](regression.md). |
| R3 | **Done 2026-11-08** | D032 records U, `[` / `]`, B; pad R3 only for lock. Phase 5 section is in `docs/CONTROLS.md`; feature notes record stock-map scans. |
| R4 | **Done 2026-11-08** | D033 records the reworded criterion and three beams. |
| R5 | **Owner session `owner-p5-r5` on candidate 8, 2026-11-09 (~4.9 min, restore PASS), 8BitDo controller (owner).** Owner: “everything passed”. Logs corroborate R3 lock/unlock (ten lock-on transitions, three target handles, lease released only after TDM's unlocked acknowledgement), lock then F6 entry and return, lock then Start-menu release with no re-lock, and a clean quit. The R3 hold (native POV) is owner-reported and not independently corroborated; F8 has no log coverage. F7 recenter left no recenter-request entry; the owner reaffirmed it passed (“the F7 recent passed”), so it is recorded as owner PASS with the log gap noted. | [R5 owner record](owner-r5.md) |
| R6 | **Final disposition uses each route’s latest build.** D037 ran 12 lock-enabled routes on candidate 6: 12/12 PASS after #49. D038 ran those 12 plus 12 lock-off routes that press F6 on candidate 7: 21/24 PASS, 3 FAIL solely on missing precision-trace event 11. D039 reran the seven `iPrecisionTrace` routes on candidate 8: 7/7 PASS; event 11 with `camera=9/0` appears in six, while combined-main passed with probe 9>0 and no event 11 line because trace budget was spent before F6 and the route does not assert it. The other 17 routes keep their recorded build. See [regression record](regression.md) and [Defects found by R6](#defects-found-by-r6). | [Regression record](regression.md); `session-p5/ledger.md`; reports under `/srv/storage/Projects/SkyCRPG/session-p4/autotest/runs/batch-p5-r6-*/` |
| R7 | **Unattended routes recorded.** `p5-r7-a` had no target in view and no lock evidence (NOT_RUN). `p5-r7-b` engaged on `001106F9`, 877 locked trace rows; auto-retarget NOT_RUN. `p5-r7-c` engaged on `001106FD`, 1,122 locked rows; mouse yaw, held pitch and wheel zoom were measured (zoom by screenshots), while owner framing judgment remains NOT_RUN. `p5-r7-d` runner FAIL only at final malformed `since` mark; preceding 42 steps PASS, and menu/F8/console interruption, no self-relock and fresh U relock were measured. `p5-r7-e` measured target death ending lock; auto-retarget NOT_RUN because the second spawn was not verified as a live target in range. Dialogue and load interruption remain NOT_RUN. All five restores PASS; game folder clean afterwards. | [Target-lock evidence](target-lock.md#unattended-routes-2026-11-09); reports under `/srv/storage/Projects/SkyCRPG/session-p4/autotest/runs/batch-p5-r7-{a,b,c,d,e}-00/` |
| R8 | **Unattended breadth route recorded.** `p5-r8-a` BLOCKED after `coc` with loading fade/Fader Menu; `p5-r8-b` runner PASS 2/2. Bench and door prompts/candidates were measured. Wardrobe was not isolated; a loose owned-item prompt was observed. Tankard pose invalid (NOT_RUN), Hulda was not logged as selected; the stool was selected (NPC NOT_RUN), and no LOS rejection logged (LOS-through-wall NOT_RUN). Radius 180, `capped=0`; across the full `p5-r8-b` Selection log, sampled scan `cost_us` ranged 127–420 µs; sampled `pick_cost_us` ranged 4–36 µs. | [Proximity-activation evidence](proximity-activation.md#unattended-routes-2026-11-09); reports under `/srv/storage/Projects/SkyCRPG/session-p4/autotest/runs/batch-p5-r8-{a,b}-00/` |
| R9 | **Done 2026-11-08** | #35 body and spike note header record D028 (`6f9834c`); #34 and #37 bodies name last code commits and DLL prefixes; #36 body and selection note reconciled (build PASS, co-load PARTIAL, DLL-absent by construction; `d133dae`); #38 body opening line and owner row corrected; fixed-height pre-run table marked superseded (`4e9d8e7`). |
| R10 | **Measured on candidates 5–7 where reruns cover the row.** Candidate 7 lock frame standing/turn p95 `16.794/16.979` ms; all frame `16.832/16.790` ms; resource-all `16.937/16.971/16.950/16.994` ms, over33 `0/0/29/13`, window private commit `1.688→2.133 GB` (601 samples). Candidate 5 and 6 figures are kept alongside them in [regression.md](regression.md). Lock-off frame rows (frame-off/fixed/selection/baseline) were last measured on `890a5da`, before #50 added the per-frame lock-off camera hook (review: one atomic check per idle frame); not rerun under D038/D039. | [Regression record](regression.md); `session-p5/ledger.md` (2026-11-09 entries) |
| R11 | **Ruled PASS by the owner without a hands-on session (D040).** Quoted: “We can skip my playtest for R11 and rule it as a PASS. I've watched the automated tests where it's done these tests and I approve”. The combined smoke route has no activation key press, so cursor activation of a door rests on this ruling. | [DECISIONS D040](../../DECISIONS.md) |

## Documentation obligations

### Defects found by R6

- **MAJOR — lock configuration refresh never completed (DYL-197).** Candidate 5 showed the refresh staying pending and blocking F6 entry or return. PR #49 (`d4ac5dc`) fixed the serialized refresh drain; candidate 6 lock-enabled reruns passed 12/12. Probe-confirmed F6 intervals and returns, `PrecisionReturn reason=returned` in all five F6 routes, zero refresh-pending and zero lock refusals are recorded in [regression.md](regression.md).
- **MAJOR — lock-off F6 never entered first person on candidates 4–6 (DYL-197).** Introduced by c655b93 / #47 and found in `p5-r6-f6off` on candidate 6: parity-off, combined-main and cam-malformed-ini FAIL; cam-disabled PASS; cam-obstruction BLOCKED by orchestrator SIGTERM; seven NOT_RUN. Exact batch-stop facts from the ledger: the first SIGTERM (09:17:21, via the shell/timeout wrapper) DID reach `run_batch`; route 04 cam-obstruction was BLOCKED “interrupted by operator (SIGTERM); cleanup ran” at 16:17:40Z, restore rc 0 (Plugins dir absent), routes 05-11 were NOT_RUN “batch stopped”. The batch had already stopped before the owner rejected the second stop and said Continue; later “still running” checks were `pgrep -f` matching the watcher's own command line. Before `p5-r6-f6off`, no lock-off F6 route had run on candidates 3–6. Root cause: lock-off F6 published `precisionReturn.requestEntry()`, but the deferred SKSE task ran off Main in 120/123 executions and the camera-call servicing hook was installed only when target lock was enabled; diagnostics were gated on that flag too. PR #50 (`ecfd90c`) services the path through the per-frame camera hook. On candidate 7, all 11 Camera-enabled lock-off routes showed F6 entry; entry-and-return coverage varies by route, and combined-main and combined-ui-disabled recorded `9>0` only; cam-disabled passed with native F6. See [regression.md](regression.md).
- **MINOR diagnostics — precision-trace claim event 11 missing (DYL-197).** Missing since c655b93 / #47, found on candidate 7; #51 restores the event when tracing is enabled. Candidate 8 verified event 11 with `camera=9/0` in six trace routes; combined-main passed with probe 9>0 and no event 11 line because trace budget was spent before F6 and the route does not assert it. The three candidate 7 FAILs were trace assertions; F6 worked (probe 0>9>0, entry-claimed). The debug sample never gates F6.
- **Probe lesson.** Probe assertions, not step completion, establish F6 cycles; route checks require `camera_state` 0 then 9.
- **Feature-flag lesson.** A fix scoped by a feature flag needs routes in the other flag state; include lock-off F6 routes when validating lock-path changes.

The earlier cursor toggle queued from native-menu text remains fixed in #44: menu-owned presses are dropped at the key edge. Its candidate 2 `on-main` rerun resolved the prior BLOCKED route; history remains in [regression.md](regression.md).
The Phase 5 gate remains **REVISE (D030)**. The route detail, candidate hashes, performance measurements, and D036 rerun statuses are in [regression.md](regression.md).

This PR carries the Phase 5 sections in [STATUS](../../STATUS.md), the [release checklist](../../RELEASE_CHECKLIST.md), the [runtime matrix](../../compatibility/runtime-matrix.md) and the Phase 5 controls in [CONTROLS](../../CONTROLS.md). D025 and D029–D039 are recorded here; the acceptance matrix update remains part of R6. The #48 follow-up to put the source wiring audit in the repository and run it in Camera CI is **done in `3cdf2d0`** (`lock_refresh_wiring_test.py` runs in the Camera portable step).

## Out of scope, unchanged

Automatic merges and public release (D023), Stage 2 walls and ceilings, cursor-aimed projectiles, provider forks, click-to-move, turn-based or party commands, a new camera controller, and any expansion of the four-phase MVP claim.

## Owner decision

**REVISE — owner decision 2026-11-08 (D030).** The owner ruled, verbatim: “REVISE, do R1–R11”. All eleven items are required before the Phase 5 ACCEPT question is put again. This is a ruling on the gate. Hands-on evidence includes the earlier per-feature sessions (keyboard/mouse; controller for target-lock R3 only) and the combined R5 controller session on candidate 8, with per-step limitations recorded in [owner-r5.md](owner-r5.md). The hands-on R11 route was waived under D040; Phase 5 remains REVISE. D031–D034 record packaging, bindings, fixed-height criterion and initial regression scope; D035 records merge process; D037–D039 record the successive R6 rerun scopes; D040 records the R11 waiver:

| Item | Owner choice |
| --- | --- |
| R2 packaging | **Selection ships in its own archive** (`SkyCRPG_Selection`); the lock indicator overlay ships inside Camera's archive. Camera and UI remain independent; cursor mode needs Camera and Selection. (D031) |
| R3 keyboard | **U** toggles target lock; **[** cycles next and **]** previous. (D032) |
| R3 controller | **R3 short press lock / 0.4 s hold native POV only** (D027/D032). Pad cycle, target switch and cursor stay unbound in Phase 5. |
| R4 fixed height | Criterion reworded to **“no additional beam-induced pull-in, subject to enclosure”**; 2026-11-03 owner acceptance closes it under this wording. Three beams stay listed (`0602A6`, `0602AF`, `0616EA`). (D033) |
| R6 regression scope | Keep 41 routes; skip byte-identical UI-only reruns except the `ui-main` sentinel and skip 19 union cases; record 28 as NOT_RUN, owner-waived. (D034) |
| R1 merge process | Per-PR green CI and Astra review; PASS permits merge, CHANGES loops through fixes, review, CI and re-review. (D035) |
| R6 candidate 6 scope | After #49 fixed lock refresh, rerun only 12 lock-enabled routes; the other 29 retain recorded builds. Alternative considered: rerun the full 41-route set. (D037) |
| R6 candidate 7 scope | After candidate 6 exposed lock-off F6 regression and #50 fixed it, rerun 12 lock-enabled plus 12 lock-off F6 routes. Alternative: retain the earlier 12-route lock-enabled scope. (D038) |
| R6 candidate 8 scope | After #51 restores claim trace, rerun only seven routes with `iPrecisionTrace` enabled: on-main, on-ui-disabled, parity-off, combined-main, cam-malformed-ini, cam-obstruction, combined-ui-disabled. Alternative: rerun all 24 D038 routes. (D039) |
| R11 combined route | 2026-11-09: owner waives the hands-on R11 route and rules it PASS on the automated route evidence: “We can skip my playtest for R11 and rule it as a PASS. I've watched the automated tests where it's done these tests and I approve” (D040). This is not the Phase 5 ACCEPT ruling. |

D029 records cursor defaults. DYL-202 remains open (its Linear state is kept at In Progress by the orchestrator; a GitHub integration may auto-close it on merged branches, which does not change the ruling); Phase 5 is not accepted. R1 merges follow D035 and its recorded owner authorization.
