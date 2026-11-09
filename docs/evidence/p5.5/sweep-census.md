# P5.5-01 offline census and pilot — 2026-11-09

**PASS** offline parsing and synthetic checks; **NOT_RUN** game, Proton, harness, runtime view sample, visibility, owner acceptance and performance. Source: `docs/phases/PHASE_5_5.md` P5.5-01 and D044–D045 in `docs/DECISIONS.md`. No engine accessor, vtable, offset, address ID or native code was added, so binary verification was not applicable; **NOT_RUN**.

## Inputs and artifacts

Source: `tools/coverage/cell_census.py` scans the five named masters in load order, hashes them, and records winning records. This is a parser result, not proof of complete engine semantics or reachability. Full source sizes, hashes, rule definitions, match counts, all cell data and worldspace records are in `census-wave2a.json` under `/srv/storage/Projects/SkyCRPG/session-p55/census/`.

| Input | SHA-256 |
| --- | --- |
| Skyrim.esm | `e198c3b85e5e48e0c92a6580d8f66e644256b68d812ee32b61735cf9b753df73` |
| Update.esm | `b298b0f65fa0127fd13c1f5ef6bb30eaf9d21fd19f5e6f8f91f829c9007d3280` |
| Dawnguard.esm | `fc8f92cb7ed55217046ee96b84caa54c058c12f7c408ac2a71da880a7bd3eab7` |
| HearthFires.esm | `0f681496771e6d1983c6c1c9dd6602964068837f015bfa21e69a149c3b050047` |
| Dragonborn.esm | `635f21a938a17b8677f524cf41210933afc7f23820f2e210ab3bb6d3cd837046` |
| tools/coverage/cell_census.py | `b1478adc368ec84818ec5a681e1d7945179e340c0fc94a6043e67105c68dbb8d` |
| tools/coverage/classify.py | `16fb734a4dfab8788732a3413c7e1172ef5e9b71b6619da75544b19e7905656c` |
| tools/coverage/make_sweep_routes.py | `c8b04fa50c1dc6dfcff713059b7a9960e838f4712dd314679ebe034f29c898b0` |
| tools/coverage/sweep_report.py | `17a6306c1a5fd2789cc414dff350fe8614de290b52daa3aa624420a0c92c3b41` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/census-wave2a.json | `e12d05e78af53cf3725f3849fa78aaaedd2be94e366b816b48f956b435e6fa00` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/pilot-include.txt | `7e00e36949369009fe37f956f51cb18f0365f8b5d6c63e999212de7d3236a21c` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/manifest.json | `b4c0fe7794c16f62c16ef82b8ae74ea93247927598fdb4bdb98b941081f8d709` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/report-wave2a.json | `0ea19c6733c6c1488459a1a88d4633565e56cd3eb605bcb7b521e507dd3ea362` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0001.json | `f704d5488bad6a41db3634a89ca6c9add01f91838284640c8746a939fd53f29c` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0002.json | `edc1c763e4152293215b3b7334b8d568b86df65051acb283a240fcb76b0e3ee9` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0003.json | `abcceeb35a8c354362baff2a70e8343177e723a2b9ff5edf37eb2f49eae5074b` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0004.json | `52dc92fe514eab97675264dcb93f8a5cf5c38b80902be1ba3bfdf20b56c81c89` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0005.json | `2d5137bfaeeaaebc6275c1846498870e1ac7ccbd409ba6f6fa76bc17634bb67a` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0006.json | `49a83eb19366c47f42b5064286d293ca3f653915fdaa604eac00880927f025d5` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0007.json | `80ec5158401b30ebfcf048457a77d9d7424915f8cb1c1251d5ae3516e522c2c9` |
| /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/sweep-0008.json | `88f33bf0d9515801ff3ed51bfde496ce81115fb028a569408effcba690703cee` |

## Offline jobs

Owner: P5.5-01 worker. Each job had a 600-second `timeout`, completed by process exit, with logs in the census directory unless noted. Commands:

```text
python3 runner/tests/run_all.py                           # /srv/storage/Projects/SkyCRPG/session-p4/autotest; /tmp/p55-runner-before.log, /tmp/p55-runner-final.log
python3 -m unittest discover -s tools/tests -p 'test_coverage*.py' -v
python3 tools/coverage/cell_census.py --out /srv/storage/Projects/SkyCRPG/session-p55/census/census-wave2a.json
python3 tools/coverage/make_sweep_routes.py /srv/storage/Projects/SkyCRPG/session-p55/census/census-wave2a.json --out /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2 --include /srv/storage/Projects/SkyCRPG/session-p55/census/pilot-include.txt --limit 30 --snap-key F1
python3 tools/coverage/sweep_report.py /srv/storage/Projects/SkyCRPG/session-p55/census/routes-wave2a-v2/manifest.json --out /srv/storage/Projects/SkyCRPG/session-p55/census/report-wave2a.json
```

Source: runner baseline **PASS**, 254 tests; final **PASS**, 259 tests. Intermediate runs failed on new test import and fixture setup; corrected and rerun. Focused coverage **PASS**, 5 tests. Final census **PASS** as parsing, 759 interior rows. Pilot **PASS** as generation, 30 selected, eight draft routes; all eight passed runner schema validation. Report **PASS** as aggregation, zero attempted and 759 NOT_RUN. The runner simulator returns an explicitly unavailable synthetic view sample, not an invented camera pose. Source: `runner/simulate.py`.

## Distribution and screening

Source: `tools/coverage/classify.py` and the two generated census files. Keyword matches precede editor-ID tokens, then static kit prefixes; match counts and order are in the new census. These categories and the utility screen are **hypotheses**, not engine-verified labels. Utility screening is independent of classification; 50 rows carry a reason. There are 114 unclassified rows, listed below.

| Classification | Wave 1 | Wave 2a |
| --- | ---: | ---: |
| cave_or_mine | 193 | 155 |
| dwemer_ruin | 52 | 43 |
| fort_keep_prison | 27 | 65 |
| great_hall_palace | 8 | 19 |
| interior_worldspace | 21 | 0 |
| multi_storey_house_inn | 20 | 22 |
| nordic_ruin | 105 | 117 |
| sewer_or_tunnel | 7 | 2 |
| ship_shack_tent_cellar | 14 | 1 |
| small_single_storey | 165 | 221 |
| tower | 44 | 0 |
| unclassified | 103 | 114 |

## Worldspaces and limitations

Source: `tools/coverage/cell_census.py` tracks world GRUP ownership, WRLD parent and DATA flags, and counts non-deleted exterior CELL records separately from the interior-cell rows. The census lists all 48 non-Tamriel WRLD records, IDs, parents, flags, counts and categories. **Hypothesis:** named enclosed worlds and cave-like EDIDs are enclosed candidates; city EDIDs and Solstheim are exteriors; remaining worlds require review. Parentage alone is inadequate: Blackreach and Soul Cairn have Tamriel parents. The screen has 16 enclosed candidates, 7 city-labelled, 1 other exterior and 24 review. Classification, winning-record association, flags and the completeness of interior-worldspace coverage need independent validation; the pilot covers only interior CELL rows, not worldspace cells.

Source: `tools/testprobe/README.md` “View samples”: `@view <label>` emits `tag` equal to the label, not the entire wire command. The runner matches this actual protocol exactly with `requested:true`, records the full sample, stores it for `state(tag)`, and blocks unavailable samples with the reason. The brief's phrase “tag equals the exact request text” conflicts with the probe protocol; matching the wire command would never acknowledge a valid request. Source: `runner/executor.py`, `runner/schema.py`. The current fixed-camera implementation, initial heading and chosen F1 input are not established. Source: `tools/coverage/make_sweep_routes.py`: routes press then sample and assert wrapped yaw against base + 0/90/180/270 within a parameterized tolerance and assert `player.cell.editor_id`; F1, timing and base heading are placeholders. Paths and copied save are explicit parameters; ready with placeholder values is refused. Routes use COC and a second displacement where available, not a validated walk. Screenshot and geometric rays do not prove rendered visibility. No validated oracle, threshold, candidate hashes, calibrated headings or walking routes exist; all remain **NOT_RUN**. A complete sweep must include the independently identified interior worldspaces and a reviewed eligible set.

## Unclassified interior cells

Source: `census-wave2a.json`, all rows where classification is `unclassified` (ID and editor ID):

- `Dawnguard.esm:000804` — `VampireDungeon01`
- `Dawnguard.esm:000811` — `DLC1VampireCastleTest`
- `Dawnguard.esm:003358` — `testJoelDLCholdingCell`
- `Dawnguard.esm:00362A` — `testFalmer`
- `Dawnguard.esm:00373B` — `TestRSearsDLC01`
- `Dawnguard.esm:00384F` — `DLC1FalmerTemple01`
- `Dawnguard.esm:00477C` — `XTestRyanInt`
- `Dawnguard.esm:006429` — `DLC1SoulCairnReaperCell`
- `Dawnguard.esm:0064B5` — `TestCoryDLC01`
- `Dawnguard.esm:0066A4` — `DLC1LDUtilityCell`
- `Dawnguard.esm:007065` — `DLC1TestCellRJ`
- `Dawnguard.esm:007D94` — `testPhil`
- `Dawnguard.esm:007D95` — `testGiant`
- `Dawnguard.esm:00DF7D` — `DLC1DBTest`
- `Dawnguard.esm:015054` — `DLC1VQHoldingCell`
- `Dawnguard.esm:0154D5` — `DLC1VampireCastleZCell2`
- `Dawnguard.esm:018CE1` — `DLC1zFalmerValley04`
- `Dawnguard.esm:019DFE` — `DLC1VampireCastleBossRoom`
- `Dragonborn.esm:0142F3` — `DLC2Book03Dungeon`
- `Dragonborn.esm:0142F4` — `DLC2Book07Dungeon`
- `Dragonborn.esm:0142F5` — `DLC2POIBookLevel02`
- `Dragonborn.esm:016E1E` — `DLC2aaaMarkers`
- `Dragonborn.esm:01DA5D` — `DLC2AbandonedLodge`
- `Dragonborn.esm:01E9A2` — `DLC2POIBookLevel01`
- `Dragonborn.esm:01EDE2` — `DLC2Book02DungeonNEW`
- `Dragonborn.esm:01EDE3` — `DLC2POIBookLevel03`
- `Dragonborn.esm:0209C6` — `DLC2RRHoldingCell`
- `Dragonborn.esm:03109D` — `DLC2Book01Dungeon`
- `Dragonborn.esm:035A22` — `DLC2MoesringPass01`
- `Dragonborn.esm:03B065` — `DLC2Book01DungeonA`
- `HearthFires.esm:002FE4` — `BYOHHouseUtilityCell`
- `Skyrim.esm:013A66` — `HelgenTorolfsMill`
- `Skyrim.esm:013A67` — `HelgenHomestead`
- `Skyrim.esm:0151FA` — `SerpentsBluffRedoubt01`
- `Skyrim.esm:01521B` — `Icerunner01`
- `Skyrim.esm:0152BD` — `AzurasStarInterior01`
- `Skyrim.esm:0161EB` — `SkyHavenTemple`
- `Skyrim.esm:0161FF` — `GoldenglowEstate01`
- `Skyrim.esm:016DF7` — `MarkarthShrineofTalos`
- `Skyrim.esm:01FA4C` — `CWGuardTemplates`
- `Skyrim.esm:0221B5` — `CWMessageMarkers`
- `Skyrim.esm:028ACF` — `AzuraVoiceCell`
- `Skyrim.esm:02A976` — `MalynVarenVoiceCell`
- `Skyrim.esm:02D3E8` — `GoldenglowEstate02`
- `Skyrim.esm:034B75` — `vampireDreamworld03`
- `Skyrim.esm:034B94` — `vampireDreamworld02`
- `Skyrim.esm:034BA4` — `vampireDreamworld01`
- `Skyrim.esm:0362D8` — `ThalmorEmbassy03`
- `Skyrim.esm:036ED6` — `WhiterunGuardHouse`
- `Skyrim.esm:037EE0` — `SolitudeBlackSmith`
- `Skyrim.esm:0399D1` — `ReachwindEyrie01`
- `Skyrim.esm:039F67` — `WICourierCell`
- `Skyrim.esm:03A183` — `FalkreathGraveConcoctions`
- `Skyrim.esm:0414DD` — `JaphetsFolly02`
- `Skyrim.esm:05EAC7` — `aaaMarkers`
- `Skyrim.esm:06C3B6` — `AAADeleteWhenDoneTestJeremy`
- `Skyrim.esm:06DAA0` — `ThalmorEmbassy04`
- `Skyrim.esm:06F3EB` — `MainMenuCell`
- `Skyrim.esm:070469` — `SolitudeStables`
- `Skyrim.esm:078715` — `WerewolfPreload`
- `Skyrim.esm:07B7AB` — `DaintySload01`
- `Skyrim.esm:07C98B` — `ThalmorEmbassy01`
- `Skyrim.esm:07DCFC` — `ThalmorEmbassy02`
- `Skyrim.esm:087764` — `HighHrothgar`
- `Skyrim.esm:095C44` — `Sovngarde01`
- `Skyrim.esm:0A013B` — `GoldenglowEstate04`
- `Skyrim.esm:0A0359` — `WarehouseBookshelves`
- `Skyrim.esm:0A1793` — `SolitudeRedWave`
- `Skyrim.esm:0A95D6` — `MolagBalVoiceCell`
- `Skyrim.esm:0B1783` — `CTest`
- `Skyrim.esm:0B9BC6` — `Elsweyr`
- `Skyrim.esm:0BBB2F` — `Katariah01`
- `Skyrim.esm:0BBCB2` — `WEMerchantChests`
- `Skyrim.esm:0C0BBA` — `ThievesGuildHoldingCell`
- `Skyrim.esm:0C7348` — `HallOfTheVigilant01`
- `Skyrim.esm:0DDF7C` — `AnisesCabin01`
- `Skyrim.esm:0DEBCD` — `DrelasCottage01`
- `Skyrim.esm:0EA28B` — `UnownedCell`
- `Skyrim.esm:0EF325` — `FalkreathBarracks`
- `Skyrim.esm:0F7631` — `XTestGrantInt5`
- `Skyrim.esm:0FE47B` — `CWQuartermasterContainers`
- `Skyrim.esm:1037E7` — `WIDeadBodyCleanupCell`
- `Skyrim.esm:1052FD` — `LoadScreenTesting`
- `Update.esm:002F98` — `ccEEJSSE003_Interior01`
- `Update.esm:002F99` — `ccMDRSSE001_Interior01`
- `Update.esm:002F9A` — `ccBGSSSE016Interior02`
- `Update.esm:002F9B` — `ccBGSSSE016Interior01`
- `Update.esm:002F9C` — `ccBGSSSE043_Interior01`
- `Update.esm:002FCF` — `ccPlaceholder01Interior01`
- `Update.esm:002FD0` — `ccPlaceholder02Interior01`
- `Update.esm:002FD1` — `ccPlaceholder03Interior01`
- `Update.esm:002FD2` — `ccPlaceholder04Interior01`
- `Update.esm:002FD3` — `ccPlaceholder05Interior01`
- `Update.esm:002FD4` — `ccPlaceholder06Interior01`
- `Update.esm:002FD5` — `ccPlaceholder07Interior01`
- `Update.esm:002FD6` — `ccPlaceholder08Interior01`
- `Update.esm:002FD7` — `ccPlaceholder09Interior01`
- `Update.esm:002FD8` — `ccPlaceholder10Interior01`
- `Update.esm:002FD9` — `ccEEJSSE004_Interior01`
- `Update.esm:0030DF` — `ccPlaceholder25Interior01`
- `Update.esm:0030E0` — `ccPlaceholder24Interior01`
- `Update.esm:0030E1` — `ccPlaceholder23Interior01`
- `Update.esm:0030E2` — `ccPlaceholder21Interior01`
- `Update.esm:0030E3` — `ccPlaceholder22Interior01`
- `Update.esm:0030E4` — `ccPlaceholder20Interior01`
- `Update.esm:0030E5` — `ccPlaceholder19Interior01`
- `Update.esm:0030E6` — `ccPlaceholder18Interior01`
- `Update.esm:0030E7` — `ccPlaceholder17Interior01`
- `Update.esm:0030E8` — `ccPlaceholder16Interior01`
- `Update.esm:0030E9` — `ccPlaceholder15Interior01`
- `Update.esm:0030EA` — `ccPlaceholder14Interior01`
- `Update.esm:0030EB` — `ccPlaceholder13Interior01`
- `Update.esm:0030EC` — `ccPlaceholder12Interior01`
- `Update.esm:0030ED` — `ccPlaceholder11Interior01`

## Proposed shared-file changes

- Add the focused coverage tests to CI and record the pinned runner/probe revisions, measured timing, reviewed worldspace universe, oracle and candidate hashes before acceptance.
- Reconcile the ticket wording with the shipped probe's label-only acknowledgment tag; the implementation follows the probe contract.
