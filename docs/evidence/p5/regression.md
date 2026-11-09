# Phase 5 regression set (R6/R10), candidates 890a5da through 53410cd

## Candidate provenance

Candidate manifests are under `session-p5/dist-p5-candidate[-2…-8]/candidate.json`; each all-module build passed and the manifests record archive hashes. UI rc1 was reused in each candidate because `modules/ui` and `shared` were unchanged from `6b4c0fc`.

| Candidate | Source commit | All-module CI run | Camera archive SHA-256 | Change from prior candidate |
| --- | --- | ---: | --- | --- |
| c1 | `890a5da` | `42557859729` | `0bd64ec6772b92fe274b3720787a9085fc7421a3d33a2f97e3a4dc3f533605d7` | Stage-run candidate (`890a5da`); initial candidate in this record. |
| c2 | `8dbc5f9` | `42576728526` | `1bafd28abcba5994d93c452df8ee237cbbe5f040255ac5546df2d06c85c0248b` | PR #44 cursor fix; D036 cursor-affected reruns. |
| c3 | `e4230c6` | `42595315743` | `95d29bb6ac327d0bd8e42e41d74d6e586a2a40d000d3b30d53577f3d1f05d09e` | PR #46 services the F6 return while target lock is enabled. |
| c4 | `d405a06` | `42610951416` | `e1b545b2a6ec40e44c83735568416a82d7952ef3301e540bd28c44c0a2f4883a` | PR #47 preserves return tokens over idle frames, enters on Main, and cancels on menu focus regardless of lock state. |
| c5 | `b2c7960` | `42624091357` | `2b6b6bb962043e43c219db85e48e10c93a112efc2b001935902b3837fcbcb7b7` | PR #48 allows F6 entry while lock configuration refresh is pending. |
| c6 | `d4ac5dc` | `42641017814` | `eb31d8e7e986f02b95e51f3de627e8cadb17f75b505d77ffec98dea156bc55d3` | PR #49 drains refresh reads on the serialized timer worker; Main applies scalars. Console-close and Loading-Menu-close are refresh producers. Camera CI runs the source wiring audit; log-once read-failure diagnostic fixed in `3cdf2d0`. |
| c7 | `ecfd90c` | `42660266908` | `962138f228758099e4d8e68b0c8395b850e2b6fceacee036ea0be6ba8f5c223e` | PR #50 services lock-off F6 through the per-frame camera hook. Candidate 7 routes exposed the trace-only claim-event regression fixed by PR #51. |
| c8 | `53410cd` | `42679051460` | `fc392483591e1469755ada847031c1264a98f6e78ee450436d34135d9a188877` | PR #51 restores precision-trace claim event 11. |

Candidate 7 Selection archive SHA-256 is `30e91bbbdbfe5f5b94d85aecc51f406b8cf00b9086432b69394ad7e4ffaa6365`. Candidate 7 Camera DLL SHA-256 begins `91a6555c`. Candidate 6 Selection archive SHA-256 is `643601289964ce09337121fb460a143b8a72e889bc8d5cd58eff00c44d2540fc`. Reused UI rc1 archive SHA-256 is `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb`. Candidate 6 Camera DLL SHA-256 begins `5630247b`.

Stages were built with `make_stages.py --candidate`. Runner: session-p4 autotest runner, unattended; one Skyrim at a time; profile/save restoration after every route. **Runner PASS means asserted checkpoints passed on that build; it is not feature acceptance.** Plan stages are 1, 2, 4, 5, 6, 7, 8 and 9. D036 directed cursor-affected route reruns after #44. D037–D039 set successive owner-approved scopes: 12 lock-enabled routes on candidate 6, 24 lock-enabled/lock-off F6 routes on candidate 7, then seven trace-enabled routes on candidate 8. The history column preserves prior batch outcomes and Camera builds.

## Approved route results

The seven routes with `iPrecisionTrace` enabled were rerun on candidate 8 under D039; their candidate 7 outcomes remain visible in the history column. Under D037 the 12 lock-enabled routes were rerun on candidate 6; D038 then reran 12 lock-enabled plus 12 lock-off F6 routes on candidate 7. The other 17 approved routes retain their recorded build.

| Route | Plan stage | Latest batch | Gate status | Camera build | Latest runner result | Feature evidence | Earlier batch history |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| `p5-reg-parity-off` | 1 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`, F6 probe 0>9>0>9>0>9. | p5-r6-f6off:FAIL@d4ac5dc (first-person assert); p5-r6-c7:FAIL@ecfd90c (trace event 11); p5-r6-c8:PASS@53410cd; p5-r6-s1:PASS@890a5da |
| `p5-reg-ui-main` | 1 | `p5-r6-s1` | PASS | — | PASS | UI sentinel 19/19 | p5-r6-s1:PASS@ |
| `p5-reg-combined-main` | 1 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; entry-claimed and probe 9>0; trace budget was spent before F6, so no event 11 line is asserted. | p5-r6-f6off:FAIL@d4ac5dc (probe remained 9); p5-r6-c7:PASS@ecfd90c (probe 9>0); p5-r6-c8:PASS@53410cd; p5-r6-s1:PASS@890a5da |
| `p5-reg-cam-disabled` | 2 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; Camera disabled, native F6. | p5-r6-f6off:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c; p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-malformed-ini` | 2 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`, F6 probe 0>9>0>9>0>9. | p5-r6-f6off:FAIL@d4ac5dc (first-person assert); p5-r6-c7:FAIL@ecfd90c (trace event 11); p5-r6-c8:PASS@53410cd; p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-no-mcmhelper` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | missing-provider checkpoints 2/2 | p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-no-smoothcam` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | missing-provider checkpoints 2/2 | p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-no-tdm` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | missing-provider checkpoints 2/2 | p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-obstruction` | 2 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`, F6 probe 0>9>0>9>0>9. | p5-r6-f6off:BLOCKED@d4ac5dc (orchestrator SIGTERM); p5-r6-c7:FAIL@ecfd90c (trace event 11); p5-r6-c8:PASS@53410cd; p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-schema` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | schema refusal checkpoints 2/2 | p5-r6-s2:PASS@890a5da |
| `p5-reg-cam-unsafe-tdm` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | unsafe-TDM refusal checkpoints 2/2 | p5-r6-s2:PASS@890a5da |
| `p5-reg-combined-cam-disabled` | 2 | `p5-r6-s2` | PASS | 890a5da | PASS | combined disabled-camera checkpoints 4/4 | p5-r6-s2:PASS@890a5da |
| `p5-reg-combined-ui-disabled` | 2 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-c8:PASS@53410cd |
| `p5-reg-smoke-cam-lock` | 4 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe 9>7>9>0>9>7>9, returned. | p5-r6-lock6:PASS@d4ac5dc (12/12); p5-r6-c7:PASS@ecfd90c |
| `p5-reg-smoke-cam-fixed` | 4 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe cycle and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-s4:PASS@890a5da |
| `p5-reg-smoke-cam-selection` | 4 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe cycle and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-s4:PASS@890a5da |
| `p5-reg-smoke-cam-cursor` | 4 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe cycle and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-s4:PASS@890a5da; p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-smoke-cam-all` | 4, rerun in 6 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe 9>7>9>0>9>7>9, returned. | p5-r6-lock6:PASS@d4ac5dc (candidate 5 return failure); p5-r6-c7:PASS@ecfd90c |
| `p5-reg-smoke-combined-on` | 4 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe 9>0>9>7>9>7>9, returned. | p5-r6-lock6:PASS@d4ac5dc (candidate 5 return failure); p5-r6-c7:PASS@ecfd90c |
| `p5-reg-cursor-no-selection` | 5 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe cycle and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-s5:PASS@890a5da; p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-cursor-external-off` | 5 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe cycle and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-s5:PASS@890a5da; p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-on-main` | 6 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c; p5-r6-c8:PASS@53410cd |
| `p5-reg-on-cam-disabled` | 6 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; Camera-disabled configuration passed. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-on-ui-disabled` | 6 | `p5-r6-c8` | PASS | 53410cd | PASS | PASS; event 11 reports `camera=9/0`. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c; p5-r6-c8:PASS@53410cd |
| `p5-reg-controls-off` | 6 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe 9>0>9 and entry-claimed. | p5-r6-f6off:NOT_RUN@d4ac5dc (batch stopped); p5-r6-c7:PASS@ecfd90c; p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-controls-all` | 6 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; F6 probe 9>0>9, returned. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-lifecycle-off` | 6 | `p5-r6-rerun` | PASS | 8dbc5f9 | PASS | Candidate 2 lifecycle-off checkpoints PASS. | p5-r6-s6:NOT_RUN@; p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-lifecycle-all` | 6 | `p5-r6-c7` | PASS | ecfd90c | PASS | PASS; lifecycle checkpoints passed. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-frame-baseline` | 7 | `p5-r6-s7a` | PASS | — | PASS | standing/turn p95 16.844/16.906 ms | p5-r6-s7a:PASS@ |
| `p5-reg-frame-off` | 7 | `p5-r6-s7a` | PASS | 890a5da | PASS | standing/turn p95 16.669/16.850 ms | p5-r6-s7a:PASS@890a5da |
| `p5-reg-frame-lock` | 7 | `p5-r6-c7` | PASS | ecfd90c | PASS | Candidate 7 standing/turn p95 16.794/16.979 ms. | p5-r6-lock5:PASS@b2c7960; p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-frame-fixed` | 7 | `p5-r6-s7a` | PASS | 890a5da | PASS | standing/turn p95 16.799/16.796 ms | p5-r6-s7a:PASS@890a5da |
| `p5-reg-frame-selection` | 7 | `p5-r6-s7a` | PASS | 890a5da | PASS | standing/turn p95 16.762/17.117 ms | p5-r6-s7a:PASS@890a5da |
| `p5-reg-frame-cursor` | 7 | `p5-r6-rerun` | PASS | 8dbc5f9 | PASS | Candidate 2 standing/turn p95 16.698/17.038 ms. | p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-frame-all` | 7 | `p5-r6-c7` | PASS | ecfd90c | PASS | Candidate 7 standing/turn p95 16.832/16.790 ms. | p5-r6-lock5:PASS@b2c7960; p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-dense-selection` | 8 | `p5-r6-s89a` | PASS | 890a5da | PASS | 19 summaries; capped=0; max scan cost 212 µs | p5-r6-s89a:PASS@890a5da |
| `p5-reg-dense-cursor` | 8 | `p5-r6-rerun` | PASS | 8dbc5f9 | PASS | Candidate 2 cursor entered; 512 samples (trace cap). | p5-r6-rerun:PASS@8dbc5f9 |
| `p5-reg-dense-all` | 8 | `p5-r6-c7` | PASS | ecfd90c | PASS | Candidate 7 passed; trace samples remain capped at 512. | p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
| `p5-reg-resource-baseline` | 9 | `p5-r6-s89a` | PASS | — | PASS | Baseline standing/turn/inventory/resource p95 16.842/16.888/16.760/16.741 ms. | p5-r6-s89a:PASS@ |
| `p5-reg-resource-off` | 9 | `p5-r6-s89a` | PASS | 890a5da | PASS | Feature-off standing/turn/inventory/resource p95 16.702/16.911/16.709/16.770 ms. | p5-r6-s89a:PASS@890a5da |
| `p5-reg-resource-all` | 9 | `p5-r6-c7` | PASS | ecfd90c | PASS | Candidate 7 p95 standing/turn/inventory/resource 16.937/16.971/16.950/16.994 ms; over33 0/0/29/13; window private commit 1.688→2.133 GB (601 samples). | p5-r6-lock5:PASS@b2c7960; p5-r6-lock6:PASS@d4ac5dc; p5-r6-c7:PASS@ecfd90c |
## R6 and defect record

R6 covers 41 owner-approved routes. D037 reran the 12 lock-enabled routes on candidate 6 after PR #49; the other 29 retained their recorded builds. D038 then reran the 12 lock-enabled plus 12 lock-off F6 routes on candidate 7 after the candidate 6 lock-off regression was found and fixed by PR #50. Candidate 7 had 21/24 PASS and 3 FAIL; the three failures were the trace-only event assertion, not F6 behavior. Under D039, all seven `iPrecisionTrace` routes passed on candidate 8 after PR #51; six report event 11 with `camera=9/0`, while combined-main passed without that line because its trace budget was spent before F6 and the route does not assert it. The other 17 approved routes retain their recorded builds. These scope decisions change reruns only; the gate remains REVISE under D030.

**MAJOR — lock configuration refresh never completed (DYL-197).** On candidate 5, `smoke-cam-all` entered first person but did not return while a lock-configuration refresh remained pending; `smoke-combined-on` behaved the same, and `controls-all` refused entry. PR #49 (`d4ac5dc`) drains refresh reads on the serialized timer worker and applies scalars on Main. Candidate 6 then passed all 12 lock-enabled routes. Probe sequences include `smoke-cam-lock` `9>7>9>0>9>7>9`, `smoke-cam-all` `9>7>9>0>9>7>9`, `smoke-combined-on` `9>0>9>7>9>7>9`, `controls-all` `9>0>9`, and `on-main` `9>0`; each of the five F6 routes logged `PrecisionReturn reason=returned`, all 12 had zero `refresh-pending` and zero lock refusals, and LockRefresh counters advanced with state returning to `0x0`.

**MAJOR — lock-off F6 never entered first person on candidates 4–6 (DYL-197).** Introduced by c655b93 / #47, this was found by candidate 6 batch `p5-r6-f6off`: parity-off and combined-main failed the `camera_state==0` probe (probe remained 9 in combined-main); cam-disabled passed; cam-malformed-ini failed the first-person probe; cam-obstruction was BLOCKED; seven routes were NOT_RUN. The exact stop record: the first SIGTERM (09:17:21, via the shell/timeout wrapper) DID reach `run_batch`; route 04 `cam-obstruction` was BLOCKED “interrupted by operator (SIGTERM); cleanup ran” at 16:17:40Z, restore rc 0 (Plugins dir absent), routes 05-11 were NOT_RUN “batch stopped”. The batch had already stopped before the owner rejected the second stop and said Continue; later “still running” checks were `pgrep -f` matching the watcher's own command line. Root cause: lock-off F6 published `precisionReturn.requestEntry()` only; `servicePrecisionEntry()` ran from `serviceDeferredCameraWork()` only when its SKSE task executed on Main, and from `serviceFirstPersonReturn()` only when `installLockCameraService()` installed the camera-call hook under `targetLockEnabled`. In `f6off-00`, the task ran off Main in 120/123 executions; diagnostics were also gated on target lock, so no `PrecisionReturn` lines appeared. The earlier `890a5da` and `8dbc5f9` path forced first person directly in the input sink. Before `p5-r6-f6off`, no lock-off F6 route had run on candidates 3–6. PR #50 (`ecfd90c`) fixed the Main-thread servicing path through a per-frame camera hook. On candidate 7, all 11 Camera-enabled lock-off routes showed F6 entry; entry-and-return coverage varies by route, and combined-main and combined-ui-disabled recorded `9>0` only; cam-disabled passed with native F6. The only three candidate 7 failures were missing `precision_trace event=11` assertions in parity-off, cam-malformed-ini and cam-obstruction.

**MINOR diagnostics — precision-trace claim event 11 missing (DYL-197).** Since c655b93 / #47, the precision-trace `toggleClaim` event was missing from Main-serviced entry. Candidate 7 found it in three routes; PR #51 restores the event when `iPrecisionTrace` is enabled, verified on candidate 8 in six routes. Combined-main passed without an event 11 line because its trace budget was spent before F6 and the route does not assert it. This debug sample never gates F6 entry.

**Probe lesson.** Probe assertions, not step completion, establish F6 cycles: require `camera_state` 0 then 9 to demonstrate entry and return.

Every R6 result is runner evidence, not owner feature acceptance. Route reports are under `/srv/storage/Projects/SkyCRPG/session-p4/autotest/runs/batch-p5-r6-{lock6,f6off,c7,c8}-NN/`.

## R10 matched performance results

The display cap caveat remains: the `frame_summary` sample had about 3,713 frames per 60-second window and medians of 16.41–16.42 ms; over33=0 / over50=0. Medians sat on the display cap, so p95 ratios measure headroom under the cap, not raw policy cost. All measured frame rows are within the ≤10% budget.

| Feature row | Candidate 5 | Candidate 6 | Candidate 7 | Status |
| --- | --- | --- | --- | --- |
| Baseline | — | — | — | PASS; standing/turn p95 16.844/16.906 ms |
| Off | `890a5da`: 16.669/16.850 ms | — | — | PASS |
| Lock | `b2c7960`: 16.612/16.851 ms | `d4ac5dc`: 16.614/16.724 ms | `ecfd90c`: 16.794/16.979 ms | PASS |
| Fixed | `890a5da`: 16.799/16.796 ms | — | — | PASS |
| Selection | `890a5da`: 16.762/17.117 ms | — | — | PASS |
| Cursor | `8dbc5f9`: 16.698/17.038 ms | — | — | PASS |
| All | `b2c7960`: 16.709 ms; 16.806 ms | `d4ac5dc`: 16.764 ms; 16.741 ms | `ecfd90c`: 16.832 ms; 16.790 ms | PASS |

Values are standing p95 then turning p95. Lock-off frame rows (Off, Fixed, Selection and Baseline) were last measured on `890a5da`, before #50 added the per-frame lock-off camera hook (review: one atomic check per idle frame); they were not rerun under D038 or D039.
| Dense row | Camera build | Result |
| --- | --- | --- |
| Selection | `890a5da` | 19 summaries; capped=0; maximum `cost_us` 212; private commit 1.47→1.53 GB (max 1.58 GB). |
| Cursor | `8dbc5f9` | PASS; 512 samples (trace cap). |
| All | `d4ac5dc` | PASS; candidate 5 log recorded 512 samples (trace cap). |

Resource observations (p95s in milliseconds; over33 counts are standing/turn/inventory/resource):

| Row | Candidate 5 | Candidate 6 | Candidate 7 |
| --- | --- | --- | --- |
| Baseline | Standing/turn/inventory/resource p95 16.842/16.888/16.760/16.741 ms; over33 0/0/10/11; inventory max 64.1 ms; resource max 389.8 ms (cell transitions); resource window private commit 1.681→2.127 GB (+0.446 GB), full run 1.47→2.13 GB. | — | — |
| Features off | Standing/turn/inventory/resource p95 16.702/16.911/16.709/16.770 ms; over33 0/0/20/12; inventory max 86.1 ms; resource max 401.9 ms (cell transitions); resource window private commit 1.689→2.138 GB (+0.449 GB), full run 1.46→2.09 GB. | — | — |
| All | Standing/turn/inventory/resource p95 16.644/16.838/16.704/16.701 ms; over33 0/0/21/12; window private commit 1.696→2.143 GB (601 samples). | 16.643/16.845/16.692/16.703 ms; over33 0/0/21/12; window private commit 1.703→2.149 GB (601 samples). | 16.937/16.971/16.950/16.994 ms; over33 0/0/29/13; window private commit 1.688→2.133 GB (601 samples). |


Candidate 2 `resource-all` measured standing/turn/inventory/resource p95 16.794/16.992/16.858/16.822 ms, over33 0/0/20/12, inventory max 88.0, resource max 372.5 ms, and private commit 1.687→2.136 GB (+0.449 GB, 601 samples); candidate 6 resource-all figures are in the table. Resource measurements do not change D024 or the Phase 5 gate ruling.

## Gate state

R6 final dispositions use each route’s latest build: the seven D039 trace routes all PASS on candidate 8; the other 34 use their latest recorded results, including candidate 7’s 21/24 PASS within its 24-route rerun. The three candidate 7 trace-assertion failures are superseded by candidate 8 PASS. The other 17 approved routes retain their recorded builds. R10 frame and resource rows now include candidates 5–7 where measured; lock-off frame rows remain from `890a5da`. The gate ruling remains **REVISE (D030)**; no feature or gate item is promoted to ACCEPT or Done.
