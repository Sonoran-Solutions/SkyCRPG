# DYL-164 / P3-02 — grid inventory and equipment presentation

## Candidate status and identities

**PASS:** actual-AS mock contracts, 500-entry/18-clip fixture, portable equipment policy, existing inventory integrity regression, Python contracts and local SWF compilation. **NOT_RUN:** Windows/MSVC DLL compilation, gameplay, actual GFx rendering/input/timings, executable/package verification and owner acceptance. No Skyrim, Steam, Proton, deployment, commit or push was performed. This is an implementation candidate, not completed U3/U4/U6 or a public release.

Ticket: [DYL-164](https://linear.app/devils17/issue/DYL-164). Worktree `p3-grid`, branch `phase3/dyl-164-grid`, base `945afd19598d52621d34df90cadf84bc7ff59ebc`. Candidate source `<head SHA>` plus worker diff; Windows `<CI run>`; DLL `<DLL sha256>`; internal package `<package sha256>`; owner runtime executable/save/profile hashes **NOT_RUN**. Movies below replace the `<SWF sha256>` placeholders for this local source candidate only.

Local host: `dq-MEG-Z590-Trident-X-MS-B926`, Linux, Python 3.14.4, g++ 15.2.0, Node v24.19.0. Target remains Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0 (`7a60f4de`), SkyUI Community 6.11 and the recorded Proton host. The owner must attach the actual executable, dependency, host/Proton, loaded-file winners, source, DLL, package, SWF and copied-save hashes to every gameplay result. Prior DYL-163 passes are retained in [inventory integrity](inventory-integrity.md); they do not establish runtime acceptance of these new movies or this equipment reader.

## Design and contracts

The layout uses the 1280×720 movie stage: left equipment panel at x18, width264; middle native list at x300, width542; right native item card at x852, width408. The content ends above the existing bottom bar. Narrow stages fit proportionally; larger visible stages center the baseline. Categories, search widget, sort header and column/order controls retain their native implementations and listeners. Search is fitted to 108 units and the column button to 18; the native list scrollbar sits at x526 relative to its panel. The footer retains native gold, carry weight/capacity and device prompts.

The grid remains presentation on native ItemList entries, clips, mouse listeners and backing `itemIndex`. There are three columns and six rows, with exactly eighteen pooled clips; no new inventory database or item-capacity cap. The player menu installs two instance methods for row-aligned scroll/max-scroll calculations. Maximum is `max(0, ceil(filteredSize / 3) * 3 - 18)`; the start is clamped and rounded down to a multiple of three. Thus 19/20 entries and 500 entries have partial last rows without column shifts. Other menus do not install these methods. Existing ScrollingList/TabularList/TabularListEntry builder transforms remain unchanged. A final static comparison also confirms the HUD branch and all four DYL-163 safety files are byte-for-byte unchanged from the base.

Each cell draws its native loaded icon when a requested frame resolves; it resets to frame1 before trying a label, so a missing label cannot retain an old recycled icon. An original broad-type vector symbol remains underneath during missing/unloaded icon cases. Names use a bounded two-line/32-character display with measured-height truncation; full names remain in the native card. Counts have a separate badge. E/F/S/Q letters mean equipped/favorite/stolen/quest; `?` means unavailable, never false. Native entry fields take precedence. Only absent flags consult the DYL-163 copied observation already acquired by the action lifecycle, matching native index/count and `available`; ambiguous observations are not promoted into definite display flags. List/view invalidation discards that observation. Paint never acquires a snapshot, renews a token, changes selection, or dispatches an item action. Unavailable snapshot flags can remain unknown until a fresh selection observation succeeds.

The native card still owns full name, damage/armor, weight, value, effects, restrictions and item-specific content. A textual status line supplements it; no inferred damage, armor, weight or value is added. Native quantity/list/confirmation submenus keep their native focus and continuation. Opening one centers/fits the existing card above an opaque full-stage backdrop; closing returns it to the details column. The old native item-preview mouse rectangle is confined to the details column. This is not a new 3D preview engine.

Equipment is a read-only original static vector silhouette plus eight named slots: head (head/hair/circlet), body, hands, feet, left/shield, right hand, amulet and ring. A single fresh armor-inventory scan copies worn forms and existing worn custom display names; native hand getters supply weapons, spells or torches. Missing custom names use the base form name. Multi-slot armor appears in each covered slot. Conflicting records yield Unknown rather than choosing a form; known absence yields Empty. Symbols describe native broad form type. Equipment selection/actions use the item in the grid; the silhouette slots are displays.

Refresh is a coalesced 100 ms one-shot after configuration, native list/view changes and native player-info updates. Invalidation clears old values immediately. The reader is synchronous on the verified main thread, requires the live InventoryMenu, loaded/alive 3D player/current process and no Loading Menu, and returns copied strings/numbers/booleans only. It has no retained native pointer, native cache, queued native work, new hook/offset or lifecycle service. An unsupported thread returns unavailable equipment; it does not alter the owner-approved native action fallback. Close and movie unload cancel pending AS work and discard display observations. There is no per-frame equipment polling.

DYL-163 row coverage, drift, reconciliation, rebuild guard, gesture re-arm and session reset are untouched. Its four existing `skycrpgInventoryAllow` dispatch gates and all ordered native delegate calls remain unchanged. Native favorite toggle stays unguarded by owner ruling; off-main-thread action validation retains native behavior by owner ruling. Counts, ownership, carry weight, equipment, favorites and quest restrictions remain engine authoritative. Container, barter, crafting, follower trade, magic, journal and map contexts retain native/SkyUI access and their handlers. Native paused inventory semantics and Camera independence remain unchanged.

Shared integration was applied directly within the owner's authorization: one equipment header include/registration after the existing registrations and one additional source on the UI `add_library` line. No separate integration patch is needed. No HUD/minimap/follower/compass/Camera/config/CI/packaging file was changed.

## Changed files

- `docs/UX_SPEC.md`
- `docs/evidence/p3/grid-inventory.md`
- `modules/ui/CMakeLists.txt`
- `modules/ui/scaleform/grid-methods.as.inc`
- `modules/ui/scaleform/inventory-actions.as.inc`
- `modules/ui/scaleform/inventory-list-layout.as.inc`
- `modules/ui/scaleform/inventory-presentation.as.inc`
- `modules/ui/src/inventory/equipment_bridge.cpp`
- `modules/ui/src/inventory/equipment_bridge.h`
- `modules/ui/src/inventory/equipment_policy.h`
- `modules/ui/src/plugin.cpp`
- `modules/ui/tests/grid_contract.js`
- `modules/ui/tests/inventory_equipment_test.cpp`
- `modules/ui/tests/inventory_fixture_test.py`
- `modules/ui/tools/build_scaleform.py`

External runner: `session-p3/tools/dyl-163-run-checks.py` (worktree/log routing plus added test jobs). External logs, binaries, fixture outputs and compiled movies remain outside Git.

## CommonLib 7.2.0 API audit

Read-only root: `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0/include`. Every new engine API/field below was checked there; this is source verification, not MSVC compilation or runtime-layout proof.

| API / data used | Verified declaration and relevant source |
| --- | --- |
| `PlayerCharacter::GetSingleton()`; `IsDead(bool=true)`; inherited `Is3DLoaded()` | `RE/P/PlayerCharacter.h`, `RE/A/Actor.h`, `RE/T/TESObjectREFR.h` |
| `GetActorRuntimeData().currentProcess` | `RE/A/Actor.h`; used only to refuse unavailable equipment |
| `Actor::GetEquippedObject(bool left) const` and `GetEquippedEntryData(bool left) const` | `RE/A/Actor.h`; `src/RE/A/Actor.cpp` verifies null-process/middleHigh handling and the borrowed-entry lifetime; never stored |
| `GetInventory(std::function<bool(TESBoundObject&)>, bool noInit)` and owned entry copies | `RE/T/TESObjectREFR.h` and `.cpp`; `true` avoids initializing extra changes for a display read |
| `TESForm::IsArmor()`, `As<TESObjectARMO>()`, `GetName()`, `GetFormID()`, `GetFormType()` | `RE/T/TESForm.h`; names/IDs/type copied within the call |
| `InventoryEntryData::IsWorn()`, `object`, `extraLists` | `RE/I/InventoryEntryData.h`; no call to the potentially mutating `GetDisplayName()` |
| `ExtraDataList::HasType<ExtraWorn/ExtraWornLeft>()`, `GetByType<ExtraTextDisplayData>()` | `RE/E/ExtraDataList.h`, `ExtraWorn.h`, `ExtraWornLeft.h`, `ExtraTextDisplayData.h`; existing `displayName` copied through `BSFixedString::empty/c_str` |
| `TESObjectARMO::GetSlotMask()` via `BGSBipedObjectForm` | `RE/T/TESObjectARMO.h`, `RE/B/BGSBipedObjectForm.h`; `REX/REX/EnumSet.h::underlying()`; all nine used BOD2 bits asserted against the engine enum in the adapter |
| `UI::GetSingleton`, `IsMenuOpen`; `InventoryMenu::MENU_NAME`, `LoadingMenu::MENU_NAME` | `RE/U/UI.h`, `RE/I/InventoryMenu.h`, `RE/L/LoadingMenu.h` |
| `Main::GetSingleton`, `threadID`, `REX::W32::GetCurrentThreadId` | `RE/M/Main.h`, `REX/W32/KERNEL32.h`; off-thread reader emits no values |
| GFx handler `Params`, `CreateObject/CreateArray/CreateFunction`, value/string/array accessors | `RE/G/GFxFunctionHandler.h`, `GFxMovie.h`, `GFxMovieView.h`, `GFxValue.h`; matches existing inventory bridge idiom |

Armor mapping has a portable policy and exhaustive single-bit tests, including exclusion of forearms/calves/long hair/mod-only slots. Native `GetWornArmor(slot, true)` was also inspected in `Actor.h/.cpp`; it scans inventory each call, so the implementation instead performs one scan for all slots. It is not called. No unverified engine API is intentionally relied upon.

## Bounded automated evidence

Records are external at `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/`. `run-job.py` writes purpose, exact argv, expected result, owner DYL-164/Codex, UTC deadline, completion signal, log and cleanup before spawn, then records PID/process group, elapsed time, exit, reap and group-absence result. Focused commands use `timeout 600`; lane/SWF commands use `timeout 900`, with FFDec children limited to 600 seconds each and 512 MiB heap. All owned jobs completed; none timed out. No game job was started.

| Check | Result / evidence |
| --- | --- |
| Actual transformed-AS grid harness | **PASS**, `full-grid_contract.log`: native paint/list/selection bodies, custom/missing icon, very long name, E/F/S/Q/unknown, native counts, exact 18-clip pool, 0/1/17/18/19/20/21/499/500 entries, entry 500 reachable, reversed native indices, row alignment, empty-filter cleanup, navigation to the existing action gate, refusal suppressing dispatch, eight equipment slots, empty/unknown and 500 refresh requests coalesced to one timer. |
| Actual layout/modal helper bodies | **PASS** in the same harness: 1280×720, 1920×1080 and narrow/offset stage fitting, control bounds, separate equipment/grid/card columns, native modal above shade, and non-player layout helper no-op. Stub bounds/fonts/timelines do not establish pixels in GFx. |
| Portable equipment policy | **PASS**, `focused-equipment*.log`, `full-inventory_equipment_test*.log`: every BOD2 bit, robe covering multiple slots, empty masks, same/different-form conflict policy; strict C++20 build/run. |
| Updated DYL-163 lane runner | **PASS**, 19/19 final jobs, `lane-final-regression.log`, `lane-final/jobs.json`; path points at this worktree, adds equipment, grid and fixture-generator tests. Earlier pre-fixture run: 18/18 PASS. Runner remains outside Git. |
| All Python discovery | **PASS**, 21 tests, no skips, `python-final-discovery.log`. Earlier pre-fixture discovery: 19/19 PASS. |
| Every JS contract | **PASS**, all six: follower, grid, HUD, inventory actions, minimap compass, navigation. `full-*.log`; final lane reruns action/navigation/grid on freshly prepared sources. Actual portable ActionIntents replay remains passing. |
| Every `*_test.cpp` | **PASS**, all eight compile/run with `-std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src`; individual `full-*-build.log` and `full-*.log`. |
| Offline fixture helper | **PASS**, two synthetic-record tests: nested/compressed records, extended lengths, truncation refusal, unique eligible 500, filtered scripts/flags/mod IDs and insufficient-record refusal. Actual Skyrim.esm input and console execution **NOT_RUN**. |
| FFDec imports, all existing SWF targets | **PASS / COMPILED_NOT_RUN**, final `swf-build-3.json` and `swf-3/build.json`; no new target. Builds1/2 PASS retained; build2 adds search fitting, modal layers and unload cleanup; build3 coalesces invalidation painting as well as native reads. The changed helper passes `coalesced-equipment-grid.log` (500 requests cause one invalidation draw and one fresh draw). |
| Windows/MSVC `/W4 /WX /permissive-` equipment adapter/DLL | **NOT_RUN**, no Windows build or DLL hash on this Linux host. Portable tests do not compile the engine adapter. |
| Gameplay, real rendering/performance, owner acceptance | **NOT_RUN** throughout. |

The initial grid harness failed twice during setup: its AS type-removal regex consumed a ternary `: String(...)`, then the list stub lacked its initial empty entries array. Changed-input fixes are retained in `focused-grid*.json/log`; the actual candidate passed after the parser/stub corrections. No unchanged failing retry loop occurred. FFDec reports inability to open its user-config log on this read-only host; external per-job stdout logs are retained, all imports exit0 and produce nonempty SWFs with changed hashes. This warning is not hidden or a GFx rendering result.

Exact reproduction commands from the worktree root (the recorded drivers expand loops to individual jobs):

```sh
timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"
# OUT and the provider/node paths are external; mkdir OUT before preparation.
OUT=/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/reproduce
SKYUI=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
NODE=/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node
mkdir -p "$OUT"
SKYCRPG_SKYUI_SOURCE="$SKYUI" timeout 600 python3 -I modules/ui/tests/inventory_prepare_contract.py "$OUT"
for source in modules/ui/tests/*.js; do
  argument="$SKYUI"
  case "$source" in *inventory_actions_contract.js) argument="$OUT/InventoryMenu.as";; esac
  timeout 600 "$NODE" "$source" modules/ui "$argument"
done
for source in modules/ui/tests/*_test.cpp; do
  binary="$OUT/$(basename "$source" .cpp)"
  timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src "$source" -o "$binary"
  timeout 600 "$binary"
done
timeout 900 python3 modules/ui/tools/build_scaleform.py --skyui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 --nexus-interface /srv/storage/Projects/SkyCRPG/session-20261026/ui-nexus-bsa/interface --java /srv/storage/Projects/SkyCRPG/session-20261026/ui-toolchain/jdk-17.0.21+9-jre/bin/java --owner DYL-164 --out /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/swf-3
```

The final build command above was executed once. Reproduction must use a **new** `--out` directory (for example `swf-4`), never overwrite the retained build. Production SWF input/source/toolchain pins were not changed. Final local outputs, external `swf-3/Data/Interface/`:

| Movie | SHA-256 |
| --- | --- |
| `inventorymenu.swf` | `bcb3be2001d33e734b6c81694a0f79d3cb94ecd6a97713cc11d6d476bb25ec63` |
| `skyui/inventorylists.swf` | `8643994e174ce3086d138baf2ef1395dcfece01ae3ac4c3006df07bd6847f651` |
| `hudmenu.swf` (unchanged HUD source/output) | `cd1e1fa7d8439b1399dbf5d66be590eba68f76a34344777ed0cd8b7a0347f96d` |

## Acceptance ledger

| Ticket criterion / obligation | Automated | Runtime acceptance |
| --- | --- | --- |
| AC1 find, inspect, equip, consume, favorite, drop with accurate counts/weight | **PASS** native enumeration/dispatch preservation, action replay and presentation contracts | **NOT_RUN** actual actions, native stats/counts/weight and footer after layout changes |
| AC2 long names, missing custom icons, large inventory safe fallbacks/bounds | **PASS** actual AS under stubs | **NOT_RUN** actual art/fonts/timeline/localization/720p/1080p/scaled text |
| AC3 scrollable dataset 500 usable | **PASS** exact 500, 18 clips, entry 500 reachable; no reduced cap | **NOT_RUN** real opening/scroll timing, input latency and retained memory |
| Equipment slots freshly match native worn/hand state | **PASS** portable mapping, copied-value/source audit, empty/unknown and refresh contracts | **NOT_RUN** native reader, multiple covered slots, custom names and real refresh ordering |
| Quantity/confirmation readability and focus | **PASS** fitted card/shade layering and unchanged native gates in mocks | **NOT_RUN** native focus, physical controller/mouse and actual dialog legibility |
| Exit path / native contexts / UI-only + combined | **PASS** context guards and existing source/routing contracts | **NOT_RUN** current candidate UI-only/combined and native-menu route |
| Owner acceptance | **NOT_RUN** | **NOT_RUN** |

## Owner route — every runtime step NOT_RUN

Use an isolated UI-only profile and a disposable **copy** of `SkyCRPG_Fixture_CombinedAcceptance`; repeat a short combined smoke after UI-only. Never overwrite the preserved fixture. Before each launch record purpose, exact profile/command, owner, PID/run URL, log, deadline, expected observations, completion and cleanup. Maximum one Skyrim process/profile; 20 minutes per manual route and a separate <=10-minute performance sample. If a step cannot fit, mark it NOT_RUN and schedule a distinct bounded job. Stop for wrong-copy actions, duplicate counts, input lock or off-screen modals; preserve logs/captures and restore only task-owned changes.

Attach candidate `<head SHA>`, `<CI run>`, DLL/package/SWF SHA-256, executable and dependency versions/hashes, host/Proton, actual loaded-file winners, copied save hash, device, resolution and text scale. Use the final DLL that includes the new equipment reader. The old DLL deliberately produces Unknown equipment. Record native counts/flags/weight **before and after each mutation**, with console context changes followed by a fresh selection; never replay an old token after console use.

| Step / expected observation | Runtime result |
| --- | --- |
| Keyboard/mouse: physically switch the XInput controller OFF, open with I; mouse-hover three different rows then R once each; click equip/unequip and another item; arrows/E/R operate the selected native entry. No category jump or duplicate action. | **NOT_RUN** |
| Controller: separate pass with the 8BitDo/XInput ON. Adjacent movement ±1, vertical ±3; Up from first row reaches categories, Left/Right changes category, Down/Confirm returns without equipping. Search, native sort column/order controls, empty Favorites/search results and cancellation remain escapable. | **NOT_RUN** |
| Inspect fixture plain, tempered and self-enchanted Iron Swords, stolen/owned helmets, potions, Nettlebane and existing CC item. Compare selected names, damage/armor, weight/value, counts, E/F/S/Q letters and full native card; preserve native stolen-plus-owned merged-row semantics. | **NOT_RUN** |
| Favorite/unfavorite through the native binding; equip/unequip each distinguishable sword and armor once. Compare all eight equipment slots after native refresh. Check two-handed weapon, left-hand spell/torch/shield, multi-slot robe, ring, amulet and genuinely empty slots. A conflicting/unsupported slot may say Unknown; it must never invent a name. | **NOT_RUN** |
| Drop plain/tempered/enchanted rows separately; two completed presses on one refreshed stack; hold/repeat/double press and check one native mutation per gesture. Tab close/reopen then first-press action. Native gold, inventory weight and capacity must agree before/after. | **NOT_RUN** |
| `player.additem 0003EADD 8`, close console and reselect: open native potion quantity, drop3 once, cancel another dialog, open then close inventory with dialog visible. Confirm native count delta3 only. Use/read an appropriate item and attempt Nettlebane drop; quest count must not change. | **NOT_RUN** |
| Native confirmation: apply an existing potion poison to an appropriate weapon through the normal selected-item path (if none exists, `help "weak poison" 4`, verify a returned ALCH ID, add1). Confirm/cancel native message. Grid/category input behind quantity/confirmation must be disabled; native cancel restores selection/controls. | **NOT_RUN** |
| Long name: on the copied fixture use its native enchanter to name a disposable enchanted sword with a long supported name, then inspect it in the grid and native card. CC/modded type without a custom icon must show its broad vector fallback. Capture all three columns without overlap. | **NOT_RUN** |
| Close grid, open container/barter/crafting/follower trade/Magic/journal/map through normal game paths. They must retain native/SkyUI layout and actions. Open grid again, save/load copy, reselect and act once; no retained old equipment display or input lock. | **NOT_RUN** |
| 1280×720, 1920×1080 and one increased native text setting: capture categories/search/sort, focused cells, native card/full name, footer, all equipment slots, quantity and confirmation. Native search caret/slider/mouse hit targets must be usable. | **NOT_RUN** |
| UI-only then combined: ordinary camera/UI-only and existing Camera + minimap/follower coexistence. Check no competing compass/HUD/Camera changes, normal close and task-owned cleanup/restoration. | **NOT_RUN** |

### 500 distinct-entry console fixture

**NOT_RUN on game records or in Skyrim.** Do not use `player.additem 00012EB7 500`: that is one native stack, not 500 entries. The helper emits exactly 500 different base-game form IDs, one `player.additem XXXXXXXX 1` line each, from named eligible WEAP/ARMO/BOOK/ALCH records in an **offline read-only copy** of the pinned `Skyrim.esm`. It skips header non-playable/deleted/ignored records, scripts, absent names and non-00 load-order IDs. It copies no names/art/game records into Git. The original fixture items remain, so the runtime dataset is **at least 500** distinct base forms; record the exact native row count (it may exceed 500 due to existing/modified instances). The automated performance fixture above is exactly 500.

Owner/orchestrator preparation, outside this worker's game/install restrictions: place a read-only source copy at `session-p3/fixtures/Skyrim.esm`, then record this as a bounded <=10-minute fixture job with command, log, PID/deadline, owner and cleanup:

```sh
timeout 600 python3 -I modules/ui/tests/inventory_fixture_test.py --source /srv/storage/Projects/SkyCRPG/session-p3/fixtures/Skyrim.esm --out /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/fixture-500
```

The new output contains `skycrpg-grid-500.txt` (the **exact console lines**) and `manifest.json` (500 ordered IDs, source hash and command hash). Generation fails if it cannot find 500 eligible unique records. Review the manifest/source hash; do not substitute unverified web IDs. During the isolated route the integrator stages that batch file only as a task-owned console fixture, records its hash, and uses:

```text
player.getav inventoryweight
player.getav carryweight
bat skycrpg-grid-500
player.showinventory
player.getav inventoryweight
player.getav carryweight
```

Verify all 500 manifest forms actually appear as native inventory rows; an input-file count alone is not a runtime PASS. Retain console output and exact native row count. Do not modify carry weight to disguise encumbrance. This is a menu performance fixture; quit back to the untouched copied acceptance save rather than save this large inventory over it. Restore/remove only the staged task-owned batch file.

Both devices: All category, sort by name, scroll from first to last entry using native scrollbar/wheel and controller ±3/±1, inspect entry 500, reverse sort, search for a middle/end item, clear search and switch categories. Capture 19/20 visible-data boundary and partial last row with stable columns/native selection. Warm once, then measure five opening/close samples and a bounded scroll segment; record median/p95 opening time against the existing <=250ms planning target, input latency and baseline-relative frame time (<=10% p95 overhead target), sample duration and memory before/after ten menu cycles. Failure or insufficient capture remains FAIL/NOT_RUN; clip-count mocks and FFDec are not game performance evidence. Runtime result: **NOT_RUN**.

## Limitations and owner decisions

- **NOT_RUN:** engine adapter MSVC build, runtime equipment API behavior, native callback/timer ordering, actual font/movie geometry, old item-preview interactions, all physical device/modal routes and measured 500-entry performance.
- Equipment slots are read-only status; custom existing display names are copied when available, otherwise base names; temper suffix generation is left to the native card. Head covers only the documented head/hair/circlet bits. Mod-specific slot conflicts are Unknown. Broader equipment frameworks are untested.
- Missing flags/bridge/unsupported thread display Unknown or `?`; native action fallbacks remain. No implicit false/empty data is fabricated. Generic icons may remain until a native repaint after asynchronous art loading. Actual shader/label/tint fidelity and increased-text readability need captures.
- Runtime fixture generation requires an offline source copy and native row verification; it has only synthetic parser evidence here. No personal save/game binary/dependency archive is committed.
- Existing DYL-163 native favorite and unsupported-thread action rulings remain accepted scoped limitations. No lower inventory cap, provider change or new product-scope decision is proposed; `DECISIONS.md` and ticket scope need no amendment.
- **Owner decisions needed:** accept or request layout adjustments after the route, then decide ticket acceptance using the runtime ledger. Any future relaxation of 500-entry performance/capacity or broadened slot/framework scope needs a separately recorded owner decision. Public redistribution/installer rights remain DYL-171/174; local derivative SWFs do not grant distribution rights.

## Owner runtime — UI-only route, 2026-10-27: PASS (bounded)

**PASS (owner-reported).** Run `e0abf83561b2494888fdef9e542cf823`.

| Field | Value |
| --- | --- |
| Source / CI | `c1ae156`, Windows run [40601052249](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40601052249) **PASS** (scope, windows-x64 (ui)); ZIP `ffd8e2b0fdbb8dfb…`, DLL `b0d45120bed14f37…` (includes `getInventoryEquipment`) |
| Movies | `inventorymenu.swf` `bcb3be2001d33e73…`, `skyui/inventorylists.swf` `8643994e174ce308…`, `hudmenu.swf` `cd1e1fa7…` (unchanged) |
| Profile / host | UI-only, 175 files (merged Phase 3 stage with only the two inventory movies replaced); Steam 1.7.104.0 (exe `846efccf…`), SKSE 2.3.1, Address Library 13, Proton Experimental 11, 1280×720 baseline |
| Input | **Both keyboard/mouse and controller**, as reported by the owner. Per-step device attribution is not recorded. |
| Save | `SkyCRPG_Test_DYL164`, a disposable copy of the `SkyCRPG_Fixture_CombinedAcceptance` fixture (preserved fixture untouched) |
| 500-entry fixture | `skycrpg-grid-500.txt` (`ebd8547e66dd84e7…`, 500 `player.additem` lines from an offline read-only copy of `Skyrim.esm`) was placed in the game root for `bat` only for this run and removed on restore (hash rechecked before removal) |
| Run | About 4 min of a 20 min budget, normal quit, watchdog did not intervene (minimum 13.2 GiB available) |
| Logs | `SkyCRPG_UI.log` `66fb3c6e…`: minimap preflight `exact=1`; **no inventory refusals of any kind** |
| Cleanup | Restoration **PASS**; no changed candidate files, no active profile, no Skyrim process |

Owner route, all reported passing with no issues:
1. **Layout:** the 3-wide grid with icons, counts and E/F/S/Q markers, the equipment silhouette, the item card and the gold/weight footer showed with no overlap or off-screen elements.
2. **Mouse:** hover plus R dropped on the first press; click equip, unequip and another item each acted once.
3. **Keyboard:** arrow navigation in the grid with Up to the categories; E equips and R drops.
4. **Equipment panel:** sword and armor slots updated on equip/unequip; empty slots read empty; no wrong names.
5. **Rows:** plain, tempered and enchanted Iron Sword rows dropped separately; the stolen/owned helmet row and favorite toggle worked; Nettlebane could not be dropped.
6. **Potions:** `player.additem 0003EADD 8`, quantity-dialog drop of 3, a cancelled dialog and a dialog-open close changed the count by 3 only.
7. **500 entries:** `bat skycrpg-grid-500` added 500 distinct items; scrolling All top to bottom and back reached the end **with no noticeable lag** (owner observation; no timing instrumentation).

Updated acceptance:

| Criterion | Runtime |
| --- | --- |
| AC1 find/inspect/equip/consume/favorite/drop with accurate counts | **PASS (owner-reported, bounded)**; native consume of the plugin item was not repeated here (covered for the merged DYL-163 build in its combined run) |
| AC2 long names, missing icons, large inventories | **PASS (owner-reported)** for large inventory and layout at 1280×720; long custom name and CC-icon fallback **NOT_RUN** in game |
| AC3 500 entries usable | **PASS (owner-reported)**: no noticeable lag, end reachable; no instrumented timing |
| Equipment slots | **PASS (owner-reported)** for sword/armor/empty; two-handed, left-hand spell/torch, multi-slot robe, ring and amulet **NOT_RUN** |

Still **NOT_RUN**:
- a dedicated controller-only pass (grid ±1/±3 movement, categories, search/sort escape);
- the native poison confirmation dialog;
- the long custom name;
- native container/barter/crafting/Magic/journal/map coexistence plus save/load in this build;
- 1920×1080 and increased text scale;
- the combined Camera+UI smoke on this build.

## Owner runtime — combined Camera + UI route, 2026-10-27: PASS (bounded)

**PASS (owner-reported, log-confirmed).** Run `07b392d246d54aedbcb44b6871532af1`.

| Field | Value |
| --- | --- |
| Source / CI | UI `c1ae156` (CI 40601052249 **PASS**; ZIP `ffd8e2b0fdbb8dfb…`, DLL `b0d45120bed14f37…`); camera main `28aae27` (`b97997f62df7c914…`) |
| Movies / stage | Merged Phase 3 combined stage with `inventorymenu.swf` `bcb3be20…` and `skyui/inventorylists.swf` `8643994e…`; HUD `cd1e1fa7…`; profile `combined`, 214 files |
| Input | **Both keyboard/mouse and controller** (owner), not attributed per step |
| Save | `SkyCRPG_Test_DYL164` (disposable copy in the combined profile) |
| Run | About 13 min of a 20 min budget, normal quit, watchdog did not intervene (minimum 13.2 GiB available) |
| Logs | `SkyCRPG_UI.log` `9edc3727…`: preflight `exact=1`, **no inventory refusals**. Compass lease: 11 `settling` → 11 `suppressed`, with `menu`/`hidden`/`stale-sample` releases. Camera: SmoothCam is the sole transform writer, no failures |
| Cleanup | Restoration **PASS**; only our own `SkyCRPG_Camera.ini`, re-saved by the camera module (as in earlier combined runs), was preserved and removed |

Owner route, all reported passing:
1. **Grid navigation:** ±1/±3 cell movement, up to the categories and category change, return without equipping, escaping an empty search and an empty Favorites. Mixed input: this is **not** a dedicated controller-only pass.
2. **Native poison confirmation:** a Weak Poison was added; one apply was accepted and one cancelled. The grid did not react behind the dialog, and control returned on cancel.
3. **Long name:** a disposable Iron Sword was self-enchanted with Fire Damage at Farengar's enchanter and given a long name. The grid cell truncated it neatly and the native card showed the full name.
4. **Native menus:** container, barter, crafting station, Magic, journal and map kept their native/SkyUI look and actions.
5. **Save/load:** after a reload, the first-press drop worked and the equipment panel was current.
6. **Coexistence:** the minimap, the follower panel and the camera behaved as usual.

Updated rows:
- AC2 long name: **PASS (owner-reported)**.
- Quantity/confirmation readability and focus: **PASS (owner-reported)**.
- Exit path and native contexts: **PASS (owner-reported)**.
- Combined Camera+UI: **PASS (owner-reported)**.
- Controller navigation: **PASS under mixed input** (no controller-only attribution).

Remaining **NOT_RUN**:
- 1920×1080 and increased text scale;
- equipment slots for a two-handed weapon, left-hand spell/torch, multi-slot robe, ring and amulet;
- the CC-item icon fallback in game.

## Senior review P1: coalesced repaint, no SkyUI interval

2026-10-28 UTC, branch `phase3/dyl-164-grid`, base `fd79d55134c93fe77186dc84d730844d2cd3b725` plus uncommitted changes. Addresses [PR #20 senior review](https://github.com/Sonoran-Solutions/SkyCRPG/pull/20#issuecomment-6456859814). `gh pr view 20 --comments` could not connect; the connected GitHub tool supplied the latest review. This correction has automated/build evidence only; prior gameplay rows above apply to their recorded candidates.

Pinned SkyUI Community 6.11 source root: `/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11`, commit `3d537a366f1a45c14a7012ea0aa00be18108f073`. In `source/actionscript/Common/skyui/components/list/BasicList.as:209–220`, `requestUpdate()` creates a 1ms interval without checking `_updateRequestID`; only a pending `_invalidateRequestID` suppresses it. `commitUpdate()` at lines 238–245 clears only the currently stored ID and calls `UpdateList()`. Repeated requests overwrite the ID and orphan earlier repeating intervals. `requestInvalidate()` at lines 193–207 and `commitInvalidate()` at lines 222–236 likewise can clear only the stored update ID. Native SkyUI also requests updates (`InventoryLists.as:217`, `ItemMenu.as:389`), so guarding only our calls against an already pending native request would still permit a later native call to overwrite our interval ID.

`skycrpgInventoryPresent()` still publishes the latest observation immediately, then coalesces repaint requests into one `_skycrpgRepaintTimer` per menu. Its callback first clears/deletes that timer, returns during closing, and skips painting when either native list request ID is pending. Otherwise it calls the native `UpdateList()` directly. `skycrpgPresentationClose()` clears/deletes the repaint timer; both the existing close hook and `onUnload()` use that cleanup. No SkyCRPG code calls `requestUpdate()` or `requestInvalidate()`, and `BasicList.as` is not added to compiled sources. Painting, native selection/action gates and equipment acquisition are unchanged.

Audit command: `rg -n 'requestUpdate|requestInvalidate|setInterval|onEnterFrame' modules/ui/scaleform/`, also checked against the DYL-164 base `945afd1`. The only DYL-164 intervals are the new repaint one-shot and the existing equipment one-shot. `_skycrpgEquipmentTimer` still coalesces at 100ms, clears/deletes itself before reading equipment, and is cancelled on close/unload. The existing HUD/minimap/follower frame handlers predate this ticket and are unchanged.

### Regression and negative control

`grid_contract.js` extracts and executes the actual pinned `requestUpdate`, `requestInvalidate`, `commitUpdate` and `commitInvalidate` bodies. It verifies the BasicList source SHA-256 `99012374aeece667f0a56fdc74119df9a6c0c42ec53c21333b28111de6eea7a2`. Its instrumented AS2 timer registry retains repeating IDs until explicitly cleared; a bounded five-tick drain detects leaks.

- **PASS:** 27 combinations of 2, 3 or 50 snapshots in one tick, native update absent/before/after, and invalidation absent/before/after. Each ends with zero intervals and 1–2 visible-window repaints (18–36 cell paints), retains the latest snapshot, and stays idle after input stops.
- **PASS:** pending native update/invalidate commits paint exactly once when our callback runs first; close and unload remove each menu's pending repaint independently; an already closing menu neither repaints nor schedules another timer.
- **PASS:** the existing 500-entry/18-clip, native selection/action-gating and geometry checks. A 500-request equipment burst still creates exactly one 100ms timer, clears it after firing, and leaves zero intervals after close.
- **FAIL reproduced on old code (expected):** a temporary copy of `modules/ui` used the unchanged `fd79d55:modules/ui/scaleform/inventory-presentation.as.inc` with the new contract. Command: `timeout 600 /home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/grid_contract.js /tmp/dyl164-old-repaint-5jjca31u/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11`. Exit **1**, assertion `2 snapshots, native update none, invalidate none: zero active intervals after callbacks`, actual **1**, expected **0**. Log: `session-p3/logs/dyl-164/senior-p1/old-code-negative-control.log`. The temporary old-code tree was removed; the working tree was never reverted.

Initial harness setup **FAIL**: the new timer fixture left rendered clips before the existing navigation fixture replaced its entries; navigation then tried an unassigned clip index. Repainting the new two-entry fixture before navigation corrected that setup. The final focused contract and all subsequent jobs pass. Initial log/job record retained under `session-p3/logs/dyl-164/senior-p1-initial/`; no unchanged retry was made.

### Commands and results

Host `dq-MEG-Z590-Trident-X-MS-B926`, Linux; Python 3.14.4, Node v24.19.0, g++ 15.2.0. Before execution, all 28 final job specifications recorded purpose, exact argv/environment, expected output, owner (DYL-164 senior P1 / Codex), completion signal, log, timeout and cleanup in `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/senior-p1/jobs.json`. Records include PID, UTC start/deadline, exit, elapsed time and verified process-group cleanup. All jobs completed as expected, including the intentional old-code exit 1. Runner: `/srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-164-review-checks.py`. The lane runner retains its own child-job records under `lane-final/`.

```sh
timeout 900 python3 -I /srv/storage/Projects/SkyCRPG/session-p3/tools/dyl-163-run-checks.py
SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p "*.py"

NODE=/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node
SKYUI=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11
OUT=/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/senior-p1
SKYCRPG_SKYUI_SOURCE="$SKYUI" timeout 600 python3 -I modules/ui/tests/inventory_prepare_contract.py "$OUT"
for test in modules/ui/tests/*.js; do
  case "$test" in
    */inventory_actions_contract.js) timeout 600 "$NODE" "$test" modules/ui "$OUT/InventoryMenu.as" ;;
    *) timeout 600 "$NODE" "$test" modules/ui "$SKYUI" ;;
  esac
done
for test in modules/ui/tests/*_test.cpp; do
  binary="$OUT/$(basename "$test" .cpp)"
  timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src "$test" -o "$binary"
  timeout 600 "$binary"
done

timeout 900 python3 modules/ui/tools/build_scaleform.py --skyui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 --nexus-interface /srv/storage/Projects/SkyCRPG/session-20261026/ui-nexus-bsa/interface --java /srv/storage/Projects/SkyCRPG/session-20261026/ui-toolchain/jdk-17.0.21+9-jre/bin/java --owner DYL-164 --out /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/swf-review-1
```

| Check | Result |
| --- | --- |
| Requested DYL-163 lane runner | **PASS**, all 18 child steps |
| All Python discovery | **PASS**, 21 tests, no skips |
| All six JS contracts | **PASS**: follower panel, grid, HUD, inventory actions, minimap compass, navigation |
| All eight `*_test.cpp` compile/run pairs | **PASS**: favorite slots, follower policy, HUD settings, inventory equipment, inventory policy, menu focus, minimap policy, resource snapshot |
| Focused fixed-code / temporary old-code proof | **PASS** / expected **FAIL**, respectively |
| SWF compilation | **PASS**, all three FFDec jobs; manifest `COMPILED_NOT_RUN` |
| Runtime high-frequency highlight/scroll smoke | **NOT_RUN**; no Skyrim launch |

New output directory `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-164/swf-review-1`; SHA-256 values verified against `build.json` and the output files:

| SWF | SHA-256 |
| --- | --- |
| `inventorymenu.swf` | `5b64a4a050f05a6479af835e4513f4b6349bb50a838e717c5c31ae02b87340bc` |
| `skyui/inventorylists.swf` | `8643994e174ce3086d138baf2ef1395dcfece01ae3ac4c3006df07bd6847f651` |
| `hudmenu.swf` | `cd1e1fa7d8439b1399dbf5d66be590eba68f76a34344777ed0cd8b7a0347f96d` |

Runtime smoke remains **NOT_RUN**: the orchestrator should rapidly highlight/scroll the existing 500-entry copied-save fixture, stop input and confirm repaint work stops, then close/reopen and verify selection, native equip/drop gates and equipment refresh. Automated timer ordering and compilation do not establish actual GFx scheduling or game performance. No C++, plugin registration, CMake, builder, dependency or other feature was changed. No commit, push, deployment or game launch was performed.
