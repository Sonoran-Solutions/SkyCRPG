# Phase 4 internal candidate checklist

This is not public-release authorization. Final candidate: source `6b4c0fc6a6da4effd9faeafc9a2c59874a612271`, [main CI run 41376680950](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41376680950); UI SHA-256 `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb`, Camera SHA-256 `e3f6d61e2d2075a10d8a31c07ccfbe9c78e324230f5c399cadbc2c3c39c86838`. Packaged automated gameplay is bounded **PASS**; the owner accepted the Phase 4 MVP on 2026-11-02 ([D025](DECISIONS.md)).

## Phase 5 (2026-11-08): no candidate yet

The owner recorded **REVISE, do R1–R11** on 2026-11-08 (D030); Phase 5 ACCEPT is not recorded ([gate](evidence/p5/gate.md)). There is no combined Phase 5 archive to list here. Feature builds `63e992e` (lock), `c10e6bc` (fixed height) and `4946c92` (cursor, Camera `8ca54149…` with Selection `6d8b12e2…`) have feature evidence; `owner-cursor-1` ran with cursor mode on and target lock and thin occluder off (preserved INI), with capped logs; no session or route has combined two Phase 5 features. D031 decides Selection ships in its own archive and the lock overlay folder ships inside Camera's archive. D032 approves U and `[` / `]` keyboard bindings; pad cycle/switch/cursor stay unbound. D033 rewords fixed height as “no additional beam-induced pull-in, subject to enclosure” and retains three beams. Remaining combined candidate and regression work follows R1/R2/R5/R6/R11 in the gate. Public redistribution stays deferred by D023.

## Phase 4 rows

| Ticket | Current state | Evidence / remaining work |
| --- | --- | --- |
| [DYL-170](phases/PHASE_4.md) | Scoped runtime **PASS**: 22/22 automated routes on Proton Experimental `experimental-11.0-20261027b-x86_64`, Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, test probe `ac28d3c` (#30), UI `2d39d30` and Camera `1d95915`; the owner approved all 40 review screenshots on 2026-11-01. Camera `1daf2aa` adds the notice-intent fix after the runs (portable tests and Windows CI only). | [UI](evidence/p4/blocker-resolution-ui.md), [Camera](evidence/p4/blocker-resolution-camera.md). Controller play, follower lifecycle, the CC missing-icon fixture and marker travel accuracy remain **NOT_RUN**. Aim FAIL and lock BLOCKED stay accepted MVP limitations, not passes. |
| DYL-171 | **PASS** for strict assembly/reproduction and bounded MO2 automated routes (5/5); manual written-instruction install, controller and Vortex **NOT_RUN** | [Packaging](evidence/p4/packaging.md). |
| DYL-172 | Frame cost and warm inventory latency **PASS**; memory **FAIL** with the optional minimap stack | Four-profile rerun `dyl172-proton-01` on the rc1 archives: combined p95 +0.58% standing, +0.72% turn; warm t_settled max 203 ms. Baseline (no SkyCRPG module), UI-only and combined grew about 43 MiB/min with the minimap stack; camera-only and combined without the stack were not flagged. Accepted as an external-provider limitation by owner decision 2026-11-02 ([D024](DECISIONS.md)); not a pass. [Performance](evidence/p4/performance.md). |
| DYL-173 | Bounded automated packaged regression **PASS** (22/22); controller **NOT_RUN** | [Regression](evidence/p4/regression.md); broader acceptance remains open. |
| DYL-174 | Final documentation merged in #32; package identities cross-checked; public rights clearance deferred | [Docs review](evidence/p4/docs-review.md), [D023](DECISIONS.md). |
| DYL-175 | **ACCEPT, owner decision 2026-11-02** | [Record](evidence/p4/acceptance.md), [owner route](PLAYTEST.md). Per-area observations were not recorded. |

Do not claim C1–C5, U1–U6, M1–M3 or Q1–Q3 pass for the final candidate without recorded evidence. Record source, package and dependency hashes, exact host, device and outcome. No unresolved critical crash, item loss, control lock or navigation loss may be silently waived. Public redistribution and associated rights clearance are deferred until at least Phase 5 completion by [D023](DECISIONS.md).
