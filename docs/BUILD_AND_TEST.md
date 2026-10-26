# Build and test setup

Status (2026-10-25, P1-01 / DYL-150): Windows x64 build, packaging and isolated test profiles exist and are verified for **load-only skeletons**. No camera or UI behavior exists yet. Host facts: [evidence/p1/environment.md](evidence/p1/environment.md). Tested combinations: [compatibility/runtime-matrix.md](compatibility/runtime-matrix.md).

## Environment record

| Field | Current value | State |
| --- | --- | --- |
| Game storefront and executable | Steam Skyrim SE, `SkyrimSE.exe` 1.7.104.0, Steam build 24914197, sha256 `846efccf…1402f` | Observed |
| SKSE | SKSE64 2.3.1 (`skse64_1_7_104.dll` sha256 `d5158f8c…fb94e`, loader `cde6ae26…dd59`) | Tested (load only) |
| Address Library | Not used by the skeletons. DYL-151 ([PR #2](https://github.com/Sonoran-Solutions/SkyCRPG/pull/2)) recommends Address Library 13 for the feature providers; not acquired or tested here. | Pending ADR-001 |
| Camera/movement providers | None installed (DYL-151 desk candidates: SmoothCam 1.8.1, TDM 2.3.1) | Pending P1-02/P1-03 |
| UI/minimap/follower providers | None installed (DYL-151 desk candidates: SkyUI Community 6.11, Dragon's Eye fork 1.7.5, Infinity UI NG 2.1.3, LMU fork 1.3.4, Party Sheet 3.7) | Pending P1-02/P1-04 |
| CommonLib fork/commit | Not used by the skeletons (DYL-151 conditional candidate: alandtse/CommonLibSSE-NG 7.2.0 `7a60f4de`). If adopted, it replaces the hand-written ABI subset in `skeleton.h`. | Pending ADR-001 |
| Compiler / generator | GitHub-hosted `windows-2022`: Visual Studio 2022 Enterprise 17.14.42 (MSVC), CMake 3.31.6, Visual Studio 17 2022 generator, Python 3.12.10 | Verified in CI |
| Game host / Proton | Ubuntu 26.04.1, Proton Experimental `experimental-11.0-20261014` (DXVK v3.1.1-97), Steam Linux Runtime 4 | Tested (load only) |
| Mod manager and profiles | No mod manager; `tools/profile.py` manifest-tracked profiles | Verified |
| Fixture saves | Copied Whiterun exterior/Breezehome pair (2021 saves); load on 1.7.104 unverified | Route NOT_RUN |

## Source layout

| Path | Contents |
| --- | --- |
| `modules/camera/` | `SkyCRPG_Camera.dll` skeleton and `config/SkyCRPG_Camera.ini` |
| `modules/ui/` | `SkyCRPG_UI.dll` skeleton and `config/SkyCRPG_UI.ini` |
| `shared/include/skycrpg/skeleton.h` | Build-time header: the SKSE plugin ABI subset plus the load-log writer. Each DLL compiles its own copy, and nothing is exported between the modules. |
| `tools/package.py` | Deterministic archives, manifests, `SHA256SUMS`, independence checks |
| `tools/profile.py`, `profiles/profiles.json` | Isolated baseline/camera-only/ui-only/combined profiles on the Proton host |
| `tests/test_profile_tool.py`, `.github/workflows/profile-tool.yml` | Fixture checks for the profile tool (no game) |
| `.github/workflows/build.yml` | Windows x64 build, reproducibility check, packaging, artifacts |

The skeletons are plain SKSE plugins. Each exports `SKSEPlugin_Version`, pinned to runtime 1.7.104 with SKSE 2.3.1 as the minimum, plus `SKSEPlugin_Load`. They use no game addresses or structures, so they need neither CommonLib nor Address Library, and the DYL-151 stack choice stays open. On load, each writes its own log and reads its own settings file, then does nothing else.

| Package | Archive contents (rooted at `Data/`) | Log | Settings |
| --- | --- | --- | --- |
| SkyCRPG Camera | `SKSE/Plugins/SkyCRPG_Camera.dll`, `SKSE/Plugins/SkyCRPG_Camera.ini` | `My Games/Skyrim Special Edition/SKSE/SkyCRPG_Camera.log` | `SkyCRPG_Camera.ini` |
| SkyCRPG UI | `SKSE/Plugins/SkyCRPG_UI.dll`, `SKSE/Plugins/SkyCRPG_UI.ini` | `My Games/Skyrim Special Edition/SKSE/SkyCRPG_UI.log` | `SkyCRPG_UI.ini` |

## Build and package

A Linux build does not produce a Skyrim plugin. The root `CMakeLists.txt` rejects any non-Windows-x64 configuration.

**CI (canonical).** Pull requests that touch `modules/`, `shared/`, the CMake files, `tools/package.py` or the workflow run `build / windows-x64`. Docs-only changes do not trigger it. The job builds the PR head SHA. It then rebuilds in a second clean directory and fails if either DLL differs. Finally it packages and uploads `skycrpg-packages-<sha>` (archives, manifests, `SHA256SUMS`) and `skycrpg-pdb-<sha>`. The job timeout is 15 minutes; the measured run took about 35 s.

Fetch a build's packages onto the playtest host:

```bash
gh run download <run-id> -R Sonoran-Solutions/SkyCRPG -n skycrpg-packages-<head-sha> -D dist
```

**Local Windows (equivalent).** From a VS 2022 x64 developer prompt:

```bat
cmake --preset windows-x64-release -DSKYCRPG_SOURCE_COMMIT=<sha>
cmake --build --preset windows-x64-release
python tools/package.py --build-dir build/windows-x64-release --commit <sha>
```

`package.py` fails in three cases: two packages install the same path, either DLL imports a `SkyCRPG*` module, or a DLL is missing. MSVC `/Brepro` and `/PDBALTPATH:%_PDB%` keep the DLLs identical across build directories. The zips use fixed timestamps and sorted entries.

## Isolated test profiles

`tools/profile.py` runs on the Linux/Proton host. Profile staging lives outside the repo and outside Steam, in `/srv/storage/Projects/SkyCRPG/profiles/` (override with `SKYCRPG_PROFILE_ROOT`):

| Profile | Deployed on top of the vanilla game |
| --- | --- |
| `baseline` | SKSE64 2.3.1 only |
| `camera-only` | SKSE + SkyCRPG Camera |
| `ui-only` | SKSE + SkyCRPG UI |
| `combined` | SKSE + both packages |

Rules the tool enforces:

- It verifies the recorded 1.7.104.0 exe hash before deploying.
- It validates the whole deployment before the first write. SKSE and archive entries must be relative paths, free of `..`, backslashes, drive letters and symlinks, and they must resolve inside the game directory (SKSE executables) or `Data/` (everything else). No two sources may claim the same path, compared case-insensitively as Wine does. It never overwrites a file it does not own.
- Every file and directory is recorded in the `profiles/active.json` journal **before** it is created. Files are written to a `*.skycrpg-partial` temporary and hard-linked into place without overwriting. A deploy interrupted at any point is recovered by a normal `undeploy`.
- `undeploy` removes only paths whose content still matches what the tool wrote. That includes `SkyrimCustom.ini`, which must match the exact tool-written bytes. Any changed path is kept and reported. The command exits 3 and the profile is marked `needs_reconciliation`, which blocks the next deploy. Inspect the kept paths, then run `undeploy --release-kept`.
- Only one profile is deployed at a time. `launch` refuses while any `SkyrimSE.exe` is running.
- Saves are isolated through a tool-owned `SkyrimCustom.ini` (`SLocalSavePath=SkyCRPG\<profile>\Saves\`). This lies outside the Steam Cloud root `My Games/Skyrim Special Edition/Saves`, so test saves are not synced and the owner's saves are only ever read (copied by `init`).
- Every launched process carries a `SKYCRPG_RUN_ID` environment tag. Cleanup stops only tagged processes and then confirms they are gone.
- The game is launched like Steam does it: the Steam Linux Runtime 4 entry point runs Proton Experimental `waitforexitandrun skse64_loader.exe`, with Steam running. Steam launch options and the game launcher are not modified.

SKSE is an external prerequisite. Download it manually from Nexus (mod 30379, archive sha256 `7bad616e…ae250` for 2.3.1), then extract it into its own directory, e.g. `profiles/prereqs/skse64_2_3_1_extract/`. Its files are never committed.

```bash
python3 -I tools/profile.py init baseline --save <owner save file name> --save <another>
```

```bash
python3 -I tools/profile.py deploy camera-only --skse /srv/storage/Projects/SkyCRPG/profiles/prereqs/skse64_2_3_1_extract/skse64_2_03_01 --dist dist
```

```bash
python3 -I tools/profile.py launch --until-loaded --timeout 300 --settle 30
```

```bash
python3 -I tools/profile.py undeploy
```

`launch --until-loaded` records `LOADED` only when positive markers newer than the launch are present while `SkyrimSE.exe` runs. The markers are:

- SKSE 2.3.1 initialized for runtime `01070680`;
- data load reached (message 8);
- `plugin <package>.dll (...) loaded correctly` for each package in the profile;
- each module log's `runtime check: PASS`.

After `--settle` seconds, the wrapper and `SkyrimSE.exe` must still be running and the markers are re-checked before the run is stopped. Otherwise the result is `EXITED` (the wrapper or game ended before settling completed, or the interactive exit was unsuccessful), `TIMEOUT`, or `LOAD_FAILED` (logs exist but a marker is missing or rejected); `run.json` lists the missing markers. `launch --interactive --timeout 1200` waits for the player to quit, which is the 20-minute route budget; it succeeds (`QUIT`) only if the load markers were verified before the player quit and the wrapper exits with status 0. Nonzero exits fail even after a verified load; `run.json` preserves the process exit code. Each run is collected under `<profile>/runs/<timestamp>/`, with `run.json` (command, PID, result, log hashes, leftover processes), SKSE/module logs and the Proton log. Proton logs contain local paths; keep them out of the repo.

Exit status: 0 success; 1 error, refused deploy or unsuccessful launch; 2 processes from the run still alive after cleanup; 3 undeploy kept changed paths. Automation must treat any nonzero status as a failed check.

**Fixture checks.** `tests/test_profile_tool.py` runs the tool against a temporary fake game tree and fake launch processes; it never touches a real install. It covers:

- traversal, absolute, backslash, colliding and symlinked paths are refused with zero writes;
- a deploy/undeploy round trip leaves the game tree unchanged;
- a preexisting identical file is left in place;
- an edited `SkyrimCustom.ini` or a changed deployed file is preserved;
- an interruption at each write stage is recoverable;
- launch outcomes: verified load, early exit, crash during settle, timeout, rejected plugin, stale logs, interactive quit with and without a verified load, and nonzero interactive exit after load.

The `profile-tool` workflow runs these checks on `ubuntu-24.04` when the tool, profiles or tests change (10-minute budget). To run them locally:

```bash
python3 -I -m unittest discover -s tests -v
```

2026-10-26 launch-result re-review correction: **PASS**. The two new fake-process regressions reproduced the false `LOADED`/`QUIT` successes on `e053fa24` before the fix. With the correction, both return `EXITED`, `success=false`, and process exit code 1. Python compilation and all **22/22** fixtures passed locally (12.216 seconds for the full suite, within the 10-minute budget). No build or game run was performed for this tool-only correction. The rewritten tool's first live use and the owner baseline route remain **NOT_RUN** under DYL-150; the original runtime evidence below still belongs to `ec34e4d`.

Side effects outside the tool's manifest: on its first launch the game itself created `Skyrim.INI` and `SkyrimPrefs.ini` in My Games, and `Creations/`, `Mods/` and `Data/ShaderCache/` in the install. That is normal first-run behavior. Collected module logs also remain in `My Games/.../SKSE/` between runs; `launch` only accepts logs newer than its own start.

## P1-01 verification (2026-10-25)

Source and artifacts: commit `ec34e4d717ed6ad145341e6d543255fbd2266fec`, [CI run 40246039569](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40246039569).

| Artifact | SHA-256 |
| --- | --- |
| `SkyCRPG_Camera-0.0.1-ec34e4d.zip` | `14d6afad7bbd23426d659dc47994d70882e3b2e55f88fd9f86fec761d038738b` |
| `SkyCRPG_Camera.dll` | `5862c085bf4d25daecbdbb97655576eb03fc2ba1fd91e6a4f1178e0026049dc6` |
| `SkyCRPG_UI-0.0.1-ec34e4d.zip` | `ad9696f854bc9e2d3c3712904384a7516f63c48cbded867ca773a3d077d6bbd4` |
| `SkyCRPG_UI.dll` | `ac732bb3fb300686a110fb255e1addf4fa6a093c80753eb11cc103e3c70087c2` |

| Check | Result | Evidence |
| --- | --- | --- |
| Windows x64 build + package from clean checkout | PASS | CI run above; both DLLs byte-identical across two build directories; imports are `KERNEL32`, `SHELL32` and `ole32` only |
| Baseline (SKSE only) reaches data load | PASS (load only) | `skse64.log`: SKSE 2.3.1 `01070680` init, no plugins, message 8 (data loaded) dispatched |
| Camera package loads alone | PASS (load only) | `SkyCRPG_Camera.dll ... loaded correctly`; module log shows commit `ec34e4d`, SKSE 2.3.1, runtime 1.7.104, its own ini found |
| UI package loads alone | PASS (load only) | `SkyCRPG_UI.dll ... loaded correctly`; Camera not present in plugin scan |
| Combined minimal load | PASS (load only) | Both plugins loaded correctly (handles 1 and 2); separate logs |
| Baseline outdoor/interior route | NOT_RUN | Owner will play it; procedure below |

The load checks above ran with the profile tool as of `ec34e4d` plus the log-collection fix. The later safety/reporting rewrite (review of PR #1) has not been rerun against the game. Its positive load markers were checked offline against the four real run logs: all four satisfy their own profile's markers, and camera-only logs fail the ui-only markers. Log excerpts and hashes are in [environment.md](evidence/p1/environment.md#p1-01-load-evidence). A load PASS proves only that SKSE loaded the plugin and the game reached data load under this stack. It says nothing about camera behavior, inventory integrity or provider compatibility.

## Baseline route (owner, NOT_RUN)

Setup: `deploy baseline`, then `launch --interactive --timeout 1200`. Use keyboard and mouse at 1920x1080 windowed (game default) with default graphics preset settings.

1. From the main menu, **Load** `Quicksave0_…WhiterunWorld…` (Whiterun exterior). If the 2021 save fails to load, note the failure and start a new game instead; in that case record the save as unusable.
2. Walk to Breezehome's door and note any hitching. Enter the house; this is the load transition.
3. Walk the ground floor, go up the stairs and back down, and exit to the exterior.
4. Draw a weapon and sheathe it. Open and close the inventory (Tab/I), then quit to desktop.
5. Record start/end time, whether each step worked, visible FPS problems and anything unusual. The run's `run.json` and logs are collected automatically. Also copy the new save from `My Games/Skyrim Special Edition/SkyCRPG/baseline/Saves/` into the profile's `saves/` as the P1-03/P1-04 fixture.

## Remaining setup

- Address Library, CommonLib and providers: added only after DYL-151/ADR-001 selects them, with versions pinned here and in `dependency-lock.md`.
- Controller: none on this host; controller checks stay NOT_RUN until one is attached.
- Never upload owned Skyrim binaries, personal saves, Proton logs or third-party archives to CI or the repo.
