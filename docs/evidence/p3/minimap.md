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
