# Phase 5 regression set (R6/R10) on candidate 890a5da

## Candidate and method

Candidate source: `890a5da1354ba29130ac88596cb3cfd0b242e1b5`. Candidate archive SHA-256 values from `session-p5/dist-p5-candidate/candidate.json`:

| Module | Archive | SHA-256 |
| --- | --- | --- |
| Camera | `SkyCRPG_Camera-0.1.0-rc.1-890a5da.zip` | `0bd64ec6772b92fe274b3720787a9085fc7421a3d33a2f97e3a4dc3f533605d7` |
| Selection | `SkyCRPG_Selection-0.1.0-rc.1-890a5da.zip` | `6dc40a9733dec7500ca373a85be8ea51e86f3126ef1e7327c6dcf477fac99fa0` |
| UI rc1 | `SkyCRPG_UI-0.1.0-rc.1-6b4c0fc.zip` | `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb` |

Stages were built by `make_stages.py --candidate`. Runner: session-p4 autotest runner, unattended; one Skyrim at a time; profile/save restoration after every route. **Runner PASS means asserted checkpoints passed on this build; it is not feature acceptance.** Route plan stages are 1, 2, 4, 5, 6, 7, 8 and 9, per `session-p5/routes/regression-plan.md`. “RERUN PENDING (D036)” means the candidate must be rebuilt after PR #44 and that route rerun. The other 25 route results remain tied to candidate 890a5da.

## Approved route results

| Route | Plan stage | Batch | Runner result on 890a5da | Build | Feature evidence | Gate status |
| --- | ---: | --- | --- | --- | --- | --- |
| `p5-reg-parity-off` | 1 | `p5-r6-s1` | PASS | 890a5da | parity-off 2/2 sections | RUN |
| `p5-reg-ui-main` | 1 | `p5-r6-s1` | PASS | 890a5da | UI sentinel 19/19 | RUN |
| `p5-reg-combined-main` | 1 | `p5-r6-s1` | PASS | 890a5da | combined startup 5/5 | RUN |
| `p5-reg-cam-disabled` | 2 | `p5-r6-s2` | PASS | 890a5da | disabled-camera recovery checkpoints 2/2 | RUN |
| `p5-reg-cam-malformed-ini` | 2 | `p5-r6-s2` | PASS | 890a5da | malformed-INI refusal checkpoints 2/2 | RUN |
| `p5-reg-cam-no-mcmhelper` | 2 | `p5-r6-s2` | PASS | 890a5da | missing-provider checkpoints 2/2 | RUN |
| `p5-reg-cam-no-smoothcam` | 2 | `p5-r6-s2` | PASS | 890a5da | missing-provider checkpoints 2/2 | RUN |
| `p5-reg-cam-no-tdm` | 2 | `p5-r6-s2` | PASS | 890a5da | missing-provider checkpoints 2/2 | RUN |
| `p5-reg-cam-obstruction` | 2 | `p5-r6-s2` | PASS | 890a5da | obstruction route checkpoints 2/2 | RUN |
| `p5-reg-cam-schema` | 2 | `p5-r6-s2` | PASS | 890a5da | schema refusal checkpoints 2/2 | RUN |
| `p5-reg-cam-unsafe-tdm` | 2 | `p5-r6-s2` | PASS | 890a5da | unsafe-TDM refusal checkpoints 2/2 | RUN |
| `p5-reg-combined-cam-disabled` | 2 | `p5-r6-s2` | PASS | 890a5da | combined disabled-camera checkpoints 4/4 | RUN |
| `p5-reg-combined-ui-disabled` | 2 | `p5-r6-s2` | PASS | 890a5da | combined disabled-UI checkpoints 4/4 | RUN |
| `p5-reg-smoke-cam-lock` | 4 | `p5-r6-s4` | PASS | 890a5da | lock engaged, 479 trace rows / 415 locked | RUN |
| `p5-reg-smoke-cam-fixed` | 4 | `p5-r6-s4` | PASS | 890a5da | ThinOccluder engaged lines 604 | RUN |
| `p5-reg-smoke-cam-selection` | 4 | `p5-r6-s4` | PASS | 890a5da | selection smoke checkpoints 3/3 | RUN |
| `p5-reg-smoke-cam-cursor` | 4 | `p5-r6-s4` | PASS | 890a5da | cursor entered and exited for toggle/F6/F8/menu | RERUN PENDING (D036) |
| `p5-reg-smoke-cam-all` | 4, rerun in 6 | `p5-r6-s4`, `p5-r6-s6` | PASS | 890a5da | initial run had bow-drawn refusal; sheathed rerun: lock 481/417 rows, cursor entered with 426 samples, F6 return remained first person | RERUN PENDING (D036) |
| `p5-reg-smoke-combined-on` | 4 | `p5-r6-s4` | PASS | 890a5da | cursor entered; lock trace had 0 rows because no lockable target (NOT_RUN-expected for lock) | RERUN PENDING (D036) |
| `p5-reg-cursor-no-selection` | 5 | `p5-r6-s5` | PASS | 890a5da | no cursor samples; expected Selection-absent refusal | RERUN PENDING (D036) |
| `p5-reg-cursor-external-off` | 5 | `p5-r6-s5` | PASS | 890a5da | no samples; `bAllowExternalTargets=0` refusal, “opt-in required” | RERUN PENDING (D036) |
| `p5-reg-on-main` | 6 | `p5-r6-s6` | BLOCKED | 890a5da | overlay opened during console setup; recovery refused to continue | RERUN PENDING (D036) |
| `p5-reg-on-cam-disabled` | 6 | `p5-r6-s7a` | PASS | 890a5da | corrected disabled-camera assertion 4/4 | RUN |
| `p5-reg-on-ui-disabled` | 6 | `p5-r6-s6` | NOT_RUN | — | batch stopped after `on-main` BLOCKED | RERUN PENDING (D036) |
| `p5-reg-controls-off` | 6 | `p5-r6-s6` | NOT_RUN | — | batch stopped after `on-main` BLOCKED | RERUN PENDING (D036) |
| `p5-reg-controls-all` | 6 | `p5-r6-s6` | NOT_RUN | — | batch stopped after `on-main` BLOCKED | RERUN PENDING (D036) |
| `p5-reg-lifecycle-off` | 6 | `p5-r6-s6` | NOT_RUN | — | batch stopped after `on-main` BLOCKED | RERUN PENDING (D036) |
| `p5-reg-lifecycle-all` | 6 | `p5-r6-s6` | NOT_RUN | — | batch stopped after `on-main` BLOCKED | RERUN PENDING (D036) |
| `p5-reg-frame-baseline` | 7 | `p5-r6-s7a` | PASS | 890a5da | standing/turn p95 16.844/16.906 ms | RUN |
| `p5-reg-frame-off` | 7 | `p5-r6-s7a` | PASS | 890a5da | standing/turn p95 16.669/16.850 ms | RUN |
| `p5-reg-frame-lock` | 7 | `p5-r6-s7a` | PASS | 890a5da | standing/turn p95 16.776/16.801 ms | RUN |
| `p5-reg-frame-fixed` | 7 | `p5-r6-s7a` | PASS | 890a5da | standing/turn p95 16.799/16.796 ms | RUN |
| `p5-reg-frame-selection` | 7 | `p5-r6-s7a` | PASS | 890a5da | standing/turn p95 16.762/17.117 ms | RUN |
| `p5-reg-frame-cursor` | 7 | — | NOT_RUN | — | toggle defect affects route | RERUN PENDING (D036) |
| `p5-reg-frame-all` | 7 | — | NOT_RUN | — | toggle defect affects route | RERUN PENDING (D036) |
| `p5-reg-dense-selection` | 8 | `p5-r6-s89a` | PASS | 890a5da | 19 summaries; capped=0; max scan cost 212 µs | RUN |
| `p5-reg-dense-cursor` | 8 | — | NOT_RUN | — | toggle defect affects route | RERUN PENDING (D036) |
| `p5-reg-dense-all` | 8 | — | NOT_RUN | — | toggle defect affects route | RERUN PENDING (D036) |
| `p5-reg-resource-baseline` | 9 | `p5-r6-s89a` | PASS | 890a5da | 10-min resource window, 920 samples | RUN |
| `p5-reg-resource-off` | 9 | `p5-r6-s89a` | PASS | 890a5da | 10-min resource window; Camera/Selection present, features off | RUN |
| `p5-reg-resource-all` | 9 | — | NOT_RUN | — | toggle defect affects route | RERUN PENDING (D036) |

## R6 and defect record

The ledger records **25 of 41 routes run (24 PASS, on-main BLOCKED by the defect)**. Sixteen cursor-affected routes are scheduled for rerun on the rebuilt candidate under D036; route rows above retain the result on 890a5da where one exists. Stage 6 stopped after `on-main`, so `on-ui-disabled`, `controls-off`, `controls-all`, `lifecycle-off` and `lifecycle-all` were not run. The `on-cam-disabled` route later passed 4/4 in `p5-r6-s7a`.

**MAJOR — cursor toggle queued from a native menu.** In `p5-reg-on-main` (run `batch-p5-r6-s6-01`), runner console text `player.additem 00012EB7 1`, `0001CF2B`, and `save …_combined_prep` contains B/b, the cursor toggle (`[CursorMode] iToggleKey=48`). Probe events show Console closing at `05:13:18.463Z` and `SkyCRPGCameraWorldPointer` opening at `05:13:18.495Z`; the overlay remained open after setup and recovery refused to continue. The input sink queued the toggle, then service logic applied it after the menu closed.

Fix work is PR #44 `phase5/dyl-200-cursor-console-toggle`, commits `c0fee40` and `a0bb23c`: menu-owned presses are dropped at the key edge using retained keys and one bounded try-lock attempt, with once-only logging. Astra senior review round 1 returned CHANGES for two MAJOR findings (string-pool interning on the input path; off-main toggle-off dropped); round 2 passed at `a0bb23c`. The Windows build was pending a GitHub release-download outage (504) when recorded. PR #44 was not merged at record time. D036 directs a candidate rebuild after merge and reruns only the 16 cursor-affected routes listed in the decision.

**Unresolved smoke discrepancy:** `smoke-cam-all` (lock + fixed + cursor ON) stayed in native first person after its F6 pair while `smoke-cam-cursor` returned to third person. Cause is a hypothesis recorded in PR #44's evidence note; not fixed.

**MINOR diagnostics:** weapon-drawn refusal logs under name `precision` and once per session. The owner quicksave loads with the bow drawn, so the cam-all smoke now sheathes first.

## R10 matched performance results

All five measured frame rows were within ±1.3% of baseline, under the ≤10% budget. The `frame_summary` sample had about 3,713 frames per 60-second window, median 16.41–16.42 ms throughout, and over33=0 / over50=0. Medians sat on the display cap, so p95 ratios measure headroom under the cap, not raw policy cost.

| Feature row | Standing p95 ms | Delta vs baseline | Turn p95 ms | Delta vs baseline | Status |
| --- | ---: | ---: | ---: | ---: | --- |
| Baseline | 16.844 | 0.00% | 16.906 | 0.00% | PASS |
| Off | 16.669 | -1.04% | 16.850 | -0.33% | PASS |
| Lock | 16.776 | -0.40% | 16.801 | -0.62% | PASS |
| Fixed | 16.799 | -0.27% | 16.796 | -0.65% | PASS |
| Selection | 16.762 | -0.49% | 17.117 | +1.25% | PASS |
| Cursor | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | NOT_RUN |
| All | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | NOT_RUN |

Resource observations (p95s in milliseconds; over33 counts are standing/turn/inventory/resource):

| Row | Standing p95 | Turn p95 | Inventory p95 | Resource-window p95 | over33 counts | Inventory max | Resource max | Private commit |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: | --- |
| Baseline | 16.842 | 16.888 | 16.760 | 16.741 | 0/0/10/11 | 64.1 ms | 389.8 ms (cell transitions) | 1.47→2.13 GB |
| Features off | 16.702 | 16.911 | 16.709 | 16.770 | 0/0/20/12 | 86.1 ms | 401.9 ms (cell transitions) | 1.46→2.09 GB |
| All | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING | RERUN PENDING |

The ledger records baseline and off private commit growth as equivalent, both about +0.63 GB over the matched window. Off versus baseline deltas: standing -0.83%, turn +0.14%, inventory -0.30%, resource +0.17%. `resource-all` awaits the post-fix rerun. Dense Selection reported 19 summaries, capped=0, maximum `cost_us` 212; its private commit was 1.47→1.53 GB (max 1.58 GB). Cursor and combined dense rows await rerun. These resource measurements do not change D024 or the Phase 5 gate ruling.

## Gate state

R6 and R10 remain partial. D036 changes only which routes must be rerun after PR #44 merges and the candidate is rebuilt. The gate ruling remains **REVISE (D030)**; this record does not promote any feature or gate item to ACCEPT or Done.
