# DYL-199 Windows compile fix and arm keys — worker report

Source base `4341298`, branch `phase5/dyl-199-occlusion-spike`. Changes are uncommitted. No commit, push, PR, game/Proton launch, input injection, third-party contact, download, runner/input edit or shared-file edit.

Both discarded SmoothCam `[[nodiscard]]` results are fixed with `static_cast<void>`: `RegisterInterfaceLoaderCallback` and `RequestInterface`. Reviewed the whole plugin against the pinned SmoothCam/CommonLib headers for additional nodiscard calls, unused parameters/variables, signedness, narrowing and shadowing; no further issue identified. No TDM calls exist in this file. Corrected native compilation is NOT_RUN, not proven by the portable tests.

**Concrete blocker:** requested KP7 (DIK `0x47`) is mapped to native Hotkey7: line 43, `Hotkey7 0x08,0x47`, in the exact installed `Skyrim - Interface.bsa` controlmap. Member SHA-256 `54886af6a32c18fa5a8b5805ab3070b6af1b9ddc92f777206494099d4e3353ee`. The claim that neither requested key is mapped is FAIL. KP9 (`0x49`) and proposed KP0 (`0x52`) are unmapped in all stock keyboard contexts; all three are already supported by the runner. Runtime native mapping refusal is retained. Source/routes currently use the explicitly requested KP7/KP9; missing-F13/F14 blocks were removed and replaced by the confirmed native-mapping blocker. The owner was asked about KP0/KP9; no answer received during this pass.

Files changed:

- `p5-spike/tools/occlusion-spike/src/plugin.cpp`
- `p5-spike/tools/occlusion-spike/SkyCRPG_OcclusionSpike.ini`
- `p5-spike/docs/evidence/p5/occlusion-spike.md`
- `p5-spike/tools/occlusion-spike/worker-report.md`
- `session-p5/routes/write_routes.py`
- `session-p5/routes/spike_next.py`
- `session-p5/routes/test_route_tools.py`
- `session-p5/routes/README.md`
- Eight regenerated JSONs in `session-p4/autotest/routes/`: `p5-spike-baseline.json`, `p5-spike-baseline-turning.json`, `p5-spike-observe.json`, `p5-spike-observe-turning.json`, `p5-spike-filter-template.json`, `p5-spike-filter-turning-template.json`, `p5-spike-fade-template.json`, `p5-spike-fade-turning-template.json`.
- New validation evidence directory `session-p5/routes/validation/dyl199-kp7-kp9/`: `schema.json`, `summary.json`, and 12 per-route dry-run logs.

All paths above are under `/srv/storage/Projects/SkyCRPG/`. Every other pre-existing route JSON hash is unchanged. `analyze.py` has no key-name matching and needed no edit. Scratch scripts, test binaries, extracted controlmap and raw logs stay in `/tmp/dyl199-compile-keys/`.

Test commands and actual results (purpose/command/expected output/deadline/owner/cleanup recorded before execution in `/tmp/dyl199-compile-keys/jobs.md`):

- `timeout 600 bash /tmp/dyl199-compile-keys/portable.sh`: PASS, exit 0. Compiles/runs all three `tools/occlusion-spike/tests/*_test.cpp` with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic`; then `python3 -B -m unittest discover -s tools/occlusion-spike/tests -p 'test_*.py' -v`: 12 tests PASS. Log: `portable.log`.
- `timeout 600 python3 -B /srv/storage/Projects/SkyCRPG/session-p5/routes/test_route_tools.py`: final PASS, exit 0, 18 tests; log `route-tools-final.log`. Initial FAIL (17 passed, one fixture error) retained in `route-tools.log`: test expected old `p5-stage-manifest.json`, existing builder writes `p5-stage-manifest-off-lock-selection-spike.json`. Corrected only test expectation and reran once. Synthetic staging never loads the fake plugin fixtures.
- `timeout 600 python3 -B /srv/storage/Projects/SkyCRPG/session-p5/routes/validate_routes.py --out /srv/storage/Projects/SkyCRPG/session-p5/routes/validation/dyl199-kp7-kp9`: schema/keys PASS (12/12, no unsupported keys); dry runs BLOCKED, aggregate exit 2 and all 12 dry-run exits 2. Refusals: deployed profile, existing probe DLL/INI, Steam unavailable; route-specific existing lock save or missing selection/spike stage/dist; native KP7 blocker and template observe/reference prerequisite. Exact per-route reasons retained in `summary.json`. No host resources were changed to evade these refusals.
- `timeout 120 python3 /tmp/dyl199-compile-keys/controlmap.py`: FAIL, exit 1, unmapped-KP7 assertion disproved. `timeout 120 python3 /tmp/dyl199-compile-keys/controlmap-diagnostic.py`: PASS, exit 0, confirms Hotkey7 conflict and unmapped KP0/KP9 alternative using tab-delimited keyboard fields, comma alternatives and chord components. Both logs retained.
- `git diff --check` and SHA-256 comparison against `routes-before.json`: PASS; exactly eight Spike JSONs changed. Log: `final-checks.log`.
- Corrected Windows build and every game/input/visual/performance row: NOT_RUN. No cmake/MSVC available. Original CI C4834 FAIL is owner-reported; no CI success or package hash invented. All worker foreground jobs completed; synthetic temporary directories cleaned by the test contexts.

Decisions made: preserve behavior by explicitly discarding both handshake results while retaining the data-load provider guard; centralize requested DIK values; preserve native input/refusal and template review gates; replace obsolete key-availability blocker with the concrete stock-map conflict; update stale test manifest expectation without changing builder/runner/input code. No product/owner decision was changed. Shared-file proposals remain only in the evidence file's “Proposed shared-file changes” section.

Unverified: MSVC `/W4 /WX` success; native key delivery/remapping refusal; all existing collision/fade/restoration/culling/thread/performance/owner acceptance obligations. The requested stock-key verification cannot pass for KP7. No GO/REVISE/HOLD decision is inferred.

Question for owner: approve KP0 (DIK `0x52`) for arm/apply with KP9 (DIK `0x49`) for restore, or retain requested KP7/KP9 with its confirmed native-mapping blocker? KP0/KP9 is the verified unmapped, runner-supported proposal; actual delivery would still need the serialized game run.
