# P1-04 / DYL-153 — native UI candidate

2026-10-26. **Owner UI-only route and final controller correction PASS (owner-observed). Automatic compass replacement and full follower/hotbar remain open.**
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
| Real weapon equip and unequip exactly once; counts/carry weight | PASS owner-confirmed run071034 on previous candidate; changed candidate retest NOT_RUN |
| Distinct same-base instance traits/ownership | NOT_RUN |
| Map visible inside/outside | PASS owner-confirmed run071034 on previous candidate |
| Precise map positioning | NOT_RUN |
| Native compass usable with minimap hidden | PASS owner-confirmed run071034 on previous candidate |
| Provider-absent startup recovery | NOT_RUN |
| Combined-load smoke | NOT_RUN |
| Focused controller grid/category navigation and equip/unequip regression | PASS owner-confirmed run075205 on imported-class candidate |
| Full modal/controller/layout matrix | NOT_RUN |

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
evidence. The screenshots establish live presentation; owner confirmation separately establishes the scoped action/map checks below. Changed candidate navigation requires retest.

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
<node> modules/ui/tests/navigation_contract.js modules/ui <pinned-SkyUI-checkout>
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

The parent subsequently supplied the owner's explicit confirmation for
**UI run20261026T071034**: live grid, single-press equip/unequip (one action per
activation), unchanged counts/carry weight, minimap indoors/outdoors, and usable
native compass with the minimap hidden are **PASS (owner-observed)** on the
previous candidate. This is separate from what can be inferred from screenshots.
See [session evidence](session-2026-10-26.md#first-provider-routes-and-recovery)
and its indexed runtime logs. The manifest composite SHA-256 is
`c329cb61ea5c810e1b37bca20f1882902efed350ffa7efdfe6bfffce278b8682`.
The controller correction changes input routing, so its navigation and native
action regression retest remain **NOT_RUN**; earlier PASS evidence is preserved,
not reused to certify the changed route. Provider-absent startup and distinct
same-base instance/ownership scenarios still lack scoped proof.

Category edge review also inspected the actual pinned callback chain:
`CategoryList.onItemPress` dispatches `itemPress`, which calls
`InventoryLists.onCategoriesItemPress` → `showItemsList`; the latter clears
item selection, resets scroll, changes the filters, and requests an update.
`ItemMenu.onCategoryChange` is empty. The focused harness now executes these
actual methods to check two successive native category changes retain category
focus and Down returns to the grid. It also checks empty Favorites/search
enumerations can escape through categories, including when no item was selected.

Painting no longer changes focus at all: an input-only mouse-rollover override
returns focus to grid, delegating original rollover behavior to BasicList. This
prevents a repaint or restored selection from implicitly leaving category mode.
A fourth source check enforces this input/presentation boundary. These callback
checks still use native-menu stubs and do not certify Scaleform event scheduling.

## Corrected candidate compile (persistent host inputs)

After the parent's monitored camera route ended cleanly, the compile hold was
released. **PASS**: all three corrected candidates compiled against the same
actual Nexus6.11 BSA base hashes, with a fixed **512MiB Java heap** (`-Xmx512m`),
600-second per-job limits, sequential execution, explicit PIDs/logs and cleanup.
Elapsed compiler times were0.514s,0.565s and0.765s. No game was launched/deployed
by this worker. Corrected gameplay navigation/action regression remains
**NOT_RUN pending owner retest**; the previous candidate's owner PASS checks
remain separately recorded above.

Candidate root:
`/srv/storage/Projects/SkyCRPG/session-20261026/ui-candidate-controller-fix/`.
`build.json` records all exact commands, input/patch/output hashes, process IDs,
timeouts, durations and exit codes. Bounded export/decompile checks under the
same heap limit confirm grid routing occurs before native category handling,
category-focus logic exists in bytecode, painting has no focus mutation and the
mouse rollover delegates to its native parent. See `verification-jobs.json`,
`bytecode-check.json` and export logs in that same local artifact directory.

| Corrected Data/Interface winner | SHA-256 |
| --- | --- |
| inventorymenu.swf | `5b474c27fd84f1e82022bde4b79184d14b1334bf4f5f8ec8739d8e4b47215f80` |
| skyui/inventorylists.swf | `bee2ca0fb283bfb24dc674c9f442f1cd52de5dc7078a6ad0315f45ec53211a4e` |
| hudmenu.swf (unchanged) | `c42e7a26bb276bc8ee0a2f877c2248e8bb8bcd73bee9798119ae3ebdddde3e38` |

Four focused source safety checks and the Node actual-source callback/navigation
regression **PASS** after the final changes. Results/job records persist in
`session-20261026/ui-controller-fix/`. The legacy-category-first variant still
fails its adjacent-cell assertion, demonstrating this test detects the reported
consumer-order regression.

Rebuild with persistent recovered inputs (new output directory required):

```bash
python3 modules/ui/tools/build_scaleform.py \
  --skyui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 \
  --nexus-interface /srv/storage/Projects/SkyCRPG/session-20261026/ui-nexus-bsa/interface \
  --out <new-persistent-output> \
  --java /srv/storage/Projects/SkyCRPG/session-20261026/ui-toolchain/jdk-17.0.21+9-jre/bin/java
```

Parent retest: stage these winners, open Weapons, select Iron Arrow and press
Right once (stick and D-pad separately): Iron Battleaxe should become selected
with Weapons retained. Check adjacent/vertical movement, then Up from row0,
two successive category switches, Down to grid, and an empty Favorites category
escape. Confirm once after returning to a selected weapon, then confirm once
again; record equip/unequip/count/weight before and after. Check category-entry
Confirm itself performs no item action. Parent owns process tracking, deployment,
logs and teardown, followed by any combined smoke.

## Controller retest074633 — imported class omission

**FAIL (owner-observed)**: after parent deployed exact corrected hashes
`inventorymenu5b474c27…` and `inventorylistsbee2ca0f…`, Right still changed
Iron Arrow's category to Apparel. The parent confirmed the deployed winners
and a clean prior undeploy. Run20261026T074633 quit cleanly without leftovers.
The earlier source/stub tests and single-movie bytecode check did not prove
which duplicate class supplied the live imported panel.

Actual acquired SWF binary tags establish the missing linkage:

- `inventorymenu.swf` ImportAssets2 tag71 imports symbol **InventoryLists**,
  local character5, from **skyui/InventoryLists.swf**. It separately exports
  its own `__Packages.InventoryLists` definition as character11.
- `skyui/inventorylists.swf` exports the **InventoryLists** symbol as character100
  and its own `__Packages.InventoryLists` as character113. Its initialization
  calls `Object.registerClass("InventoryLists",InventoryLists)`.
- The failed candidate's decompiled menu definition had corrected routing,
  while its imported movie's definition still had the original category-first
  handler. This is a concrete packaging omission. Source `swfsources.cmake`
  also includes InventoryLists.as in both movie targets.

Changed hypothesis/correction: compile the original routing patch into **both**
InventoryLists definitions. No new dispatcher, synthetic activation or inventory
backend is introduced. Both source variants retain all native delegate/focus
calls. Non-player menus sharing the imported panel stay behind the existing
`itemList.skycrpgGrid` guard. Their cross-menu lifecycle still needs runtime
coverage; compilation does not establish runtime ownership or acceptance.

Builder `TARGETS` now explicitly includes `ItemMenus/InventoryLists.as` in
**inventorymenu.swf and skyui/inventorylists.swf**. A fifth focused test fails
if either target omits it. The source callback/navigation test still passes.
This addresses the confirmed imported movie boundary rather than repeating
the same build input/hypothesis. No extra diagnostics or action hooks were needed.

New candidate:
`/srv/storage/Projects/SkyCRPG/session-20261026/ui-candidate-imported-routing/Data/Interface/`.
All three compile jobs **PASS** sequentially at512MiB heap/600s limit. Five source
checks and the actual-source callback/navigation logic check **PASS**. Bounded
export/decompile now verifies routing before category handling in **both**
compiled `__Packages.InventoryLists` classes, retained imported-symbol class
registration and unchanged two native ItemSelect call sites. Job manifests,
logs, hashes and `bytecode-check.json` persist beside the candidate. Raw acquired
linkage extraction is `ui-controller-fix/actual-class-linkage.json`.

| Data/Interface winner | SHA-256 |
| --- | --- |
| inventorymenu.swf (unchanged from failed correction) | `5b474c27fd84f1e82022bde4b79184d14b1334bf4f5f8ec8739d8e4b47215f80` |
| skyui/inventorylists.swf (new imported-class correction) | `7f0ce7fa3eec3845fc9cfce3776beba7f89cf2e6b9fe21d2851886deca852cd8` |
| hudmenu.swf (unchanged) | `c42e7a26bb276bc8ee0a2f877c2248e8bb8bcd73bee9798119ae3ebdddde3e38` |

Owner retest of this new imported-class candidate is **NOT_RUN**. Parent owns
deployment, controller/action checks and any combined smoke. Previous owner
UI/action/map PASS remains valid only for its recorded candidate; neither the
failed correction nor this compiled correction is promoted to gameplay PASS.

## Final imported-class controller retest075205 — PASS

**PASS (owner-observed)** on the final imported-routing candidate. After the
parent requested the focused checklist, the owner's exact response was:

> Ok everything passed for the UI fix, it's working as intended now

This confirms the requested Right **Iron Arrow → Iron Battleaxe** selection,
Up/Down grid rows, Up from the first row into categories, Left/Right category
changes and Down/Confirm back into the grid without an item action. The focused
equip/unequip regression passed with one action per activation and unchanged
counts/carry weight. This is the owner's reported behavior, distinct from
compilation/stub-test evidence. Prior controller failures remain recorded in
this report; run074633's failed candidate is not relabeled PASS.

The parent deployed final generated winners from implementation source
`1be22bbbb0b0519e0b3c876ce4929a395541161d`:

| Final tested Data/Interface winner | SHA-256 |
| --- | --- |
| inventorymenu.swf | `5b474c27fd84f1e82022bde4b79184d14b1334bf4f5f8ec8739d8e4b47215f80` |
| skyui/inventorylists.swf | `7f0ce7fa3eec3845fc9cfce3776beba7f89cf2e6b9fe21d2851886deca852cd8` |
| hudmenu.swf | `c42e7a26bb276bc8ee0a2f877c2248e8bb8bcd73bee9798119ae3ebdddde3e38` |

Run **20261026T075205**, UI-only,07:52:05–07:55:03 America/Phoenix:
**QUIT**, success=true, exit0, no load problems, no leftover/game processes.
The parent's memory watchdog had no intervention (`stop_reason=null`).
Local evidence is `/srv/storage/Projects/SkyCRPG/profiles/ui-only/runs/20261026T075205/`;
job/telemetry are `session-20261026/ui-imported-retest-job.json` and
`ui-imported-retest-memory.jsonl`. The UI worker performed no deploy/game launch.

| Runtime record | SHA-256 |
| --- | --- |
| run.json | `9be43ba2636bf63674aff40d8c4798362d5ab90782f2b1eefca93f565401e7a4` |
| skse64.log | `97f6f29a568127480125c87569ff241ca21de8e218d93a60cbfcc8001e664dbc` |
| skse64_loader.log | `9dbb43d59e94159423fab9790dd833a0d89891769ac6a189f081edd77874622d` |
| SkyCRPG_UI.log | `522655e4c05a0da1a82d584ff0f164a4232db770bdce50698c0bed65adbcc04d` |
| InfinityUI.log | `e2d5af3719603b8b8b2d23327adca4314816e5e2452743929d1099453bc9416a` |
| LocalMapUpgrade.log | `f41d399306f9669752239fdd66b7d218558c239d9e3ecb39190d3738f502ced7` |
| DragonsEyeMinimap.log | `bb47138cfa7d92f5f570274fc13fa7365b47d2ba44409c87eb98ba499d3ef4aa` |

Runtime remains Steam1.7.104.0/SKSE2.3.1 on the recorded Proton host. The loaded
UI DLL is the unchanged load-only build reporting source`d1f6193`; the functional
controller patch is the generated SWF source/hash above, not a new DLL claim.
The SKSE log lists DEM/InfinityUI/LMU/UI only; no Camera DLL, SmoothCam or TDM.
UI-only with the native camera is independently usable for this focused proof.

**Proven minimal strategy:** native InventoryMenu/Scaleform presentation over
live backing entries and native equip/unequip callbacks, with original bounded
input routing. Five source checks, all three bounded compile jobs, both compiled
class-handler inspections and the actual-source callback/navigation regression
remain PASS; no unchanged build/tests were repeated for this report-only update.
The owner route supplies the missing game evidence.

**Remaining scope:** complete eight-slot hotbar/follower UI and observer/dispatch
interfaces were probed only, not accepted as implemented. The minimap's native
compass remains visible: automatic readiness-driven compass replacement is still
open, as are provider-absent startup, distinct same-base ownership/instance
fixtures and the full modal/controller/layout matrix. Previous owner run071034
records map inside/outside and usable compass with map hidden. Combined smoke
is parent-owned and was running as075556 when this report was updated; no result
is inferred here. No Phase1 contract is frozen and no Phase2 work is included.

Reviewable focused provenance is committed in
[ui-spike-provenance](ui-spike-provenance/build.json): exact bounded compiler
commands/PIDs/input-output hashes and exit results,
[source test output](ui-spike-provenance/source-tests.log),
[navigation/callback test output](ui-spike-provenance/navigation.log),
[both-class bytecode checks](ui-spike-provenance/bytecode-check.json),
[actual binary class linkage](ui-spike-provenance/actual-class-linkage.json) and
[source provenance](ui-spike-provenance/source-provenance.json). These small
records contain no SWFs, assets, third-party source dumps or exported movie trees.
Parent's shared indexed runtime logs supply the separate native startup/run proof.
