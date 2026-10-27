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
- **An unsupported callback thread falls back to native behavior.** Previously, a GFx call off `Main::threadID` refused every activation and drop, which would trap the inventory if the actual dispatch thread differs. Live reads are still skipped on such a thread. Validation now returns permit, so the native menu, which remains authoritative, acts alone. It logs `unsupported-thread; native behavior retained` once. Any such log line means the validator was inactive for that call. Owner ruling 2026-10-27 (below): this fallback is accepted, and the line is a diagnostic, not a gate failure.

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

## Off-thread diagnostic rerun — 2026-10-27

**No off-thread call reproduced. The gate stays open.**

| Field | Value |
| --- | --- |
| Source / CI | `7caac58`, Windows [run 40547955137](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40547955137) **PASS** |
| Package | `SkyCRPG_UI-0.0.1-7caac58.zip` `03f1f95900515ee41d5136dd92a37282b5aaab0938836069b20d63e2074d33bd`; DLL `40480fdf9d9976aab56f567f12bf9ebc82155aeb4b9dabc98b39b1f58330e816`; INI unchanged; same SWFs as above |
| Runs | `327b6a51…`: closed at the main menu after about 1 min, no save loaded, clean exit, no crash log (no evidence). `e8b145fbf60e4805a029fa3d9dae8697`: about 2 min of a 10 min budget, normal quit, watchdog did not intervene (minimum 13.1 GiB available) |
| Save | Copied `Save3 … 20261027170446` from the fixture route; new quicksave kept in the test profile only |
| UI log | `77981c4b…`: no `Inventory` lines at all — no `off-thread`, no `unsupported-thread`, no refusals |
| Cleanup | Both restorations **PASS** |

The owner opened the inventory at least once in each of six ways, acting on an item each time:
- right after load;
- with Tab → Items while standing;
- while moving;
- right after closing a container;
- after quicksave plus quickload;
- with rapid close and reopen.

None produced an off-thread call. Ordinary opening paths are therefore not the trigger, at least not reliably. The fixture route's two occurrences happened during steps this rerun did not repeat: the quantity dialog (drop/cancel/close), grindstone crafting, console item changes, `player.kill` and reload, and the carry-weight drop. The next diagnostic run should repeat those steps with the `7caac58` package.

## Targeted off-thread diagnostic — 2026-10-27

**Not reproduced. Across three runs, `unsupported-thread` has appeared only in the original fixture route.**

| Field | Value |
| --- | --- |
| Package | Same `7caac58` package as the rerun above (ZIP `03f1f959…`, DLL `40480fdf…`) |
| Run | `e0e93ec476764d139e5719639c23f5b3`, about 5 min of a 20 min budget, normal quit, watchdog did not intervene (minimum 12.5 GiB available) |
| Save | Copied `Save3 … 20261027170446`; new saves written only to the test profile |
| UI log | `0f675911…`; timestamped watcher copy in `session-p3/p3-inv-163-diag3/log-timeline.txt` |
| Cleanup | Restoration **PASS** |

The owner did the five steps in order, pausing between them:

1. **Quantity dialog:** one dialog drop of 3 potions. After that the stack was below SkyUI's `quantityMenu.minCount = 6` (pinned `data/interface/skyui/config.txt`; `InventoryMenu.as` line 100), so later drops went one at a time without a dialog. This is native SkyUI behavior. Dialog cancel and close-with-dialog-open were **NOT_RUN** here; they ran in the fixture route.
2. **Grindstone:** tempered a sword, then equipped it.
3. **Console:** added and removed items, then drank a potion.
4. **Death:** saved, ran `player.kill`, reloaded, then acted on an item.
5. **Carry weight:** added 40 swords and dropped them through the quantity dialog.

The log has four `duplicate-input` refusals (17:58:24, 17:58:50, 18:01:54, 18:02:12 UTC): the validator ran on the main thread and stopped repeated presses. There were **no `off-thread` or `unsupported-thread` lines**.

**Conclusion:** the off-thread call is rare and intermittent. It happened in 2 inventory sessions of the fixture route and in none of the ordinary-opening or targeted steps. On those calls the bridge left native behavior unguarded, as designed, and the owner saw no wrong-copy or double-count effect. The design question goes to review (owner/senior):
- **(a)** accept the native fallback on an unsupported thread and keep the bounded diagnostic; or
- **(b)** marshal off-thread Snapshot/Validate onto the main thread (`AddTask`) and refuse the custom path for that call.

Option (b) needs a design change and Windows verification. The gate stays open until that is decided.

## Owner ruling — 2026-10-27

> Approve option (a) for DYL-163: accept native fallback on an unsupported thread and keep the bounded diagnostic.

The thread gate is closed by owner decision (recorded in `docs/DECISIONS.md`). On an unsupported thread the native menu stays authoritative and acts alone for that call. The bounded `Inventory off-thread op=… thread=… main=…` diagnostic stays, so any recurrence is logged. No code change.

This ruling does not accept DYL-163. Still open:
- Senior review of #17.
- Runtime rows still **NOT_RUN**:
  - a same-base self-enchanted copy;
  - a quest-item drop refusal;
  - a modded form;
  - the controller pass;
  - the missing-DLL/old-bridge fallback;
  - a combined profile.
- The native favorite pre-dispatch guard, still **BLOCKED** (no AS callback).

## Senior review P1 correction

2026-10-27, DYL-163, working tree based on `f8b5752` (`phase3/dyl-163-inventory`, including merged #15). [PR #17 senior review](https://github.com/Sonoran-Solutions/SkyCRPG/pull/17#issuecomment-6450506611) identified a stale selected-row authorization window. The old path queued a native list update on whole-inventory drift, then allowed `Publish` with an unchanged menu row and token. Bounding its count by the total base-form count did not prove that the selected instance still matched: a row of 1 could pass against a newly increased live count of 2, or omit a new distinguishable extra list. This correction supersedes the reconciliation safety claim in “Orchestrator review changes” above.

`inventory/row_match.h` now owns the engine-free `MatchRow` policy. It compares the selected form's copied live observation with that form's previous observation, if present in this menu session. Selected-form drift refuses; a change confined to another form still only queues the existing native update. Every represented extra must belong to the live entry; the adapter retains its pointer-membership check before calling `Traits` or `GetCount` on a menu extra. Portable inputs use per-sample live-instance indices plus copied traits/counts, never retained engine pointers.

Coverage must account for the row's count using its represented matching extras and the live unextra'd/plain count, with no duplicate extras or inconsistent counts. The count must also fit the live count of matching traits. If the row needs an unrepresented remainder and another live same-base instance has differing temper, enchantment, ownership/stolen, name, poison, charge, worn/worn-left, favorite or quest traits, it refuses. A fully represented matching row can still permit beside a differing same-base row. A menu session with no prior observation still has to prove coverage. Native grouping that cannot prove this remains refused; the adapter never substitutes another same-base copy.

Refusal is an actual false `Sample` result, before `Publish`, rather than a diagnostic-only flag. `RefuseRow` logs `row-drift`, `row-coverage` or `row-extra-missing` through the existing bounded `LogRefusal`, invalidates the snapshot/generation, clears selection and pending intent, and marks the list dirty while requesting native reconciliation. Validate retains false and cannot begin/confirm an action from that sample. Further samples cannot bypass the refusal just because the copied live observation has advanced: the dirty gate stays closed until the queued native `ItemList::Update(player)` finishes. A reconciled, matching row can then publish a fresh selection; old tokens remain stale. No input is replayed.

Passive menus, owner-approved off-main-thread native fallback and bounded diagnostics, native event invalidation, generation bumps, native actions, and ActionScript are unchanged. No hooks, offsets, dependencies, shared integration changes, or new native actions were added.

### Portable and source verification

**PASS:** expanded `inventory_policy_test.cpp` covers no-event same-base count increases/decreases; same-count trait changes; empty and partial extra lists with all listed distinguishable traits; matching-count shortage despite sufficient total count; fully matched rows after reconciliation; unrelated-form drift; missing extras checked before trait/count inspection; duplicate/mixed/malformed extras; nonfinite traits; stale selection and quantity confirmation after invalidation. It remains in the existing lane runner's `inventory_policy` target, so no runner change is needed.

All 18 final automated jobs **PASS**: six portable C++ compile/run pairs, the pinned-source transform preparation, all four JavaScript contracts, and Python discovery (**18 tests, no skips**). Owner: DYL-163 / Codex; timeout: 600 seconds per command; expected output: exit 0 and successful compile/assertions/contracts; completion: process exit plus log. Specifications were recorded before execution in `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p1/jobs.json`, with PID, UTC deadline, command, result, log and cleanup. All owned children were reaped and process groups verified absent. The focused inventory compile/run preceded the remaining suite. Initial focused fixture **FAIL**: the quest variant attempted to reserve a drop under the strict portable quest rule. Corrected the fixture to supply the adapter's existing `nativeQuestFeedback=true`; no policy was relaxed. Initial records/logs and `changed-input.txt` are retained alongside final evidence.

Exact reproduction commands (each compile/run below was a separate recorded job; variables expand to the recorded argv):

```sh
OUT=/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p1
SKYUI=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
NODE=/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node
for T in inventory_policy favorite_slots hud_settings menu_focus minimap_policy resource_snapshot; do
    timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src "modules/ui/tests/${T}_test.cpp" -o "$OUT/${T}_test"
    timeout 600 "$OUT/${T}_test"
done
SKYCRPG_SKYUI_SOURCE="$SKYUI" timeout 600 python3 -I modules/ui/tests/inventory_prepare_contract.py "$OUT"
timeout 600 "$NODE" modules/ui/tests/inventory_actions_contract.js modules/ui "$OUT/InventoryMenu.as"
for T in hud_contract minimap_compass_contract navigation_contract; do
    timeout 600 "$NODE" "modules/ui/tests/${T}.js" modules/ui "$SKYUI"
done
SKYCRPG_SKYUI_SOURCE="$SKYUI" timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"
```

The recorded driver invocations were `timeout 600 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p1/run-checks.py --focused`, then the same command with `--remaining` (reusing the focused PASS). Outputs remain outside the repository.

CommonLib declarations rechecked against `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0/include`: `TESObjectREFR.h` defines `Count = std::int32_t`, `InventoryItemMap` and `GetInventory(std::function<bool(TESBoundObject&)>, bool)`; `InventoryEntryData.h` declares `object` and `BSSimpleList<ExtraDataList*>* extraLists`; `ExtraDataList.h` declares `std::int32_t GetCount() const`, `GetByType<T>()`, `HasType<T>()` and `HasQuestObjectAlias()`; `StandardItemData.h` declares `std::uint32_t GetCount()` and `bool GetEnabled()`; `ItemList.h` declares `items`, `GetSelectedItem()` and `Update(TESObjectREFR*)`. Existing trait fields/accessors (`ExtraHealth::health`, `ExtraEnchantment::enchantment`, `ExtraOwnership::owner`, `ExtraTextDisplayData::displayName`, `ExtraPoison::poison/count`, `ExtraCharge::charge`, `InventoryEntryData::IsOwnedBy(Actor*, bool)`) were also verified. `SKSE/Interfaces.h` retains `AddUITask(TaskFn) const`. No new CommonLib API was introduced. Signed native counts are checked before unsigned conversion and sums use 64 bits; the new code uses no `min`/`max` calls.

### Runtime confirmation still required

**NOT_RUN:** Windows MSVC `/W4 /WX /permissive-` compilation for this correction and runtime confirmation with tempered/enchanted/stolen rows undergoing a no-event same-base change. Portable compilation and mocked contracts do not establish native grouping, pointer lifetime or task/callback ordering. Skyrim was never launched. No corrected DLL/SWF/package hashes or owner acceptance are claimed; existing runtime rows and the favorite-guard **BLOCKED** status above are unchanged.

For the orchestrator's existing isolated copied-save fixture, within a single 20-minute route: record the selected row/token/generation/count/traits and sibling instances; use a controlled fixture change without an inventory event to (1) increase the same-base count from 1 to 2 and (2) add/change a tempered, enchanted or stolen instance while the row has empty or partial extras. Attempt activation/drop and quantity confirmation before native reconciliation; expect no native dispatch and one bounded row refusal. After the native list rebuild, explicitly reselect a fully represented matching row and perform one deliberate action; verify only that instance changes, with native counts/ownership/weight preserved. Repeat an unrelated-form change and confirm a still-matching selected row remains usable. Record callback thread diagnostics: an off-main-thread fallback is owner-accepted native behavior and cannot count as guarded-row confirmation. Result: **NOT_RUN**.

**Orchestrator review note on the correction.** The remainder rule refuses a row with no extras (or only some) whenever the same base form has a differing live instance that the row does not list. That is correct if the native menu merges those copies into one row. If the native menu instead shows the plain stack and the tempered/stolen/enchanted copy as **separate rows**, the plain row's `extraLists` may never list the other copy, and the plain stack would be refused for as long as both exist: safe, but a usability FAIL. The native grouping is not established from source. Each refused selected row therefore also logs one bounded line (≤32 per process): `Inventory row <reason> form=<id> row=<count> ambiguous=<0|1> represented=<n>/<count> live=<total> extras=<n>/<count> differing=<n>`. The runtime fixture must act on the **plain** row while a tempered, a stolen and an enchanted copy of the same base exist. If those actions are refused with `row-coverage`, the rule needs a native-grouping-aware relaxation backed by that log before acceptance.

## P1 correction runtime — 2026-10-27: FAIL

**FAIL (owner-observed, log-confirmed).** The `effea52` rule refuses rows the native menu itself produces, and its refusal path loops.

| Field | Value |
| --- | --- |
| Source / CI | `effea52`, Windows run 40564740950 **PASS**; ZIP `9c7bf7a7aa97aeb9…`, DLL `94090ad3cac6c9b8…`; movies unchanged (`211115dc…`, `8766d2b2…`, HUD `2de109b3…`) |
| Save | Vanilla fixture (Nexus mod 178, "All Main Quests"); run continued from the copied Dragonsreach save |
| Run | `3c5b1837459744378427ad3c2f5983b8`, about 12 min, normal quit, watchdog did not intervene (minimum 12.5 GiB available); restoration **PASS** |
| UI log | `5f859c0b…`: 73 `row-coverage`, 7 `row-drift`, 2 `duplicate-input`, no off-thread lines. Timestamped copy in `session-p3/p3-inv-163-review/log-timeline.txt` |

What the owner saw and the log shows:
- **Plain row refused.** The plain Iron Sword row was refused whenever a tempered or self-enchanted copy existed: `row=3 represented=0/0 live=4 extras=1/1 differing=1`, then `row=2 … extras=2/2 differing=2`. The native menu shows plain units as their **own row** with no extra lists, and the modified copies as separate rows. The `effea52` remainder rule can never prove that row.
- **Enchanted row flickers and won't drop.** The enchanted and plain counts kept switching between 2 and 1, and dropping the enchanted copy did nothing. The log shows bursts of more than 15 refusals in the same second (19:33:44). Each refused sample calls `Refresh()` → `ItemList::Update`, the rebuild fires another selection sample, and that sample is refused again: a refresh feedback loop that `effea52` introduced.
- **Stolen helmets merged and trapped.** The owner had 1 stolen Iron Helmet; after `player.additem 00012E4D 2` the menu showed **3 stolen**. That merge is very likely native: vanilla stacks stolen and owned units of one base into a single entry and flags it stolen. SkyCRPG does not mutate items. `effea52` then refuses the merged row because its units have differing traits, so the helmets cannot be dropped at all.
- **Spurious drift.** `row-drift` on single items (`represented=1/1 differing=0`) right after they changed. These evented changes already trigger reconciliation, so the drift check costs a press.

Conclusion: the P1 correction must refuse **stale** rows, not native groupings. The corrected rule (next commit): a selected row is permitted only when its count equals its represented extras, or its represented extras plus all plain live units of that base (the native plain-unit holder). It must be fresh: no unevented drift of that form. Refresh is requested at most once per refusal episode. Native merged rows act with native semantics, as without SkyCRPG. The native-merge claim for stolen plus owned is to be confirmed on the baseline (no SkyCRPG) profile.

## P1 correction, round 2

2026-10-27, DYL-163, `phase3/dyl-163-inventory`, base `e63a683` plus uncommitted corrections. This section supersedes the row-matching and reconciliation claims in “Senior review P1 correction”; it does not change any other acceptance row. The [P1 correction runtime FAIL](#p1-correction-runtime--2026-10-27-fail) establishes why `effea52` failed: the native menu gives plain units their own row with empty extras (`row=3 represented=0/0 live=4 extras=1/1 differing=1`), and also merges a stolen extra with plain owned units. The old remainder/differing-instance rule rejected both native groupings. The timestamped `/srv/storage/Projects/SkyCRPG/session-p3/p3-inv-163-review/log-timeline.txt` also shows more than 15 refusals at 19:33:44: refresh rebuilt the list, selection sampled again, and refusal requested another refresh. Evented equip/container changes also incurred unnecessary `row-drift` refusals.

The revised portable `MatchRow` policy requires membership before any represented extra is inspected, no duplicate represented extras, and exact equality between each represented extra's copied traits/count and its live instance. All live extra counts must be positive, valid and account for no more than the live total. Let `representedCount` be the represented extra count and `plainCount = live.count - sum(all live extra counts)`. Exact coverage means **`row.count == representedCount` or `row.count == representedCount + plainCount`**; any other coverage refuses with `row-coverage`. A plain holder therefore covers all plain units, while an extras-only row can coexist with other same-base rows. Unrepresented differing extras no longer cause refusal. Missing represented extras still refuse before dereference (`row-extra-missing`).

Freshness still refuses unevented changes of the selected form (`row-drift`); unrelated-form drift permits an otherwise valid selected row while requesting reconciliation. Evented invalidation now clears `g_native` for container/equip/context/death changes, in addition to the existing menu/load resets. Tracked list/view changes and queued native reconciliation also reset that baseline. The next reconciled sample must prove membership/consistency/coverage again, without being refused for an already reconciled event.

**Deliberate change for senior review:** `ambiguous` remains in the copied snapshot and the AS-facing row `available` flag remains `!ambiguous`, limiting SkyCRPG's own features. It no longer refuses native activation/equip/drop/favorite in `MatchRow` or `Validate` merely because represented extras differ among themselves. Invalid traits, stale generations, identity/count changes, quantities and native restrictions retain their checks. SkyCRPG refuses stale or inconsistent rows; it does not override native grouping or select an alternative copy. Native merged rows act with native semantics, as without SkyCRPG.

`ListReconciliation` holds the single pending request through the entire queued `ItemList::Update(player)`, including synchronous selection callbacks. Only UI-task completion clears it (a task for an expired world/menu session retires without rebuilding that session). Refusal keeps the snapshot invalid and the list dirty until reconciliation. A refused Snapshot returns undefined without the former extra handler-level request; dirty/refused selection samples and drift detection do not call refresh/request again while pending. Refused Validate can return false while sharing the existing request. Bounded `LogRowDetail` and `LogRefusal` remain. The owner-approved off-main-thread fallback is unchanged. No new engine hooks, offsets, native actions, dependency or integration changes.

### Automated verification

**PASS:** the rewritten `inventory_policy_test.cpp` covers plain row 3 beside one tempered/enchanted extra in live 4; extras-only row 1; stolen extra 1 plus plain 2 merged into row 3; stale plain row 2 versus plain 1 plus tempered 1; unevented same-form count/trait drift; unrelated-form drift; missing extras; represented count/trait mismatch; duplicate/malformed extras; partial plain-holder coverage; mixed stolen extras with different owners; event-reset observations; stale quantity/selection invalidation; and native validation with informational ambiguity. Its portable reconciliation harness simulates repeated refused highlights, a refused Validate, and reentrant selection callbacks during an update, proving only one request until task completion and permitting a new request in a later episode.

All requested checks **PASS**: lane runner **15/15 steps**, Python discovery **18 tests, no skips**, and the separate minimap compass contract. The focused portable compile/run also **PASS**. Exact commands, run from the worktree root:

```sh
timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/inventory_policy_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/p1-round2/inventory_policy_test
timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/p1-round2/inventory_policy_test
timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"
timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/minimap_compass_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
```

Before execution, job purpose, command, expected output, timeout, owner (DYL-163 / Codex), completion signal and cleanup were recorded in `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/p1-round2/{focused,remaining}-jobs.json`; those records retain PIDs, UTC deadlines, results and log paths. The runner records its 15 child jobs in the parent `jobs.json`. Owned jobs were reaped and their process groups verified absent. The final full check run followed an additional pending guard on the drift path; no failing retry occurred. No game or deployment job was started.

CommonLib 7.2.0 declarations rechecked under `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0/include`: `SKSE/Interfaces.h` has `AddUITask(TaskFn) const`; `RE/I/ItemList.h` has `Update(TESObjectREFR*)` and `GetSelectedItem()`; `RE/I/InventoryEntryData.h` has `BSSimpleList<ExtraDataList*>* extraLists`; `RE/E/ExtraDataList.h` has signed `std::int32_t GetCount() const`; `RE/S/StandardItemData.h` has unsigned `std::uint32_t GetCount()`; `RE/T/TESObjectREFR.h` has signed `Count`, `InventoryItemMap` and the existing filtered `GetInventory` overload. No new CommonLib API is used. Positive signed counts are validated before unsigned conversion; coverage sums use `std::uint64_t`. No new `min`/`max` expressions.

### Runtime retest

**NOT_RUN:** Windows MSVC `/W4 /WX /permissive-` build and runtime retest for this correction. Portable tests do not establish actual GFx/UI-task ordering or gameplay behavior. No candidate DLL/package hashes or owner acceptance are claimed (`<head SHA>`, `<CI run>`, `<DLL sha256>`, `<SWF sha256>` remain for the integrator). Skyrim was never launched. Existing runtime rows, including the favorite pre-dispatch guard **BLOCKED**, remain unchanged.

For the orchestrator's next isolated copied-save route (20-minute budget), act on the plain holder beside tempered and enchanted copies, then act on each modified row; verify stable displayed counts, one native action per deliberate press, native carry weight, and no refresh refusal burst. Exercise a merged stolen-plus-plain row and two stolen extras with different owners; record native semantics and compare the merge with the no-SkyCRPG baseline. Repeat equip/container event changes and confirm the next reconciled selection needs no drift-recovery press. Finally exercise a controlled unevented same-form increase and stale partial plain coverage: expect refusal before dispatch, at most one pending native reconciliation, then a fresh deliberate action after reconciliation. Repeated highlights and selection callbacks during that update must not queue further updates. Result: **NOT_RUN**.

## P1 round 2 runtime — 2026-10-27: PASS with one usability defect

**PASS (owner-reported, log-confirmed) for row integrity; FAIL for first-press usability with mouse highlighting.**

| Field | Value |
| --- | --- |
| Source / CI | `e202dfd`, Windows run 40578525017 **PASS**; ZIP `285f5fd0f11c12ec…`, DLL `34f2b56de91d51b5…`; movies unchanged (`211115dc…`, `8766d2b2…`, HUD `2de109b3…`) |
| Save | Vanilla fixture (Nexus mod 178); the owner rebuilt the copies and saved them as a reusable fixture (newest save in `session-p3/p3-inv-163-round2/after-test-saves`) |
| Run | `5f95c0b048c74bde959ec6f5df90f85c`, about 8 min, normal quit, watchdog did not intervene (minimum 12.3 GiB available); restoration **PASS** |
| UI log | `c1bdef18…`: **0** `row-coverage`/`row-drift`/`row-extra-missing`, **0** off-thread, no refusal bursts; 7 `duplicate-input` (20:53:26–20:54:31 UTC, about 10 s apart) |

The owner reports that everything passed:
- Plain, tempered and self-enchanted Iron Sword rows each acted on their own copy, with steady counts.
- The stolen-plus-owned helmet row dropped.
- Double presses did one action.

This resolves the `effea52` FAIL: native split and merged rows are permitted, and the refresh loop is gone.

**Defect:** for every drop, the **first** press did nothing and the cursor jumped to the category list; re-highlighting the item and dropping again worked. The 7 `duplicate-input` refusals match those first presses. The original mouse-hover/token diagnosis is superseded by the source and regression evidence below.

## Round 3: first-press refusal on engine-delegated drop

Base: `9cef8d798cce83600af4a07b952b03eaf1ee3308`, branch `phase3/dyl-163-inventory`, including the uncommitted round-3 AS changes. Round 3b replaces the earlier evidence section. The owner-observed first-press failure remains the runtime finding; this work does not claim a runtime retest.

### Cause and evidence

The base already captures after `super.onItemHighlightChange(event)`. Ordinary mouse-highlight tests passed there, so “capture occurs only on key-down” was incorrect. The earlier callback-before-highlight regression exposed a separate `focus` refusal; it did not reproduce the owner's `duplicate-input` failure.

Pinned SkyUI 6.11 `source/actionscript/ItemMenus/InventoryMenu.as` registers both `DropItem` and `AttemptEquip` with `gfx.io.GameDelegate.addCallBack`. `DropItem()` gates on `shouldProcessItemsListInput(false) && selectedEntry != undefined`, then either immediately calls `onQuantityMenuSelect({amount:1})` or opens the native quantity dialog. `AttemptEquip` dispatches `ItemSelect`; mouse `onItemSelect` also dispatches `ItemSelect`. `BasicListEntry.onPress` calls the list's `onItemPress`, which supplies that mouse selection event. None of these dispatch sites guarantees our `handleInput` hook has received the current key-down first.

The old `ActionIntents::Begin` spends `_consumed`; only a down edge with no held keys clears it. Therefore previous action → down/up completed → refreshed selection → engine `DropItem` → late `handleInput(keyDown)` refuses the new press as `duplicate-input`. The refusal clears `selectedIndex` and invalidates the list, explaining the category jump. The late down used to re-arm the refused gesture. `AttemptEquip` has the same policy hazard, although the owner reported no equip failure.

**FAIL reproduced on the pre-round-3b policy:** the expanded AS harness runs the actual transformed menu and list invalidation methods against the actual portable C++ `ActionIntents`, using a small replay adapter rather than a JavaScript copy of the policy. A temporary copy of the old header and round-3 transformed menu successfully completes a preceding action, then refuses the next callback-before-down press with exactly `['duplicate-input']`. Separate runs reproduce this on different items and on the same stack with a native list refresh between actions. Expected regression assertion exits: 1. Artifacts/logs: `session-p3/logs/dyl-163/round3b/baseline/`, `baseline-as.log`, `baseline-same-stack.log`, `as-jobs.json`. The repository header was never reverted for these runs. This verifies the corrected causal path; the actual deployed engine event order remains uninstrumented here.

### Chosen rule

A tracked key-up that releases the **last held key** re-arms `_consumed`. Down only records a held key; it never re-arms a completed or refused action. Unmatched ups cannot re-arm. Thus the previous completed down/up permits the next press whether the engine callback arrives before or after its down. Mouse callbacks use the same portable rule through the existing synthetic mouse key `65535`. No clock, timestamp protocol or AS validator argument change is needed.

Every `Begin` attempt, including duplicate and pending refusals, spends the gesture. Fresh snapshots, another action kind, a late down or list refresh cannot retry it. `_lastToken` remains an independent guard across release: the same spent token is still refused; a newly captured selection is also required. `Confirm` spends its confirmation gesture and validates the saved quantity intent at most once, including when confirmation is refused. Releasing the initiating drop key does not discard the pending quantity intent. Only a later explicit action can dispatch; edges/refresh/Observe never dispatch.

`ActionIntents::Invalidate` cancels pending authority while retaining physical held/consumed state, rather than fabricating either an all-up or a new consumed gesture. Snapshot world/session/view generations still invalidate selections in the existing adapter. AS view invalidation likewise retains its held-key map. This prevents view changes from synthesizing a fresh down, and avoids an artificial first-press block after an already released gesture. Held lifecycle invalidation, empty lifecycle invalidation and unmatched releases are covered by portable tests.

The round-3 native-index capture guard, capture before `super.onItemHighlightChange`, existing capture after `super`, and different-index acquisition in `skycrpgInventoryAllow` are retained: their tested callback-before-highlight and reentrant-highlight cases remain useful. Repeated captures of the same backing entry cannot renew a consumed token. `InventoryLists.InvalidateListData`, `onFilterChange`, `onSortChange` and `showItemsList` already invoke `skycrpgInventoryViewChanged`; it clears the token before list processing. In particular, `InvalidateListData` then dispatches a highlight notification when selection survives. The same-stack regression executes this actual transformed method, proving refresh clears and reacquires the token even when `itemIndex` is unchanged. A refresh without release still cannot bypass `_consumed`.

No matching/coverage, reconciliation, drift handling, off-thread fallback, `inventory_bridge.cpp`, `plugin.cpp`, CMake, dependency, package or other feature changes were made. Genuine refusals retain the existing focus-clearing UX.

### Tests

**PASS — portable inventory policy:** `GestureOrdering` covers repeated engine callback-before-down and down-before-callback actions for drop/activation; held/repeated downs and second dispatch with different tokens; unmatched ups; all-devices release; old-token refusal; a refused gesture followed by late down, refreshed token or observation; lifecycle invalidation; and quantity confirmation once. Existing snapshot, matching, reconciliation and stale/modal tests remain. A pending refusal now explicitly requires release before a later action, rather than retrying after `Observe`.

**PASS — AS contract:** engine `DropItem` before the real transformed `handleInput(keyDown)` twice in succession on different items and twice on the same refreshed stack; `AttemptEquip` and mouse `onMouseDown`/`ItemSelect` in both down/callback orders; callback-before-highlight and reentrant highlight; same-entry token retention; held repeat across native list refresh; spent refusal in both orders; frozen quantity selection and confirm/cancel; quantity `ItemDrop` exactly once in both confirmation orders; native dispatch preservation and absent/old bridge fallback. `inventory_prepare_contract.py` pins the capture ordering, all four invalidation hooks and token invalidation before native list refresh.

Commands from the worktree root (job records include the executable paths, profiles and deadlines):

```sh
timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/inventory_policy_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/round3b/inventory_policy_test
timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/round3b/inventory_policy_test
timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"
timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/minimap_compass_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
```

Results: focused portable build/test **PASS**; candidate AS harness **PASS**; lane runner **PASS, 15/15 steps**; Python discovery **PASS, 18 tests, no skips**; minimap compass contract **PASS**; `git diff --check` **PASS**. The lane runner also builds the AS replay adapter with the same strict g++ flags. MSVC `/W4 /WX /permissive-` verification is **NOT_RUN** on this Linux host.

Records under `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/round3b/` include purposes, exact commands, expected outputs, 600-second focused / 900-second lane limits, owner (DYL-163 / Codex), completion signals, process groups, logs, deadlines, exit codes and cleanup. `as-jobs.json` records both intentional baseline failures and candidate PASS; `full-jobs.json` records the requested checks. Initial harness setup failures are retained separately: a missing copied dependency, a Node synchronous stdin EOF timeout (changed to file replay), and an invalid fixture form ID (corrected). These were changed-input diagnostics, not unchanged retry loops or product runtime results. All owned job groups were reaped and verified absent.

### Runtime retest

**NOT_RUN:** native DLL/MSVC build, SWF compilation, packaged-movie hashes, actual GFx/input/release ordering, runtime menu/modal lifecycle and in-game first-press usability. No Skyrim, Steam, Proton, deployment, commit or push was performed. Existing round-2 row-integrity PASS evidence and favorite pre-dispatch guard **BLOCKED** status remain unchanged.

Next owner/orchestrator copied-save route (**NOT_RUN**): verify candidate DLL/SWF hashes; drop once on A, then once on B, then twice from the same remaining stack with list refresh between; repeat with keyboard/controller selection; equip with successive presses and mouse clicks; hold/repeat and check one native mutation; open quantity, release the initiating key, confirm once or cancel, then perform another action; exercise menu close/reopen and focus transitions. Record input/down/up/callback order, native counts/weight, focus retention and any `duplicate-input` refusal, together with executable/runtime, dependencies, host, source commit and package hashes.

**Orchestrator review addition.** Re-arming only on the release of the last held key means a key-up that is never delivered keeps `_held` non-empty. Closing the inventory with Tab does exactly this: Tab's down reaches the menu, its up does not. The round-3 change also stopped clearing `_held` on invalidation, so every action in the next session would have been refused. `ActionIntents::ResetSession()` now clears held keys and re-arms when the InventoryMenu opens or closes and on load. No engine callback fires without a press inside the menu. `inventory_policy_test.cpp` covers a lost Tab up across a session and shows that the session would stay blocked without the reset.

## Round 3 runtime — 2026-10-27: PASS

**PASS (owner-reported, log-confirmed).**

| Field | Value |
| --- | --- |
| Source / CI | `15f291a`, Windows run 40585055566 **PASS**; ZIP `336cc0c70afaab4c…`, DLL `2e0a0815eed7d36f…`; `inventorymenu.swf` `695ddda0af550d67…`, `skyui/inventorylists.swf` `8766d2b2…`, HUD `2de109b3…` |
| Save | Owner-made `SkyCRPG_Fixture_InventoryItems` (from the vanilla Nexus fixture; plain, tempered and self-enchanted Iron Swords; stolen and owned Iron Helmets) |
| Runs | `39b6e9bf…`: ended at about 1 min with no inventory activity, clean exit, restore PASS (no evidence). `ec0b4e3cd75f47568b46d20190a8f3da`: about 4 min, normal quit, watchdog did not intervene (minimum 11.9 GiB available); restoration **PASS** |
| UI log | `f510e97a…`: **no inventory refusals of any kind** (0 row, 0 `duplicate-input`, 0 stale, 0 off-thread) |

Owner route, all reported passing:
1. Three different items highlighted with the mouse each dropped on the first click, with no jump to the categories.
2. Two drops in a row from the same stack.
3. The inventory closed with Tab, reopened, and the drop worked first time.
4. A quick double press dropped only the expected item.

The log has no `duplicate-input`, so the double press did not reach the guard as a second dispatch for the same press. The engine either issued one drop, or the second tap was a separate completed press acted on as one. The owner's observation is the evidence for the outcome.

This closes the round-2 first-press defect. Row integrity from round 2 (plain, tempered and enchanted rows; the merged stolen-plus-owned row) is unchanged in round 3.

Still open for DYL-163:
- Senior review of the round-2/3 design points: native-row coverage rule, `ambiguous` informational for native actions, re-arm on release plus session reset.
- Favorite pre-dispatch guard: **BLOCKED**.
- **NOT_RUN:**
  - quest-item drop refusal;
  - modded form;
  - controller;
  - missing-DLL fallback;
  - combined profile;
  - baseline (no SkyCRPG) confirmation that vanilla merges stolen and owned units.

## Senior re-review P2: no sampling during native list rebuild

Base: `bdae681`, branch `phase3/dyl-163-inventory`, 2026-10-27. Addresses the bounded P2 in the [latest senior re-review](https://github.com/Sonoran-Solutions/SkyCRPG/pull/17#issuecomment-6454817858). `gh pr view 17 --comments` could not connect to GitHub on this host; the connected GitHub tool supplied the discussion, including that latest review. This correction does not close the remaining acceptance gates above.

### Risk and correction

An unrelated-form drift can queue `RequestListUpdate()` with `g_dirty` still false. `g_mutex` is recursive: synchronous selection/highlight callbacks from native `ItemList::Update(player)` can re-enter the snapshot handler on the same thread. Pending reconciliation coalesces requests but did not prevent sampling a partially rebuilt native list.

The pinned CommonLibSSE-NG 7.2.0 source at `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0/src/RE/I/ItemList.cpp` only forwards `Update(TESObjectREFR*)` to `Update_Impl`, an opaque `RELOCATION_ID(50099, 51031)` call. It provides no reconstructed-list sequencing proof. The existing `Update(TESObjectREFR*)` signature was verified against the pinned `include/RE/I/ItemList.h`. No new engine API, hook or offset is used.

`ListReconciliation::UpdateScope` sets a portable `Updating()` state and restores the previous state on scope exit, including nested scopes and unwinding. It does not alter `Request`/`Pending`/`Complete`. The queued UI task holds this scope around the actual native update and sets `g_dirty` true for the call, including updates requested by unrelated drift; normal completion clears dirty. The pending request completes afterward as before.

`Sample()` tests `Updating()` first and returns false before acquiring or reading the list, requesting reconciliation, refreshing or logging. Reentrant Snapshot therefore leaves the GFx return undefined and clears its cached selection. Validate explicitly returns false during Update before argument/stale-token handling, with no logging or extra update request, even if it retained a formerly valid token. The off-thread native fallback remains unchanged.

After Update returns, normal sampling resumes with the existing cleared drift baseline. An unrelated-form update does not bump the snapshot generation merely to mark the temporary dirty window; a still-matching selected row remains usable afterward. If a reentrant highlight obtained undefined, the existing AS first-acquisition path recaptures on the first explicit action after Update. No AS changes were needed. Row coverage, gesture rules, session reset, shared minimap/follower integration and all other production files are unchanged.

### Verification

All job purposes, exact commands, expected outputs, owners, limits, completion signals, PIDs/process groups, logs, deadlines, exits and cleanup results are recorded under `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p2/` in `run_checks.py`, `jobs.json` and `suite.log`. Focused jobs have 600-second limits; the requested lane runner has a 900-second outer limit. All 24 jobs met their expected results and their owned process groups were reaped and verified absent. Outputs stayed outside the repository.

| Check | Result and limits |
| --- | --- |
| Portable `ReentrantListUpdate` regression | **PASS:** 32 synchronous sample callbacks during a mock rebuild perform zero list reads/publications and request no second update; retained-token Validate refuses. Pending alone permits sampling. The next post-update sample and validation of the unchanged selected row succeed. Nested scope and unwinding restore the state. |
| Guard-disabled negative control | **PASS (expected regression FAIL):** compile the unchanged candidate test with a temporary copy of `list_reconciliation.h` whose scope no longer sets the update marker. It exits with SIGABRT (`-6`) at `reads == 0 && publications == 0` inside the reentrant callback. Compilation succeeds; `unguarded-reentrant-negative.log` records the assertion. The repository guard was never removed. |
| Actual transformed AS replay | **PASS:** reentrant highlight acquisition gets undefined with no published token, Observe or refresh; the first post-update drop recaptures and dispatches once in both callback-before-down and down-before-callback orders, without an extra press. Native callbacks are mocked; this is not GFx/game evidence. Existing gesture/modal/fallback cases also pass. |
| Native adapter source contract | **PASS:** checks immediate Sample refusal, dirty marking within the RAII native-call scope, and early false Validate before stale-token handling. This source check does not compile or execute the engine adapter. |
| Requested lane runner | **PASS, 15/15 steps**, including portable inventory policies and the AS replay adapter. |
| Python discovery | **PASS, 19 tests, no skips.** |
| Follower, HUD, minimap compass and navigation JS contracts | **PASS, all four.** |
| Every portable `modules/ui/tests/*_test.cpp` | **PASS, all seven build and execute** with `-std=c++20 -Wall -Wextra -Werror -pedantic`. |

Commands from the worktree root (the recorded runner expands every loop into individual build/run jobs):

```sh
timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src modules/ui/tests/inventory_policy_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p2/inventory_policy_focused
timeout 600 /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p2/inventory_policy_focused
timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"
for test in follower_panel_test hud_contract minimap_compass_contract navigation_contract; do
  timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node "modules/ui/tests/$test.js" modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
done
for source in modules/ui/tests/*_test.cpp; do
  binary="/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163/senior-review-p2/$(basename "$source" .cpp)"
  timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src "$source" -o "$binary"
  timeout 600 "$binary"
done
```

### Runtime gate

**NOT_RUN:** native Windows/MSVC `/W4 /WX /permissive-` build, SWF compilation, packaged candidate hashes, actual native rebuild/GFx callback ordering and first-press usability in game. No Skyrim, Steam, Proton, deployment, commit or push was performed. Existing round-2/3 runtime PASS evidence and favorite pre-dispatch **BLOCKED** status are unchanged.

Owner/orchestrator follow-up (**NOT_RUN**, copied save and isolated profile): record source/DLL/SWF hashes, executable/runtime, dependencies and host; cause unrelated inventory drift, observe reconciliation/highlight callbacks, then drop or equip the unchanged selected row on the first press. Check native counts/weight, no partial-list crash/refusal storm or automatic retry, then repeat with a native list refresh and quantity action. A future native runtime result is required before claiming this guard's in-game acceptance.

## Owner ruling — native favorite toggle (2026-10-27)

> Owner ruling for DYL-163: the native favorite toggle stays native and unguarded, because it only flags an item and never moves, drops or consumes it; record it as an accepted, scoped limitation.

Recorded in `docs/DECISIONS.md`. The favorite pre-dispatch row moves from **BLOCKED** to an **accepted limitation (owner ruling)**. Pinned SkyUI 6.11 exposes no Scaleform callback before the native favorite dispatch, and guarding it would need a new engine hook. DYL-163 continues to guard activate, equip and drop, including quantity confirm, and still observes favorite state in snapshots.
