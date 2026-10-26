# ADR-001: Recommended MVP stack and ownership

Status: **PROPOSED — partial playable evidence; unresolved aiming/input gates.**
Date: 2026-10-25. Research owner: DYL-151 / P1-02. P1-03/P1-04 supply proofs; P1-05 freezes architecture.
This ADR does not amend owner product decisions or select a license for original SkyCRPG work.


## Execution update — 2026-10-26

PR1 and PR2 are merged; DYL-150/151 are Done. Their historical reconciliation notes below describe the earlier audit snapshot, not current acquisition state. The rewritten profile tool now has live load-only proof and an owner-observed vanilla cave/exterior baseline route with captures; see [session evidence](../evidence/p1/session-2026-10-26.md). Pinned Nexus packages are acquired and hashed in the dependency lock. Provider/gameplay acceptance is still separate.

P1-03 implements an original SmoothCam framing preset with no second transform writer. Source inspection finds no held-pitch/recenter config setter; those remain unproven and a small original adapter is only justified after a reproduced provider limitation. P1-04 compiles an original three-column presentation over native SkyUI backing entries/callbacks and a diagnostic HUD against the acquired Nexus6.11 assets. The native compass remains visible while minimap clip attachment is only diagnostic; automatic compass handoff is unimplemented. No renderer, controller or native-action acceptance is inferred from compilation.

Owner gameplay now verifies the live native inventory grid, equip/unequip with unchanged counts/weight, minimap indoors/outdoors, and usable compass fallback. Controller grid navigation failed both the initial and first corrected candidate; the imported SWF class remained unpatched. A changed-hypothesis correction must pass a new game test. Camera movement/melee is usable, ten F8 handoffs and interior/exterior restoration work, and first-person precision aiming works. Top-down third-person bow/spell aiming fails by aiming at the player's feet.

The bounded targeting alternative was audited against pinned TDM2.3.1 source. Lock-on also writes third-person yaw/pitch and does not request SmoothCam camera ownership (its SmoothCam calls manage crosshair ownership). Therefore enabling TDM lock alongside SmoothCam would violate this ADR. Keep lock disabled. **Recommendation:** reuse SmoothCam configuration/obstruction handling and proven native POV fallback; investigate a small original adapter with explicit exclusive camera handoff for a later targeting mode. This adapter is not implemented or proven here. An exclusive custom camera controller is not justified by this evidence. Automatic compass replacement and complete follower/hotbar interfaces also remain open; no core requirement is removed.

These are candidates under review, not a contract freeze. No original-work license, runtime change, inventory-capacity change or broad custom camera/backend is selected. DYL-154 and Phase2 remain gated.

## Context

Camera and UI must install independently while sharing one real executable baseline. Reuse existing
movement, menu and local-map systems; preserve action combat and native inventory weight/instance rules.
Candidate names are not evidence of a compatible API, appropriate mechanics or modification permission.
The full comparison is [REUSE_RESEARCH](../REUSE_RESEARCH.md), with exact releases/commits in
[dependency-lock](../compatibility/dependency-lock.md) and [primary evidence](../evidence/p1/reuse-audit.md).

## Recommended decision

| Boundary | Recommendation |
| --- | --- |
| Target runtime/host | **Steam Skyrim 1.7.104.0**, owner's Ubuntu 26.04.1/Wayland, Proton Experimental `experimental-11.0-20261014`, RTX 3080 10 GB. No other supported runtime/host claimed. |
| Common external prerequisites | **SKSE 2.3.1 Steam**; **Address Library 13** for feature providers; **SkyUI Community 6.11** for menus/configuration. DYL-150's plain SKSE skeletons do not require Address Library/CommonLib. |
| Camera package | **SmoothCam 1.8.1**, its AE 1.7.99+ DLL, as sole third-person transform writer. **TDM 2.3.1** for directional movement, **MCM Helper 1.6.3** for configuration. Original preset plus bounded pitch/input/handoff adapter if needed. |
| UI rendering/inventory | **Native InventoryMenu / Scaleform AS2, SkyUI-dependent player-grid patch** retaining native list selection/actions, pause/dialogs/search/sort. Original uniform-grid layout and equipment-slot silhouette. |
| HUD/follower | External **Party Sheet 3.7** configured for player and one vanilla follower; original neutral silhouette. If its external configuration fails U1, original narrow observer/widget, no follower framework. |
| Hotbar | Native favorites/hotkeys plus original eight-slot display/controller selector. Use actual item extra data and game-thread equip/use actions; no spell automation. |
| Minimap | **Dragon's Eye settings fork 1.7.5 + Infinity UI NG 2.1.3 + LMU settings fork 1.3.4**, each matched to Skyrim 1.7. Existing renderer and stock art first; optional framework and item-marker add-on omitted. |
| Native build library, only if needed | **alandtse/CommonLibSSE-NG 7.2.0**, commit `7a60f4de794095d7b0f8928d1b930a52e9a7da83`; keep DYL-150's Windows x64 GitHub Actions route. No added toolchain scaffolding in this PR. |

Reasons: the observed executable has plausible exact-provider candidates; SmoothCam already has collision,
framing and raycast aiming; TDM provides directional action movement; real SkyUI menu callbacks preserve
more inventory semantics than rebuilding its transaction backend; minimap rendering already exists.
The remaining original work is bounded presentation/input/adapter code, not four more phases of discovery.

## Ownership and failover proposal

- **One camera writer.** SmoothCam writes normal third-person transforms. TDM camera auto-adjust and target
  lock are off in the first profile. SkyCRPG owns intended pitch/input policy and provider config. Suspend
  overhead on precision aiming, menus/dialogue/loading/death and unsupported locomotion/scripted states;
  native/provider view takes precedence. Retain collision and emergency reset. If this cannot meet C1–C5,
  evaluate one exclusive original controller after P1-03; never force writes past a refused ownership request.
- **One native inventory authority.** The grid reuses engine list entries and delegates; selection follows
  the live backing entry through filtering/sort and quantity dialogs. No copied item database or base-FormID
  shortcut. Close/load/invalidation clears pending actions. Custom view is paused with native InventoryMenu;
  containers, barter, crafting and follower trade remain native/SkyUI.
- **One action consumer.** Gameplay, grid, modal, favorites or other menu owns focus at a time. Hotbar HUD
  activation is blocked by menus; controller selector dispatch is original glue, not an assumed global
  GameDelegate API. Inventory input never also moves camera or activates a slot.
- **One compass owner.** SkyCRPG UI hides/restores the native horizontal compass after replacement readiness
  is proved. Disable provider compass ring in this route. DEM has no verified public readiness export;
  the IUI lifecycle/clip adapter must be proved. A narrow GPL readiness bridge is the bounded alternative.
  Required SWF preflight is essential: the stock provider can abort startup on missing attachment, beyond
  any live compass adapter's recovery. Do not promise corrupt-install recovery without addressing that path.
- **Two independent packages.** Camera and UI own disjoint configs/logs/artifacts, neither requires the other.
  UI-only has ordinary camera and no mandatory TDM/SmoothCam. Camera-only has ordinary UI plus its provider
  settings prerequisites. External mods remain external, with exact file winners recorded by the installer.

## Alternatives and cost

| Alternative | Disposition / reason |
| --- | --- |
| Grid Inventory `c0d8718` | Rejected: physical capacity/pickup gates, weight boosts, bags/gold and broader menu takeover conflict with native-weight/player-only baseline. Removing them is not a skin. |
| Inventory Reforged 1.5.0 | Reference only: useful equipment display but no uniform grid. A second InventoryMenu replacement adds merge/maintenance work; original silhouette is cheaper for MVP. |
| Original ImGui inventory | One fallback if native menu patch fails. Requires new transaction identity, pause/focus, controller and render-hook work; higher risk/cost than reusing native menu. |
| Glasswork Favorites 1.0.6 / 1.7 Test | Reference-only hotbar alternative. Exact 1.7 build remains a test file and broader casting/wheel behavior must be disabled/verified. Native backend already supplies the required action semantics. |
| Copy/fork Party Sheet | No verified modification/source grant. External configuration or original one-follower widget is practical; no portraits imported. |
| Original camera controller now | Deferred: it inherits collision/lifecycle burden SmoothCam already addresses. Only justified by a concrete P1-03 failure and exclusive ownership. |
| Upstream map trio / new minimap | Upstream 2.0.3/3.1.0 downloads do not supply selected 1.7 lines; no reason to write another renderer. GPL settings forks remain external. |
| Change/downgrade runtime | Not selected: actual Steam 1.7.104 installation is available. Older 1.6.1170 provider routes are comparative evidence, not authority to change the owner's game. |

## Distribution consequences

No third-party implementation/assets are incorporated by this audit. SmoothCam/Party Sheet archives have
restricted modification/upload terms. SkyUI explicitly permits credited dependency-requiring skins, with
separate icon-author restrictions; do not infer a blanket MIT grant. TDM/CommonLib/selected map forks carry
GPL/exception or GPL notices as recorded, and upstream MIT notices must remain with covered derivatives.
Source and packaged art/fonts/portraits have separate rights. LMU's optional frame provenance needs archive
review; stock external art/configuration avoids importing it into SkyCRPG.

Source reuse may impose notices, license compatibility and corresponding-source requirements on a linked
or modified derivative. Original-work licensing D015 remains with the owner; do not start a broad fork or
publish an incompatible derivative based on this ADR. A deployable route exists using external providers,
original assets/configuration and properly attributed permitted menu patches, subject to the listed proofs
and actual archive checks. No owner approval request or mod-author contact was made by this audit.

## Path through Phase 4

| Phase | Concrete implementation boundary / gate |
| --- | --- |
| Phase 1 | P1-01 obtains baseline/independent loads; P1-03 proves held camera/aim/collision; P1-04 proves native grid equip, map/compass and provider/input feasibility. Pin acquired archive hashes. P1-05 chooses tested contracts, not this desk audit. |
| Phase 2 | Tune Camera preset and only missing held-pitch/state adapter; implement configured player/follower HUD and original hotbar. Keep one writer and native actions. |
| Phase 3 | Complete player grid layout, categories/search/sort/details/equipment slots; test instance/quantity/quest rules, minimap readiness/disable recovery and combined route. Do not add container/barter redesign or Tetris capacity. |
| Phase 4 | Separate package manifests, attribution/source obligations, removal/reset checks, recorded layout/input/performance fixtures and owner playtest. External dependency downloads stay external. |

The grid focus/clip mapping and minimap readiness are the largest feasibility risks. Native inventory actions
are real, but their preservation under a new grid is **NOT_RUN**. Held pitch is **NOT_RUN**, and a camera
offset preset alone does not establish it. Safe fallback views count as intended behavior; omission of the
required grid/minimap or controller support does not.

## Environment reconciliation and remaining gates

DYL-150 [snapshot `cd49b3c`](https://github.com/Sonoran-Solutions/SkyCRPG/blob/cd49b3cfc73278350342db4ae7a27c3b5e6a9644/docs/evidence/p1/environment.md)
supplies actual executable/storefront/host/build availability, not game-load results. The preliminary
recommendation (`8fa4126`, DYL-151 comment) was published before final reconciliation; the follow-up retained
1.7.104 and corrected skeleton dependencies to SKSE only. That lane owns deployment/build code, environment,
runtime-matrix, BUILD_AND_TEST and shared STATUS; this audit changes none of them.

**Selection status:** desk recommendation and runtime narrowing complete; **tested stack/architecture NOT
FROZEN**. No missing executable fact blocks selection now. Missing evidence: acquired package hashes/license
contents, P1-01 loader/baseline logs, P1-03 camera proof, P1-04 native grid/compass/provider proof, and controller
hardware for U6. Initial snapshot has no installed SKSE/providers; DYL-150 records its acquisition blocker.

DYL-151 remains open pending review/acceptance. Do not merge this as evidence of a playable release, mark
Phase 1 complete, or start the spike tickets on the strength of documentary compatibility alone. Reconcile
actual P1-01 evidence and record readiness in their assigned tickets before execution.
