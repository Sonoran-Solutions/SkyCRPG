# DYL-163 / P3-01 — inventory integrity candidate

## Current status and identities

**PASS:** portable identity/intent fixtures, actual-AS dispatch harness, existing UI checks, pinned-source integration transforms and patch applicability. **NOT_RUN:** Windows DLL build, SWF compilation, every gameplay/weight/encumbrance check and owner acceptance. **BLOCKED:** verified pre-dispatch interception of native favorite input; the pinned InventoryMenu ActionScript has no favorite mutation callback. This is an implementation candidate with open gates, not completed U4 or ticket acceptance.

Ticket: [DYL-163](https://linear.app/devils17/issue/DYL-163). Base `abd5735c98fffd6ad5202e3f674f7b22697cc859`; implementation/integration source `<head SHA>`; Windows build `<CI run>`; DLL `<DLL sha256>`; generated movies `<SWF sha256>` (record inventorymenu.swf and skyui/inventorylists.swf separately); internal package SHA-256 **NOT_RUN**. Worker changes are uncommitted; no branch creation, commit, push, PR, deployment or game launch was performed.

Local checks: host `dq-MEG-Z590-Trident-X-MS-B926`, Linux, g++ 15.2.0, Python 3.14.4, pinned Node v24.19.0. Intended runtime: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0 (`7a60f4de`), SkyUI Community 6.11, recorded Proton host. Executable/host/Proton/build, loaded plugin order, save provenance/hash, input device, resolution and candidate manifests must be recorded by the orchestrator with the runtime evidence. None was sampled or changed by this worker. Existing Phase 2 runtime passes do not validate this candidate.

## Design and affected contracts

The portable model stores copied native traits, originating-plugin/local/runtime form IDs, count, world/menu/inventory/view generations and a generation-scoped native backing slot. Same-base entries with enchantment, temper, owner/stolen state, quest alias, custom name, worn/worn-left, favorite, poison or charge differences remain distinct. Even otherwise identical native rows have different scoped slots; equivalent copies in a native stack remain one entry with native count. No UUID, raw engine pointer, menu/GFx clip, inventory index or intent is persisted across game/menu lifetimes. Dynamic forms have an empty originating plugin rather than a fabricated plugin identity.

The adapter reacquires the current player/menu and reads `GetInventory(..., true)` immediately before guarded dispatch. It verifies that every menu extra list still belongs to live inventory before dereferencing it. A copied live-inventory observation detects changes without an event, including favorites, names and temper; a change invalidates the token and requests native list reconciliation. Mixed distinct traits grouped by one native row are marked unavailable rather than selecting a copy by base ID. This strict refusal is safe but may leave a required fixture workflow blocked until native grouping is verified. There is no arbitrary 500-entry cap; sample cost is unmeasured.

One native action may be pending. A gesture stays spent across snapshot refreshes, repeated/down/up callbacks and overlapping device presses; another action requires a new press after release. The quantity dialog reserves the same drop intent, validates its amount/identity again on confirm, permits one confirmation and cancels on cancellation, view/context/close/load/death changes. Native container/equip events invalidate observations; no task retries an action or predicts counts/equipment. Refusals log once per token, capped at 256 distinct tokens per menu session. Refresh work captures generations only and reacquires the menu on an SKSE UI task.

The supplied player-only AS gates the two existing ItemSelect paths and ItemDrop, including quantity entry/confirm. All native GameDelegate calls and order are preserved. Both InventoryLists definitions notify view changes through a method that exists only on InventoryMenu; container/barter/gift/crafting are not patched into player actions. Refusal clears positional selection, invokes native list invalidation and queues a native list rebuild; it does not replay input. Missing, old or partially registered bridges fall through to the existing native actions. Quest drops explicitly reach native ItemDrop after all other checks so Skyrim can reject them with its normal feedback. The strict portable policy also tests quest drop refusal; neither mode bypasses native restrictions.

Contracts: ARCHITECTURE “Input and menu contract” and all seven inventory lifecycle steps; U4 (primary), U3/U6 (dependent presentation/focus), Q1. No carry weight, capacity, encumbrance, economy, item record, pause flag, Camera state or provider/toolchain pin is written. `getInventorySnapshot()` is an observation, not inventory truth; the native card/footer remain authoritative.

Shared changes are proposed only in `/srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-163.patch` and explained in `dyl-163.md`: one CMake source, plugin include/registration/lifecycle forwarding, builder gates/view anchors and an architecture candidate paragraph. No shared entry point or shared document was edited in the working tree.

## Orchestrator review changes

Senior review of the worker candidate changed three behaviors in `inventory_bridge.cpp` before commit. The portable policy and AS hook are unchanged.

- **List reconciliation no longer refuses the next action.** Previously, any difference between the copied and live inventory (for example after an equip) invalidated the snapshot and marked the list dirty, so the next press on the same item was refused once. A difference now requests a native list update without marking the list stale. Per-row safety is unchanged: each menu extra list must still belong to the live inventory before it is read, counts are bounded by live counts, traits are re-read, and `Publish` bumps the generation when rows differ. Engine container/equip events and stale tokens still mark the list dirty until the UI-task update runs.
- **Passive overlays keep the inventory context.** `HUD Menu`, `Cursor Menu`, `Fader Menu` and `Mist Menu` open/close events no longer cancel a pending quantity intent or spend the current gesture. Any other menu still invalidates.
- **An unsupported callback thread falls back to native behavior.** Previously, a GFx call off `Main::threadID` refused every activation and drop, which would trap the inventory if the actual dispatch thread differs. Live reads are still skipped on such a thread. Validation now returns permit, so the native menu, which remains authoritative, acts alone. It logs `unsupported-thread; native behavior retained` once. Any such log line means the validator is inactive on that host and DYL-163 stays open.

Re-run after these changes with the lane runner — all 15 steps **PASS**. The runner holds host paths, so it lives outside the repository at `/srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py`. Since integration it runs 15 steps; the pre-integration `git apply --check` step is retired. The bridge is not compiled locally; Windows CI is the compile check.

## CommonLib 7.2.0 source audit

Read-only root: `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0`. These declarations and relevant implementations were checked; this is source evidence, not Windows compilation or runtime-layout proof.

| Used API / data | Verified declaration / implementation |
| --- | --- |
| `UI::GetSingleton`, `GetMenu<InventoryMenu>()`, `IsMenuOpen`, `IsModalMenuOpen`, `menuStack`/menu flags | `include/RE/U/UI.h`, `RE/I/IMenu.h`; GPtr `.get()` in `RE/G/GPtr.h` |
| `InventoryMenu::GetRuntimeData().itemList` | `include/RE/I/InventoryMenu.h` public runtime accessor; no custom offset arithmetic |
| `ItemList::items`, `GetSelectedItem()`, `Update(player)` | `include/RE/I/ItemList.h`, `src/RE/I/ItemList.cpp`; selection is current native selectedIndex, not filtered enumeration position |
| `StandardItemData::objDesc`, virtual `GetCount()`/`GetEnabled()` | `include/RE/S/StandardItemData.h`; return types `uint32_t`/`bool` |
| `PlayerCharacter::GetSingleton`, `Is3DLoaded`, `IsDead`, `GetInventory(filter, true)` | `RE/P/PlayerCharacter.h`, `RE/A/Actor.h`, `RE/T/TESObjectREFR.h`; `src/RE/T/TESObjectREFR.cpp` returns owned entry copies with borrowed extra lists |
| `InventoryEntryData::object`, `extraLists`, `AddExtraList`, `IsOwnedBy(Actor*, bool)` | `RE/I/InventoryEntryData.h` and `.cpp`; temporary wrapper deletes its list container, not borrowed extras; no `GetDisplayName()` (it may add data) |
| `ExtraDataList::GetByType<T>()`, `HasType<T>()`, `GetCount()`, `HasQuestObjectAlias()` | `RE/E/ExtraDataList.h`; quest accessor is the same route used by `InventoryEntryData::IsQuestObject()` |
| Enchantment/temper/ownership/name/worn/favorite/poison/charge | Actual `ExtraEnchantment`, `ExtraHealth`, `ExtraOwnership`, `ExtraTextDisplayData`, `ExtraWorn`, `ExtraWornLeft`, `ExtraHotkey`, `ExtraPoison`, `ExtraCharge` headers and field names; base `TESEnchantableForm::formEnchanting` |
| `TESForm::GetFormID`, `GetLocalFormID`, `GetFile(0)`, `TESFile::GetFilename()` | `RE/T/TESForm.h`, `RE/T/TESFile.h`; full and FE IDs use engine resolution; no custom high-byte truncation |
| Container/equip/death/menu events and registration | `RE/T/TESContainerChangedEvent.h`, `TESEquipEvent.h`, `TESDeathEvent.h`, `RE/M/MenuOpenCloseEvent.h`, `RE/S/ScriptEventSourceHolder.h`; Actor NiPointers used only during event callback |
| Thread/task/Scaleform operations | `RE/M/Main.h::threadID`, `REX/W32/KERNEL32.h::GetCurrentThreadId()`, `SKSE/Interfaces.h::AddUITask`, `GFxFunctionHandler::Params`, `GFxMovie::CreateObject/CreateArray`, `GFxMovieView::CreateFunction`, GFxValue accessors |

There is no universal instance accessor or `InventoryEntryData::IsStolen()` in the inspected API. The adapter uses a live borrowed instance wrapper's verified `IsOwnedBy(player, true)` as the stolen-equivalent test, and `HasQuestObjectAlias()` instead of inspecting alias offsets. Native favorites have no mutation callback in pinned `source/actionscript/ItemMenus/InventoryMenu.as` or `ItemMenu.as`. No favorite delegate name/vtable offset was invented.

## Bounded automated evidence

All command specifications are recorded before jobs start in `suite-job.md`, subsequent changed-input job records, and `jobs.json` under `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/`. Owner: DYL-163 / Codex. Every focused command uses `timeout 600`; expected output is exit 0 plus assertions/portable binary/patch success. `jobs.json` records exact argv, PID, UTC start/deadline, log, elapsed time, exit/reaped status and cleanup. Completion is process exit, not quiet output. All final jobs exited normally; no test process remains pending. Binaries, staged sources and logs stay outside Git. `source-sha256.json` records the lane/integration file hashes; `cleanup.json` confirms recorded job PIDs are absent.

Reproduce the complete suite with the lane runner (host paths, kept outside Git):

```sh
timeout 600 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
```

`inventory_prepare_contract.py` also runs under `unittest discover` when `SKYCRPG_SKYUI_SOURCE` is set (skipped otherwise), so the repository's `-p "*.py"` discovery works on trees that include DYL-163.

Exact final commands (each compile/run is a separate logged job):

```sh
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I modules/ui/tests/inventory_prepare_contract.py /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163
# T is inventory_policy, favorite_slots, menu_focus, resource_snapshot, hud_settings:
timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/${T}_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/${T}_test
timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/${T}_test
timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/inventory_actions_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/InventoryMenu.as
timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/hud_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/navigation_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p test_scaleform_contract.py -v
timeout 600 git apply --check /srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-163.patch
```

| Check | Result | Evidence / limits |
| --- | --- | --- |
| Inventory policy g++ compile + fixtures | **PASS** | `inventory_policy-build.log`, `inventory_policy.log`: plain duplicate native stack; same-base enchant/temper/stolen/name differences; same-traits separate rows; normal >=05 and FE mod IDs; increases/decreases; external favorite/equip; quest restrictions; quantity confirm/cancel/stale; removal; view/menu/world/death invalidation; pending/duplicate/repeated/two-device input. Fake native entries, no engine execution. |
| Actual-AS inventory hook plus transformed pinned native dispatch bodies | **PASS** | `inventory-as.log`: one validator at each existing dispatch, refusal suppresses native call and refreshes, stale event entry refused, quantity holds token, absent/old/partial bridge fallback. Stubs do not establish physical input or native event ordering. |
| In-memory integration transforms / native delegate preservation | **PASS** | `prepare-contract.log`: all pinned source hashes/anchors, four view notifications in both list copies, unchanged ordered native GameDelegate calls. Shared files remain untouched. |
| Existing portable tests | **PASS** | `favorite_slots`, `menu_focus`, `resource_snapshot`, `hud_settings` compile/run logs; original checks reused after later AS-only changes. |
| Existing actual-AS HUD / navigation | **PASS** | `hud-as.log`, `navigation-as.log`; original snippets/actual pinned category callbacks. |
| Existing Python contracts | **PASS** | `scaleform-contract.log`: all 14 tests, no skips. |
| Integration patch | **PASS** | `integration-patch.log`; `git apply --check` against unchanged base. |
| Windows `/W4 /WX /permissive-` DLL | **NOT_RUN** | MSVC/CI is integrator-owned; no DLL hash. Linux portable compilation does not compile the adapter. |
| InventoryMenu / InventoryLists SWF compilation | **NOT_RUN** | Only staged/transformed source tested; no FFDec or other third-party compiler executed. |

Initial harness attempt **FAIL**: JavaScript parser evaluated the native AS2 constructor's `super()` as JS; corrected parser evaluates only the four native dispatch method bodies. Initial isolated Python direct-module invocation **FAIL**: `python3 -I -m unittest modules/ui/tests/test_scaleform_contract.py` cannot import `modules` under isolation. Corrected invocation uses unittest discovery under the same `-I`. Changed-input corrections and initial logs are retained in `*.initial`, `*.import-failure` and job records; no unchanged retry loop. Later fallback changes reran only the affected AS/source/Python checks. No game/profile/save/INI was touched.

## Acceptance ledger

| Ticket acceptance criterion | Automated result | Runtime / full criterion |
| --- | --- | --- |
| AC1: plain duplicates, enchanted/tempered/stolen, consumables, quest/modded forms retain identity and quantity | **PASS** portable fixtures and generation checks | **NOT_RUN** native fixtures; unresolved native grouping may block actions. |
| AC2: native weight/encumbrance unchanged, display not source of truth | **PASS** source boundary: observation/validation only, no engine mutation or weight/capacity write | **NOT_RUN** actual counts/weight/encumbrance. |
| AC3: repeated/stale UI events cannot duplicate/delete/target wrong item; failure refreshes safely | **PASS** portable/AS guarded routes | **NOT_RUN** engine input ordering and recovery; **BLOCKED** pre-dispatch native favorite guard. Do not close this criterion. |
| World/menu/quantity lifecycle, native fallbacks and module independence | **PASS** policy/source boundary | **NOT_RUN** UI-only and combined routes; no Camera code touched. |
| Owner acceptance | **NOT_RUN** | No owner acceptance claimed. |

## Orchestrator route — all runtime rows NOT_RUN

One isolated UI-only profile, one Skyrim process, copied disposable save only; <=20 minutes total. Record purpose/command/profile/PID/log/deadline/completion/cleanup before launch. Integrator must record `<head SHA>`, `<CI run>`, `<DLL sha256>`, both `<SWF sha256>` values, executable hash, dependency pins, host/Proton, input/resolution, loaded plugin list and copied save identity. Keep controller OFF for keyboard/mouse (Phase 2 host finding); use a separate controller pass only within remaining time or mark it NOT_RUN. Original saves are never overwritten. If any action targets the wrong instance, count changes twice, or the menu becomes unusable, stop the route and preserve evidence.

Prepare the copied fixture at Alvor's Riverwood forge with the following items; verify console help resolves these records before adding them:

```text
player.additem 00012EB7 4
player.additem 00049BB8 1
player.additem 0003EADD 8
player.additem 0005ACE4 3
player.getitemcount 00012EB7
player.getitemcount 00049BB8
player.getitemcount 0003EADD
player.getav inventoryweight
player.getav carryweight
```

`00012EB7` is the plain Iron Sword; `00049BB8` is the separate vanilla pre-enchanted Iron Sword of Embers record ([item reference](https://skyrim.fandom.com/wiki/Iron_Sword_of_Embers)); it alone does **not** prove same-base ExtraEnchantment. `0003EADD` is Potion of Minor Healing ([item reference](https://www.gamebanshee.com/cgi-bin/search/banshee_search.pl?_cgifunction=search&_layout=skyrim_items_page&skyrim_items.itemname=potion+of+minor+healing)); `0005ACE4` is Iron Ingot ([ID reference](https://elderscrolls.fandom.com/wiki/Console_Commands_%28Skyrim%29/Ingots)). Also prepare one same-base Iron Sword enchanted and named at a native arcane enchanter, using a legitimately available learned effect/filled soul gem on this copied fixture; record base FormID and ExtraEnchantment/custom name separately. If that fixture is unavailable, mark that subrow NOT_RUN, not equivalent to the pre-enchanted base-record check. Do not install an unpinned provider to manufacture it.

For **every action**, record before/after selected native name/base ID, stack/instance trait markers (temper/enchantment/ownership), native count (`player.getitemcount <ID>` plus selected stack count), equipped state, native stolen/favorite/quest indicators, `player.getav inventoryweight` and `player.getav carryweight`; retain video/screenshots and UI refusal log. Use the read-only snapshot diagnostics when available to record token/generations/plugin/local ID. Console/context changes intentionally invalidate selections; reselect after closing the console instead of replaying an old event.

| Budget | Route / expected observation | Runtime result |
| --- | --- | --- |
| 0–3 min | Open inventory, inspect plain stack and pre-enchanted weapon; favorite/unfavorite, equip/unequip each once. Press/hold/release confirm, then deliberate second press; mix a mouse click with controller confirm only if both devices are available. Record exactly one native action per gesture and unchanged unrelated copies. Check no `unsupported-thread` refusal and no permanent input block. | **NOT_RUN** |
| 3–6 min | At Alvor's grindstone improve one plain Iron Sword using one ingot; leave another plain. For two temper levels, improve another copy at a different native smithing effectiveness using a pre-existing smithing potion/fixture; record actual two distinct ExtraHealth values. Do not claim two levels if native effectiveness is unchanged. Inspect/equip/drop the intended tempered vs plain vs prepared enchanted/renamed copies; untouched copies retain state/count. | **NOT_RUN** |
| 6–8 min | Pick up one **Steal**-marked Iron Ingot from the pile by Alvor's outdoor workbench in Riverwood, then compare with console-added owned ingots. Fixture must not have Alvor friendship granting free supplies; if activation says Take, it is not a stolen fixture (Hadvar can grant free access; [route reference](https://www.trueachievements.com/game/The-Elder-Scrolls-V-Skyrim-Special-Edition/walkthrough/4)). Record reference/base ID and ownership before pickup. Drop/reacquire only intended ownership stack; verify the other stack unchanged. | **NOT_RUN** |
| 8–11 min | Potion stack starts at native recorded count (added eight). Consume once (count -1); hold/repeat confirm without another consumption. Drop 3 through quantity dialog; cancel another dialog (no change). Begin a dialog then cancel/close/reopen menu; old confirmation must not dispatch. Add 2 externally with `player.additem 0003EADD 2` after closing inventory; old token is invalid, reselect and verify actual count. | **NOT_RUN** |
| 11–13 min | Use Golden Claw `00039647` **only if actually available and quest-flagged on this copied save**. It can be the returned-shop version; an added/returned claw alone is not quest-alias proof ([two-ID caveat](https://elderscrolls.fandom.com/wiki/Dragon_Claws_%28Skyrim%29)). If unavailable/nonquest, use the naturally picked-up Unusual Gem `0009DFBB` on the end table in the Jarl's bedroom, Dragonsreach Jarl's Quarters ([location/ID reference](https://www.gamebanshee.com/cgi-bin/search/banshee_search.pl?_cgifunction=search&_layout=skyrim_items_page&skyrim_items.itemname=unusual+gem)); prepare this in the copied fixture beforehand. Confirm native quest flag, attempt drop, record native rejection message and unchanged count/weight. If neither real quest fixture is present, leave quest row NOT_RUN. | **NOT_RUN** |
| 13–15 min | Sort/filter/search between selection and attempted activation; switch category with empty results; externally change favorite/equipment via native Favorites after closing inventory, then reopen/reselect. Remove one plain sword externally with `player.removeitem 00012EB7 1` only on disposable fixture. No stale token may activate a surviving different copy; new snapshots show native changes. Open container/barter/Magic/journal and cancel: their native screens remain reachable. | **NOT_RUN** |
| 15–18 min | Open quantity dialog then cancel; save copied fixture, load it and reopen inventory. On a disposable copy test death/reload (console `player.kill`, then load the copied fixture). No old selection/action replay. Inspect a modded form from the already pinned load order, resolving with `help "<fixture item name>" 4`, then `player.additem <resolved runtime FormID> 1`; record originating plugin/local ID and runtime >=05 or FE. If there is no item-form plugin, leave this subrow NOT_RUN; do not invent an ID or silently add a dependency. | **NOT_RUN** |
| 18–20 min | Add enough plain swords on the disposable save (`player.additem 00012EB7 40`, or a recorded larger amount if existing capacity requires it) to cross native carry capacity; confirm weight/encumbrance matches baseline, without changing carryweight. Native drop restores the expected weight/count. Close inventory and verify native combat/menu controls. Quit normally; record process cleanup and profile restoration. | **NOT_RUN** |

Missing-DLL/old-bridge fallback requires a separately staged movie/profile with the game closed; **NOT_RUN**, do not hot-unload the DLL within this route. A combined smoke, controller-specific native button ordering, two required resolutions, and 500-entry timings remain **NOT_RUN** for their phase integration/layout gates.

## Limitations and decisions

- Native favorite mutations remain Skyrim-owned and observed, but are not intercepted by this AS layer. Pre-dispatch favorite validation is BLOCKED until a verified native integration is implemented. This is not an owner-approved scope deferral.
- Main-thread callback identity, UI-task/native callback ordering, borrowed native entry lifetimes and pointer membership assumptions require Windows/host verification. The guard refuses other-thread live reads; do not remove it to make a failing route appear to pass. Native mouse/right-hand/DropItem ordering relative to AS edges may require a reviewed input integration.
- A missing selection yields undefined; grouped distinct traits refuse custom dispatch; no fallback chooses a different same-base instance. If required fixture operations remain unavailable, the implementation gate fails and the ticket stays open.
- Complete snapshot reads occur on selection/new press and validation, not a frame loop. Performance at 500 entries and retained-state behavior are unmeasured. Diagnostic consumers must avoid per-cell/per-frame sampling.
- No generated SWF, DLL or public redistributable was produced. Existing source/license notices are retained; no third-party code/art is copied into this lane.
- Owner decisions needed: **none for this handoff**. Remaining work is engineering verification/completion, Windows build and owner/runtime evidence; no acceptance or dependency/scope decision is implied.

## Runtime smoke — 2026-10-26

**PASS (owner-reported, bounded quick check).** The owner reported that the inventory worked correctly. This was a quick check, not the fixture route above, so the fixture rows remain **NOT_RUN**.

| Field | Value |
| --- | --- |
| Source / CI | `65c69d7b9887221ff9c1f58d0e85c98e9cbc50a5`, Windows [run 40456048615](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40456048615) **PASS** |
| Package | `SkyCRPG_UI-0.0.1-65c69d7.zip` `9c07891aec28a83d08922bd9680baa06e75b34a97d135486359ab46539d658f4`; DLL `cf4a0e0a0b318c6d971290506d6ef307ce6f5858cf01f4c960ff006ffca7062b`; INI `de4162addb6df41b4d915dc7ca2a609cc6dadea8f7d62d5644bbfe43b63c229b` |
| Movies | `inventorymenu.swf` `211115dc54857086198a3c2162a6b0e47fa849f91806461c24279c89b323f45d`; `skyui/inventorylists.swf` `8766d2b2054bedae20fe8e0877fa26b83de259bc588e4415d2527a631a0363fe`; `hudmenu.swf` `279f08f2df4649811311f26b6dbcee6b41912365c00611d711ab3cdb1987f03f` (Phase 2 HUD, unchanged) |
| Profile / host | UI-only, 175 deployed files, pinned map trio; Steam 1.7.104.0, SKSE 2.3.1, Address Library 13, Proton Experimental 11, controller off |
| Save | Copied test save `Quicksave0_…_RiverwoodSleepingGiantInn_…_20261026144253` from the camera-only test profile |
| Run | `e927cb0545234cf79a8cde418932ea7c`, 141 s, normal exit; watchdog did not intervene (minimum 13.4 GiB available) |
| UI log | `116d45537ac9a7a661fefe9102125ef1acdbb9bdebc367f96fd6b5fef3e701e5`: source commit and Address Library guard PASS; **zero inventory refusals and no `unsupported-thread` line** |
| Cleanup | Restoration **PASS**: no changed candidate files, no active profile, no Skyrim process. Records under `/srv/storage/Projects/SkyCRPG/session-p3/p3-inv-163/` |

What this shows: the gated InventoryMenu loaded and native actions went through on this host, and the validator ran on the main thread without refusing anything. No new save was written during the run, so save/load was not exercised.

Still **NOT_RUN** (from the route above):
- Same-base enchanted, tempered and renamed copies.
- Stolen versus owned items.
- Quest-item drop refusal.
- Quantity dialog confirm, cancel and close.
- External add/remove with a stale entry.
- Sort/filter between selection and action.
- Save/load and death.
- A modded form.
- Carry weight and encumbrance before and after.
- Hold/repeat input.

## Fixture route — 2026-10-27

**Owner-reported PASS for the observed behavior. The thread gate stays open: two `unsupported-thread` lines.**

| Field | Value |
| --- | --- |
| Source / package | Same as the 2026-10-26 smoke: `65c69d7`, ZIP `9c07891a…`, DLL `cf4a0e0a…`, same SWFs. No shipped source changed through `e7aeda3`. |
| Profile / host | UI-only, 175 files; same host as the smoke; keyboard and mouse, controller off |
| Save | Copied `Quicksave0 … RiverwoodSleepingGiantInn … 20261026144253`; the route wrote new saves only into the test profile (`Save3`/`Quicksave0 … 20261027170446`, preserved under `session-p3/p3-inv-163-fix/after-test-saves`) |
| Run | `da296784e4b1430a8799032173b70c81`, about 9 min of a 20 min budget, normal quit, watchdog did not intervene (minimum 12.7 GiB available) |
| UI log | `6c095c49…`: `unsupported-thread; native behavior retained` twice, and `duplicate-input` refusals for tokens 89 and 207 |
| Cleanup | Restoration **PASS**: no changed candidate files, no active profile, no Skyrim process |

The owner ran the staged blocks (basic actions, tempered vs plain, stolen vs owned if available, potion quantity/cancel/close/external add, stale selection with sort/category/external remove and other menus, save/load and death, carry weight if time allowed) and reported **all passed**. The run record does not itemize which optional subrows were available. Still **NOT_RUN**: same-base self-enchanted copy, quest-item drop refusal (no Golden Claw/Unusual Gem staged), modded form, and the controller pass.

The two `duplicate-input` refusals show the validator ran on the main thread and stopped repeated presses. The refusal log resets on every inventory open/close, so the two `unsupported-thread` lines mean **two separate inventory sessions** each had at least one `Snapshot` or `Validate` call off `Main::threadID`. On those calls native behavior acted unguarded. Under the rule above, the off-thread gate keeps DYL-163 open.

Follow-up in this branch: every off-thread call now also logs `Inventory off-thread op=<snapshot|validate> thread=<id> main=<id> open=<0|1> loaded=<0|1>`, at most 32 lines per process, with no behavior change. A short rerun with that package names the operation and thread.
