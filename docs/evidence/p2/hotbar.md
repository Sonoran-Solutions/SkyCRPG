# DYL-161 hotbar candidate — incomplete validation

Base: `c3903027791e046fc6f662ccde0cb05a4f7e7633`. This is an **implementation candidate, not senior-review-ready**. Native runtime, Windows DLL build, and controller/combined acceptance remain open. It does not yet implement graphical item/spell icons or native shout recovery; the small type labels are placeholders, not icons. The controller path is the native Favorites menu and has not been verified for this use. Do not mark the ticket In Review based on this record.

## Implementation and safety boundary

The HUD asks the UI DLL for an eight-entry snapshot at most twice per second. A single pending SKSE game task samples the player's inventory and `MagicFavorites`; only copied strings, counts and booleans cross into the Scaleform callback. A generation invalidates snapshots on load/new-game transitions; snapshots older than two seconds are unavailable and hide the bar. Empty entries display EMPTY. Physical item bindings use `ExtraHotkey`; equipped state uses the bound instance's worn markers. Magic slots override physical slots as in the pinned SKSE 2.3.1 `GetHotkeyBoundObject` source. Names for physical items use the base form, not instance-specific names. `InventoryEntryData::GetDisplayName` is deliberately avoided because the pinned implementation may add extra data. Count is the total for the bound base form, not a particular instance.

The candidate registers no hotbar input handler and dispatches no synthetic actions. Existing native numbered hotkeys retain their normal ownership and existing native Favorites UI provides a potential controller path. This architecture cannot itself dispatch twice; whether the native actions and controller route satisfy all acceptance scenarios is **NOT_RUN**. Native gameplay/menu focus, combat, Camera module, inventory changes and native attack/cast behavior are not altered by this code. A writable inventory database, fabricated cooldown and separate camera service are not introduced.

## Results

| Check | Result | Evidence and limits |
| --- | --- | --- |
| Pinned external HUD SWF compile | **PASS — COMPILED_NOT_RUN** | FFDec 26.0.0, Java 17.0.21+9, Nexus SkyUI 6.11 base; 512 MiB heap, PID 224189, exit 0, 0.765 s, 600 s deadline. SWF SHA-256 `e8b40aae15dedbdffb3c5fb63c334a4f8c2abad7d790f8568beb76b4fe5b2181`; retained outside Git at `/srv/storage/Projects/SkyCRPG/session-p2-20261026/hotbar-candidate-java/Data/Interface/hudmenu.swf`. HUD source snippet SHA-256 `3d540d1fa2eccf81b1952bcbb496e58ca98c77679de6d8e2755789320b488b78`. The first attempt exited 2 because `--java` was omitted; it did not compile. The corrected command specified the existing pinned JRE. |
| Actual ActionScript snippet under HUD stubs | **PASS** | `timeout 600s node modules/ui/tests/hud_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11`; tested empty/missing snapshot, weapon/spell/consumable labels and count, disappearance, sampling throttle, native HUD mode, and clip teardown. Stubs are not Skyrim or GFx. |
| Existing Scaleform source contracts | **PASS** | `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 timeout 600s python3 -m unittest discover -s modules/ui/tests -p test_scaleform_contract.py -v`: 14 tests passed. These do not run the native bridge. |
| Native Windows build | **NOT_RUN** | The CommonLib bridge is uncompiled. No native DLL/package hash exists for this candidate. Address Library singleton/relocation compatibility and actual engine layouts must be verified before deploying. |
| Equip/select/use, binding precedence, selected state, inventory refresh, duplicate input | **NOT_RUN** | Native calls and the displayed snapshot require a bounded UI-only route on the pinned host with before/after engine state. |
| Controller selection and menu cancellation | **NOT_RUN** | Native Favorites menu is a proposed route; no new selector or controller binding exists. |
| Icons and native shout recovery | **BLOCKED / unimplemented** | Type labels do not satisfy icon acceptance; no shout recovery source is exposed. |
| UI-only, combined, 720p/1080p rendering, performance | **NOT_RUN** | The 2 Hz inventory scan needs measured safety and layout verification in the actual engine. |

The runtime target is Steam Skyrim 1.7.104.0, SKSE 2.3.1, Address Library 13, Ubuntu 26.04.1, Proton Experimental 11, RTX 3080, SkyUI Community 6.11, with other pins in [the runtime matrix](../../compatibility/runtime-matrix.md). None was launched for this candidate. Deployment, copied-save preservation, logs, capture, original INI/plugin restoration and combined camera/UI testing are orchestrator-owned and remain **NOT_RUN**. Native UI ZIP still lacks generated `hudmenu.swf`; this candidate does not resolve licensing or public distribution.

## Remaining obligations

Verify the newly used CommonLib relocations against the exact Address Library and build the Windows target; inspect actual bound item/magic precedence and instance/count behavior in game. Implement appropriate graphical icons and expose native shout recovery if available. Run weapon, spell, consumable, empty/depleted and menu/controller fixtures, confirm displayed state and at-most-once native dispatch, measure scan overhead, and run the combined smoke. Reconcile user controls and packaging documentation with verified behavior. F6/F7/F8 remain Camera-owned; no UI input mappings were added. Until these obligations pass, the candidate does not meet DYL-161 acceptance.
