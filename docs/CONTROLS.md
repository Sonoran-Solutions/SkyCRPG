# Controls contract

The normative ownership/precedence contract is in [ARCHITECTURE](ARCHITECTURE.md#input-and-menu-contract). Proven spike controls: SmoothCam F8/scan66 toggle; native POV precision fallback; controller grid adjacent/row movement and explicit category focus as recorded in [UI evidence](evidence/p1/ui-spike.md). DYL-156 now implements and owner-tests F6 native-view fallback/return and F7 recenter, documented in the candidate controls table below. Full reset/lifecycle and broader input mappings remain later-ticket obligations; the DYL-161 hotbar candidate is described below.

The high-level action table includes future work. The DYL-156 candidate table below is authoritative for the implemented Camera defaults; the DYL-161 candidate describes current hotbar behavior and its validation limits.

| Action | Intended behavior | Binding status |
| --- | --- | --- |
| Camera enable/normal view | Toggle elevated view and release to the safe underlying view. | Configurable key/controller route selected in P2. |
| Emergency camera reset | Recover usable normal view without opening the custom UI. | Must be reachable independently of UI. |
| Rotate/recenter | Yaw/snap as selected; recenter on character. | P2-01. |
| Zoom | Bounded distance adjustment; never disable wall/floor collision. | P2-01. |
| Precision aim | Native attack/aim input and selected closer-view behavior. | P2-03. |
| Hotbar slots | Up to eight favorite bindings; equip/select/use through native behavior. | Native 1–8 where appropriate, plus tested controller path. |
| Open inventory | Open player grid; retain access to other native menus. | Existing inventory action where feasible. |
| Grid navigation | Pointer or keyboard/controller focus; inspect/equip/use/favorite/drop. | P3-06 must document every action. |
| Cancel/back | Close current modal, then menu; never trigger world combat behind it. | Native convention preserved. |
| Minimap toggle/zoom | Selected provider action; restore native navigation when replacement is disabled. | Resolve provider conflicts in P1/P3. |
| UI scale/reset | Restore safe anchors and readable defaults. | P3-06. |

An action bar that merely draws icons is incomplete. Input must be usable and reflect actual selected/equipped state. No new spell cooldown, autoplay rotation, party order or turn-control binding is planned.

The DYL-161 candidate maps keyboard hotkey events 1–8 to the slot shown. Native quick-key bindings take precedence: assign a favorite in Skyrim's Favorites menu and Skyrim handles that numbered slot. Magic overrides an item at the same native index. Unbound favorites marked `FAV` fill remaining positions in form ID order; pressing the corresponding hotkey requests one native equip/use action. Weapons and armor toggle; consumables are used; spells select the right hand, powers select the voice slot, and shouts are selected. Casting and attacks still use native controls. Controller selection through the native Favorites menu has owner-reported bounded PASS; direct controller slot cycling is not implemented.

The keyboard sink observes Skyrim's mapped Hotkey1–Hotkey8 key-down events without consuming them. For a native quick-key-bound slot, Skyrim handles the press and the UI dispatches nothing. For an auto-filled `FAV` slot, the UI requests one action through `ActorEquipManager`; blocking menus prevent that dispatch. Resolve conflicting remaps in the game's or conflicting mod's bindings. F6/F7/F8 and map K/L retain their documented owners. Windows build **PASS** on UI source `ba10da6` ([build 40438723566](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40438723566)); keyboard FAV dispatch **PASS**, owner-reported and bounded, in run `8eb20c9ab8cf4a978fcfbbbfcf8833ec` with the controller off. On the test host the controller must be off for keyboard/mouse input. A truly native-number-bound press proving no duplicate action, weapon unequip on a second press, in-game shout recovery, and the full native cast/attack path are **NOT_RUN**; see [evidence](evidence/p2/hotbar.md). The voice line displays the selected shout or power and the native global voice recovery timer, sampled at most twice per second; `--` means unavailable and `0s` only means that timer is zero, not that a daily power is available.

## Phase 2 Camera candidate — DYL-156

These mappings describe the new candidate; [Camera evidence](evidence/p2/camera-controls.md) separately records build and gameplay validation. Native movement/combat/activation remain unchanged, with TDM movement-only and SmoothCam as the only normal third-person transform writer.

| Control | Candidate behavior |
| --- | --- |
| F6 (scan 64) | Explicit native first-person fallback from eligible overhead view. Press again to return only when F6 owns that request; it does not reclaim a native POV request. From disabled policy in eligible third person, enable policy without forcing a state. |
| F7 (scan 65) | Recenter yaw toward player heading at the configured rate. |
| F8 (scan 66) | Existing SmoothCam emergency toggle, preserved. It releases to underlying normal third person; SkyCRPG never re-enables the provider itself. |
| Mouse horizontal / controller right-stick horizontal | Native yaw path; vertical input is filtered to hold configured pitch while overhead policy is eligible. |
| Native wheel / controller zoom gesture | Bounded requested zoom; provider collision may move the actual camera closer. Native POV remains available for precision aiming. |
| Controller custom toggle/recenter | Unbound by default. Only explicitly configured unmapped single-button masks are eligible. No native gameplay button or unverified chord is stolen. Mapping/feel require owner validation. |

Camera preferences use version 1 in `SkyCRPG_Camera.ini`: default pitch 55 degrees, bounded 15–75; requested zoom 0.65 with default limits 0.25–0.90. Reset with the shipped INI and a restart. Settings remain outside Skyrim saves. Malformed fields recover to defaults or documented safe suspension; unknown schemas are not overwritten. Persistent writes cover only owned enable/zoom/schema keys.

For precise bow, spell or small-object interaction, deliberately use F6 to enter native first person; aim and activate with Skyrim's normal controls. F6 does not automatically switch for a drawn weapon. If an observed provider or special-state interruption cancels the owned session, use native Toggle POV to return to third person, then F6 to enable the elevated policy. TDM target lock remains disabled in this configuration. The bounded owner route and its limits are recorded in [combat and activation evidence](evidence/p2/combat-and-activation.md).

Native controller POV can request first person without an F6-owned session; returning uses native POV as well. The default custom F6-equivalent controller binding is unassigned, and this controller route still needs a device-specific gameplay check.

Native menus, dialogue, loading/death, first person and unsupported special states take precedence. Held controls are discarded through focus changes and require fresh input; no gameplay event is replayed. F8 and map K/L are reserved against custom bindings, and keys already mapped to native gameplay are refused. Full obstruction, targeting and lifecycle recovery remain DYL-157/158/159.

## Input bindings and focus

DYL-168 candidate; physical device passes and resolution captures remain **NOT_RUN** in [input/layout evidence](evidence/p3/input-and-layout.md). Use the current native prompts if bindings were remapped. On the pinned host turn the 8BitDo XInput controller OFF for the keyboard/mouse pass and ON for a separate controller pass; record which device performed each step.

| Action | Keyboard/mouse | Controller (XInput defaults) |
| --- | --- | --- |
| Open/close inventory | I / native Inventory; Tab or native Back to close | Native character menu → Items; B backs out |
| Select / inspect | Hover/click or arrows; native card shows selected item | D-pad/stick navigation ±1 horizontally, ±3 vertically; native card follows selection |
| Categories | Click category; Up from first row, Left/Right, Down/Enter to return | Up from first row, Left/Right, Down/A to return; return does not equip |
| Equip/unequip / consume/read | Native E/Enter or mouse equip/use prompt; native hand controls | Native Equip/Use prompt (A or hand triggers as shown); no new dispatch |
| Drop / quantity | Native R; native quantity slider/arrows and confirm; cancel with native Back | Native X; D-pad/slider and A confirm; B cancels without a drop |
| Favorite/unfavorite | Native F or prompt | Native Y or remapped prompt |
| Equipment slots | Display-only; select the equipped item's grid row for full inspection/actions | Same; no slot focus stop or pointer requirement |
| Search | Space or click search; type; native Enter/Back ends editing | R3 opens character pad; D-pad moves, A chooses; Done applies, B restores previous filter; Clear + Done removes it |
| Sort | Click native header/column menu | LB/RB selects native sort column; tap L3 reverses order, hold L3 opens native column dialog; B cancels |
| Quantity / poison confirmation | Native confirm/cancel; one dialog owns input | A confirms, B cancels the native modal and returns to the menu |
| Hotbar assign | Favorite an item; native Favorites (Q by default) → select, press 1–8 for a native numbered binding, or use auto-FAV placement | Favorite with native Y; native Favorites (D-pad Up/Down in gameplay) → select/use; eligible unbound favorites automatically occupy spare HUD slots |
| Hotbar use | Native numbered 1–8 has precedence; auto-FAV slot uses one existing native equip/use request | Select/use through native Favorites; native attack/cast after selection; no direct custom slot-cycling capture |
| Hotbar page | One fixed eight-slot page; no page action | Same. SkyUI Favorites groups are separate provider functionality, not HUD pages |

The new R3 path uses SKSE keycode 273 only inside the player grid, when neither list nor category is disabled. F6/F7/F8, map K/L and gameplay bindings retain their current owners. The character pad accepts A–Z, 0–9 and space (32 characters); it has a `>` marker and outlined selection, consumes input before native sort/search, and blocks inventory actions underneath. Its filter caption sits beneath the grid. B/Tab/Escape cancels it; after Done or Cancel focus returns to categories. Empty results permit category change, R3 → Clear → Done, or B to exit. Native keyboard search retains its own input-end behavior; Back from an empty category reaches the native menu exit.

Grid selection uses corner brackets as well as color; separate brackets mark the active category/grid region. Native search caret, sort selector, dialog slider/buttons and Favorites keep their provider indicators. The equipment/follower panels and the HUD bar are display-only and never capture focus. Native Favorites provides the controller action path; individual numbered assignment of all eight slots from the controller and custom HUD page/cycle gestures are not implemented. Favorite assignment and native Favorites use are the MVP controller path carried from DYL-161, not an assertion that SkyUI groups equal numbered native hotkeys.

Reset HUD layout: exit Skyrim, set `[HUD] bResetLayout=1` in `Data/SKSE/Plugins/SkyCRPG_UI.ini`, and restart. With the DYL-168 integration patch, scale/offsets become 100/0/0 at load; enable and compass settings are preserved. The flag is read-only and remains 1. Set it to 0 and restart to resume the stored preferences, or write `iScalePercent=100`, `iOffsetX=0`, `iOffsetY=0` for persistent default keys. There is no live reload or engine hook. Scale is 85–115%; X offset is ±120, Y ±60, with viewport clamping. These preferences scale the owned HUD panels, not the native inventory fonts, quest text or DEM.

On gamepad, the native inventory bottom bar labels search with the R3 button (SKSE keycode 273) in the player grid instead of the keyboard Space key; keyboard/mouse keeps the configured key.

Inventory 3D preview: `[Inventory] bManageItemPreview=1` (default) centres the native 3D item preview over the item card and scales it to fit while the player inventory is open. It adjusts only the game's in-memory preview placement for that menu (not during zoom; zoom in/out keeps working) and writes no INI setting or file; container, barter, gift and magic menus start from the game's own values. Optional `fItemPreviewScale` (0.4–1.0, a factor on the game's preview scale) and `fItemPreviewXShift` (−100 to 20 position units added to the game's horizontal position instead of centring; negative is right) override the computed values; leave them empty for the defaults. Set `bManageItemPreview=0` and restart to leave the preview untouched.
