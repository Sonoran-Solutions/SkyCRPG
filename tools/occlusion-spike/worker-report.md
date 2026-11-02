# DYL-199 first-game-result fixes — worker handoff

Base `32933770d074ffb0f5e05cb86455f9f043bbce95`. Changes are uncommitted. No commit, push, PR, Skyrim/Proton launch, third-party contact or binary/archive/mod download. Outcome remains **pending**. No shared build/config or production Camera/UI source edited.

## Files changed

Within `p5-spike`:

- `tools/occlusion-spike/src/runtime_policy.h`: hull-diameter progress, bounded all-contact gap policy, exact native contact exclusions.
- `tools/occlusion-spike/src/config.h`: combined modes, same-reference constraint, explicit `none` empty-ID token.
- `tools/occlusion-spike/src/plugin.cpp`: guarded recasts, combined arm/restore, native per-geometry clone attempt and cache-aware lifetime, bounded fade contact counters.
- `tools/occlusion-spike/SkyCRPG_OcclusionSpike.ini`: mode/config/logging contract.
- `tools/occlusion-spike/analyze.py`: combined authority, safe-gap completion, clone scope and counter reporting.
- `tools/occlusion-spike/tests/classifier_test.cpp`: progress/limit/enclosure/inside-solid fixtures and mode/config tests.
- `tools/occlusion-spike/tests/test_analyze.py`: combined, gap, clone and counter journal tests.
- `docs/evidence/p5/occlusion-spike.md`: actual batch-k/j results, identities, new offline ABI findings, current tests and remaining gates, proposed shared-file text.
- `tools/occlusion-spike/worker-report.md`: this handoff.

Within `/srv/storage/Projects/SkyCRPG/session-p5/routes`:

- `write_routes.py`, `spike_next.py`, `test_route_tools.py`.
- New `gen/combined/p5-spike-filter.json`, `p5-spike-filter-material.json`, `p5-spike-filter-alphaprop.json`, `p5-spike-observe-selection.provenance`.

No runner or non-p5 route changes. Existing generator/file parity tests pass.

## Test commands and actual results

Job records and raw logs: `/tmp/dyl199-game-fixes/`.

- **PASS**, exit 0: `timeout 600 bash /tmp/dyl199-game-fixes/portable.sh`. Compiles each existing `tools/occlusion-spike/tests/*_test.cpp` with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic`, runs three executables, then Python unittest discovery: 16 tests. Log `portable-final.log`.
- **PASS**, exit 0: final affected-suite recheck `timeout 600 python3 -B -m unittest discover -s tools/occlusion-spike/tests -p 'test_*.py' -v`: 16 tests, `analyzer-final.log` (adds original-contact rediscovery requirement).
- **PASS**, exit 0: `timeout 600 python3 -B /srv/storage/Projects/SkyCRPG/session-p5/routes/test_route_tools.py`: 20 tests, `routes-final.log`. First attempt **FAIL**, 18 passes/2 errors: runner schema rejects blank INI values. Changed input to explicit `none`, added parser coverage, one corrected retry passed; runner unchanged. Synthetic preflight correctly reports Steam unavailable; no game preflight pass claimed.
- **PASS**, exit 0: separate `timeout 120 python3 /tmp/dyl199-game-fixes/offline.py`, `material.py`, `verify-abi.py`: read-only executable/library hashes, native material setter/clone/copy/refcount/cache/exact restore, gap sweep/collector/locking and prologue checks. Matching logs and disassembly retained.
- **PASS**, exit 0: `timeout 60 python3 -B /srv/storage/Projects/SkyCRPG/session-p5/routes/spike_next.py /srv/storage/Projects/SkyCRPG/session-p5/logs/collected/j-p5-spike-observe/SkyCRPG_OcclusionSpike.jsonl --reference 000E7A7B --combined --out-dir /srv/storage/Projects/SkyCRPG/session-p5/routes/gen/combined`: three routes plus provenance, `generation-final.log`.
- **PASS**: `git diff --check`.

Reviewed old game results: all four batch-k runner and restoration reports PASS; filter functionality FAIL (zero committed discards, four repeated beam recasts then rollback); alpha/material visual transparency NOT_RUN because camera remained compressed; app-cull is Hidden/Culled, not Transparent. All three fade logs have six recorded restore PASS events. New code does not inherit those passes.

## Unverified

**NOT_RUN:** new Windows `/W4 /WX` compile/link and DLL hash; every new in-game result, including real Havok gap/start-overlap behavior, fixed-height stability, material clone render behavior and lifetime, all restoration exits, contact-counter observations, matched above-beam screenshots and actual ≤10% p95 cost. No local Windows toolchain. Orchestrator owns CI and serialized game runs. External reference modes and the existing near-clip ABI blocker remain unchanged.

## Engineering decisions

- Advance `max(30, 2*hull)` from the farther of current start and projected contact; at most eight steps, ten gateway calls/hull writes including context reset.
- Before jumping, query all contacts with the same hull, including start overlaps, and require original-collidable rediscovery. Bound storage to 256 contacts and parent walks to 32. A wall/unknown/cap within the skipped interval rolls back to the original native hit, never to clear. A non-allowlisted suffix hit keeps its real native outputs; a true continuation miss returns the goal. Closely overlapping beam/enclosure is deliberately refused rather than synthesizing a wall contact.
- Combined modes require the same single explicit reference, apply fade before filter arming, and use one restore path.
- Use the native unique-material Create/CopyMembers/cache path, retain/release via native cache-aware refcounting, and restore exact original pointers. Shared shader or clone failure stays explicitly labelled shared; locality claims are bounded to the attached-world shader scan.
- Fade counters count original entry results without sampling, report ≥500ms windows plus actual lifecycle boundaries, and explicitly log zero for the selected reference. Native binary inspection shows app-cull can exclude contacts; its role in batch-k is a source-backed hypothesis until measured.

## Questions for owner

None needed for this implementation brief. Visual acceptance and any GO/REVISE/HOLD decision remain pending after the orchestrator's build and game evidence. No owner decision was changed.
