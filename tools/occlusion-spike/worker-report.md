# DYL-199 final evidence worker handoff

Base `b13a228b5dcc59f78097b017cb7b441dc8e0bd33`. All changes are uncommitted. No push, PR, Skyrim/Proton run, third-party contact, download, shared-file edit or production Camera/UI change. Owner-approved m-* see-through look is recorded; final game gates remain NOT_RUN.

Files changed in `p5-spike`:

- `tools/occlusion-spike/src/plugin.cpp`: quiet journal/counters, main-thread height series at verified native call site, process-lifetime private allocations, zero-length passthrough, collector resolution under the sweep lock, per-reference classifier counts, timing sums/duration and diagnostic filter+seed2.
- `tools/occlusion-spike/src/runtime_policy.h`: exact-zero probe helper, cumulative timing sum.
- `tools/occlusion-spike/SkyCRPG_OcclusionSpike.ini`: default-OFF quiet journal and default-0 diagnostic room-seed setting.
- `tools/occlusion-spike/analyze.py`: quiet summaries, nonlocal-write FAIL, height range/numeric reversals/steady duration, hook/frame share, classifier reference/model counts, negative-control zero-discard gate, matched/unmatched p95 comparisons.
- `tools/occlusion-spike/tests/classifier_test.cpp`, `timing_test.cpp`, `test_analyze.py`: focused regressions.
- `docs/evidence/p5/occlusion-spike.md`: current implementation, exact source checks, eleven-route run plan, actual offline results, remaining runtime gates and proposed shared-file text.
- `tools/occlusion-spike/worker-report.md`: this handoff.

Files created/changed in `/srv/storage/Projects/SkyCRPG/session-p5/routes`:

- New `write_final_routes.py`, `run_final.sh`, `summarize_final.py`.
- Updated `test_route_tools.py`; existing runner/helper implementations unchanged.
- New `gen/final/final-fixtures.provenance` and eleven generated JSON routes: `p5-final-budget-filter`, `p5-final-budget-filter-material`, `p5-final-budget-baseline`, `p5-final-boundary`, `p5-final-corridor`, `p5-final-corridor-filter`, `p5-final-doorway`, `p5-final-exterior`, `p5-final-hall`, `p5-final-culling`, `p5-final-culling-seed2`.

Actual tests and results:

- **PASS:** 28 standalone C++ tests compiled/executed with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic`; module tests additionally use `-Imodules/camera/include` or `-Imodules/ui/src`. Includes all 3 spike tests. The initial broad harness omitted those include paths and failed 11 compiles; only those commands were corrected/retried, all PASS.
- **PASS:** `python3 -B -m unittest discover -s tools/occlusion-spike/tests -p 'test_*.py' -v`: 23/23.
- **PASS:** `python3 -B /srv/storage/Projects/SkyCRPG/session-p5/routes/test_route_tools.py`: 23/23. First validation found duplicate section/state tag `fixture`; corrected to `fixture-state`, regenerated final routes, rerun PASS.
- **PASS:** `python3 -B -m unittest discover -s tools/tests -p '*test*.py' -v`: 24/24.
- **PASS:** 19/19 existing UI portable Python tests and 7/7 JS contract scripts, plus their C++ action-intent adapter. Three Python tests initially skipped due unset source env, then passed using the existing pinned SkyUI checkout and Node. No UI edits.
- **PASS:** exact exe/library address/prologue/call-site checks, ESM EDIDs/references, actual `iVSyncPresentInterval` string and runner override support, `bash -n run_final.sh`, missing-evidence report 11/11 NOT_RUN, `git diff --check`.
- Raw m-* logs remain journal-only PASS: filter 4670 discards; material 4650; alpha-property 4651. Six PASS material restores and six PASS alpha-property restores. No new metrics invented for old logs.

Job records, exact commands/result JSON and logs: `/tmp/dyl199-final/`. Logs include `sources.log`, `abi.log`, `camera-callsite.asm`, `all.log`, `include-retry.log`, `analyzer-final.log`, `routes-final.log`, `ui-sources.log`, `reports.log`. All handles completed; no worker-launched game process exists. Generated fixtures/source metadata are durable in the routes directory; scratch extracts remain in /tmp.

Unverified: Windows MSVC /W4 /WX compilation/linking and new DLL hash; actual quiet-mode cost/cadence, native height sampling, fixture navigation/boundary crossing, classifier quality, uncapped <=10% matched p95, material lifetime during rendering, and both ceiling/culling diagnostics. All eleven new game rows are NOT_RUN. The orchestrator must stage the newly built DLL/INI; old binaries cannot supply this telemetry. Steam was unavailable to synthetic preflight; no runtime claim follows from the portable tests.

Engineering decisions: retain allocations for process lifetime; pass through exact finite zero-length probes only; report-only classifier; relative height reversal threshold 1 unit and 250 ms gap limit; cap-on short observe captures versus cap-off 60 s quiet budget captures; ceiling seed2 strictly disposable/diagnostic; compare fading against opaque filter at the same pose, with compressed baseline deltas explicitly unmatched. No owner/product scope changed.

Questions for owner: none needed to finish this implementation. Owner gate/P5-05 authorization remains pending the outstanding evidence.

Orchestrator execution, after CI/artifact review: `bash /srv/storage/Projects/SkyCRPG/session-p5/routes/run_final.sh final-<unique-label>`. This serializes runs and collection, preserves invocation correlation, continues after normal route failures, and blocks further launches while a residual Skyrim/loader process exists. Per-route budget 900 s; outer bound 1170 s plus 30 s cleanup. `summarize_final.py --prefix final-<unique-label>` prints the review report.
