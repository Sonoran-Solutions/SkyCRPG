# DYL-199 third-pass worker report

Implemented the review corrections. No commit, push, PR, Skyrim/Proton launch, third-party contact or binary/archive/mod download. Outcome remains pending; every game row is NOT_RUN. Full evidence and updated run plan: `docs/evidence/p5/occlusion-spike.md`.

Files changed in this pass:

- `tools/occlusion-spike/src/plugin.cpp`
- `tools/occlusion-spike/src/config.h`
- `tools/occlusion-spike/src/runtime_policy.h` (new)
- `tools/occlusion-spike/tests/classifier_test.cpp`
- `tools/occlusion-spike/tests/timing_test.cpp`
- `tools/occlusion-spike/tests/test_analyze.py`
- `tools/occlusion-spike/analyze.py`
- `tools/occlusion-spike/SkyCRPG_OcclusionSpike.ini`
- `docs/evidence/p5/occlusion-spike.md`
- `tools/occlusion-spike/worker-report.md`

Pre-existing shared CMake/workflow/ci_scope changes are preserved, not edited by this pass. Production modules and tools/testprobe are untouched.

Test commands and actual results:

- `timeout 600 bash /tmp/occlusion-third-tests.sh`: PASS. Compiles every spike `*_test.cpp` with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic`, then executes it: 3 C++ executables PASS.
- Same script runs `python3 -B -m unittest discover -s tools/occlusion-spike/tests -p 'test_*.py' -v`: PASS, 12 tests.
- Same script runs `python3 -B -m unittest discover -s tools/tests -p test_ci_scope.py -v`: PASS, 8 tests. Combined log: `/tmp/occlusion-third-tests.log`.
- `timeout 60 python3 /tmp/occlusion-third-static.py`: PASS for main-thread sink boundaries, heap rings, retained material, no production/testprobe changes, independent CI artifact jobs and whitespace. Log: `/tmp/occlusion-third-static.log`.
- `git diff --check`: PASS.
- `timeout 120 python3 /tmp/occlusion-third-offline.py`, `/tmp/occlusion-third-flags.py`, targeted `objdump -d -M intel`: PASS for exact BSA mapping, thread-log recount, caller/EDX flow and caster context side effects. Initial scratch-helper failure and corrected field selection are disclosed in the evidence. Existing exe/library hashes were rechecked; no package hash invented.
- Windows DLL compilation: NOT_RUN; no cmake/MSVC. Orchestrator CI follows review/commit.
- All game tests: NOT_RUN. Foreground jobs completed; no game or background process was created.

Decisions made:

- Use live Main::threadID; sinks post atomic requests; only verified main tasks mutate/release fade state and summarize timing. Stop requests dominate queued arms. Off-main caster metadata remains unavailable and never authorizes filtering.
- F13/F14 replace F9/F10. Exact native controlmap contains Quickload 0x43 and no 0x64/0x65 binding. Live native mapping refusal stays in place.
- Continue through allowlisted contacts using the original gateway, same hull, at most 8 recasts. Return a real deeper hit or genuine continuation miss; restore phantom position with an original-input gateway call. Failed progress, caps, lifecycle interruption or journal failure roll back to the first native hit. Log/flush each step and discard; count kept non-allowlisted hits behind the beam.
- Heap-backed timing summary; distinct capture IDs/labels preserve standing and turning. Zero matched hits/discards, caps or incomplete journals are INCONCLUSIVE, never stability PASS.
- Retain original materials by intrusive reference and measure sharing with a bounded live-scene scan. Reject alpha below 0.10. Material swaps restore only the retained allocation by compare-and-restore and report conflict.
- Match room mode 2 sampling order. Explicitly disclose standalone mode 3's difference from the reference clipping gate; publish its eye snapshot/active state atomically.
- Keep near_clip refused: seven callers inspected; all load RCX=0x143331F40, three explicitly zero EDX and four pass ESI/EDI. EDX bits 0x02/0x04/0x80 affect uploaded constants; correct semantic flags for projection update/restoration remain unresolved.
- Preserve deliberate shared-change spike CI selection. Its independent failed job can make the workflow red but cannot gate/cancel Camera/UI build and artifact upload jobs.

Unverified:

Native adapter compilation/linking; actual main-task/input dispatch; continuation collision/broadphase/cache behaviour and adjacent/interpenetrating enclosure preservation; fade lifecycle/thread ordering, transparency, material-sharing visual effects and restoration; culling, performance, F8 and the simultaneous fixed-height-plus-transparency conjunction. A conservative recast may retain a beam; that is not a feasibility PASS. All remain NOT_RUN. No GO/REVISE/HOLD or P5-05 authorization is asserted.

Questions for owner: none blocking this pass. The orchestrator must collect the serialized Windows/game evidence before the owner decides feasibility and any P5-05 authorization.
