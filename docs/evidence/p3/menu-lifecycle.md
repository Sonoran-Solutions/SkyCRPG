# DYL-167 — menu focus and lifecycle

**Provenance.**
- **Bases:** originally branched from main `6e7dff671c1ca4e53eb116f3d3e4cf47925ba79f`, then merged with main `b1bb0a2` (DYL-168, #22) at `9a8692e`.
- **Tested candidate:** code head `9a8692e4b5592edd26ecf8e82733a067862f3b13`; later commits on this branch change documentation only. Windows CI on `9a8692e`: run 40638507073, **PASS**.
- **UI package** `SkyCRPG_UI-0.0.1-9a8692e.zip`:
  - ZIP `79b79809fcc744b58792192551114c908a4d436c8fff7381f90f22e81029a91c`;
  - DLL `c3ef6c3a1fef2e0df1d70a6c2ad864252b6ed1df90ac9cec3a5202819c71ed53`;
  - shipped INI `c7222ebd3f694d074cb6e37dee551f16c6df31ac099742169f0b1410a5a1ba27`.
- **Generated SWFs:**
  - `inventorymenu.swf` `2302b306cd3beb6e69a1962a76030f57f1c85a2fcea43ec9938c818e8fb1b3ba`;
  - `skyui/inventorylists.swf` `34e32f04521a28acf16b00e263a479ca233a0814e986a8e0d91bcc46b4024518`;
  - `hudmenu.swf` `54937d9d48d9a5de4c757ca2ce874fff4a41ed72b39048ad2eabd98292a31628`.
- **Camera package** for the combined runs: `SkyCRPG_Camera-0.0.1-28aae27.zip`, ZIP `b97997f62df7c914959938938229384a1d7b3b558d6a1c75681270ec4db156dc`, DLL `2ae4d51326604eeaa1ef2dd2b021be41ee53652602ab865b144b84d01ba1fa0d`.
- **Host:** Steam SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SkyUI 6.11, Proton.

The implementation lane did not launch the game or build a Windows DLL. The orchestrator built the packages through CI and ran the in-game routes on this candidate, as recorded in [Route results](#route-results-2026-10-28).

## Audit

Tags for **this candidate** (`9a8692e`), detailed in [Route results](#route-results-2026-10-28):
- `R`: automated route PASS, keyboard/mouse;
- `O`: owner manual session PASS (owner-reported, controller and keyboard/mouse);
- `C`: malformed-INI session PASS.

Tags for **earlier evidence**:
- `P`: bounded owner-reported PASS on the cited earlier candidate, not this one. It is kept for context and is not re-attributed.
- `I`: implemented now, runtime NOT_RUN.
- `N`: NOT_RUN on any candidate.

Where a cell has both, the `P` result is older and the `R`/`O` result is new. A cell does not imply all input modes or nested contexts passed. Save/load/new-game/quit columns mean that transition while the row context is relevant; they do not imply that saving is available in that menu.

| Context | Open | Close | Nested | Save | Load | New game | Quit to menu |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Gameplay | P [M] | N | N | P [M]; R | P [M]; R | O | O |
| Inventory | P [G]; R | P [A]; R | P [A] quantity; R quantity | N | N | N | N |
| Quantity | P [A]; R | P [A] cancel; R cancel | P [A]; R | N | N | N | N |
| Console | P [A] visible; R | R | N | R (console `save`) | R (console `load`) | N | N |
| Dialogue | O | O | N | N | N | N | N |
| Crafting | P [G] | P [G] | N | N | N | N | N |
| Container | P [G] | P [G] | N | N | N | N | N |
| Barter | P [G] | P [G] | N | N | N | N | N |
| Magic | P [G] | P [G] | N | N | N | N | N |
| Map | P [G] | P [G] | N | N | N | N | N |
| Journal | P [G] | P [G] | N | N | N | N | N |
| Tween/Favorites | R (Favorites) | R (Favorites) | N | N | N | N | N |
| Loading | P [M]; R | P [M]; R | N | N | P [M]; R, O | O | O |
| Death | O (`player.kill`) | N | N | N | O (reload after death) | N | N |

[G] [Grid combined route `07b392d246d54aedbcb44b6871532af1`](grid-inventory.md#owner-runtime--combined-camera--ui-route-2026-10-27-pass-bounded), native screens and exit; device not attributed per step. [A] [Inventory combined `69f8c8e004e54d97abc60eaa3eb623fb` and keyboard `4885a59564794db7a433d5952180097d`](inventory-integrity.md#combined-camera--ui-route--2026-10-27) cover specific quantity, item and close/reopen actions, not console input suppression. [M] [Minimap combined `5298264e872545c0ab28f5b07becc551`](minimap.md#combined-camera--ui-route--2026-10-27) covers menus, load and compass; exact menus and every transition are not independently attributed. [Follower `bf2a8387`](follower-hud.md#natural-follower-route-on-a-vanilla-fixture-save--2026-10-27) covers follower observations and reload on an earlier candidate.

| Subsystem | Audit and transition handling |
| --- | --- |
| Hotbar/favorites | `MenuBlocks`, `RefreshMenuFocus` and `MenuFocusEvents` fail closed on unavailable UI, named console/dialogue/Favorites/loading/main menu and flagged stack menus. `DispatchHotbar` resamples focus and bindings; native-bound slots are left to Skyrim. The focus epoch invalidates queued auto-FAV presses on focus open/close and on load or new game, and dead players can't receive a dispatch. **Runtime:** with keyboard/mouse, R shows no dispatch behind the console, Favorites or inventory, and none from a key held across inventory close. O covers dialogue, quit then load, new game, death then reload and combat on both devices. No held-key state or cursor capture is owned by this sink. Engine event ordering beyond these routes is N. |
| HUD resources/hotbar snapshots | `ResourceLifecycle` invalidates generations on pre/post load and new game; sampled values are time bounded. Menu focus hides the hotbar. Death and quit are not explicit snapshot reset events, so dead values may be shown until resampled or menu teardown. Runtime: the HUD returns after death then reload, and after quit then load (O); R logs HUD readiness after a console load. Whether the HUD shows dead values before the reload is not separately verified. |
| Inventory and gestures | `inventory::OnSkseMessage`, `Events` and `ResetSession` clear world/menu selection, native observations and action intents on load/new game and inventory open/close; other non-passive menu events invalidate. Player death invalidates world. The AS/native gates retain native fallback and do not own the game's cursor. [A] provides bounded action evidence on an earlier candidate. On this candidate, R covers quantity cancel and close/reopen with no drop, and O covers death then reload and quit then load. Load while a modal is active is N. |
| Equipment | `equipment_bridge.cpp` reads copied current values only while inventory is open and loading is closed; it retains no separate equipment cache. Inventory invalidation and AS close/unload clear presentation. [G] bounded evidence; N for all transition combinations. |
| Follower | `follower::OnMessage` clears handle, name, cell and availability on pre/post load and new game; periodic sampling classifies death/unavailable. [Follower route](follower-hud.md#natural-follower-route-on-a-vanilla-fixture-save--2026-10-27) is bounded and on an earlier candidate. On this candidate, with Serana present, the follower panel returns after quit then load (O). New-game follower clearing was not checked separately, and follower death cases are N. |
| Minimap and compass | `minimap::OnMessage` invalidates availability, cell and advances token on pre/post load/new game. Sampling handles death/cell; `compass-policy.as.inc` releases on stale, menu, hide and teardown, reacquiring only when eligible. [M] and [provider-absent `a16a95c6690844f39de2844ba9123d99`](minimap.md#updated-2026-10-26-ledger-and-follow-ups) provide bounded evidence. No fresh-render signal exists. On this candidate the minimap returns after quit then load (O), and R logs `Minimap preflight exact=1`. The C session shows the native compass fallback when `bReplaceCompass` is invalid. Death then reload was checked for the HUD and hotbar, not the minimap specifically; other death/quit combinations are N. |

Source search of `modules/ui/src` and Scaleform found no global pause or time-scale setter. Inventory uses native pause; `GameIsPaused()` is a query. Presentation settings come from the separate INI, not an inventory/follower database. No AS lifecycle code changed; existing inventory and compass harnesses exercise their established teardown/gesture behavior, not engine ordering.

## Automated evidence

Logs and job specifications: `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/`. All commands use the worktree as cwd. **PASS:** `timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/run-checks.py`; **PASS:** `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p '*.py'` (21 tests); **PASS:** every `modules/ui/tests/*_test.cpp`, built with `timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src <file> -o <log-dir>/<name>` and run under `timeout 600`; **PASS:** every JS harness under `timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node <test> modules/ui <skyui-source>`, except inventory actions which uses the prepared `InventoryMenu.as`. `lifecycle_dispatch_test` covers epoch invalidation and missing/malformed/bounded settings; it does not simulate every subsystem reset. The initial standalone prepare failed because the environment variable was omitted, and its dependent JS failed for the missing generated file; the corrected prepare with `SKYCRPG_SKYUI_SOURCE` and dependent JS passed. The lane runner also passed its prepare and inventory harness. The implementation lane changed and built no SWF; the generated SWFs listed under Provenance come from the orchestrator's CI build.

## Acceptance ledger

| Criterion | Status |
| --- | --- |
| 1. Native controls/no unintended action | Keyboard/mouse hotbar suppression behind console, Favorites and inventory, plus a key held across inventory close: automated PASS [R]. Dialogue: owner-reported PASS on controller and keyboard/mouse [O]. Remaining contexts: N. |
| 2. Save/load/new game and configuration recovery | Console save/load with the same equipped item, HUD and minimap: automated PASS [R]. Quit to main menu then load, new game, and death then reload: owner-reported PASS on controller and keyboard/mouse [O]. Malformed/out-of-range INI: PASS [C] (log-verified and owner-reported). iVersion-mismatch whole-layout reset at runtime: N (unit-tested only). |
| 3. Native menu coexistence without loss/trap | P [G] bounded menu route, [A] bounded item/quantity actions; inventory quantity cancel with no drop: automated PASS [R]. Complete transition matrix: N. |
| 4. No global pause in combat | Source audit supports no UI pause/time-scale setter; real-time combat with hotbar use: owner-reported PASS on controller and keyboard/mouse [O]. |

## Route results (2026-10-28)

All runs use the combined profile (`stage-combined-167b` + `dist-combined-167b`): UI `9a8692e` (ZIP `79b79809…`, DLL `c3ef6c3a…`) and Camera `28aae27` (ZIP `b97997f6…`, DLL `2ae4d513…`). The save is a disposable copy of `SkyCRPG_Test_DYL167`, never a fixture. Every run restored with PASS.

**[R] Automated route** `session-p3/autotest/routes/dyl167-combined.json`, keyboard/mouse through virtual uinput devices, controller off, with the never-shipped test probe (PR #23, `1b4e4c5`) installed only for the run:

- Supervised run `auto-dyl167-combined-20261028T181647Z`: PASS.
- Unattended run `auto-dyl167-combined-20261028T181916Z`: PASS. No prompt; the owner confirmed nobody touched the machine.

Both runs checked:

- baseline state;
- auto-FAV slot 1 equips Auriel's Bow;
- no dispatch with the console, Favorites or inventory open;
- no dispatch from a key held across inventory close;
- quantity slider open and cancelled with no drop;
- console save/load with the same right-hand item, HUD present, and HUD and minimap readiness logged.

**[O] Owner manual session** `p3-manual-167`, 18:24–18:30Z. The owner stated they ran all five items on both the controller and keyboard/mouse, and reported all of them PASS:

1. NPC dialogue;
2. quit to main menu, then load;
3. new game;
4. death (`player.kill`), then reload;
5. real-time combat with hotbar use.

Log notes from that session: `SkyCRPG_UI.log` has 4 slot-1 keyboard dispatches (equip, unequip, equip, equip) and no `decision=none` lines. Controller hotbar use goes through native Favorites, which logs no `Hotkey` lines. `skse64.log` shows 4 pre-load and 3 post-load dispatches, and no new-game dispatch.

**[C] Malformed-INI session** `p3-ini-167`, about 18:31–18:37Z.

- **Setup:** after prepare, with the game closed, only the deployed `SkyCRPG_UI.ini` was replaced. The shipped `c7222ebd…` became the test copy `session-p3/logs/dyl-167/malformed-ini/SkyCRPG_UI.malformed.ini` (`cbd4a14e…`). The test values were:
  - `bEnabled=yes`;
  - `iScalePercent=400`;
  - `iOffsetX=-9999`;
  - `iOffsetY=abc`;
  - `bResetLayout=maybe`;
  - `bManageItemPreview=7`;
  - `fItemPreviewScale=5`;
  - `fItemPreviewXShift=right`;
  - `bReplaceCompass=2`.
- **Log:** `SkyCRPG_UI.log` matched every expected line:
  - `version=1; enabled=1 (valid=0); scale=115% (valid=1 clamped=1); offset=(-120,0) (valid=1/0 clamped=1/0)`;
  - `bManageItemPreview=1 (invalid, default) fItemPreviewScale=set (clamped) fItemPreviewXShift=auto (invalid, auto)`;
  - no reset-layout line;
  - runtime check PASS.
- **Compass:** `skse64.log` shows the SkyCRPG compass lease `disabled`, so the native compass was used as the fallback.
- **Owner checks:** all reported PASS: startup, HUD scaled and shifted but on screen, native compass, hotbar toggles once per press, and inventory preview placement. The UI log shows 18 keyboard hotbar dispatches, alternating 9 equip and 9 unequip.
- **Restore:** PASS. The test INI was recorded as an owned-file change and removed, and no user INI changed.

**Still N:**

- iVersion-mismatch whole-layout reset at runtime (covered by `lifecycle_dispatch_test`);
- unsupported-provider combinations;
- UI-only disable/restart;
- the rest of the transition matrix.

## Owner route — original plan (historical; superseded by Route results above)

Use a disposable copy of `SkyCRPG_Fixture_CombinedAcceptance` in the combined profile; never overwrite the fixture. Record purpose, command/profile, run ID/PID, logs, 20-minute deadline, completion and cleanup before launch. Record executable, runtime/dependency versions, host, source and DLL/SWF/package hashes. Do separate controller and keyboard/mouse passes and state the actual device; switch the controller off for the keyboard/mouse pass on this host. Stop and preserve evidence on item loss, lock or unintended action.

1. In safe gameplay, note a consumable count and follower/compass; try hotkeys while opening and closing console, dialogue, Favorites/Tween and inventory, including a key held across the transition. Confirm no item use/attack behind menus, native close and fresh gameplay input work; check combat remains real time. Exercise a nested quantity cancel and close, then reopen inventory and verify count.
2. Save, load, inspect the same item and follower, and check compass returns and minimap reacquires; quit to main menu and load the disposable save again. Start a new game only if the remaining time permits, then verify no old follower/item display. Exercise player death/reload if time permits. Record unvisited cells N rather than extending the job.
3. In a separate closed-game configuration setup within the same budget, back up the owned INI, introduce malformed and out-of-range values, restart and verify usable defaults/bounds and compass fallback; restore the exact backup with the game closed. If time is insufficient, record N and schedule a separate bounded job. Native menu coexistence already covered by [G] need not be repeated here unless a regression appears.

The matrix has more untested cells than a single 20-minute route can establish; the remaining combinations, UI-only disable/restart, unsupported providers, follower/minimap death cases beyond the bounded player death then reload (O), and exhaustive nested transitions remain N. Owner acceptance and release gates remain open; no owner decision is requested by this implementation.
