# DYL-168 / P3-06 — input and layout evidence

Candidate implementation and automated checks **PASS** as scoped below. The owner controller-only run and its visual failures are recorded below; other new gameplay and external-HUD measurements remain **NOT_RUN**. This is not ticket acceptance or a public release.

Ticket: DYL-168. Base `6e7dff671c1ca4e53eb116f3d3e4cf47925ba79f`; branch `phase3/dyl-168-input-layout`, worktree `p3-layout`. Candidate `<head SHA>` plus this diff; Windows `<CI run>`, DLL `<DLL sha256>`, package `<package sha256>`. Final local `<SWF sha256>` values are listed below. `plugin.cpp` startup integration remains an external patch, not applied here.

Local host `dq-MEG-Z590-Trident-X-MS-B926`, Linux, Python 3.14.4, Node v24.19.0, g++ 15.2.0. Target unchanged: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0 (`7a60f4de`), SkyUI Community 6.11, DEM 1.7.5 / Infinity UI NG 2.1.3 / LMU 1.3.4 with Common + Line-17, recorded Proton host. No runtime host, save, profile, game INI or install was touched. No engine API was introduced. The integration uses the existing Win32 INI reader and logger; Windows MSVC compilation remains **NOT_RUN** here.

## Owner controller-only run `2683055a…`

Owner reports all ten functional steps **PASS**, controller only, on `0b8c865`, UI-only profile, 1920×1080 windowed. This records the owner's aggregate result; the individual ten step descriptions and per-step observations were not supplied with this report. It does not establish keyboard/mouse or other display-size coverage.

The owner screenshots show two visual **FAIL** results on that same build: the engine-rendered bow/quiver obscures the item card, and the native FILTER/search control collides with the DAM sort label; the row is also misaligned. The faint text behind WEAPONS and category text overlapping the final icon remain open observations. Screenshots: `session-p3/p3-input-168/owner-screens/weapons-arrow-selected.webp` and `weapons-bow-selected.webp`.

**Fix: automated PASS; in-game NOT_RUN.** Pinned SkyUI `ItemMenu.positionFloatingElements()` locks `mouseRotationRect` to the top and assigns 0.55 of `Stage.visibleRect.height`; its model-interaction area is the basis for reserving the top 55% of the right column. The non-modal card uses runtime art bounds, uniform scale at most 1, a 408-design-unit column cap, and an 8-design-unit gap below the reservation and above the runtime bottom-bar top. The minimum scale is 0 for a degenerate available box, preventing preview intrusion. Modal placement and ownership remain unchanged. At 1280×720 the fitted design has equipment right edge 298 and grid right edge 833, while the preview starts at 843 (rounded); neither intrudes. Runtime-measured category label and controls share a row, the search keeps its native width unless limited (minimum 48 units), and the native header moves below the row. The selected category label is placed below the measured category list. The separate right-end “Favorite” caption is not identified in the staged ActionScript and remains **OPEN**, as does the faint ghost: the staged code identifies `categoryLabel` as the selected-category text and `searchWidget` as a separate native control, but does not establish the ghost's timeline clip. Automated geometry and modal contract **PASS** at 1280×720, 1920×1080 and 2560×1080 with x −320; these stubs do not establish actual GFx font, art or engine-rendered model extents. Full regression and SWF compilation **PASS**; see `session-p3/logs/dyl-168/fix/final-2.md`. In-game verification of the fix is **NOT_RUN**.

## Audit before implementation

Sources: `grid-routing.as.inc`, `grid-methods.as.inc`, `inventory-list-layout.as.inc`, `inventory-presentation.as.inc`, `inventory-actions.as.inc`, `hud-init.as.inc`, `follower-panel.as.inc`; native pinned `InventoryLists`, `InventoryMenu`, `ItemMenu`, `ItemCard`, `TabularList`, `FavoritesMenu`, `NameFilter`, `HUDMenu`, and SkyUI `config.txt`. Existing DYL-163 action/identity rules, session/gesture re-arm, DYL-164 repaint coalescing, follower visibility and DYL-166 compass lease are retained.

Reused evidence is scoped to its artifacts. [DYL-164 UI-only](grid-inventory.md#owner-runtime--ui-only-route-2026-10-27-pass-bounded) `e0abf83561b2494888fdef9e542cf823` and [combined](grid-inventory.md#owner-runtime--combined-camera--ui-route-2026-10-27-pass-bounded) `07b392d246d54aedbcb44b6871532af1` used both devices without per-step attribution. They are **not controller-only evidence**. DYL-163 [combined route](inventory-integrity.md) `69f8c8e004e54d97abc60eaa3eb623fb` covers native item rules; favorite dispatch remains native/unguarded under the recorded owner ruling. No new action database or synthetic item dispatch is added.

| MVP action | Existing code / keyboard and mouse path | Controller path and gap audit | Existing evidence / this candidate runtime |
| --- | --- | --- | --- |
| Select | Native hover/click, arrows → native enum/backing index | ±1 adjacent, ±3 row; grid routing before categories | DYL-164 both runs, mixed **PASS**; fresh separate passes **NOT_RUN** |
| Inspect | Native item card follows selected row | Same card, no pointer needed | DYL-164 both runs, mixed **PASS**; fresh **NOT_RUN** |
| Equip/unequip | Native E/Enter/mouse hand prompt → existing gated ItemSelect | Native Equip/Use/hand prompt; list disable flags gate actions | DYL-164 both runs **PASS**; isolated controller **NOT_RUN** |
| Consume/read | Existing native activation and restrictions | Native Use prompt, then native attack/cast if selected spell | DYL-163 combined `69f8c8e` bounded **PASS**; fresh devices **NOT_RUN** |
| Drop with quantity | Native R → quantity continuation → gated native ItemDrop | Native X, slider/D-pad, A confirm, B cancel | DYL-164 UI-only quantity -3/cancel bounded **PASS**; fresh **NOT_RUN** |
| Favorite/unfavorite | Native F; state observed, native toggle intentionally unguarded | Native Y/prompt; no parallel favorite store | DYL-164/DYL-163 bounded **PASS**, per-device **NOT_RUN** |
| Category | Click or Up from first row, Left/Right, Down/Enter | Same navigation; Confirm returns without equipping; empty lists permit Back | DYL-164 combined mixed **PASS**; added region brackets; controller-only **NOT_RUN** |
| Search | Pinned PC Space or click, native text entry and inputEnd | **Gap found:** pinned config has only `controls.pc.search=57`. Added R3 (273) character pad, A–Z/0–9/space, 32 characters, native name filter; Clear/Done and B cancel | Empty-search escape mixed **PASS** in DYL-164; controller text entry newly implemented, **NOT_RUN** |
| Sort | Native header/column menu mouse clicks | Existing LB/RB (274/275) columns; L3 (272) tap order / hold column dialog | Native source + contracts; dedicated physical sort route **NOT_RUN** |
| Close | Native Tab/Back after focused child declines it | Native B mapped to TAB; category routing never consumes Back | DYL-164 exit/menu mixed **PASS**; controller-only **NOT_RUN** |
| Equipment-slot inspection | Display-only names/type icons; full inspection through the item's grid row | No focus stop, pointer hit handler or action in equipment cells | DYL-164 sword/armor/empty **PASS**; remaining slot fixtures unchanged/open |
| Quantity / confirmation | Native ItemCard owns input; native confirm/cancel | Native A/B; no grid/hotbar action under modal | DYL-164 UI-only quantity + combined poison accept/cancel mixed **PASS**; own panels now hidden under native modal, fresh **NOT_RUN** |
| Hotbar assign | Native favorite flag; Favorites Q → native numbered assignment 1–8 or auto-FAV spare positions | Native favorite flag populates spare FAV slots; Favorites selects/uses actual favorite | DYL-161 [controller Favorites](../p2/hotbar.md) run `f8167949cb92443e95bd42d5de5ff0cf` bounded **PASS**; fresh **NOT_RUN** |
| Hotbar use | Native bound number wins; auto-FAV uses existing one native equip/use request | Native Favorites navigation/use; gameplay casting/attack stays native | DYL-161 controller run above; keyboard auto-FAV `8eb20c9ab8cf4a978fcfbbbfcf8833ec` bounded **PASS**; all eight physical actions **NOT_RUN** |
| Hotbar page | One eight-slot page, no page state or binding exists | Same; SkyUI groups are not HUD pages | MVP U2 requires up to eight slots; no new page/cycling feature. Individual numbered assignment of all eight by controller remains unimplemented; native Favorites is the controller action path |

## Layout and readability audit

HUD coordinates are Stage design units. Actual GFx `Stage.visibleRect` can remain 1280×720 on a 1920×1080 display, as recorded in DYL-160; physical pixel resolution is not interchangeable with that rectangle. Tests also exercise a real 1920×1080 rectangle and a 2560×1080 rectangle beginning at x−320. Stage-to-HUD conversion retains the full parent transform, including the recorded translated origin 616.65,475.45 and nonunit nested scales.

| Element | Existing / implemented policy | 1280×720 rectangle, scale 100 | 1920×1080 rectangle, scale 100 | Runtime safe area |
| --- | --- | --- | --- | --- |
| Party/follower | Changed Stage.width/height clamps to visibleRect origin/extent; 194×49, 16-unit margins; default inset20,62 | x20..214, y62..111 | x20..214, y62..111 | DYL-165 Serana run `bf2a8387cdb942628b56b922bf7aefcb` bounded visibility/status **PASS**; new display sizes **NOT_RUN** |
| Bottom/status | 420×52, centered; at least 16 left/right/top and 12 bottom, settings bounded | x430..850, y656..708 | x750..1170, y1016..1068 | New geometry **PASS** mocks; real captures **NOT_RUN** |
| Hotbar + voice | 536×77; stack above status with 12-unit gap scaled together; replaced independent clamps that could overlap voice/title at 115% | x372..908, y567..644 | x692..1228, y927..1004 | New stack **PASS** mocks; actual fonts/native shout strip **NOT_RUN** |
| Inventory equipment / grid / card | 1280×720 design centered/fitted with scale=min(1,(rect.width−32)/1280,(rect.height−32)/720), 16 outer margins; native footer unchanged | scale 0.95556 | scale 1, centered | DYL-164 baseline bounded **PASS**; revised margin,1080p **NOT_RUN** |
| Quantity / confirmation | Native card measured/fitted to max 700×540; opaque shade below card; owned panels hidden so no visible overlap | Inside margin16 in harness | Same | Prior mixed dialogs **PASS**; new layer behavior physical **NOT_RUN** |
| Quest text | Native `QuestUpdateBaseInstance`; HUDMenu preserves compass-relative Lock(T) placement; no SkyCRPG quest positioning API | Native bounds unknown until rendered | Native bounds unknown until rendered | DYL-166 owner reported no quest overlap; exact margins **NOT_RUN** |
| Minimap | DEM/provider-positioned, never moved here; lease merely observes bounds and owner-approved placement | DYL-166 UI-only clip≈1136..1280 ×0..134; combined clip1024.5..1285.9 ×−0.9..217.7 on1280 Stage | No measured1080p rectangle for current candidate | Not a margin16 guarantee: historical clip can overhang. Provider safe-area/quest/effect checks **NOT_RUN** |
| Compass / shout / temperature | Native authored anchors and lock; DYL-166 lease remains unchanged | Native top anchor when map absent/disabled | Native anchor preserved | DYL-166 combined `5298264e872545c0ab28f5b07becc551` and absent `a16a95c6690844f39de2844ba9123d99` bounded **PASS**; current sizes **NOT_RUN**. Temperature remains hidden with compass |

Equipped/favorite/stolen/quest already use E/F/S/Q (`?` means unavailable). Grid focus now adds corner brackets to the existing selected border; category/grid region brackets change only through navigation or rollover, never repaint. Native sort/search/modal indicators are preserved and need rendered verification. Hotbar adds E for equipped state. Follower health/unavailable/untested is text, absent/dead hidden, with no invented actor state.

Grid/equipment retain bounded two-line, 32-character ellipsis/height handling; full name stays in the native card. DYL-164 combined long enchanted-sword name **PASS** applies to its prior movie. Follower names now ellipsize to 22 including `...`, hotbar to 12, voice name to 60, with width fitting. The new harness checks very long names, E status and follower health text. Width fitting and the mocked font metrics do not prove actual legibility.

### Active-effect overlap measurement carried from DYL-166

Historical **FAIL**: [DYL-161 combined dispatch](../p2/hotbar.md) observed the minimap partially covering active-effect icons at 1080p; DYL-166 deferred exact measurement here. DYL-166's owner report of no quest overlap does not resolve it. **Current quantitative measurement: NOT_RUN** (no gameplay permitted in this lane). No map movement, effect repositioning, or waiver is implied by this change.

At1080p record separately the DEM map body, its quest pointer/distance-label overhang, quest text and every visible active-effect icon/label. Use screenshot pixel boxes `(left,top,right,bottom)`, actual Stage rectangle and image size. For two boxes A/B, intersection width=max(0,min(A.right,B.right)−max(A.left,B.left)); height analogously; area=width×height. Record clipped/occluded pixels and the minimum signed edge clearance. Repeat at 720p and 115% owned HUD scale. Fill these exact rows after capture:

| Capture / boxes in physical pixels | Intersection (width×height; area) / edge clearance | Status |
| --- | --- | --- |
| 1080p100% DEM body vs active effects; overhang vs effects; quest vs both | NOT_RUN; no numeric bounds available | **NOT_RUN**, historical overlap **FAIL** retained |
| 720p100% same | NOT_RUN | **NOT_RUN** |
| 1080p115% same, plus owned bottom stack vs native shout/status | NOT_RUN | **NOT_RUN** |

DEM is provider-positioned and this lane cannot move it. If overlap remains, attach the measured boxes to the integration/ticket follow-up and request a supported provider/native-widget adjustment or an explicit owner decision. Do not mark AC3 PASS because only owned panels pass geometry tests.

## Implementation and reset policy

R3 opens a player-grid-only character pad using existing name-filter/input-start/end callbacks and native disable flags. The builder's minimal additive change inserts its handler before native sort/search in both InventoryLists definitions and updates region focus/caption on native keyboard search callbacks. It consumes navigation/confirm/cancel while open, blocks underlying list dispatch, and clears selection on return to categories. No new timer, engine hook, global listener, native mutation callback or retained engine/menu pointer is introduced. Its ASCII substring entry is deliberately bounded; punctuation/non-Latin typing remains keyboard-only. Native inventory destruction destroys the pad.

`[HUD] bResetLayout=1` overrides only scale/offsets at startup to 100/0/0. `[HUD] bEnabled`, `[Minimap] bReplaceCompass`, future-schema handling and the file itself are preserved. Missing flag defaults0; malformed/non-boolean values do not request reset. The flag is not automatically cleared. Set0 and restart to use stored preferences, or explicitly write100/0/0. This is a restart-based command, no live reload. Portable parser tests cover valid/malformed flags, invalid scale, enable/provider preservation and schema fallback.

Integration request: `/srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-168.patch` with `dyl-168.md`. It reads the flag in the existing `ReadHudSettings`, passes the optional parser argument and emits a reset-applied log. `git apply --check` **PASS**. It is required for the shipped flag to work; no CMake change. `plugin.cpp` and DYL-167 lifecycle ownership remain untouched here.

## Automated jobs and movies

Owner DYL-168 / Codex Sol. Before each job, external `run-job.py` records purpose, exact argv/environment, expected exit/output, timeout, UTC start/deadline, log, owner, PID/process group, completion and cleanup. Records/logs/scripts/binaries are under `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-168/`. Completed outer jobs report reaped/group absent; the lane runner and SWF manifests retain child PIDs/logs. No timeout or game job occurred. Focused commands use600s, lane runner/SWF compilation900s (builder children600s,512MiB heap). Exact manifests include all expanded paths.

```sh
LOG=/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-168
NODE=/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node
SKYUI=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11

# Copy of required shared DYL-163 runner; only repo/log/owner paths changed.
timeout 900 python3 -I "$LOG/run-checks-final.py"
SKYCRPG_SKYUI_SOURCE="$SKYUI" timeout 600 python3 -I -m unittest discover -s modules/ui/tests -p '*.py'
for test in modules/ui/tests/*.js; do
  case "$test" in
    */inventory_actions_contract.js) timeout 600 "$NODE" "$test" modules/ui "$LOG/lane-final2/InventoryMenu.as" ;;
    *) timeout 600 "$NODE" "$test" modules/ui "$SKYUI" ;;
  esac
done
for test in modules/ui/tests/*_test.cpp; do
  binary="$LOG/$(basename "$test" .cpp)"
  timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/ui/src "$test" -o "$binary"
  timeout 600 "$binary"
done
timeout 600 git apply --check /srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-168.patch

timeout 900 python3 modules/ui/tools/build_scaleform.py --skyui "$SKYUI" --nexus-interface /srv/storage/Projects/SkyCRPG/session-20261026/ui-nexus-bsa/interface --java /srv/storage/Projects/SkyCRPG/session-20261026/ui-toolchain/jdk-17.0.21+9-jre/bin/java --owner DYL-168 --out "$LOG/swf-3"
```

| Check | Result |
| --- | --- |
| New `layout_focus_contract.js` | **PASS** actual transformed AS bodies: keyboard/controller category/grid navigation, non-color markers, empty Back, R3 pad reachability/text bound/Space/Erase/Clear/Done, keyDown vs hold/up, B/Tab/Escape cancellation, native sort/search blocked beneath pad, keyboard search ownership callbacks, pinned native quantity/confirmation cancel, native menu modal-before-close routing |
| Geometry harness | **PASS** 720p/1080p/ultrawide and offset/narrow visible rectangles, nested nonunit parent transforms, 85/100/115% HUD and extreme offsets; own panel boxes inside margins and separated; native modal is sole visible content layer above shade; huge HUD names bounded. Native widget shapes/fonts/fresh DEM imagery are not modeled |
| Required DYL-163 runner | **PASS**, 18 steps; `lane-final2/jobs.json` |
| Python discovery | **PASS**,21 tests, 0 skips |
| All JS | **PASS**, 7: follower,grid,HUD,inventory actions,layout/focus,minimap compass,navigation |
| All portable `*_test.cpp` | **PASS**, 9 strict compile/run pairs: favorite,follower,HUD settings,inventory equipment,inventory policy,layout reset,menu focus,minimap,resource snapshot |
| Patch apply check | **PASS** |
| All 3 SWFs | **PASS**, `COMPILED_NOT_RUN`; hashes below |
| Windows DLL / runtime | **NOT_RUN** |

Initial failures retained: `focused-as-1` **FAIL** because the new movie stub returned a constant highest depth; corrected it to increment, then passed. `lane-regression-initial` / `lane-initial/` **FAIL** because an existing HUD assertion requires the recorded default status y656. The first candidate moved it to652. Keeping the existing 12-unit bottom margin restores y656 while the hotbar still stacks above with 12-unit separation. Changed-input rerun passed. Later source changes synchronized the native search focus/caption and device-specific hint; affected AS checks and movie builds were rerun. Unchanged portable C++ evidence was reused. Earlier successful movie builds remain historical, not final artifacts.

Final movies: `/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-168/swf-3/Data/Interface/`; pinned source and acquired Nexus base hashes, include hashes, FFDec hash, command argv and outputs in `swf-3/build.json`. No third-party binaries/art or compiled output committed.

| Movie | SHA-256 |
| --- | --- |
| `inventorymenu.swf` | 6f8f4ba3870833ced32a832ff673275b9fe5c43ded34f644ceb3d592c7781bd4 |
| `skyui/inventorylists.swf` | 734ea7108ecfa606f406c04ed9c9ae6c30270d02bb7394b6c311278904bf8596 |
| `hudmenu.swf` | 54937d9d48d9a5de4c757ca2ce874fff4a41ed72b39048ad2eabd98292a31628 |

## Acceptance ledger

| Ticket criterion | Already covered | Implemented now / automated | Current candidate gameplay |
| --- | --- | --- | --- |
| AC1 every MVP inventory/hotbar action in both modes | Bounded DYL-164 mixed workflows; DYL-161 native Favorites controller/auto-FAV keyboard; no per-action controller-only proof | **PASS** source/harness for grid/category/search path; native equipment/Favorites path retained, one HUD page | **NOT_RUN** separate physical full routes |
| AC2 visible focus, safe dialogs, reliable controller cancel | Prior quantity/poison mixed route **PASS** | **PASS** shape focus, bounded controller pad with cancel, native modal routing and hiding owned panels | **NOT_RUN** actual GFx/device callbacks, caret/buttons/legibility |
| AC3 safe party/quest/map/bottom at 720p/1080p | Prior bounded follower/map/resource evidence, historical1080p effect overlap **FAIL** | **PASS** owned visibleRect geometry, scale/offset bounds, ultrawide; external bounds not fabricated | **NOT_RUN** captures and map/effect measurement; AC3 remains open |
| Status text/icons, long names, reset | Existing E/F/S/Q and native long-name card mixed **PASS** | **PASS** hotbar E, ellipses/fitting, startup reset policy | **NOT_RUN** reset adapter/115% rendered check; patch must be integrated |

## Owner route — separate passes, every row NOT_RUN

Use an isolated UI-only profile and a disposable copy of `SkyCRPG_Fixture_CombinedAcceptance` (the preserved fixture is untouched); `SkyCRPG_Fixture_InventoryItems` / `SkyCRPG_Fixture_MainQuests` are alternatives if required. Record exact copied save hash/provenance. Repeat a short combined smoke after the UI-only route. Maximum one Skyrim process/profile; budget each manual route20min, with a separate bounded run if it does not fit. Before launch record purpose, profile/command, expected observations, owner, PID/run URL, logs, deadline and completion. Stop for wrong-copy mutation, input lock, duplicate use/drop or off-screen dialogs; preserve evidence, terminate only the owned tree if needed, verify cleanup/restoration.

For every runtime result attach `<head SHA>`, `<CI run>`, DLL/package/SWF/INI/executable hashes, dependency versions/loaded-file winners, host/Proton, actual device and any remaps, physical resolution, Stage rectangle if available, scale, screenshot timestamps/hashes, before/after native item count/ownership/weight, exit and restore result. Never infer controller coverage from a mixed pass. Do not save fixture mutations over the preserved save.

Resolution/scale setup (orchestrator/owner only; none executed here): with Skyrim fully exited, snapshot the disposable profile's `SkyrimPrefs.ini` and `Data/SKSE/Plugins/SkyCRPG_UI.ini`. In `SkyrimPrefs.ini` `[Display]`, set `iSize W=1280`, `iSize H=720`; retain the profile's existing fullscreen/window settings. Launch and verify the capture is1280×720; if the compositor forces a different size, record actual size and leave that row **NOT_RUN** until corrected. Exit, change W/H to1920/1080 and repeat. For the scaled case at 1080p set UI `[HUD] bResetLayout=0`, `iScalePercent=115`, `iOffsetX=0`, `iOffsetY=0`; restart. This scales owned HUD text/panels, not native inventory or DEM. Default cases use100/0/0. Optional supported ultrawide uses2560/1080 with the same restart/capture-size verification. Restore only the task-owned copies from byte backups after the final exit, and verify hashes; do not edit personal INIs. INI keys use the exact names with spaces shown above.

| Pass / step and expected result | Runtime |
| --- | --- |
| Controller-only: turn 8BitDo XInput ON, report device, do not use mouse/keyboard gameplay. Open Items; traverse full first/last/partial row, Up to categories, Left/Right change, Down/A returns with no equip. Enter empty Favorites and B out; reopen. Visible region/cell focus at every transition | **NOT_RUN** |
| Controller search: R3, spell IRON with D-pad/A, Done; inspect match. R3 append then B, prior filter returns. Clear/Done. Search ZZZZZ/Done (empty), categories and B remain usable. No equip on Done or cancel. R3/A/B hold/release does not repeat activation; no world sneak/attack | **NOT_RUN** |
| Controller native sort: LB/RB columns, tap L3 order, hold L3 column selection, B closes; no stale grid focus. Re-enter after custom search. Read full details of an equipped grid row; equipment cells have no hidden focus stop | **NOT_RUN** |
| Controller actions: inspect distinct plain/tempered/enchanted swords and stolen/owned helmets; native equip/unequip, favorite/unfavorite, use one potion, drop3 from a stack with native quantity, cancel another drop, Nettlebane rejection, accept/cancel native poison confirmation. Record exact item/count/weight before/after; no grid or world action under dialogs | **NOT_RUN** |
| Controller hotbar: favorite weapon/spell/consumable; native Favorites open, select/use each, close; verify eight HUD entries/EMPTY/depleted state and E flag. Native cast/attack uses its normal controls; B backs out, no custom HUD focus. If only2 native numbered quick slots exist on controller, record that limitation and use native Favorites for the remaining actions | **NOT_RUN** |
| Keyboard/mouse-only: exit, turn controller OFF, report device; fresh profile run. I, hover/click and arrows, categories, E/Enter equip/use, R drop/quantity/cancel, F favorite, Space search/type/empty result/clear/Back, header sort and native column dialog; inspect equipment through grid. Confirm all actions occur once | **NOT_RUN** |
| Keyboard/mouse hotbar: Favorites → establish actual native numbered badge and press1–8 once; separately test auto-FAV. Native-bound press has no duplicate request; weapon second press, consumable count−1, spell/shout selection then native cast. Closing/in-menu press does not fire hotbar/world action | **NOT_RUN** |
| Each required size at 100%: capture gameplay with party, quest update/text, minimap, active effects, compass fallback (K hide map), bottom status/hotbar/shout; inventory with focused category and cell, long name/native card, all equipment slots and native footer; native quantity and poison confirmation; controller search pad | **NOT_RUN** |
| 1080p115%: capture same HUD/quest/effects and at least one inventory modal. Read labels/percentages without clipping or overlap. Record the exact active-effect intersection rows above. Check native search caret, sort selection and prompt buttons remain visible | **NOT_RUN** |
| Reset: with game exited first set85/120/−60 with `bResetLayout=0`, restart and capture. Exit, set only `bResetLayout=1`, restart: defaults restored, logger says reset applied, enable/compass policy and INI bytes unchanged by plugin. Set flag0 and default keys100/0/0, restart or restore backups | **NOT_RUN** |
| If host permits2560×1080: capture default HUD/inventory/pad/modal; center bottom stack, upper-left follower and provider map remain usable. Verify actual screenshot dimensions; otherwise retain NOT_RUN | **NOT_RUN** |
| Combined smoke: copied combined fixture, one device explicitly stated; inventory/action/dialog/cancel, Favorites, map toggle, camera movement then native menu exit; no Camera state writes/dependency from UI. Quit normally, attach logs, verify no processes/deployments and backup hashes restored | **NOT_RUN** |

## Limitations and decisions

Native widget rendering/fonts, hit targets, keyboard search's external widget implementation, physical controller callback ordering, per-device no-duplicate behavior, Windows startup flag adapter and all screenshots remain unverified. The harness executes the pinned native quantity/confirm cancel and menu routing with stubs; it does not run GFx or the external keyboard-search widget. The authored native footer/quest/compass and DEM need runtime bounds; generated source cannot measure them. The historical active-effect overlap remains a real open finding. No provider version, scope or owner ruling was changed.

No new owner scope decision is requested before integration. The owner/orchestrator must run the separate-device/layout route and resolve or explicitly decide the measured external-HUD overlap before AC3/ticket acceptance. Custom controller direct slot cycling/pages, arbitrary controller text entry and broader accessibility/localization remain outside the implemented baseline. No owner playtest acceptance, commit, push, deployment, game launch or public release is claimed.

### Owner keyboard/mouse run `d5100bba9fc34afcbec22b45232b83b8` on `eef1154`

Owner reports all functional steps **PASS**, at 1920×1080; device confirmation pending from the owner. The card below the model, the two-row header and the NAME/sort-radio ghost are fixed in this run. Four visual defects are **FAIL** on `eef1154`: bow crosses the grid/gear corner; grid-region brackets do not hug the visible grid; the right-end Favorite/Equipped caption overlaps the last icon; the category rule extends past the panel. Screenshots were not saved to disk.

Round 3 moves the equipment/grid block left as needed and uniformly scales it only when necessary to keep the 542-unit panel left of `visibleRect.x + 0.63 * width`. The original three 165-unit tiles (172-unit spacing) remain; the minimum rendered tile width across the four requested fixtures is 126 pixels. The card remains below 0.55 H with an eight-scaled-unit gap, limited to 408 scaled units in the model column and above the bottom bar. The model interaction rectangle now spans that whole column over the top 0.55 H. Focus-region brackets use runtime panel, list, header and first visible tile bounds; category background width is clamped to panel width. Per-tile selection brackets and modal ownership remain unchanged.

Automated geometry/focus contracts **PASS** for 1280×720, 1920×1080, 2560×1080 (x −320), and 1024×600; full lane tests and SWF compilation **PASS**. These are stub and compile evidence, not game measurements. Category caption **fixed, orchestrator review**: the right-end "Equipped"/"Favorite" text is not a SkyUI clip. It is SkyCRPG's own `_skycrpgDetailPanel` full-status line (`grid-methods.as.inc` `skycrpgStatus(entry,true)`, drawn in `inventory-presentation.as.inc` `UpdateItemCardInfo`). DYL-164 fixed it at design (852,40), the top of the old card column. The pinned SkyUI and Nexus timeline searches found nothing because the source is ours. It now sits in the model column directly below the 0.55 H reservation, and the card moves down 28 scaled units beneath it. `layout_focus_contract.js` asserts that it is below the preview, inside the column and clear of the card, for all four rectangles. Final SWFs: inventorymenu `6e4a1d771bf4c7593018f3a0bb8aa5517c328403ee3d2ad35c69ee2428d60ee6`, inventorylists `8366814fe4dd49de2b7ed272ad7d31f416b7200126238f08e0a87a87b717b1c2`, hudmenu unchanged `54937d9d…`. The rule clamp depends on SkyUI refreshing its selector from background width; actual GFx bounds and behavior require owner verification. Round-3 in-game verification **NOT_RUN**.
