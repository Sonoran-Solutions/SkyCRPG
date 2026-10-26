# DYL-155 / P1-06 — Phase 1 acceptance gate

Gate accepted by the owner, 2026-10-26. Base main `ea054c78b42ccf051598d34af4f795dd7e18319f` includes merged PRs 1–5. DYL-154 is Done after senior review and owner acceptance. This gate reuses existing proofs and runs only the two missing critical feasibility scenarios. No Phase 2 implementation, provider upgrade or native/SWF rebuild.

**Gate disposition: GO — Phase 1 accepted by the owner on 2026-10-26.** All 21 Phase 1 feasibility criteria have sufficient bounded evidence, using the demonstrated native first-person precision fallback and the existing pitch-only owner defer. Both missing critical scenarios now have fresh owner-observed PASS results. **Phase 1 is complete within this bounded feasibility scope; Phase 2 has not started.** This acceptance is not a full MVP acceptance or a waiver of remaining failures.

Owner instruction: “Record my acceptance of phase 1 GO then approved to merge”. This explicitly accepts the evidence and disposition submitted at `0929e0e1dbd6aa7d24000c5ff666246b4fca007c` and authorizes merging PR6 with this acceptance record. It records owner acceptance, not an additional senior review or a new gameplay test.

## Exact baseline and reused evidence

Steam Skyrim 1.7.104.0, SKSE 2.3.1, Address Library 13; Ubuntu 26.04.1, Proton Experimental 11 `experimental-11.0-20261014-x86_64`, RTX 3080. Exe SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Before this gate's deployment, executable hash and installed Proton version were rechecked, Steam was running, and no active profile or Skyrim process existed.

Camera: SmoothCam 1.8.1 AE1.7.99+, TDM 2.3.1 movement with competing yaw/lock disabled, MCM Helper 1.6.3, SkyUI Community6.11. UI: SkyUI 6.11 + DEM settings fork 1.7.5/Infinity UI NG 2.1.3/LMU settings fork 1.3.4 Common + Line-17. The absence case intentionally omits all three map packages and retains unchanged common/UI files. No substitute provider is installed. Party Sheet remains inspection-only.

Exact external archive/member/source pins: [runtime matrix](../../compatibility/runtime-matrix.md), [dependency lock](../../compatibility/dependency-lock.md), [tested profiles](tested-artifacts.json), [native inputs](native-package-inputs.json). Loaded Camera DLL source `cc8a4dcde7b817d392bc66c9d2dc93cfb1beac62`; UI DLL source `d1f6193681df687860681881de7425a5e00ebbd1`; functional UI patch source `1be22bbbb0b0519e0b3c876ce4929a395541161d`. These differ from documentation heads and are not rebuilt for the gate.

| Evidence key | Source/run/artifact | Scope |
| --- | --- | --- |
| F | [Foundation/session](session-2026-10-26.md), runs 065153/070429, original independent loads in [environment](environment.md) | Real profile deploy/load/undeploy, copied vanilla Helgen interior/exterior baseline, independent native loads. Original2021 modded fixture failed and was replaced; no relabeling. |
| C | [Camera](camera-spike.md), owner run 073401 and combined 075556 | Exploration/qualified melee, first-person bow/spell fallback, activation, ten F8 pairs and usable transition. Camera artifact `1f5535e69c40975f7fd85c7a239bacced75af8b9e95d1a83d199be777327798c`. |
| U | [UI](ui-spike.md), owner071034 then corrected075205 | Native live grid/action/count/weight, map indoors/outdoors and hidden-map compass; final controller routing/action regression. Final UI artifact `1e25367e992e029be27738c707a5b6424fe5bbd5e81d1d7ccab944b09fc7af5e`. Earlier two controller failures retained. |
| X | [Combined/session](session-2026-10-26.md),075556 | All8 DLL startup + owner checkpoint/captures; artifact `8a31a81a18b196cf51431f570d609f8b0cf0375f967299ac75a22488432ef307`. Not a complete combined MVP matrix. |
| A | [Architecture](../../ARCHITECTURE.md), [ADR](../../decisions/ADR-001-stack.md), [contract audit](contract-freeze.md) | Reviewed ownership, 17 acceptance IDs, 13 camera states, package independence and exact support classification. |
| O | Fresh obstruction run 20261026T100826 | Same165 installed game files + profile INI as prior Camera artifact; owner confirms all requested obstruction areas usable without needing F8 to unstick; clean exit. |
| N | Fresh UI-only absent-provider route | Controlled staging omits 38 map-provider paths; 134 retained game files match final UI profile. Run20261026T101713: owner confirms save load, compass, movement/navigation, inventory, Magic and journal all work without failures; clean exit under the same 1200s watchdog. |

Senior review sources: [PR 3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3#issuecomment-6419920725), [PR 4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4#issuecomment-6419920095), [PR 5](https://github.com/Sonoran-Solutions/SkyCRPG/pull/5#issuecomment-6421221642). GitHub merges verified directly. At gate start Linear returned DYL-152 **Done**, DYL-153 **In Progress**, DYL-154 **Done**, DYL-155 **Backlog**; DYL-155 moved In Progress. DYL-152's changed tracker status is not treated as obstruction proof and was not automatically reopened. Gate evidence remains authoritative for that obligation.

## All Phase 1 acceptance criteria

The ordinal IDs below follow the unchanged checkboxes in [PHASE_1](../../phases/PHASE_1.md). PASS is bounded feasibility/contract acceptance, not full MVP feature completion. Existing evidence is reused rather than replayed.

| Criterion | Result | Evidence / distinction |
| --- | --- | --- |
| P1-01.1 — reproducible separate packages and independent loads | PASS | F; deterministic native CI/package manifests, separate Camera/UI SKSE loads. Full-feature final archives remain later work. |
| P1-01.2 — baseline play and independent logs identify runtime/artifacts | PASS | F; replacement vanilla Helgen route/captures and indexed load/run logs; exact pins retained. Baseline full controller matrix still NOT_RUN. |
| P1-01.3 — tested versus candidate environment distinguished | PASS | Runtime matrix, executable/SKSE/provider hashes and actual host; all other environments NOT_RUN. |
| P1-02.1 — version/source/rationale matrix for MVP components | PASS | Dependency lock/ADR/reuse audit; Party Sheet/API uncertainty explicitly bounded. |
| P1-02.2 — exactly one camera writer | PASS | C/A; SmoothCam normal third person, native first person/special states; TDM yaw/lock disabled. |
| P1-02.3 — compatible map/grid route and explicit rights/API boundaries | PASS | U/X, dependency lock; live map/native actions; no inferred public readiness/follower API. |
| P1-02.4 — no borrowed material shipped merely because repository public | PASS | External providers/derivative SWFs stay external; original licensing separately authorized under D015, third-party rights unchanged. Public distribution not performed. |
| P1-03.1 — controllable Camera-only movement/melee and accurate aiming fallback | PASS | C owner reports qualified melee usable and first-person bow/spell precision works. Overhead third-person ranged aim remains FAIL. |
| P1-03.2 — walls/floors obstruct; difficult interiors recover without stuck camera | PASS | O owner confirms all requested corridor/low-ceiling/doorway/stair/open-room checks, no manual F8 recovery needed; screenshot + exact-artifact logs. Bounded feasibility, not complete C2 coverage. |
| P1-03.3 — ten toggles and interior/exterior transition preserve usability | PASS | C, ten F8 pairs/transition owner-confirmed. Pitch persistence FAIL with explicit narrow owner defer; zoom/control retained. |
| P1-03.4 — explicit unimplemented states and chosen ownership route | PASS | C/A; small future adapter unimplemented; no simultaneous TDM/SmoothCam ownership. |
| P1-04.1 — UI-only with ordinary third-person camera | PASS | U final075205 logs exclude Camera/SmoothCam/TDM; owner route demonstrates live UI. |
| P1-04.2 — intended native item action once with state invariants | PASS | U owner plain-weapon equip/unequip/count/weight checks, repeated after controller fix. Exceptional instance fixture remains later NOT_RUN. |
| P1-04.3 — map visible and disable/provider-loss navigation safe | PASS | U covers map visible inside/outside + manually hidden compass fallback; N adds clean whole-provider absence with native navigation and menus usable. Automatic compass replacement remains later work. |
| P1-04.4 — evidence-based renderer/input/pause route and unavailable follower fallback | PASS | U/A; native paused menus, final controller correction, explicit unavailable follower state. Complete follower/hotbar/modal UX is later implementation. |
| P1-05.1 — selected/rejected stack, licenses and exact baseline in ADR | PASS | A, PR 5 senior review/owner acceptance and merge. D015 subsequently resolved by current owner direction; derivative-rights work remains. |
| P1-05.2 — every transition owner/exit; no durable raw pointer IDs | PASS | A,13-state camera table plus native inventory/HUD/menu lifecycles. |
| P1-05.3 — all MVP IDs map to tests, decisions have owners | PASS | A,17-ID traceability and assigned follow-ups; no future ticket converts failure to PASS. |
| P1-06.1 — independent prototypes run | PASS | F/C/U/X existing actual game evidence, not merely builds. |
| P1-06.2 — overhead interior/aiming recovery, native item action and map evidenced | PASS | C/U prove native precision fallback, item action and map; O/N supply the missing obstruction and provider-absence proofs. |
| P1-06.3 — stack/contracts/limitations explicit, no hidden critical gap | PASS | This ledger and A carry all critical and later-scope gaps separately. O/N now have scoped runtime evidence; the owner explicitly accepted this GO disposition on 2026-10-26. |

## Critical test A — obstruction

Purpose: missing Phase 1 feasibility only; no comprehensive DYL-157 implementation. Profile Camera-only, unchanged artifact above, 20-minute limit with one tagged Skyrim process. Route: copied Prisoner/Helgen exterior → `coc BleakFallsBarrow01` → entrance room/stairs/corridor/low ceiling/narrow doorway → larger room/return. Console relocation only; normal collision and player camera retained. Re-establish angle after loading because pitch defect is already known. Distinguish automatic close-view recovery from manual F8/native fallback.

Required observations: walls/floors block camera, player visible/controllable, no geometry clipping to preserve distance, usable close/native fallback, open-space recovery, no trapped/rapid oscillation, F8 available. Result **PASS (owner-observed bounded route)**. Owner first reported the first Bleak Falls room looked good, then explicitly confirmed: “Tested all parts, did not need F8 to unstuck it” in response to the complete corridor/ceiling/doorway/stair/open-room, geometry/control/recovery checklist. No stuck/oscillating/clipping failure was reported; no new adjustment or retry was needed. [Capture](captures/gate-20261026-bleak-falls-room.png) shows the elevated Nordic interior view; it does not independently prove dynamic collision. [Observation record](gate-20261026/camera-owner-observations.json) preserves exact scope. A fresh F8 press was not separately confirmed; reuse the existing ten-pair PASS rather than invent one. Comprehensive C2 implementation/coverage stays DYL-157.

## Critical test B — absent versus incomplete minimap

Case A deliberately removes **the entire selected DEM/Infinity UI NG/LMU external set**, including configuration/assets/notices, from a new external UI staging tree. All retained SKSE/Address Library/SkyUI/UI DLL and final generated SWFs match the final075205 artifact. The installed profile manager's normal path/hash/runtime checks remain unchanged; no plugin binary is patched and no corrupt provider DLL is launched. The provider-specific required-file check is not a requirement to enable an intentionally absent optional integration.

Required fresh observations: clean startup, native compass/navigation, unaffected inventory/native menus, safe UI continuation/unavailable-map state, no stale compass suppression. Case A result **PASS (owner-observed bounded route)**. Exact owner report: “The save loaded, native compass is visible, all movement, navigation, inventory, magic and journal functions worked fine with no failures.” The game then exited normally, with no load errors or leftover processes. [Owner record](gate-20261026/ui-owner-observations.json) and [run/log hashes](gate-20261026/ui-run.json) preserve the result; no new screenshot was supplied for this route. This proves safe whole-provider absence for this profile, not automatic compass suppression/recovery that has not been implemented. Case B **PASS (expected preflight refusal only)**: an external fixture contains the three exact provider DLLs and MinimapArt.swf but omits required Minimap.swf; unchanged preflight exits1 with BLOCKED before any launch. [Negative check](gate-20261026/preflight-negative.log) and [bounded job](gate-20261026/preflight-negative-job.json) are retained. Full incompatible-binary/crash recovery is not claimed or deliberately provoked. DYL-166 still owns automatic compass replacement/readiness and comprehensive recovery.

## Aiming and owner defer

For bounded Phase 1 feasibility, the existing owner-confirmed first-person bow/spell precision fallback, usable melee/activation and reliable return controls demonstrate a viable implementation direction with one camera owner. Reuse that evidence; no new aim test solely to obtain another PASS. This does not prove an automatic aiming adapter, target-lock handoff, every spell type or full C3 encounter acceptance.

| Behavior | Result / disposition |
| --- | --- |
| Top-down third-person bow/spell aim | FAIL — projectiles at feet; DYL-158. Not repaired or waived as working. |
| Native first-person precision fallback | PASS — owner-confirmed; native engine owns first person. Viable chosen Phase 1 fallback. |
| Qualified melee / activation | PASS within recorded owner route. |
| TDM lock with simultaneous SmoothCam transforms | BLOCKED — source shows competing writes; keep disabled. |
| Load-transition pitch persistence | FAIL — explicit owner defer to DYL-159, nonblocking for bounded prototypes only. |
| Zoom persistence / usable control | PASS within recorded transition route. |

No pitch defer is extended to obstruction, aiming fallback or provider absence.

## Broader implementation obligations, not new Phase 1 failures

Held pitch/recenter/persistence and complete state handling: DYL-156/159. Comprehensive obstruction/aiming: DYL-157/158. Full HUD/eight-slot bar/follower: DYL-160/161/165. Distinct same-base inventory traits, native action types and 19+ row-aligned/500-entry grid: DYL-163/164. Automatic compass replacement/readiness: DYL-166. Modal/menu/layout/controller/lifecycle matrix: DYL-167/168. Final packaging/rights/performance/support matrix: DYL-170–175. These are bounded future requirements; the [MVP acceptance matrix](../../compatibility/acceptance-matrix.md) retains their full-criterion statuses.

## Preservation, jobs and next work

Before deployment,185 existing original/copied save/co-save files were hashed, original INIs/Plugins.txt were backed up externally, and disposable Camera/UI save folders backed up against autosave changes. The memory watchdog aborts owned processes below 4GiB available RAM or memory full avg10 above 10%; each route has a 1200s timeout, PID/run tag, log and cleanup plan. No heavy builds run alongside Skyrim. Final restoration **PASS**: all 185 pre-existing save/co-save hashes unchanged; original Skyrim.INI, SkyrimPrefs.ini and Plugins.txt restored to their pre-test hashes; no active deployment, owned deployed paths or Skyrim processes remain. [Restoration and telemetry summary](gate-20261026/restoration.json) records the checks. Isolated test saves, logs and external staging remain available for reproduction. Minimum available RAM was 16,760,348 KiB (Camera) and 16,825,544 KiB (UI), memory full avg10 stayed 0, and neither watchdog intervened. This is host-safety telemetry, not a performance acceptance test.

No new owner scope ruling is required: obstruction and provider absence were demonstrated, first-person precision is the proven fallback, and the pitch-only defer is unchanged. The owner has accepted this gate and authorized PR6 to merge. DYL-155 is Done; DYL-153 is reconciled to Done on the strength of its completed P1-04 evidence and this explicit acceptance, not merely PR4 merging. DYL-152 remains Done. DYL-156 and DYL-160 are now ready to be assigned in parallel with separate ownership under the [ready-to-use handoff prompts](phase2-handoff.md). Both remain Backlog; neither implementation is started.

## Fresh runtime records and reproduction

Camera run 100826: QUIT/exit 0, load problems none, no leftover/game processes, no watchdog intervention. All185 pre-existing save/co-save hashes unchanged at camera cleanup, original INIs/plugin list restored before switching profiles. Native/provider logs, artifact maps, owner observations and capture provenance are retained in [gate evidence](gate-20261026/camera-run.json). Whole-map-absence artifact (135 files including profile INI): `cdccba7819106c8ce8e065f363d88279ee9b65cbc7ce55f46b2278345fef98ea`; [exact file map and 38 omitted paths](gate-20261026/ui-absence-artifact.json).


UI run 101713: QUIT/exit 0, no load problems, no leftover/game processes and no watchdog intervention. Retained UI files are byte-identical to the final UI candidate; no Camera providers are installed. The normal provider-present combined artifact is unchanged, so its existing smoke is reused rather than repeated.

To reproduce, use the unchanged profile tool and external prerequisites in [BUILD_AND_TEST](../../BUILD_AND_TEST.md), the exact pins above and copied Helgen fixtures. First verify no active game/profile, back up original configuration and hash saves. Deploy the recorded Camera profile, enable its recorded plugins, and follow test A; collect logs and undeploy/restore before switching. For test B stage the final UI profile minus exactly the 38 paths in the absence artifact's removed-path list, retain all other hashes, deploy UI-only with its SkyUI plugin entry, and follow the native-menu/navigation checklist. Do not enable partial map packages. Run each under a tracked 1200-second process deadline and memory guard; stop only owned run-tag processes if necessary. After each run compare deployed hashes, undeploy, restore original configs and recheck saves. No third-party archive, game binary, proprietary SWF or save is committed here.

[SHA-256 evidence index](gate-20261026/SHA256SUMS) covers the fresh runtime records, owner reports, artifact manifests, restoration, focused validation and camera capture. Historical failures and earlier reports remain intact; their former NOT_RUN cells are superseded only within the scope of this gate.

## Focused validation

Document/link/ticket checks, all 21 Phase 1 ledger rows, all 17 MVP mappings, the 13 camera-state contracts, native package disjointness, profile overlap, canonical artifact and captured-log hashes, preservation records and license scope passed. The [validation report](gate-20261026/validation.json) records exact counts and findings; the bounded check job is [recorded](gate-20261026/validation-job.json). No executable repository tooling changed, so native builds and the 22 profile-tool fixture tests were not rerun. Both CI workflows filter out this documentation/licensing/evidence change; no new CI pass is claimed.

Raw Windows logs retain their original CRLF bytes and hashes; whitespace validation uses `core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol`. Camera provider logs retain the already-observed missing optional integration API messages and MCM Helper keybind/MergeMapper diagnostics (also present in run073401/combined075556). SKSE reported no plugin-load failures and the owner reported no route failures; this does not certify those absent integrations or MCM binding features.
