# DYL-166 / P3-04 minimap and compass evidence

Base `15bdad8`; head `<head SHA>`; CI `<CI run>`; UI DLL `<DLL sha256>`; HUD SWF `<SWF sha256>`. Target: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SkyUI Community 6.11, Proton; DEM 1.7.5, Infinity UI NG 2.1.3, LMU 1.3.4, Common + Line-17. This worker did not run the game.

## Design and owner ruling

The [2026-10-26 decision](../../DECISIONS.md) approves option (a): exact preflight and the DEM clip attached, visible and in bounds in normal gameplay; release on load/cell/menu/hide/teardown, accepting stale-frame risk. The provider has no supported fresh-render signal. The 500 ms same-cell settle allows attachment time; it does **not** prove a fresh render. Lost navigation is not accepted.

The candidate checks in-process provider DLL names and loaded-file hashes, plus the two loose SWFs, once after data load. Portable pins are checked against the preflight script. The bridge copies task-thread gameplay observations with a 500 ms expiry, and uses the existing menu-focus gate. The HUD tests visible ancestors, bounds and mode before leasing compass visibility; it restores the native-desired value. `bReplaceCompass` defaults off on missing, malformed or unsupported settings; the shipped file sets it on. Shout clips are untouched, but Survival temperature resides inside the compass and is hidden with it. Exact file checks do not prove BSA winner resolution, ABI compatibility or a rendered frame.

## Automated checks

Owner: DYL-166 worker, Linux. The job plan and logs are in `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/`; every test has a 600-second timeout and exit status as completion signal. Windows DLL and actual SWF builds **NOT_RUN**.

- **PASS:** `for name in minimap_policy favorite_slots menu_focus resource_snapshot hud_settings; do timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/${name}_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/${name}_test && timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/${name}_test; done`.
- **PASS:** `for name in minimap_compass_contract hud_contract navigation_contract; do timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/${name}.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11; done`.
- **PASS:** `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p '*test.py'` and the same command with `-p 'test_*.py'`.

## Acceptance ledger

| Criterion | Result | Remaining evidence |
| --- | --- | --- |
| UI-only and combined working map without duplicate compass | **NOT_RUN** | Both profiles and generated artifacts need runtime checks. |
| Interior/exterior navigation and quest overlap | **NOT_RUN** | Transition, settling and layout route. |
| Missing/disabled fallback and supported markers | **NOT_RUN** | Provider-absent and disabled routes, marker behavior. |
| Configuration and permissions | **NOT_RUN** | Final package and rights review; no provider binaries added here. |

## Runtime route — all NOT_RUN, 20-minute budget

Use copied saves and isolated profiles. Record host, executable, source commit, package hashes, dependency versions, logs, process handle and cleanup. Run UI-only and combined profiles separately.

1. In an exterior, verify the map updates, compass disappears only after settling, and shout meter remains; inspect quest text and active effects for overlap.
2. Press K to hide and show the map; verify native compass returns and replacement reacquires after settling. Check L and relevant input focus.
3. Enter an interior and return to the exterior; check compass during transitions and reacquisition after settling, including any stale frames.
4. Open and close a blocking menu, then save and load the copied save; verify release and reacquisition each time.
5. Restart with `bReplaceCompass=0`: verify native compass stays. Separately test the provider-absent profile and verify usable native navigation.
6. Inspect Survival temperature behavior and record layout, marker and teardown results. Preserve logs and verify cleanup.

## Limitations

Native MSVC compilation, generated SWF execution and gameplay remain unverified. The bounded clip check cannot establish current map imagery; stale frames are explicitly accepted by the owner. Temperature is hidden with the compass. Layout and marker behavior remain open. No game, Steam or Proton was launched and no commit or push was made.

## Runtime route — 2026-10-26

All runs used the UI-only profile (175 files, pinned DEM/Infinity UI/LMU Line-17 + Common, preflight `EXACT`), Steam 1.7.104.0, SKSE 2.3.1, Address Library 13 and Proton Experimental 11, with the controller off. The save was the copied test save `Quicksave0_…_RiverwoodSleepingGiantInn_…_20261026144253`. Every run used the native package from `9cb576f` ([Windows run 40457081211](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40457081211) **PASS**): ZIP `3b3152529ba2151ae27f9a19cd32bf18058619b0c88c9e5edec8e0cd3a850379`, DLL `603049abfdfcd711f7de01cfa078536775664725af0f4f28c117464fd79d0c03`, `bReplaceCompass=1`. The runs differ only in the locally built HUD movie, because the later fixes changed ActionScript and the builder only. In every run the DLL logged all five provider files `exact`, `Minimap preflight exact=1`. Every run exited normally with no watchdog intervention, and every restoration **PASS**ed (no changed candidate files, no active profile, no Skyrim process). Records are under `/srv/storage/Projects/SkyCRPG/session-p3/p3-map-166*/`.

| Run | HUD movie / source | Owner observation | Diagnosis and fix |
| --- | --- | --- | --- |
| `de93b86ff0b54fff8949a67aeead89db` (60 s) | `ac36c2ef…` / `9cb576f` | **FAIL:** compass never disappeared | Fail-safe: the lease's 4-unit edge margin rejected DEM's corner-flush frame (screenshot ≈ x 1278, y 3 on the 1280×720 HUD). The run had no lease logging yet. `ac577fc` allows an overhang and logs lease state changes to `skse64.log`. |
| `e0a74f54e995493cba4d1e7f4a6e7135` (60 s) | `6cc7834c…` / `ac577fc` | **FAIL:** compass never disappeared | Log: `bounds` with live clip 1147..1338 × 16..175. DEM's quest pointer and distance label overhang the edge. `2f6bbef` judges placement by centre in the upper-right quadrant plus ≥50% of the area on screen. |
| `815386e841184693b0c87579203a32cd` (90 s) | `608bd786…` / `2f6bbef` | **FAIL:** compass flickered, never stayed hidden | Log showed `suppressed`, but the engine calls `SetCompassAngle` every frame and writes `Compass._visible`, while the lease re-hid only at 100 ms samples. `c4a39e1` keeps the compass hidden inside that per-frame write while the lease is held. A frame-level contract fails on the previous hook. |
| `7571da8e3f4c42fbb4cfd8eaa1a6f5d7` (174 s) | `2de109b3ca9428c03f694268838e3125d0491e7bf1d1429268f384426e6729a4` / `c4a39e1` | **PASS (owner-reported): everything passed.** The compass hid with no flicker after load and after menus, interior/exterior and save/load, and returned on K | UI log `c84bc2cf…`, `skse64.log` `4ac35d2d…`. The lease cycled `menu`/`hidden`/`stale-sample` (release) → `settling` → `suppressed` across load, menus, K hide/show and cell changes. Live clip 1024.5..1285.9 × −0.9..217.7. No logged state left the map hidden with the compass suppressed. |

None of the three failures left the player without navigation: each kept the native compass visible.

Updated ledger:

| Criterion | Result |
| --- | --- |
| UI-only working minimap without duplicate compass | **PASS (owner-reported, bounded)**, run `7571da8e`, owner ruling option (a) |
| Combined profile | **PASS (owner-reported, bounded)**, run `5298264e`; see below |
| Exterior/interior transitions keep usable navigation | **PASS (owner-reported, bounded)**: compass during load, map re-acquired then suppressed |
| Quest text does not overlap the map | Owner reported no issue; the active-effect overlap at 1080p remains a DYL-168 measurement |
| Disabled (`bReplaceCompass=0`) keeps compass | **PASS (owner-reported, bounded)**, run `6bf91540` |
| Provider absent keeps compass | **PASS (owner-reported, bounded)**, run `a16a95c6` |
| Permissions / no bundled third-party binaries | Source-only; no provider files added |

### Follow-ups — 2026-10-26

Both runs used the same DLL (`603049ab…`, source `9cb576f`) and HUD movie (`2de109b3…`, source `c4a39e1`) as the passing run `7571da8e`. Both exited normally with no watchdog intervention, and both restorations **PASS**ed.

| Run | Variant | Owner observation | Log evidence |
| --- | --- | --- | --- |
| `6bf91540692f4083bc8c74973a851950` (33 s) | Package ZIP `e646d4f5169afb2fb31a7b88520784450af56fc5c863b05be95985933e0bae59`: the `9cb576f` package with only `[Minimap] bReplaceCompass=0` changed (DLL byte-identical); exact provider stack | **PASS:** minimap and compass both visible | UI log `fd5aa767…`; install `exact=1`; the lease logged only `disabled`, `menu`, `hidden` and `stale-sample`, never `suppressed`. |
| `a16a95c6690844f39de2844ba9123d99` (39 s) | Package `9cb576f` (`bReplaceCompass=1`); all Dragon's Eye, Infinity UI and Local Map Upgrade files removed, including translations and notices (141 deployed files); preflight `ABSENT` | **PASS:** no minimap, compass present, everything normal | UI log `71208316…`; all five pins `unavailable`, `exact=0`; the lease logged only `no-map`; `skse64.log` has no provider entries. |

An earlier provider-absent deployment that still contained the providers' translation text and notices was restored before launch and never run (`p3-map-166-absent-aborted`).

## Combined Camera + UI route — 2026-10-27

**PASS (owner-reported, bounded).** The owner reported that every step passed.

| Field | Value |
| --- | --- |
| UI package | `SkyCRPG_UI-0.0.1-9cb576f.zip` `3b3152529ba2151a…` (DLL unchanged through `75e2641`); `hudmenu.swf` `2de109b3…` |
| Camera package | Main's camera tree: `SkyCRPG_Camera-0.0.1-28aae27.zip` `b97997f62df7c914…`, Windows run [40450213582](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40450213582) |
| Stage | Phase 2 `stage-combined-favorites` (SmoothCam, True Directional Movement, MCM Helper, SkyUI, the map trio) with only `hudmenu.swf` replaced; profile `combined`, 214 files |
| Host | Steam 1.7.104.0 (exe `846efccf…`), SKSE 2.3.1, Address Library 13, Proton Experimental 11; **controller (8BitDo XInput)** — corrected 2026-10-27, see note below |
| Save | Copied combined-profile `Save2 … Tamriel … 20261026185830` |
| Run | `5298264e872545c0ab28f5b07becc551`, about 2.5 min, normal quit, watchdog did not intervene (minimum 13.3 GiB available) |
| Logs | `SkyCRPG_UI.log` `5d429e77…`: preflight `exact=1` on all five paths. `skse64.log` `22e42cec…`: 32 lease lines (the cap), cycling `settling` → `suppressed`, with `hidden` (K) and `menu` releases, and `stale-sample` after loads. `SkyCRPG_Camera.log` `f4d5e85d…`: `no camera transform writes`, SmoothCam sole transform writer, no failures |
| Cleanup | Restoration **PASS**: no active profile, no Skyrim process. The only changed file was our own deployed `SkyCRPG_Camera.ini`, which the camera module rewrote with the same `fZoom` (0.65 → 0.650000, blank lines dropped). It is a package file, preserved under `p3-map-166-combined/after-owned-files/` and removed on restore; no user file changed. |

Route:
1. settle (map appears, compass hides);
2. camera movement, framing and lock-on unchanged;
3. K hide/show;
4. a menu;
5. an interior/exterior transition;
6. save and load.

Combined-profile observation: DEM's clip measured `1024.5,-0.9` to `1285.9,217.7` here, compared with `1136,0` to `1280,134` in the UI-only runs. Its centre is in the upper-right quadrant and most of it is on screen, so the lease accepted it. The lease cap of 32 lines was reached, so later transitions were not logged; the owner observation covers them.

**Input device correction (2026-10-27).** The owner reports that all of the 2026-10-27 runs, including `5298264e` above, were played mostly with the controller (8BitDo Ultimate, XInput), although each launch was requested as "controller off". On this host, Skyrim ignores keyboard/mouse gameplay input while the controller is on, so this route is controller evidence. The observed results are unchanged.
