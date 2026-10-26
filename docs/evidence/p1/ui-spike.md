# P1-04 / DYL-153 — native UI candidate

2026-10-26. **Live grid/HUD presentation observed; controller horizontal navigation FAIL, correction awaiting retest.**
This is an original AS2 grid/HUD patch built against external SkyUI Community
6.11, not a screenshot or replacement inventory database. No game was launched
or deployed by this worker. Parent owns the shared game schedule and packages.

## Candidate and actual interfaces

`modules/ui/scaleform/` contains original presentation snippets;
`modules/ui/tools/build_scaleform.py` applies bounded, hash-checked changes to
an external checkout and compiles three local SWFs with FFDec. The existing
UI DLL remains a load-only skeleton. The functional candidate is the generated
Scaleform overlay, independent of that DLL and of Camera/SmoothCam/TDM.

- InventoryMenu `setConfig` enables a three-column, six-row uniform grid on its
  own item list. Other menu instances default to the unchanged list presentation.
- `ScrollingList.UpdateList` still maps each filtered/sorted enumeration entry's
  **native itemIndex** onto its existing native entry clip. It changes clip position
  and artwork only. `entryList` is never copied, reordered or resolved by FormID.
- Existing `BasicList` selection dispatch and `InventoryMenu.onItemSelect` remain
  intact. Keyboard/controller activation still reaches `ItemSelect` through the
  original menu callback. Mouse uses the original `CheckForMouseEquip` →
  `AttemptEquip` path; this patch adds no action dispatcher or synthetic input.
  `AttemptEquip` retains `shouldProcessItemsListInput` and `confirmSelectedEntry`.
- Cell labels use live `text`, `count`, `infoWeight`, `equipState`, and `enabled`.
  `infoWeight` is the real InventoryDataSetter item-card weight field; its native
  null means nonpositive weight, undefined displays `?`. Native bottom-bar carry
  weight and item cards remain available. A cell references the backing entry
  only while painting; it retains no pending item command across invalidations.
- Arrow navigation moves through the live filtered enumeration (±3 vertically,
  ±1 horizontally), then invokes the existing selection method. Enter/confirm,
  quantity dialogs, search, sorting, focus and native pause remain SkyUI-owned.
  Controller feasibility is implemented through its real navigation codes;
  hardware initially exposed a horizontal focus bug (see correction below). Edge navigation clamps; full controller UX is
  outside this proof.
- Original HUD text displays the native HUD's health/magicka/stamina meter values.
  A child clip owns its own frame callback and is destroyed with the HUD. It also
  reports map-clip attachment and unavailable follower data, with native hotkeys.
  No substitute actor values or follower API are fabricated.

Verified primary interfaces are in the pinned external SkyUI files:
`InventoryMenu.as`, `InventoryLists.as`, `ItemMenu.as`, `BSList.as`, `BasicList.as`,
`BasicListEntry.as`, `ScrollingList.as`, `TabularList.as`, `TabularListEntry.as`,
`InventoryDataSetter.as` and `Vanilla/HUDMenu.as`.
[Source tag](https://github.com/doodlum/SkyUI-Community/tree/v6.11/source/actionscript).

## Reproduction and bounded jobs

Prerequisites: external SkyUI source **v6.11**, commit
`3d537a366f1a45c14a7012ea0aa00be18108f073`, bundled FFDec **26.0.0**, Java 17.
No external source, SWF, icon, font or binary is committed here. The output directory
must be new and outside the provider checkout. The builder verifies source and base
SWF hashes before creating output and records command, PID, log, timeout and output
hashes in `build.json`. Each compiler invocation has a 600-second limit and kills
only its own Java process on timeout. Exit zero and a nonempty SWF are completion
signals; this does not establish gameplay behavior.

```bash
python3 modules/ui/tools/build_scaleform.py \
  --skyui /tmp/skycrpg-ui-skyui-v6.11 \
  --nexus-interface /tmp/skycrpg-ui-nexus-bsa/interface \
  --out /tmp/skycrpg-ui-candidate-nexus611 \
  --java /tmp/skycrpg-ui-toolchain/jdk-17.0.21+9-jre/bin/java
SKYCRPG_SKYUI_SOURCE=/tmp/skycrpg-ui-skyui-v6.11 \
  python3 -m unittest discover -s modules/ui/tests -v
python3 modules/ui/tools/minimap_preflight.py --data <resolved-external-overlay>/Data
```

Initial compile diagnosed FFDec's AS2 parser rejecting method return annotations.
Changed input: removed annotations and typed arguments. The next compile passed.
Final rerun changed the source pin from the audit checkout's later Linux-build
commit to the selected release tag; all affected source/base SWFs were identical.
No unchanged failure loop occurred. Three focused source safety checks passed
(0.001 s): ambiguous anchors fail closed; all native delegate/focus calls remain
identical; original art introduces no item transaction calls. A bounded FFDec
export/decompile confirmed `skycrpgMove`, cell paint and grid geometry in compiled
`inventorylists.swf`. Decompile verifies bytecode inclusion, not native execution.

Local artifacts: `/tmp/skycrpg-ui-candidate-nexus611/` (`build.json`, compiler logs,
`Data/Interface/inventorymenu.swf`, `skyui/inventorylists.swf`, `hudmenu.swf`).
Earlier diagnostic export/logs: `/tmp/skycrpg-ui-candidate-2/`.

| Artifact | SHA-256 |
| --- | --- |
| Base `inventorymenu.swf` | `9225ed93e1589e69f6d5305299e8e6847aaaea1323a3c39ba88360803c3d49a6` |
| Base `skyui/inventorylists.swf` | `6a762b62dbc58d8ea871620e181b1e950abd32b0cfd726ed44d1aba350469061` |
| Base `hudmenu.swf` | `e8277c1fedcda3ac8c159441f4e7c55b62fbe564ec3cc1bd602b70427f0a47cf` |
| Candidate `inventorymenu.swf` | `ff0f4ddd38da00f9df7680a0e687cdf6c41bfa27027fc903fc478615dfa87aae` |
| Candidate `skyui/inventorylists.swf` | `8d810de2fe022da0d88fd160fa60c2bfdef2a2f6499541094d359a20fca20fb0` |
| Candidate `hudmenu.swf` | `c42e7a26bb276bc8ee0a2f877c2248e8bb8bcd73bee9798119ae3ebdddde3e38` |
| FFDec CLI JAR | `5eb6b2bae13a2cc8905b2f8338996701e04eb651ab224043948fabbec942735e` |
| FFDec implementation JAR | `baccc02d815aa651ffd11bb6dfc5edfde0fff8d768305f5ce7e7704c19fcd323` |
| Temurin JRE 17.0.21+9 Linux x64 archive | `9f41bf88c7900c7097444a1bdeb84146ae692bc08842490ab098063c74bf9a8a` |

## Minimap, follower and hotbar probes

Selected external route: Dragon's Eye settings fork **1.7.5**, Infinity UI NG
**2.1.3**, LMU settings fork **1.3.4**, plus SkyUI 6.11. Acquired archives and
resolved winners are parent integration evidence. Read-only package inspection
is recorded below. No provider is embedded in the candidate.

DEM `include/MiniMap.h` identifies `_level0.HUDMovieBaseInstance.Minimap`;
`source/MessageListeners.cpp` handles IUI patch messages and can abort startup
if `Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/Minimap.swf` is missing.
The read-only preflight verifies loose files at this actual attachment path and
the three actual DLL names, records hashes and fails missing prerequisites.
It cannot inspect BSA winners, prove ABI compatibility, certify versions or catch
a corrupt provider at runtime. The negative check on the candidate-only Data
directory correctly reported **BLOCKED**, with all provider files absent.

The HUD tests actual clip existence, visibility and `MapClip` attachment for
diagnostics. That is insufficient to prove map rendering is healthy. **The spike
never hides the native compass.** Thus absent/disabled minimap preserves native
navigation; automatic handoff remains NOT_RUN and requires evidence before
enabling compass suppression. `IsReady` is internal, not an assumed public API.
Do not interpret a present clip as a functioning interior/exterior renderer.

Party Sheet 3.7 remains an optional external configuration route. No verified
public current binary API is available; the fallback explicitly displays
`Follower: unavailable`. It does not infer follower actors from unrelated aliases.
SkyUI FavoritesMenu's `ItemSelect` is a menu-specific delegate, not a general
hotbar service. The spike retains native hotkeys and does not dispatch favorites
from a HUD clip. Eight-slot presentation/dispatch stays in its later scoped task.

## Runtime gate and route for parent/owner

Target is Steam Skyrim **1.7.104.0**, SKSE **2.3.1**, Address Library **13** for
the providers; Ubuntu 26.04.1, Proton Experimental 11, RTX 3080. This worker
used Linux only for source transformation/Java compilation. The DLL build is
unchanged; no Windows native build is required for these AS2 changes.

| Check | Result |
| --- | --- |
| Three AS2 candidates compile; inserted methods decompile | PASS (compile only) |
| Source native delegate/focus preservation and fail-closed anchors | PASS |
| Missing map loose-file prerequisite detection | PASS (expected BLOCKED report) |
| UI-only live grid/HUD | PASS (owner screenshots show native presentation; see provenance below) |
| Real weapon equip and unequip exactly once; identity/count/weight | NOT_RUN |
| Map visible, positioning and exterior/interior transition | NOT_RUN |
| Provider absent/disabled navigation recovery | NOT_RUN (compass code unchanged) |
| Combined-load smoke | NOT_RUN |
| Controller navigation/activation and modal focus | FAIL horizontal navigation; correction retest NOT_RUN |

On an isolated copied save, install the selected external dependencies and stage
the three candidate SWFs as winners. Record source/package hashes and all winners.
Use UI-only with standard third person. Open native InventoryMenu; confirm the
live weapon category paints the 3×6 cells. Record a real named weapon's count,
instance traits, equipped state, individual weight and total carry weight. Select
it by mouse and activate once; confirm exactly one equip. Activate once again;
confirm unequip and unchanged count/weight/traits. Repeat selection after sort,
search and inventory invalidation, then close/reopen the menu. Verify quantity
modal/input focus without double actions. If multiple same-base weapons have
different temper/enchantment/ownership, confirm the intended instance separately.

Show the minimap outdoors, then enter Breezehome. Record rendering and position,
not just the diagnostic label. Toggle provider off/on, then test a separate
provider-absent clean launch; retain compass navigation throughout. Finish with
combined load. Budgets: 20 minutes per route; parent records PID, deadline,
logs and cleanup before launch. Native CUA desktop control is unavailable on
the current host, so automatic compilation cannot supply this owner gameplay
evidence. The later owner screenshots establish live presentation only; full inventory action and minimap gates remain open.

## Reuse and limits

Credit **SkyUI Team and SkyUI Community contributors** for the required external
menus. [Author permissions](https://www.nexusmods.com/skyrimspecialedition/mods/12604)
allow credited dependency-requiring skins/patches as recorded in the reuse audit;
do not infer blanket ownership of the menu's icons, embedded fonts or base art.
The repository contains original patches only; generated local SWFs contain
external material. Public binary redistribution/notices remain a separate
packaging task under the actual rights. No original-work license is selected.

Visual layout, row hit regions after painting, text overflow, same-base instance
selection, menu transitions and other menus sharing the list class still need
in-game checks. No equipment silhouette, full HUD skin, new hotbar backend,
container/barter replacement, follower command system or minimap renderer is
claimed. This candidate makes the native integration concrete and reviewable;
DYL-153 remains open until its required gameplay proof exists.

## Acquired-package reconciliation (before gameplay)

The acquired Nexus SkyUI **6.11** archive's `SkyUI_SE.bsa` was parsed read-only
externally (BSA105, archive flags3, seven folder blocks, 71 files, no compression).
The three real shipped SWFs were extracted to `/tmp/skycrpg-ui-nexus-bsa/interface/`.
**They differ from GitHub's source-tree SWFs.** Earlier `/tmp/skycrpg-ui-candidate-v611/`
is a compile probe only; the final candidate above uses the actual BSA SWFs as
base art/timeline and replaces the five inspected classes with patched v6.11
source. The builder's `--nexus-interface` checks their exact acquired hashes.
All three final candidates compile successfully against those actual bases.

Export/decompile inspection of the acquired binary confirms InventoryMenu's real
`AttemptEquip`, `ItemSelect`, `CheckForMouseEquip`, `setConfig`/item-list layout,
the original list `itemIndex` mapping and `TabularListEntry.setEntry`. The actual
HUD exposes `InitExtensions`, `lastHealthMeterPercent`, `lastMagickaMeterPercent`
and `lastStaminaMeterPercent`; the native setters retain percentages before
survival penalty adjustment. The diagnostic therefore shows native **percentages**,
not absolute actor values. `HUDMovieBaseInstance.Minimap` is DEM's documented
attachment; the label observes it without asserting renderer health.

Runtime packages are at `/srv/storage/Projects/SkyCRPG/session-20261026/extracted/`;
archive hashes are in that session's `acquired.json`/`extracted.json`. FOMOD inspection
confirms **Common + Line-17** for DEM/InfinityUI/LMU, never their SE-AE16 lines.
DEM's Common tree has both the required `Minimap.swf` attachment and its relative
`!assets/MinimapArt.swf`. The preflight now requires the latter too.

| Acquired Line-17 file | SHA-256 |
| --- | --- |
| InfinityUI.dll | `53ef0932485ee1bb281cc4b2baad1fe91e368b9d656989071747b2f1316a85ed` |
| DragonsEyeMinimap.dll | `918fc9b3f4bf284fb1ae8d028370ebd20908d2750f4706383fe56012bddc5c1a` |
| LocalMapUpgrade.dll | `f0fb1c9c822133f9e4ece866fc4f6190424dcafac35e040cd0b4cd037340b3d5` |
| DEM Minimap.swf | `dabdfeb3454d0a40d347ad56ab3e252578a9607e9d7d873d8e70183ffc1dd4e5` |
| DEM MinimapArt.swf | `8eebe684e51f914863918cc46ea14dbf572f766ba4c1be3a99747fdc41dd81ef` |

Shipped DEM INI has `uAnchor=1` (top right), `fScale=0.5`,
`bShowOnGameStart=1`, scan-code37 **K** hide/tap or hold-to-pan, and scan-code38
**L** zoom. Its ring defaults **on** (`[Compass] bCompassRing=1`); parent should
stage `bCompassRing=0` while retaining Skyrim's compass. LMU defaults
`bLocalMapBorder=0`, so no optional border artwork is required. The diagnostic
HUD does not own these provider INIs. No minimap readiness/compass handoff PASS
is implied by these config checks.

Party Sheet's actual package confirms additive `*_party_whitelist.ini`,
`*_party_blacklist.ini`, `*_party_allies.ini` and `*activeeffects_blacklist.ini`
configuration files. Whitelist entries require Plugin|BaseID plus an active
Follow AI package; plain display names are rejected there. These are configuration
interfaces, not a public actor-data API; the safe unavailable-data fallback remains.

Parent staging map: each final candidate `Data/Interface/<path>` goes to the same
relative path beneath the UI profile's Data overlay, winning over SkyUI's BSA.
Retain the external SkyUI ESP/BSA and all dependencies. Parent records deployed
file winners, actual runtime evidence and cleanup. This worker has not deployed
these generated derivative files or incorporated provider assets into Git.

## Owner controller reproduction and bounded correction

The owner supplied two screenshots from the UI candidate on controller. Viewed
in this conversation before a host reboot, they showed the live ALL/WEAPONS
3-column grid, native carry weight **293/300**, gold106, actual names/counts and
existing equipped marks, plus live H/M/S100 HUD. The owner reported that pressing
Right on the stick or D-pad switched Weapons to Armor instead of selecting Iron
Battleaxe beside Iron Arrow. **Result: FAIL — horizontal controller navigation.**
A controller is present; earlier hardware-unavailable wording is superseded.
These static frames do not prove an equip/unequip round trip, preserved instance
state, minimap rendering or safe provider recovery.

Capture provenance: `/tmp/codex-clipboard-84b4478c-62ad-440b-b9e8-d2f11977ca01.png`
and `/tmp/codex-clipboard-66d5e5a0-f08c-428f-95ab-a7c65b1a40b3.png`, supplied by
the owner and visually inspected by this worker. They were **not copied before
the reboot**, which cleared `/tmp`; no image file or hash is fabricated here.
The conversation retains the visual observations. Parent records the gameplay
run/source/deployed winners and the host reboot separately.

Source investigation identified the exact consumer ordering: native
`InventoryLists.handleInput` invokes `CategoryList.handleInput` before the
item-list focus target. CategoryList consumes LEFT/RIGHT unconditionally when
enabled, so the grid's list-local movement never receives horizontal input.
The correction injects original routing into InventoryLists **before** that
category consumer, under the existing SHOW_PANEL, sort/search, disable-input
and disable-selection guards. It adds no activation or item transaction call.

Controls for retest: grid Left/Right selects adjacent live entries; Up/Down moves
three entries. **Up from the first grid row enters category navigation** and
clears the item highlight; Left/Right then uses native category switching.
**Down or Confirm returns to the first item** without equipping it. Mouse item
selection returns to grid navigation. Native column/sort/search/menu controls
remain accessible. One subsequent Confirm on a selected item follows the native
activation path once. Missing selection selects the first available item.

The focused Node regression executes the actual AS2 movement/routing bodies
with native-menu stubs (only the AS2 method/argument annotations are removed).
It checks filtered native indices rather than displayed row indices, horizontal
category isolation, vertical movement, explicit category access/return, key-up
behavior, disabled input/selection, unchanged non-grid category behavior, and
one activation consumer. Removing the routing reproduces a failure under the
legacy category-first order. This is **PASS (logic only)**, not GFx/controller
acceptance. Source safety checks also verify injected routing precedes the real
category call and preserve all native delegate/focus calls.

```bash
<node> modules/ui/tests/navigation_contract.js modules/ui
# Expected assertion failure demonstrates the old consumer order:
<node> modules/ui/tests/navigation_contract.js modules/ui --legacy-category-first
```

All recovery inputs, job records and new results use persistent
`/srv/storage/Projects/SkyCRPG/session-20261026/ui-controller-fix/`; old `/tmp`
artifact paths above are historical. The interrupted build/test call had no
verified completion signal before reboot and is not counted as evidence.
No build is duplicated during the parent's monitored game retry; heavy FFDec
compilation is held until the parent releases the host. Corrected candidate
compile/hash results and owner retest must be appended before promoting this fix.
