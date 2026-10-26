# ADR-001: Camera, UI and independent module architecture

**Technical route: selected from Phase 1 evidence. DYL-154 contract accepted after senior review and merged in PR5.** Updated 2026-10-26; supersedes the DYL-151 desk proposal. This accepts an implementation direction, not all gameplay acceptance. The subsequent owner licensing ruling is recorded below. Normative ownership/lifecycle rules are in [ARCHITECTURE](../ARCHITECTURE.md); every MVP requirement is mapped in the [acceptance matrix](../compatibility/acceptance-matrix.md).

## Evidence and decision

PR [3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3) and [4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4) are merged into main `87d91626815dca145c6a876664f1ac7d0ffeafdf`. Senior code review accepted the bounded source/configuration spikes in [Camera](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3#issuecomment-6419920725) and [UI](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4#issuecomment-6419920095) issue comments (not formal submitted GitHub review objects). Review explicitly preserves gameplay gaps. Exact reviewed heads, source/artifact hashes and limited host support are in the [runtime matrix](../compatibility/runtime-matrix.md).

| Boundary | Selected route | Evidence and limit |
| --- | --- | --- |
| Only tested baseline | Steam Skyrim1.7.104.0; SKSE2.3.1; Address Library13; Ubuntu26.04.1; Proton Experimental11 `experimental-11.0-20261014-x86_64`; RTX3080 | Camera-only, UI-only and combined bounded routes. No native Windows or other runtime/host acceptance. Windows x64 is the plugin ABI, not a tested native Windows gameplay host. |
| Camera | SmoothCam1.8.1 AE1.7.99+ sole normal third-person transform writer; TDM2.3.1 directional movement; MCM Helper1.6.3; SkyUI6.11 for configuration | Original provider preset plus native POV precision fallback works within recorded limits. TDM yaw adjustment/target lock remain disabled. |
| Future Camera adapter | Small original policy/lifecycle adapter for held pitch, restoration and exclusive precision/targeting handoff | **Not implemented**. Must use validated provider interfaces or exclusive ownership; never raw concurrent transform writes. DYL-156/158/159. |
| UI/inventory | SkyUI Community6.11, native InventoryMenu/Scaleform, original player-grid presentation and bounded input routing | Live native inventory and single-action equip/unequip PASS; preserve backing entry/extra-data identity, native counts/weight/pause. Both imported/main InventoryLists classes are patched. No independent inventory database or Tetris capacity. |
| HUD/follower | Original HUD presentation over native state; Party Sheet3.7 optional external candidate only | Player-resource diagnostic works; follower API/data unverified. Neutral silhouette/unavailable state is safe fallback; working follower still required under DYL-165. |
| Hotbar | Future eight-slot presentation/input adapter over native favorites/equipment/use | Native hotkeys preserved; global HUD action API not assumed. No cooldown/combat replacement or duplicate dispatch. DYL-161. |
| Minimap | Dragon's Eye settings fork1.7.5 + Infinity UI NG2.1.3 + Local Map Upgrade settings fork1.3.4, Common + Line-17 | Inside/outside rendering PASS. UI owns future readiness-gated native compass replacement; current compass remains visible. No verified public readiness API. DYL-166. |
| Native build dependency if needed | CommonLibSSE-NG7.2.0, source `7a60f4de794095d7b0f8928d1b930a52e9a7da83` | Conditional, not used/required by current load-only skeletons. Adapter ticket must pin build inputs and satisfy license obligations before adopting. |
| Packages | Independent Camera and UI archives, no shared runtime service | Current native manifests are disjoint; generated provider configs/UI patches still need final package integration. External prerequisites remain separately installed. DYL-171. |

These choices preserve action combat, native item rules and independent installation. Reusing native menu actions avoids a second transaction backend; retaining SmoothCam avoids recreating collision and every camera state. A known aiming limitation justifies a bounded handoff adapter investigation, not a large new camera system.

## Required ownership and recovery

- Camera owns framing, mode preferences, camera input, provider configuration and recovery; SmoothCam owns normal third-person transforms. Engine owns first-person/scripted states. UI never writes transforms. TDM lock-on source writes yaw/pitch without SmoothCam camera-control grant; crosshair ownership is insufficient. Lock stays disabled until an exclusive release/grant protocol is proven.
- UI owns presentation and input routing only. Engine/native menu owns mutations, restrictions and pause. Resolve current native instance/stack immediately before one action, observe the result, refresh and invalidate on state changes. No durable raw pointer, name, row index or base-FormID shortcut for distinct copies.
- Modal/native menu focus takes input before inventory, selector and gameplay. Each module independently respects native focus; neither requires the other to suppress input. Menu close/load/death clears held state and queued intents without replay.
- UI alone owns its native compass suppression. Hide only with proven replacement readiness; restore its suppression on hide/disable/load/absence/failure. Current clip observation cannot establish healthy rendering. Reject partial provider installs before startup; do not promise a UI callback can recover a provider startup abort.
- Installer/profile integrator owns shared file winners, version checks and backups. Modules own disjoint payloads and declared configuration deltas. Preserve user changes on restoration; remove DLLs/movies only with game closed. No mandatory common service, unowned shared settings, or blanket safe mid-save removal claim.

Detailed state entry/exit/input/restoration tables and the package ownership matrix are normative in [ARCHITECTURE](../ARCHITECTURE.md), avoiding duplicate divergent protocols here.

## Three evidence classifications

| Classification | Disposition |
| --- | --- |
| Tested and working, bounded | Pulled-back exploration/qualified melee; first-person precision; activation; ten F8 pairs; transition usability; live native grid/action/count/weight; corrected controller grid/category navigation; map inside/outside; usable compass while map hidden; separate profiles and combined checkpoint; bounded Nordic obstruction and whole-provider-absence startup/navigation in DYL-155. Each PASS belongs to its recorded run/artifact, not every MVP scenario. |
| Implemented or selected, unverified | Comprehensive obstruction coverage beyond the bounded gate route; full special-state/menu/controller/layout matrix; same-base item traits; follower provider interface; eight-slot bar; automatic compass replacement/readiness; incompatible-provider runtime recovery; final full-feature packaging and performance. Selected future code is not implemented merely because this ADR specifies it. |
| Known failing or explicitly deferred | Top-down third-person bow/spell aim **FAIL** → DYL-158 (first-person fallback PASS). Pitch after load **FAIL**, explicit owner defer for spike PRs → DYL-159. 19+ grid scrolling source concern → DYL-164, gameplay NOT_RUN. Current duplicate compass does not satisfy U5. Settings-only TDM/SmoothCam target-lock coexistence **BLOCKED** by competing writers. |

Owner pitch defer does not waive obstruction, absent-provider startup, controller coverage or any other requirement. DYL-155 now supplies bounded obstruction and whole-provider-absence PASS evidence in the [gate](../evidence/p1/gate.md), with GO explicitly accepted by the owner on 2026-10-26; full runtime/performance acceptance remains DYL-173/175. See [camera](../evidence/p1/camera-spike.md), [UI](../evidence/p1/ui-spike.md), [session](../evidence/p1/session-2026-10-26.md) and [traceability](../compatibility/acceptance-matrix.md).

## Alternatives and reasons

| Alternative | Decision |
| --- | --- |
| Configuration-only camera for all requirements | Insufficient: ranged aim at feet and pitch reset reproduced. Keep proven configuration, add only missing policy/handoff work. |
| Enable TDM lock with SmoothCam unchanged | Rejected: two transform writers; zero interpolation does not remove writes. Existing provider targeting is reusable only after exclusive ownership is demonstrated. |
| Exclusive large original camera controller | Not selected: no evidence justifies inheriting collision/lifecycle maintenance. Escalate a concrete reproduced provider limitation before replacement. |
| Original ImGui inventory/backend | Not selected: native Scaleform route proved real actions/controller correction. New transaction/focus/pause/render backend adds risk without current need. |
| Grid Inventory `c0d8718` | Rejected for this MVP: physical capacity/pickup gates, bags/weight changes and broader menu takeover conflict with native-weight scope. |
| Inventory Reforged1.5.0 | Reference only; equipment display is useful, but no uniform grid and a second InventoryMenu winner creates conflicts. |
| Glasswork Favorites1.0.6 / 1.7 Test | Reference-only alternative; no need for an additional hotbar provider or expanded casting behavior. |
| Copy/fork Party Sheet | No verified source/modification grant or actor-data API. External configuration or narrow original observer remains bounded future implementation. |
| New minimap renderer / older upstream map binaries | Not selected: selected fork trio rendered in the actual host; older upstream lines do not supply the selected1.7 payloads. |
| Change/downgrade runtime | Not selected: actual Steam1.7.104 works for bounded prototypes. Other platforms/runtimes require new evidence and owner decision if support scope changes. |

Primary-source audit and comparative pins remain in [REUSE_RESEARCH](../REUSE_RESEARCH.md) and [dependency-lock](../compatibility/dependency-lock.md). No repeated research or provider upgrade is needed for this decision.

## Rights, distribution and remaining owner decisions

Keep exact archive/source pins and material-specific [permission obligations](../compatibility/dependency-lock.md#permissiondistribution-lock). External providers are not bundled. Credit SkyUI Team/Community for dependency-requiring patches; generated local SWFs contain external art/fonts/base material and do not imply general redistribution permission. SmoothCam API-header copying permission is narrowly scoped, not a fork license. TDM/CommonLib GPL exceptions and the map forks' GPL/corresponding-source plus retained MIT/third-party notices must be reviewed for the exact derivative. Party Sheet and optional art/portraits are not copied. Source rights and asset rights are separate.

**D015 is resolved by owner instruction2026-10-26:** free reuse of original code/art with devils_17 attribution, implemented as MIT source/config/docs and CC BY4.0 original art. See [LICENSING](../LICENSING.md). DYL-171/174 still own third-party derivative permissions/notices before distribution. A future incompatible dependency obligation, large provider fork, runtime change, inventory-mechanics change or removed requirement needs an owner ruling with concrete alternatives. No such scope change is necessary to freeze these contracts now.

DYL-154 is accepted and merged. DYL-155 [Phase 1 gate](../evidence/p1/gate.md) is separately accepted GO by the owner on 2026-10-26. This is bounded feasibility acceptance; Phase 2 has not started.
