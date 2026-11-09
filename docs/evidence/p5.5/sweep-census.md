# P5.5-01 offline interior census — 2026-11-09

**Status: provisional / runtime NOT_RUN.** Source: D044–D045, `docs/DECISIONS.md`; sweep criteria and oracle checkpoint: `docs/phases/PHASE_5_5.md:125`. This is a read-only scan and synthetic-test result, not proof of in-game entry, rendered visibility or complete sweep coverage. No game, Proton or harness was launched.

## Inputs and reproducibility

Source: `tools/coverage/cell_census.py:13` and generated `session-p55/census/census.json`. SHA-256:

| Input | SHA-256 |
| --- | --- |
| Skyrim.esm | `e198c3b85e5e48e0c92a6580d8f66e644256b68d812ee32b61735cf9b753df73` |
| Update.esm | `b298b0f65fa0127fd13c1f5ef6bb30eaf9d21fd19f5e6f8f91f829c9007d3280` |
| Dawnguard.esm | `fc8f92cb7ed55217046ee96b84caa54c058c12f7c408ac2a71da880a7bd3eab7` |
| HearthFires.esm | `0f681496771e6d1983c6c1c9dd6602964068837f015bfa21e69a149c3b050047` |
| Dragonborn.esm | `635f21a938a17b8677f524cf41210933afc7f23820f2e210ab3bb6d3cd837046` |
| cell_census.py | `6f9c745b7458c0797d68394ea5723c6a8715696af9bb58181c9d7f727ed86d33` |
| classify.py | `362d87fbb5ab111f5a2b4b75f28c1c8858d622ada2993cd32a30ed261b402a92` |
| make_sweep_routes.py | `f2ddf8515ae6fb114504936ac0672e649f7aad61e38b3b9ecbadccb11528fa86` |
| sweep_report.py | `5f6a267e67d3b85972184bacf44bb1a6d71070ce0dd4ea4c0400310f8d51f6a9` |
| census.json | `606aed57912e22284ce10a0440bd639467480e5ee684294e0a2d7d3c9cee11a6` |
| pilot-include.txt | `7e00e36949369009fe37f956f51cb18f0365f8b5d6c63e999212de7d3236a21c` |
| routes/manifest.json | `c2fa88053ba1f6f49780b74a80d491d08dce919b360290b632ec7b72fb331308` |
| routes/sweep-0001.json | `f433ca59af15871ddcb2a19388d975d28962342109a416428c64eb57e4d5f96a` |
| routes/sweep-0002.json | `51d5cd6e65c12c009abb4f6d89c7662a576ed71ed3633f09176658fa8956d798` |
| routes/sweep-0003.json | `0e7035c429bebeddf0ffdffd35ede30d6d307599c6db3f37c48023aeb253b1e5` |

Master sizes, winning source, complete rule list and matched counts are in the census itself. Source: `tools/coverage/cell_census.py:51` resolves each plugin's MAST indices and walks compressed records and GRUPs, using the last record per plugin/local ID; this follows the project's original `session-p5/routes/scan_vanilla.py:1`. Source: `tools/coverage/cell_census.py:126` identifies STAT `Skyrim.esm:00001F` (`RoomMarker`) and `Skyrim.esm:000020` (`PortalMarker`) by their EDIDs. Hypothesis: these named bases identify the relevant room graph; exhaustiveness and engine behavior have not been verified. Incoming pose comes from the source reference's XTEL and destination reference's cell (`cell_census.py:108` and `:121`). Hypothesis: poses are usable until checked in game.

Commands (all generated data under `/srv/storage/Projects/SkyCRPG/session-p55/census/`):

```text
python3 tools/coverage/cell_census.py --out /srv/storage/Projects/SkyCRPG/session-p55/census/census.json
python3 tools/coverage/make_sweep_routes.py /srv/storage/Projects/SkyCRPG/session-p55/census/census.json --out /srv/storage/Projects/SkyCRPG/session-p55/census/routes --include /srv/storage/Projects/SkyCRPG/session-p55/census/pilot-include.txt --limit 30 --snap-key F1
python3 tools/coverage/sweep_report.py /srv/storage/Projects/SkyCRPG/session-p55/census/routes/manifest.json --out /srv/storage/Projects/SkyCRPG/session-p55/census/report-not-run.json
timeout 600s python3 -m unittest discover -s tools/tests -p 'test_coverage*.py' -v
```

Scan PASS as an offline parse: 759 rows. Synthetic tests PASS: 3 tests; compressed record, override, door XTEL, classifier, budget/hash and missing/unavailable observations. Scan job timeout 600 s, owner P5.5-01 worker, log `census.log`, completion exit and census hash; focused test timeout 600 s, log `tests.log`, completion unittest exit. Neither result proves the census's engine semantics.

## Distribution

Source: generated census, counts by owning master: Skyrim 590, Update 31, Dawnguard 50, HearthFires 7, Dragonborn 81. Winning source differs from owner and is retained per row.

Source: decoded flags (`cell_census.py:128`): interior 759; has water 238; cannot travel 2; no LOD water 2; public area 152; hand changed 0; show sky 271; use sky lighting 1. Source: counts from marker references: 291 cells with at least one named room/portal marker and 468 without; absence is not proof there is no graph. 720 have a discovered arrival or named marker pose, 39 do not. 24 have an EDID pattern indicating possible test/unused; zero lack an EDID in this result. Unreachable and unused status is not reliably determined and the flags are screening hints only.

Source: classifier rules in `tools/coverage/classify.py:5`; these ordered pattern matches are **hypotheses**, not verified location labels. Counts: Nordic ruin 105, cave/mine 193, small single-storey 165, fort/keep/prison 27, Dwemer ruin 52, interior worldspace 21, tower 44, great hall/palace 8, sewer/tunnel 7, house/inn 20, ship/shack/tent/cellar 14, unclassified 103. Names, keywords, top static base EDIDs and lighting template EDID are searched; match priority and rules are explicit in the census. A pattern match does not establish storey count or actual kit.

## Pilot and limitations

Source: pilot manifest: 30 selected across all 12 labels including unclassified, 3 draft routes of 10. Source: `make_sweep_routes.py:22`: at placeholder `s` seconds/cell, 180 seconds reserved per batch and capacity `min(cells_per_batch, floor((1200 - 180)/s))`; nominal total for 759 cells is `759*s + 180*ceil(759/capacity)` seconds, excluding any additional retries or actual cleanup variance. This is a formula, **not a measured duration**. The 90-second default and F1 snap key are placeholders; the key is not a binding decision. Replace both after measurement/configuration. COC is selected for named cells, but its actual landing is unverified. The second pose uses a console displacement, not a walk; 22 pilot cells have a second recorded incoming pose. A safe short walking path is still required. Heading labels do not prove camera orientation; actual heading and cell assertions must be added after the probe and spike contract are pinned.

Source: runner `session-p4/autotest/runner/executor.py:959` only writes a player-state request and waits for `player_state`; `runner/schema.py:30` has no `request_view` step. The generated draft `request_view` step therefore cannot currently run; the draft status and manifest expose this blocker. Candidate paths, INI overrides, copied save and a supported snap key are placeholders. `sweep_report.py` accepts a separately validated oracle and configurable visibility fraction, but that schema and evidence need joint review against the selected mechanism, cadence, rig and floor-ring thresholds. No validated oracle exists. A snapshot or a geometric ray alone never establishes rendered visibility. The report with no input reports attempted 0 and 759 NOT_RUN (including cells outside the pilot). Runtime observations, oracle checkpoint, candidate DLL hashes, final thresholds, performance and owner acceptance are **NOT_RUN**.

Other limitations: deleted winning cells are omitted by the scanner and need a separate inventory if they are in D044 scope; interior worldspace exterior CELLs are not enumerated as interiors. EDID pattern screening cannot establish reachability or unused status. Record semantics, flag mappings, lighting template parsing, REFR group association and pose clearance need independent validation; z extent is only a static-origin range and no storey count is inferred. Counts should not be presented as final coverage until these gaps are resolved. No engine accessor, address ID, vtable slot or native code was added; binary verification was not applicable and was NOT_RUN.

## Proposed shared-file changes

- Add a CI job for `python3 -m unittest discover -s tools/tests -p 'test_coverage*.py' -v`.
- Extend the test runner schema and executor with a serialized `@view <tag>` request, matching `view_sample` acknowledgement and exact tag, then add cell/heading assertions and oracle artifacts. Align the field table with the final probe contract. The current generator is intentionally draft pending that work.
- Record the measured timing, validated cell universe, tested walking paths, runner/probe revisions and candidate hashes in the phase evidence and ticket before any sweep acceptance claim.
