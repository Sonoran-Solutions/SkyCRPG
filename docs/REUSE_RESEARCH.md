# Reuse recommendation — DYL-151 / P1-02

Audited **2026-10-25** against current main `96678333d2b0592bb586b2210d8da64f4ebde6a8`.
This is an engineering recommendation for review. **No complete stack has been loaded or feature-tested by this audit.**

Recommend the owner's observed **Steam 1.7.104.0** installation as the single spike target:
SmoothCam 1.8.1 + TDM 2.3.1 for Camera; SkyUI Community 6.11 with a player-only display-grid patch,
external Party Sheet 3.7 and Dragon's Eye settings fork 1.7.5 for UI. The minimap requires
**Infinity UI NG 2.1.3 and Local Map Upgrade settings fork 1.3.4, all using their 1.7 build lines**.
Use native favorites with a small original HUD widget rather than adding casting mechanics.

The [dependency record](compatibility/dependency-lock.md) holds exact files/revisions and evidence levels.
The [source audit](evidence/p1/reuse-audit.md) records primary links, archive inspection and conflicting claims.
[ADR-001](decisions/ADR-001-stack.md) proposes ownership and the Phase 2–4 implementation path.

## Environment received and recommendation returned

DYL-150 supplied [environment snapshot `cd49b3c`](https://github.com/Sonoran-Solutions/SkyCRPG/blob/cd49b3cfc73278350342db4ae7a27c3b5e6a9644/docs/evidence/p1/environment.md):
Steam app 489830, build 24914197, executable 1.7.104.0; Ubuntu 26.04.1/Wayland;
Proton Experimental `experimental-11.0-20261014`; RTX 3080 10 GB, driver 595.99.02.
No SKSE, Address Library, mod manager, local Windows compiler or controller was present.
The initial resolution proposal is 1920x1080 windowed. Old cloud saves are not validated fixtures.

Preliminary findings were sent on [DYL-151](https://linear.app/devils17/issue/DYL-151/p1-02-choose-reusable-mod-components-and-a-compatible-runtime)
at branch `codex/dyl-151-stack-audit`, commit `8fa4126`; a follow-up reconciled the snapshot and corrected
minimum skeleton dependencies. DYL-150's **plain SKSE entrypoint DLLs need SKSE only**. They do not need
Address Library/CommonLib or the feature providers. Keep its Windows x64 GitHub Actions build route.
CommonLib is proposed only for later native engine adapters; do not recreate DYL-150's scaffolding.

Runtime selection is no longer blocked on a missing executable fact. **The desk recommendation is complete;
the installed dependency lock and tested stack remain incomplete.** At this snapshot, DYL-150 had supplied
no compile/load logs. Its SKSE acquisition/install blocker and the missing controller remain open in that lane.
This audit neither launches Skyrim nor changes a profile.

## Component decisions

Classifications describe intended integration, not material already copied. Nothing third-party is vendored by this PR.

| Capability / component | Classification | Decision and implementation cost |
| --- | --- | --- |
| Camera transform — SmoothCam 1.8.1 | External dependency/configuration; documented adapter if required | Reuse offsets, smoothing, collision and aiming view. Original preset and bounded held-pitch/recenter input policy. Medium risk: a high offset is not held pitch. |
| Movement — TDM 2.3.1 | External dependency/configuration | Reuse directional movement. Disable camera auto-adjust and target-lock camera behavior for the initial proof; do not rebuild locomotion/combat. Low integration cost, real aiming test required. |
| Inventory — Grid Inventory, source `c0d8718` | Rejected for this MVP | Capacity gates, carry-weight modification, physical gold/bags, broad menu interception. A source fork to remove those is larger and riskier than a display-grid patch. |
| Inventory/backend — SkyUI Community 6.11 | Permitted source reuse for credited dependency-requiring patch; native menu integration | Reuse engine-populated inventory entries, filters/search/sort, item card, actions, modal dialogs and focus. Write uniform grid layout and two-dimensional selection. Medium/high UI work, bounded to player InventoryMenu. |
| Equipment — Inventory Reforged 1.5.0 release, not Debug | Reference only | Useful equipment/character-panel precedent; not a ready uniform grid. Do not stack competing inventory replacements. Start with original equipment-slot silhouette; 3D doll deferred. |
| Follower/player HUD — Party Sheet 3.7 | External dependency/configuration | Configure only required player/follower widgets; no follower-framework rewrite. Its source archive is reference only. No verified external data API is assumed. |
| Hotbar — native favorites, pinned CommonLib/SkyUI interfaces | Documented adapter; original presentation | Read native hotkey/equipment/count state; use native equip/use path once. Original eight-slot widget and controller selector, no automatic casting. Medium cost; inspect actual per-instance extra data rather than only the spell hotkey array. |
| Hotbar alternative — Glasswork Favorites 1.0.6 / 1.7 Test file | Reference only | Real bar/wheel configuration, but 1.7 file is explicitly a test build and casting/extra pages exceed this contract. Evaluate only if native widget proof fails; disable expanded casting/automation and prove controller use. |
| Navigation — Dragon's Eye settings fork 1.7.5 | External dependency/configuration | Reuse local-map rendering. Original layout/config and compass handoff adapter. No new minimap renderer. Readiness detection remains a specific P1-04 risk. |
| Infinity UI NG 2.1.3 / LMU settings fork 1.3.4 | External dependencies; documented message adapter | Match 1.7 DLLs. Reuse movie patch lifecycle and map provider interface. Do not install both upstream and fork DLLs. |
| Native adapters — alandtse/CommonLibSSE-NG 7.2.0 | Permitted source reuse, conditional | Pin exact commit if adopted. GPL plus exceptions and corresponding-source obligations require a distribution decision before shipping a linked derivative. |
| TISS isometric guide | Reference only | Existing test inspiration, not a camera package or implementation license. No additional camera-mod search is needed before the bounded SmoothCam proof. |

The only serious alternatives examined for unresolved roles are: an exclusive original camera controller,
an original ImGui player menu, a bounded native follower widget, and Glasswork for hotbar. No broad fork,
new movement system, portrait pipeline, follower AI or map renderer is proposed.

## Camera: exactly one transform writer

**Normal third-person writer: SmoothCam.** SkyCRPG owns mode/configuration and input policy, not a second
per-frame world transform. TDM supplies character movement; set `uAdjustCameraYawDuringMovement=0`,
remove target-lock/POV bindings for the first route, and disable the reticle. Target lock is optional later,
only after its explicit handoff is demonstrated. Do not add TrueHUD solely for an unused reticle.
If a native adapter is present, use TDM's `RequestDisableTargetLock` / `ReleaseDisableTargetLock` ownership
pair; the Papyrus alternative is `ToggleDisableTargetLock`. Set `bHeadtracking=0` and `bEnableLeaning=0`
for the minimal subset. Reapply the intended state after load and restore only SkyCRPG's own request.

SmoothCam's pinned source exposes config/preset functions in `SmoothCamMCM` and a V3 C++ interface with
`RequestCameraControl`, `ReleaseCameraControl`, owner queries and `IsCameraEnabled`.
The C++ interface is **ownership arbitration, not a framing/pitch setter**. Requests can be refused;
a consumer must never write anyway. The header explicitly permits copying that header for API use;
this does not grant a general license to fork SmoothCam.

| State | Proposed handoff |
| --- | --- |
| Elevated exploration/melee | SmoothCam applies per-state offset/FOV preset. SkyCRPG's bounded input adapter can hold intended pitch only if P1-03 proves it without fighting another writer. |
| Bow/targeted magic/precise activation | Select tested closer aiming settings or normal first person; keep raycast crosshair aligned. Do not layer independent projectile corrections; choose one proven path. |
| Corridor/ceiling/obstruction | Retain provider/engine collision, shorten/return to close view with hysteresis. Never clip walls to preserve the concept framing. |
| Menus/dialogue/loading/death | Suspend held-pitch input, let provider/native lifecycle take precedence, reacquire only after valid player/cell/state. Inventory input cannot also rotate the camera. |
| First person, mount, swim, furniture, transformation, killcam | Native/provider view; no overhead guarantee. Clear owned input/state on exit and revalidate on return. |
| Disable/emergency/provider absent | Restore only settings changed by SkyCRPG and release input/ownership. Native camera remains usable. |

A preset is not proof of C1. If SmoothCam cannot maintain usable held pitch, the bounded alternative is an
original controller that owns transforms **only while SmoothCam is disabled or has explicitly granted control**.
That controller then owns collision and lifecycle cost, so it requires a new scoped decision after P1-03,
not a speculative implementation now. No simultaneous writers are accepted in either route.

## Inventory and rendering: reuse native selection, replace layout

Grid Inventory's source and English feature documentation agree on the mechanical mismatch: finite footprints,
pickup blocking and replacement encumbrance; the source actively adjusts CarryWeight. Making all items 1×1
or enlarging its board does not establish native weight/search/sort behavior. No supported presentation-only
switch was found in the bounded source pass. Its public costume/provider API does not supply such a switch.

Choose **Scaleform/AS2 inside the real player InventoryMenu**, with a SkyUI-dependent patch. Implement the
small missing view: virtualized uniform tiles, row/column focus, and original equipment slots. Preserve the
native list's backing entries and selected engine item; a filtered grid index must map to its live entry.
Preserve `ItemSelect`, `ItemDrop`, favorite callbacks, quantity dialogs and list invalidation. Reuse the
menu's pause/focus/input behavior; do not emulate an inventory by mutating base FormIDs through console commands.

The inspected SkyUI source contains real action delegates and selection validation, filters/search/sort and
focus handlers. It does **not** contain a ready uniform-grid adapter. Pointer hit testing, clip mapping and
controller directional stepping must change coherently; stale selection after sorting, modal open, or item
removal must cancel/refresh. Item instance identity, count and restrictions stay authoritative in the engine.
Normal paused inventory is intentional. Containers/barter/crafting/follower trade remain ordinary SkyUI.

This is smaller than a new inventory backend, but still the largest original UI task. P1-04 must first equip
and unequip a real weapon through the patched live list, then probe distinct copies and modal focus. If the
patch cannot retain native selection, the one alternative is an original IMenu/ImGui grid and game-thread
transaction bridge. That adds item identity, pause, focus, graphics-hook and controller-navigation work;
it is a higher-cost fallback, not a ready reuse solution. No Grid Inventory capacity mechanics may slip in.

SkyUI's Nexus permission explicitly allows credited skins/patches requiring the original. Do not modify or
redistribute Psychosteve/Jeledity icon files without their separate permission; use original generic icons,
original silhouettes, and references to the user's installed assets. The inspected community tree has no
project-wide license file; a disclaimer mentioning MIT is not an MIT grant. Carry the specific patch permission
and upstream attribution with any later modified menu; do not relicense SkyUI as original SkyCRPG work.

## HUD, follower and hotbar boundaries

Party Sheet has real settings/preset surfaces (`PartySheet.ini`, `PartyUserSettings.ini`, HUD settings).
Configure the player and one recruited vanilla follower only. Use a neutral original silhouette and disable
unneeded widgets; do not import portrait/icon packs. Its overlay route can coexist with Scaleform, but visual
anchors, focus suppression and controller operation still need P1-04. No current exported status API was
verified; the older July source archive cannot establish the October 3.7 binary's API or modification rights.

If external configuration cannot meet U1, write only a native player/one-follower status observer plus our
widget: resolve handles, sample resources at a bounded rate, show unloaded/unknown state, clear on dismissal,
death and load. No recruitment/party commands or new framework. This fallback is engineering work, not a
claim Party Sheet offers a reusable data service.

For U2, integrate native favorites, whose actual source surfaces are recorded in the evidence appendix.
`MagicFavorites.hotkeys` alone does not cover physical item instances. Read item favorite/hotkey extra data
and refresh counts/equipped state. Keyboard activation delegates to native hotkeys; controller uses a focused
slot selector routed through the same native action boundary, not synthetic simultaneous inputs. Keep ordinary
favorites accessible and block activation behind any menu/modal. No cooldown/casting automation is invented.
The Scaleform `ItemSelect` delegate only exists in its native menu context; it is not a global HUD API.
Keyboard can pass through native hotkeys. Controller HUD actions need original game-thread dispatch using
the inspected `ActorEquipManager` methods with resolved item extra data/count/slot, and P1-04 must prove
consumables and spell selection follow the intended native behavior exactly once.
Glasswork's regular and 1.7 Test files are distinct; the stale SE/AE-only description is not evidence they are interchangeable.

## Minimap: existing renderer, original compass policy

Use the settings fork instead of installing upstream Dragon's Eye alongside it. Match its 1.7 build with
Infinity UI NG and LMU settings fork; preserve their shipped INIs before applying a deliberate delta.
Source config supports `[Display] uAnchor=1`, `fOffsetXTopRight`/`fOffsetYTopRight`, `fScale`, `uShape`
and `[Controls]` hide/zoom/gamepad bindings. Leave map fog/detection semantics intact and item-marker add-on out.
`Minimap.swf` and `!assets/MinimapArt.swf` are the external art surfaces; first prove configuration with stock
art. Any later reskin must resolve those assets' rights and keep provider contracts intact.

**SkyCRPG UI owns the horizontal compass hide/restore.** The fork's compass ring is not a horizontal-compass
replacement contract. For our first integrated route, disable that ring (`bCompassRing=0`) and restore native
compass when our minimap mode is disabled/unavailable. Hide only the compass clip, never the whole HUD.

Infinity UI exposes movie patch lifecycle messages, and minimap code has an internal `IsReady()`, but this is
not a public readiness export. A loaded DLL or attached clip alone does not prove the map renders. P1-04 must
prove a bounded game-thread adapter can identify the expected movie/clip, observe enable/visibility and
restore compass on missing provider, failed attachment, user hide/disable and HUD reconstruction. If no
reliable supported signal exists, a narrowly scoped GPL provider bridge exposing readiness/toggle is the
bounded permitted-source alternative. Do not call private methods as though they were a stable ABI.
Silent renderer failure detection is unresolved until that proof; always leave a manual reset/native map path.

The selected source calls `report_and_fail` if its required SWF fails to attach. A live adapter cannot recover
from that provider abort. Preflight the full package before enabling its DLL; test complete provider absence
separately from corrupt partial installation. If live failed-attachment recovery is required, the small GPL
bridge must also change that path to inert degradation. Stock 1.7.5 does not already supply that guarantee.

## Release and maintenance implications

Keep provider archives external. Ship original presets, assets and reviewed adapters in separate Camera/UI
archives with disjoint configuration. UI must not depend on TDM/SmoothCam; Camera may use ordinary SkyUI
for provider settings but must not require SkyCRPG UI. No mandatory shared runtime service.

CommonLib, TDM and Grid Inventory have different GPL/exception records; upstream MIT minimap/Infinity/LMU
licenses do not describe the later GPL fork as a whole. Source, binary, art and font rights are separate.
Current fork notices outrank stale historical MIT prose for our choice, without adopting the authors'
explanation of third-party licensing as an independent legal conclusion. Any linked/reused derivative needs
its actual notices/corresponding source; no source/asset is copied here and no original-work license is chosen.

Keep a small manifest of menu winners and provider INI deltas. Pin a release/commit and archive hash before
installation. Avoid private offsets, follow provider updates deliberately and rerun only changed fixtures.
Permission-restricted code/art has a practical alternative here: external install/configuration or original
silhouette/widget. No author contact or permission request is part of this audit.

## Proofs still required

| Owner | Specific proof | Result here |
| --- | --- | --- |
| DYL-150 / P1-01 | SKSE installation, baseline launch, independent minimal DLL builds/loads and artifact hashes on observed Proton host | No compile/load evidence supplied at reconciliation; NOT_RUN in this audit |
| DYL-152 / P1-03 | C1–C3: held pitch/yaw/recenter, collision in tight Nordic room/door/stairs, melee/bow/spell/activation with safe close fallback | NOT_RUN |
| DYL-152 / P1-03 | C4–C5: ten toggle/handoff cycles, interior/exterior transition, emergency reset; enumerate unsupported special states | NOT_RUN |
| DYL-153 / P1-04 | U3/U4: real equip/unequip through grid selection; plain/enchanted/tempered/stolen/quest/stack identity, weight and counts; quantity dialog/search/sort invalidation | NOT_RUN |
| DYL-153 / P1-04 | U1/U2: recruit/dismiss/unload/death; real hotbar count/depletion/native actions, controller feasibility, no input leakage | NOT_RUN; controller unavailable on observed host |
| DYL-153 / P1-04 | U5: exact 1.7 trio loads UI-only, exterior/interior navigation, anchor/scale/shape, minimap present/hidden/absent and native compass recovery | NOT_RUN; readiness adapter feasibility unresolved |
| DYL-153 / P1-04 | U6/M1: pause/focus/controller, 720p/1080p/scaled text, UI-only with ordinary camera and short combined smoke | NOT_RUN; controller hardware needed |
| DYL-154 / P1-05 | Freeze tested architecture, permission scope and package winners after spike evidence | Pending, not started here |

DYL-151 remains open for review of its acceptance criteria. This recommendation does not close Phase 1 or
start either gameplay spike. If the grid or minimap contract fails, report the specific feasibility blocker;
do not drop the capability to pass the gate.
