# P1-04 / DYL-153 — native UI candidate

2026-10-26. **Implementation candidate compiled; gameplay acceptance NOT_RUN.**
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
  hardware behavior is **NOT_RUN**. Edge navigation clamps; full controller UX is
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
  --out /tmp/skycrpg-ui-candidate-v611 \
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

Local artifacts: `/tmp/skycrpg-ui-candidate-v611/` (`build.json`, compiler logs,
`Data/Interface/inventorymenu.swf`, `skyui/inventorylists.swf`, `hudmenu.swf`).
Earlier diagnostic export/logs: `/tmp/skycrpg-ui-candidate-2/`.

| Artifact | SHA-256 |
| --- | --- |
| Base `inventorymenu.swf` | `915f8e688ab6a529aefa3cf46f4ab3a5805bc4ba22fb9f50f1aa591d577edb6d` |
| Base `skyui/inventorylists.swf` | `3ac8dd993fe693386c65ccf9a33c73151004f87c2634b06c95069ba524b3d033` |
| Base `hudmenu.swf` | `b586c7e1b13b4eee83e820566027243314d5019dbefc36bcac85f247325a22db` |
| Candidate `inventorymenu.swf` | `9dd9e54834d5dcda3e42cada3a2127c4b70f61b3df33382a8738c838ead6c70b` |
| Candidate `skyui/inventorylists.swf` | `648242319b9ac7ef1b0b1382f3857a546fd2c2927767a9a4e2086e1dad355220` |
| Candidate `hudmenu.swf` | `4e2f44ec8e74464a102b443150ee4c8a43b3ceadf60e82298b21a142eb0f689c` |
| FFDec CLI JAR | `5eb6b2bae13a2cc8905b2f8338996701e04eb651ab224043948fabbec942735e` |
| FFDec implementation JAR | `baccc02d815aa651ffd11bb6dfc5edfde0fff8d768305f5ce7e7704c19fcd323` |
| Temurin JRE 17.0.21+9 Linux x64 archive | `9f41bf88c7900c7097444a1bdeb84146ae692bc08842490ab098063c74bf9a8a` |

## Minimap, follower and hotbar probes

Selected external route: Dragon's Eye settings fork **1.7.5**, Infinity UI NG
**2.1.3**, LMU settings fork **1.3.4**, plus SkyUI 6.11. Provider binaries/archive
hashes and actual resolved winners are parent integration evidence, not supplied
by this source build. No provider is embedded in the candidate.

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
| UI-only live grid/HUD with ordinary third person | NOT_RUN |
| Real weapon equip and unequip exactly once; identity/count/weight | NOT_RUN |
| Map visible, positioning and exterior/interior transition | NOT_RUN |
| Provider absent/disabled navigation recovery | NOT_RUN (compass code unchanged) |
| Combined-load smoke | NOT_RUN |
| Controller navigation/activation and modal focus | NOT_RUN (no controller) |

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
evidence. No runtime acceptance checkbox is satisfied by this document.

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
