# DYL-165 / P3-03 follower HUD evidence

- Base: `abd5735c98fffd6ad5202e3f674f7b22697cc859`; candidate head: `<head SHA>`; CI: `<CI run>`; UI DLL SHA-256: `<DLL sha256>`; HUD SWF SHA-256: `<SWF sha256>`.
- Intended runtime: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0 (`7a60f4de`), SkyUI Community 6.11, Proton. Local host: Linux; game, Windows build and package were not run by this worker.
- Contract: one status-only follower, Skyrim owns actor state, no input or AI. A task-thread scan of high-process teammates selects one humanoid; a generation and handle prevent actor pointers crossing samples. Load/new-game, player death and cell changes invalidate tracked state. Dead handles are tombstoned until invalidation. Health uses native current/effective maximum; no magicka/stamina is inferred. Cached copies expire after 500 ms; AS requests at most 10 Hz. The original AS silhouette needs no provider assets. The integration patch adds the entry points and replaces the placeholder; it is not applied in this tree.

## Provider inspection (read-only)

Extracted Party Sheet 3.7 archive SHA-256 `f980e34cd2f0a32abe0b5150c0d673017d53399df6a7dd5240c96f53ed21f83d`. `objdump -p` on `SKSE/Plugins/SkyrimPartySheet.dll` (`843cb8d40024cfdd6f85d6904d528aecc6e682f2cf6c0fa594fd18ccd392ee5f`) shows only `SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version` exports. `strings` shows `PartySheetStateAPI.cpp`, `RequestHUDHidden`, `DispatchInterface` and a v1 interface dispatch: evidence of a visibility-state interface, not a verified actor-data or portrait contract. The extracted payload has one DLL, four INIs, six JSONs, one translation, one font, 125 JPGs and 439 PNGs; no SWF, Papyrus source/compiled script, API header or license/readme was found. `party_whitelist.ini` (`c5c03a6288d005209862502b2eb1518e28235f40a29b57c94cfb13a5a4809cb4`) describes form-based Party Sheet selection; `party_allies.ini` (`29fd260b5cd6ee8eb8983ae8e5a52e2392e5e7602ddf23e53872bf079499cdfc`), `party_blacklist.ini` (`a903920cb31e6bcb096b85a512103fac09aef4b587b729ef4839d99592f67613`) and `activeeffects_blacklist.ini` (`1cc51e6f167e6c1e6f0ee90d2dec8cea8b5e7a3c4958c9651f190f4cec405f07`) configure its display/filtering, not a consumable API. `PartySheet_Widgets.json` hash `16751cb54cb3d2729d1e9a2161a6849ef7954cf3e3100ad253a6f129a85554ee`. Example portrait hashes: `Lydia.jpg` `e070da4eea1a5ea3e7256dfad167832298bd83327d103f64141bd51c69b1ae3d`, `default.jpg` `c978fca451deca049e96373a2243cfd80733c5345d1134f47708af88f3aa8bbb`. Their individual provenance and permission are not verified; dependency lock separately records asset terms and source restrictions. None are copied. Party Sheet remains optional.

## Local checks

Owner: DYL-165 worker; purpose, commands, 600-second timeouts, completion signals and logs: `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-165/jobs.txt`. PASS portable `follower_policy`, `favorite_slots`, `menu_focus`, `resource_snapshot`, `hud_settings` compiled with `timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/<name>_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-165/<name>_test` and each binary run under `timeout 600`. PASS `timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/follower_panel_test.js`; PASS same Node command for `hud_contract.js` and `navigation_contract.js` with `modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11` arguments. PASS 14 base-tree tests: `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p test_scaleform_contract.py`.

The first batch stopped with FAIL (incorrect runner filename `follower_panel.js`); corrected invocation passed. The requested `python3 -I -m unittest modules/ui/tests/test_scaleform_contract.py` failed because isolated mode omits the repository root; discovery above passed. Additional integrated-patch diagnostic: **FAIL** old HUD harness reads `hud-init.as.inc` directly without the builder's include substitution; **FAIL** one old Python test requires the removed inline placeholder assignments. The integrator must update these tests, run them against the assembled HUD, and verify teardown there. Standalone panel harness exercises live, missing, unsupported, dead, empty, silhouette, long names and mode visibility, but not assembled teardown. Tests are mocks; none establishes engine behavior. `git apply --check` for the requested integration patch: PASS.

CommonLib 7.2.0 headers declare `ProcessLists::GetSingleton`, `highActorHandles`, `ActorHandle::get/reset`, `Actor::IsPlayerTeammate`, `IsDead`, `IsHumanoid`, `Is3DLoaded`, `GetParentCell`, `GetDisplayFullName`, `AsActorValueOwner()->GetActorValue` and `GetActorValueMax`. Actual Windows compile/link and engine semantics remain **NOT_RUN**. Scanning loaded high-process teammates cannot establish that an absent actor was dismissed rather than not loaded, and teammate state can include quest allies or framework actors; unsupported configurations and cross-cell reacquisition need runtime scrutiny. No universal follower-framework support is claimed.

## Acceptance ledger

| Criterion | Result | Evidence / remaining gate |
|---|---|---|
| Vanilla follower appears/disappears, live status and silhouette | NOT_RUN | Mock policy/panel PASS; real recruit, injury, dismiss and unload route pending. |
| No stale/dead handle dereference; multi-follower labeled untested | NOT_RUN | Generation, expiry, tombstone and portable classifications reviewed; real lifecycle and multi-follower verification pending. |
| Status only, no movement/combat/dialogue commands | PASS (source only) | Lane panel and adapter do not issue actions; runtime coexistence remains NOT_RUN. |
| Windows CI, integrated SWF/harness and package hashes | NOT_RUN | Integrator must resolve the two integrated test failures above. |

## Copied-save route — all NOT_RUN, budget 20 minutes

Use an isolated UI-only profile on the pinned runtime, Party Sheet absent, a copied save where Lydia is available as follower and no other teammate is recruited. Record profile, host/Proton, executable and dependency versions, candidate source commit, CI URL, DLL/SWF/package hashes, save fixture and screenshots/video with timestamps. Do not alter the original save. Select Lydia by clicking her in the console and verify the displayed name before any command; no reference ID is assumed here.

1. **NOT_RUN** Recruit Lydia through dialogue. Observe her name, original silhouette and plausible current health; check the native HUD mode and menu visibility.
2. **NOT_RUN** On the disposable save, select Lydia in console, verify selection and run `damageav health 50`, or use combat. Close console and compare the health percentage with native actor values; record update latency and no invented values.
3. **NOT_RUN** Leave and re-enter a cell with her; observe invalidation, unknown/empty during unavailability and correct reacquisition. Note if she is left behind, and any stale values.
4. **NOT_RUN** Dismiss through dialogue; observe slot disappears. Recruit again, then use a disposable nonessential follower fixture or selected actor `kill` if applicable; record clearing within one sample and stability after cell/load transitions. Essential Lydia may not die; use a disposable nonessential vanilla humanoid fixture instead, verifying the console selection by name.
5. **NOT_RUN** Reload the copied save and start a new game or return to the fixture; verify old name/status never persists. Repeat with Party Sheet absent (default) and verify no missing-provider errors. If an explicitly configured two-follower fixture exists, record the untested label and no wrong status; do not install a framework solely for this route.

Owner decisions needed: none proposed; runtime acceptance and any expansion beyond one vanilla humanoid require owner evidence/decision. In-game and integrated-build gates remain open.

## Runtime route — 2026-10-26

**PASS (owner-reported, bounded).** The owner reported that every step below passed. Party Sheet was absent, which is the default stack.

| Field | Value |
| --- | --- |
| Source / CI | `ed22860e5947f36099db49020a16118a278b9994`, Windows [run 40454404866](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40454404866) **PASS** |
| Package | `SkyCRPG_UI-0.0.1-ed22860.zip` `cef6cb7b718077c925bb8725b0526519e1202209294a6e5261ac507e7e9f9826`; DLL `ef6f65302a9c8b5722971aa92b2b0df2a73e54e82470d300c4392035051d705d` |
| Movie | `hudmenu.swf` `bbb69dff61ce0fa4db48d3e25e84e1bc425206845d26eb8925aacf322cf73e4e` (inventory movies: Phase 2 grid, unchanged) |
| Profile / host | UI-only, 175 files, pinned map trio, no Party Sheet; Steam 1.7.104.0, SKSE 2.3.1, Address Library 13, Proton Experimental 11, controller off |
| Save / fixture | Copied test save `Quicksave0_…_RiverwoodSleepingGiantInn_…_20261026144253`. Follower: Orgnar (ref `0009DCDF`, base `0006512C`), made recruitable with `addfac 5C84D 1` and `setrelationshiprank player 3`, then recruited and dismissed through native dialogue |
| Run | `1617dd2c7d574a99b9d7fe92376d8776`, 345 s, normal exit; watchdog did not intervene (minimum 13.4 GiB available) |
| UI log | `fe4a07d9d1a31bb7f248535e34acd3424f46cf7e9b82c40fd4d76cfd5a5724bb`: Address Library guard and Scaleform registration PASS; resource snapshot valid |
| Cleanup | Restoration **PASS**: no changed candidate files, no active profile, no Skyrim process. Records under `/srv/storage/Projects/SkyCRPG/session-p3/p3-fol-165/` |

Steps the owner reported as passing:

1. **No follower:** the panel was hidden. The owner's screenshot confirms this before recruitment.
2. **Recruit:** name, silhouette and health appeared.
3. **Injury:** `damageav health 50` updated the health shown.
4. **Cell change:** leaving and re-entering the inn reacquired the follower.
5. **Dismiss:** the panel disappeared.
6. **Death:** `kill` cleared the panel. Whether Orgnar is essential, and so only entered bleedout, was not recorded.
7. **Load:** reloading showed no stale name.

Still open:
- No second-follower or unsupported configuration was tested.
- The recruit used a console-made potential follower, not a vanilla housecarl or hireling.
- Magicka and stamina are not displayed.
