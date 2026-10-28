# DYL-167 — menu focus and lifecycle

Base `6e7dff671c1ca4e53eb116f3d3e4cf47925ba79f`; candidate `<head SHA>`, Windows CI `<CI run>`, DLL `<DLL sha256>`, SWF `<SWF sha256>`. Host for prior evidence: Steam SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SkyUI 6.11, Proton; source, exact package and host details are in the linked run records. This worker did not launch the game or build a Windows DLL.

## Audit

`P` = bounded owner-reported PASS on the cited earlier candidate, not this candidate; `I` = implemented now, runtime NOT_RUN; `N` = NOT_RUN. A cell does not imply all input modes or nested contexts passed. Save/load/new-game/quit columns mean that transition while the row context is relevant; they do not imply that saving is available in that menu.

| Context | Open | Close | Nested | Save | Load | New game | Quit to menu |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Gameplay | P [M] | N | N | P [M] | P [M] | N | N |
| Inventory | P [G] | P [A] | P [A] quantity | N | N | N | N |
| Quantity | P [A] | P [A] cancel | P [A] | N | N | N | N |
| Console | P [A] visible | N | N | N | N | N | N |
| Dialogue | N | N | N | N | N | N | N |
| Crafting | P [G] | P [G] | N | N | N | N | N |
| Container | P [G] | P [G] | N | N | N | N | N |
| Barter | P [G] | P [G] | N | N | N | N | N |
| Magic | P [G] | P [G] | N | N | N | N | N |
| Map | P [G] | P [G] | N | N | N | N | N |
| Journal | P [G] | P [G] | N | N | N | N | N |
| Tween/Favorites | N | N | N | N | N | N | N |
| Loading | P [M] | P [M] | N | N | P [M] | N | N |
| Death | N | N | N | N | N | N | N |

[G] [Grid combined route `07b392d246d54aedbcb44b6871532af1`](grid-inventory.md#owner-runtime--combined-camera--ui-route-2026-10-27-pass-bounded), native screens and exit; device not attributed per step. [A] [Inventory combined `69f8c8e004e54d97abc60eaa3eb623fb` and keyboard `4885a59564794db7a433d5952180097d`](inventory-integrity.md#combined-camera--ui-route--2026-10-27) cover specific quantity, item and close/reopen actions, not console input suppression. [M] [Minimap combined `5298264e872545c0ab28f5b07becc551`](minimap.md#combined-camera--ui-route--2026-10-27) covers menus, load and compass; exact menus and every transition are not independently attributed. [Follower `bf2a8387`](follower-hud.md#natural-follower-route-on-a-vanilla-fixture-save--2026-10-27) covers follower observations and reload on an earlier candidate.

| Subsystem | Audit and transition handling |
| --- | --- |
| Hotbar/favorites | `MenuBlocks`, `RefreshMenuFocus` and `MenuFocusEvents` fail closed on unavailable UI, named console/dialogue/Favorites/loading/main menu and flagged stack menus. `DispatchHotbar` resamples focus and bindings; native-bound slots are left to Skyrim. **I:** focus epoch invalidates queued auto-FAV presses on focus open/close and load/new-game; dead players cannot receive a dispatch. No held-key state or cursor capture is owned by this sink. Physical input and engine event ordering are N. |
| HUD resources/hotbar snapshots | `ResourceLifecycle` invalidates generations on pre/post load and new game; sampled values are time bounded. Menu focus hides the hotbar. Death and quit are not explicit snapshot reset events; dead values may be shown until resampled or menu teardown. N for full death/quit behavior. |
| Inventory and gestures | `inventory::OnSkseMessage`, `Events` and `ResetSession` clear world/menu selection, native observations and action intents on load/new game and inventory open/close; other non-passive menu events invalidate. Player death invalidates world. The AS/native gates retain native fallback and do not own the game's cursor. [A] provides bounded action evidence; N for load while a modal is active and death/quit. |
| Equipment | `equipment_bridge.cpp` reads copied current values only while inventory is open and loading is closed; it retains no separate equipment cache. Inventory invalidation and AS close/unload clear presentation. [G] bounded evidence; N for all transition combinations. |
| Follower | `follower::OnMessage` clears handle, name, cell and availability on pre/post load and new game; periodic sampling classifies death/unavailable. [Follower route](follower-hud.md#natural-follower-route-on-a-vanilla-fixture-save--2026-10-27) is bounded; N for quit/new game and all death cases. |
| Minimap and compass | `minimap::OnMessage` invalidates availability, cell and advances token on pre/post load/new game. Sampling handles death/cell; `compass-policy.as.inc` releases on stale, menu, hide and teardown, reacquiring only when eligible. [M] and [provider-absent `a16a95c6690844f39de2844ba9123d99`](minimap.md#updated-2026-10-26-ledger-and-follow-ups) provide bounded evidence. No fresh-render signal exists. N for all death/quit combinations. |

Source search of `modules/ui/src` and Scaleform found no global pause or time-scale setter. Inventory uses native pause; `GameIsPaused()` is a query. Presentation settings come from the separate INI, not an inventory/follower database. No AS lifecycle code changed; existing inventory and compass harnesses exercise their established teardown/gesture behavior, not engine ordering.

## Automated evidence

Logs and job specifications: `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/`. All commands use the worktree as cwd. **PASS:** `timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/run-checks.py`; **PASS:** `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p '*.py'` (21 tests); **PASS:** every `modules/ui/tests/*_test.cpp`, built with `timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src <file> -o <log-dir>/<name>` and run under `timeout 600`; **PASS:** every JS harness under `timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node <test> modules/ui <skyui-source>`, except inventory actions which uses the prepared `InventoryMenu.as`. `lifecycle_dispatch_test` covers epoch invalidation and missing/malformed/bounded settings; it does not simulate every subsystem reset. The initial standalone prepare failed because the environment variable was omitted, and its dependent JS failed for the missing generated file; the corrected prepare with `SKYCRPG_SKYUI_SOURCE` and dependent JS passed. The lane runner also passed its prepare and inventory harness. No SWF was changed or built.

## Acceptance ledger

| Criterion | Status |
| --- | --- |
| 1. Native controls/no unintended action | Keyboard/mouse hotbar suppression behind console, Favorites and inventory, plus a key held across inventory close: automated PASS [R]. Dialogue: owner-reported PASS on controller and keyboard/mouse [O]. Remaining contexts: N. |
| 2. Save/load/new game and configuration recovery | Console save/load with the same equipped item, HUD and minimap: automated PASS [R]. Quit to main menu then load, new game, and death then reload: owner-reported PASS on controller and keyboard/mouse [O]. Malformed-INI runtime: N. |
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

**Still N:**

- the malformed/out-of-range INI step with the game closed;
- UI-only disable/restart;
- the rest of the transition matrix.

## Owner route — original plan, maximum 20 minutes

Use a disposable copy of `SkyCRPG_Fixture_CombinedAcceptance` in the combined profile; never overwrite the fixture. Record purpose, command/profile, run ID/PID, logs, 20-minute deadline, completion and cleanup before launch. Record executable, runtime/dependency versions, host, source and DLL/SWF/package hashes. Do separate controller and keyboard/mouse passes and state the actual device; switch the controller off for the keyboard/mouse pass on this host. Stop and preserve evidence on item loss, lock or unintended action.

1. In safe gameplay, note a consumable count and follower/compass; try hotkeys while opening and closing console, dialogue, Favorites/Tween and inventory, including a key held across the transition. Confirm no item use/attack behind menus, native close and fresh gameplay input work; check combat remains real time. Exercise a nested quantity cancel and close, then reopen inventory and verify count.
2. Save, load, inspect the same item and follower, and check compass returns and minimap reacquires; quit to main menu and load the disposable save again. Start a new game only if the remaining time permits, then verify no old follower/item display. Exercise player death/reload if time permits. Record unvisited cells N rather than extending the job.
3. In a separate closed-game configuration setup within the same budget, back up the owned INI, introduce malformed and out-of-range values, restart and verify usable defaults/bounds and compass fallback; restore the exact backup with the game closed. If time is insufficient, record N and schedule a separate bounded job. Native menu coexistence already covered by [G] need not be repeated here unless a regression appears.

The matrix has more untested cells than a single 20-minute route can establish; remaining combinations, UI-only disable/restart, unsupported providers, death and exhaustive nested transitions remain N. Owner acceptance and release gates remain open; no owner decision is requested by this implementation.
