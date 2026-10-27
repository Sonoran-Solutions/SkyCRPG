# DYL-166 / P3-04 minimap and compass evidence

Base `abd5735c98fffd6ad5202e3f674f7b22697cc859`; head `<head SHA>`; CI `<CI run>`; UI DLL `<DLL sha256>`; HUD SWF `<SWF sha256>`. Target: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SkyUI Community 6.11, Proton host; DEM 1.7.5, Infinity UI NG 2.1.3, LMU 1.3.4, Common + Line-17. No game run by this worker. Prior Phase 2 evidence is not a pass for this candidate.

## Capability investigation and design

**BLOCKED:** no supported provider interface proves the current map image has rendered in the current location. Infinity UI's `include/API.h:14-68` publishes patch/loading events, and `source/Hooks.cpp:111-115` dispatches extension initialization, not render completion. DEM `source/MessageListeners.cpp:57-105,145-215` listens for Infinity UI/LMU and recognizes the minimap patch; logs describe attachment, not live health. DEM `include/MiniMap.h:112,192` names `_level0.HUDMovieBaseInstance.Minimap` and defines internal `IsReady()` as a `localMap_` pointer check. `source/MiniMap.cpp:84-86,136-143,1488,1747` binds MapClip, initializes it, changes `updateScaleform`, and calls rendering; none is a public fresh-frame signal. `source/WorldRendering.cpp:285-328` explicitly pauses redraw while settling or throttled; visible clip/MapClip and a previously drawn frame can therefore be stale. Logs and INI settings are not a live functional contract. No supported public DEM readiness export, broadcast, HUD variable or documented function was found.

Safest future contract: exact resolved preflight, loaded compatible provider, attached/visible in-bounds map, allowed gameplay and **a provider-owned fresh-render generation for the current HUD and world**, invalidated on hide, load, transitions, death and teardown. No such generation exists today. This candidate keeps the compass native and does not register an invented bridge. `compass_policy.h` exhaustively expresses the necessary conditions, and functional readiness is always false. The AS lease is an inert preparatory component; its synthetic true-path harness does not prove runtime safety or activate replacement. Vanilla SkyUI `source/actionscript/Vanilla/HUDMenu.as:109,140,627-629` identifies `CompassShoutMeterHolder.Compass` and separate `ShoutMeterInstance`; vanilla also writes compass visibility, requiring further integration review. The shared patch inserts the inert initialization and leaves settings/adapter unimplemented.

Preflight checks SHA-256 of five resolved loose files against extracted Line-17/Common assets. Entire DEM absent and explicit replacement-disabled with absent or exact stack are allowed; partial and mismatched stacks are refused even when disabled. Infinity UI/LMU alone can be independently installed. This is not BSA resolution, ABI validation, live load or a launch mechanism. Provider layout retains architecture's `uAnchor=1`, `fScale=0.5`, `bShowOnGameStart=1`, `bCompassRing=0`, and LMU `bLocalMapBorder=0`; no blind offset is proposed because active-effect and quest bounds require DYL-168 measurement. DEM defaults `iHideKeyCode=37` (K) and `iZoomToggleKeyCode=38` (L), distinct from F6/F8 and 1–8; actual focus interactions remain NOT_RUN. No markers, assets or binaries are synthesized or bundled.

## Automated checks

Owner DYL-166 worker, Linux, each command timeout 600 seconds; exit is completion signal and timeout owns cleanup. Logs and binaries: `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/`; job plan is `job-plan.txt`.

- **PASS:** for `name` in `minimap_policy favorite_slots menu_focus resource_snapshot hud_settings`, `timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/${name}_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/${name}_test`, then `timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-166/${name}_test`. The 64 policy combinations cover enabled, present, compatible, ready, gameplay and transition.
- **PASS:** for `name` in `minimap_compass_contract hud_contract navigation_contract`, `timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/${name}.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11`. Synthetic lease checks include prior hidden state, competing visibility, reinit and shout meter; no GFx runtime claim.
- **FAIL (invocation):** `timeout 600 python3 -I -m unittest modules/ui/tests/minimap_preflight_test.py` and `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest modules/ui/tests/test_scaleform_contract.py` both fail importing `modules` under isolated Python 3.14. No tests ran.
- **PASS (diagnostic with corrected discovery):** `timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p minimap_preflight_test.py` (1 fixture test: absent, disabled, partial, mismatch, exact); `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p test_scaleform_contract.py` (14 tests).
- **PASS:** `git apply --check /srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-166.patch`. Windows DLL and actual SWF build **NOT_RUN**.

## Acceptance ledger

| Criterion | Result | Evidence / remaining work |
| --- | --- | --- |
| UI-only and combined working minimap without duplicate compass | **BLOCKED** | Functional readiness missing; native compass remains, duplicate expected. Both gameplay profiles NOT_RUN. |
| Exterior/interior navigation and no quest overlap | **NOT_RUN** | Transition and layout route required; active-effect overlap known from P2. |
| Missing/disabled restores navigation; supported markers documented | **NOT_RUN** | Preflight fixture PASS; runtime absent/disabled and actual provider marker behavior require checks. No marker support inferred. |
| Configuration and permissions, no unrelated binaries | **NOT_RUN** | Source-only changes, no bundled provider binaries; final package and permission review remain. |

## Runtime route — all NOT_RUN, budget 20 minutes

Use copied saves, isolated UI-only profile and exact stack. Record host, source, executable, dependency versions, package hashes, run/log handle and cleanup. Current candidate intentionally keeps native compass visible throughout; no `bReplaceCompass` setting exists yet.

1. Exterior road near Riverwood: confirm map content updates with movement, native compass and shout meter remain, inspect quest text and active-effect icons for overlap.
2. Tap K: hidden map and native compass; tap K again: visible map and native compass. L toggles zoom; check F6/F8 and hotbar 1–8 remain independent when keyboard input is available.
3. Open/close inventory and map menus: no unwanted control action, compass remains native; return to gameplay and observe map.
4. Enter Sleeping Giant Inn and return outdoors: during loading and after transition native compass remains; map reacquires current interior/exterior imagery. Record any stale image or overlap.
5. Save/load copied save: compass remains native, map reacquires; inspect shout meter. Repeat relevant visibility/overlap checks on combined profile as a separate bounded run.
6. Separate provider-absent profile (all DEM map files omitted): preflight ABSENT and native compass, no minimap; check startup. Disabled-preflight profile uses `--disabled` and native compass remains; a real `bReplaceCompass=0` route is **BLOCKED** until a setting exists.

## Limitations and decisions

Replacement, immediate lifecycle handoff, map bounds and actual gameplay are not established. No Windows engine API was used or compiled; adapter deliberately omitted pending a supported readiness contract. A provider-owned render-generation interface or a separately reviewed bounded bridge is required before replacement can be enabled. Layout and marker behavior need observed evidence. No owner decision is presumed; integrator should keep DYL-166 open and coordinate the readiness contract and DYL-168 layout.
