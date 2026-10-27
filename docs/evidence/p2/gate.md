# DYL-162 / P2-07 — Phase 2 gate

**GO — owner decision 2026-10-26 (with documented deferrals).** Evidence below is scoped to the tested artifacts; the merged-main regression remains NOT_RUN. The checkbox ordinals follow [Phase 2](../../phases/PHASE_2.md). A bounded owner report is not full criterion or release acceptance; tests alone do not establish runtime behavior.

## Evidence identities

All hashes below are SHA-256. Runtime evidence uses Steam Skyrim 1.7.104.0, SKSE 2.3.1, Address Library 13, Ubuntu 26.04.1, Proton Experimental 11 and RTX 3080, with pinned providers described in the leaf records and [runtime matrix](../../compatibility/runtime-matrix.md). Build IDs refer to GitHub Actions; runtime IDs are distinct. Leaf evidence is present on this branch. Merged commits identify integration history, not additional runtime tests.

| Key / leaf evidence | Tested source, build, artifact and runtime identity |
| --- | --- |
| A — [camera controls](camera-controls.md) | Camera `8a681390db5b36c2b6fb4fcb056c0abc0e085a35`; [build 40381045722](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40381045722) (package PASS; later diagnostics upload FAIL); DLL `aedc9d53e7c7d85719fb600fe8b71129cc9aab1f67f92cffe10ea499f106d024`, ZIP `50a23e3b768ff0d5a949b88b0f97f25787f64b2155d42da675338773e7309e76`; camera-only run `ad599062e2af4187a7212b645f41f010`. Later complete workflow [40385191497](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40385191497) built `37008f2efc9850c057149f42c1e16bdce1da7d1e`, not the runtime artifact. |
| B — [combat and activation](combat-and-activation.md) | Camera `68b9bd2ecb1c36a01813f0a9f3f30211b573d72e`; [build 40394853951](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40394853951); DLL `e82d56f53a30cfc3deaac7428cd4bb8af3321cd00d196a426416157772247330`, ZIP `e2dc1a2ce35f177c59b65f8e613e8a2d2924aee1b2aad3a218b24b657ff73619`; camera-only run `bc5dc866b61b46cf80e5745742f4cc3d`. Later F6 menu retest: Camera `b33ab252cc1be2331e8055fe79bce525d801750b`, [build 40430617677](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40430617677), DLL `378c2e5d5a7f103453e02a1f80d53184cc015c91dc3153a2814dc3372cc96025`, ZIP `262802e25e1ec82ae3e5bfd67cc5ace982460b7fff1f81a3515751edc91d363f`, combined run `1c303a5f442843e4aa1ce854c9b3fdde`. |
| C — [obstruction](obstruction.md) / [lifecycle](lifecycle.md) | Integration `155a3e3671e708de06cd7f3db10bc36fcfae872c`, combining lifecycle `85f4338` and obstruction `f8ff6ad`; [build 40442507358](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40442507358); DLL `0eeef6bbb748a9a9ddee0aa8a2c882817028a25b9c6871a6c7d9c2ef62c2e5b2`, ZIP `05c369c2eee02c6192e9e13093ee80d92f6270eea139b380c0f4bdd3ee38b833`; combined run `6456672598ca49c684ce42b5cd5a90d0`, obstruction and trace enabled temporarily. Owner reported route passing; process reached its 20-minute timeout, cleanup/restoration PASS. This runtime tested an integration commit, not merged main. Lifecycle merged as #12 at `c1f83e7`; obstruction merged as #10 at `7bad145`, carrying tested `3ec6730` plus the later zoom-tracker fix. See the integration regression below. |
| D — [core HUD](core-hud.md) | UI native `e49fc30aebb008377e9a010e96757444c0f684c6`, [build 40386173282](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40386173282), DLL `29381c08436486fda835206dceebaac73b1902f7f13af9a3f8d25b070d6fe136`. UI-only route 3 SWF source `e2128ab887f961318614fb7a12f2b1ca765dcd82`, hash `eef2936eae0da412ad46f24ff929a1841b2a0c149660cc35750d2de3290700ac`; its runtime ID is not stated in the leaf record. Combined run `52b5f11b872b43c29747541646e08017` uses Camera A and SWF source `2d8896c4e8e91189489e983d94beb3444b4a32ea`, hash `b4552e17f0806848bcbb0a31f09e934e3feffc5a405305ba5707beb6b3215c05`. |
| E — [hotbar](hotbar.md) | UI-only controller: `5b06523d675b5118fef2148b503026ba04fb2b92`, [build 40400878521](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40400878521), DLL `c7196fb511e138eb3b952b4a9114aff714f986f928296d16b67fa0dbff23e0b6`, ZIP `91aefb53b8fd14c849350873856faba84c480d8dbde2c560872b759a226f926d`, SWF `a716d12a9668438f635564352539f1600d5a57c41a9d9c200b45a92c02a1f606`, run `f8167949cb92443e95bd42d5de5ff0cf`. Combined dispatch retest: UI `ba10da6a9d5c9d177f7d61d420f1170ca35cddee`, [build 40438723566](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40438723566), DLL `028c07e7f133c9c23a75f265958c4d0d9634aff5c3e61071aeaacc86482c7b7c`, ZIP `c6caaf66c34b1c050c3d7d046ca35e6d12e637c9c51d819b6f2120717533e87f`, SWF `279f08f2df4649811311f26b6dbcee6b41912365c00611d711ab3cdb1987f03f`, Camera `b33ab25` (B), run `8eb20c9ab8cf4a978fcfbbbfcf8833ec`. Auto-FAV keyboard dispatch is **PASS (owner-reported, bounded)**; native-bound no-duplicate remains NOT_RUN. #11 merged at `644095c`. Native ZIP is not a complete installer. |

## P2-01 through P2-06 checkbox audit

Statuses apply to the complete checkbox; partial runtime results are stated separately. Evidence keys carry exact identities above.

| Checkbox | Status | Scope and evidence |
| --- | --- | --- |
| P2-01.1 Exterior controls | **PASS (owner-reported, bounded)** | A: stable heading, zoom and recenter on the recorded camera-only route. |
| P2-01.2 Keyboard/mouse and agreed controller controls, no duplicate actions | **NOT_RUN** | A: bounded mouse and controller movement/yaw passed; controller custom bindings and zoom gesture coverage remain unverified; [controls](../../CONTROLS.md). |
| P2-01.3 Bad/missing config and toggle off | **NOT_RUN** | A: config defaults/clamps are unit-tested; malformed-config gameplay is untested; F6 and F8 recovery passed bounded route. |
| P2-02.1 Tight interior, no blind walking or disabling collision | **PASS (owner-reported, bounded)** | C: requested interior route passed with opt-in obstruction; geometry breadth remains limited. |
| P2-02.2 Smooth clearance return, no oscillation | **NOT_RUN** | C: owner reported the requested route passed, but the leaf does not separately identify clearance/oscillation observations; trace alternation limits diagnosis. |
| P2-02.3 Emergency return during obstruction | **NOT_RUN** | C: owner reported the requested route passed, but the leaf does not separately identify an emergency return during obstruction. |
| P2-03.1 Melee, bow, targeted magic and hit direction | **PASS (owner-reported, bounded)** | B: melee and first-person F6 bow/spell hits reported usable; elevated ranged aim FAIL; crossbow and concentration cases NOT_RUN. |
| P2-03.2 Door, loot, small object and native range/line of sight | **NOT_RUN** | B: owner reported interactions passed; range and wall behavior were not separately measured. C records default-framing furniture difficulty. |
| P2-03.3 Lock, interrupted aim, no double UI input | **BLOCKED** | B: TDM lock lease prevents lock-on; menu/F6 recovery passes on later B artifact, but full interruption and UI input coverage remains open. |
| P2-04.1 Transition recovery, preferences and no stale references | **NOT_RUN** | C: requested route owner-reported PASS; complete transition/persistence and stale-reference coverage not established. |
| P2-04.2 Unsupported special states use safe view | **NOT_RUN** | C: bounded owner route passed; full mount/swim/transformation/furniture/killcam coverage is not individually evidenced. |
| P2-04.3 Disable and restart on copied save, UI/base usable | **NOT_RUN** | C: no separately reported disable-plus-restart check. |
| P2-04.4 Repeated menus/scenes without capture lock or runaway logging | **NOT_RUN** | C: bounded route reported passed, but trace exhausted its cap; repeated scene/input/logging criterion is not established. |
| P2-05.1 UI-only and combined resources and interaction text | **NOT_RUN** | D: UI-only resources and combined prompts passed bounded routes; UI-only prompt and individual health/magicka depletion not separately reported. |
| P2-05.2 Clear combat view, scale or reset | **PASS (owner-reported, bounded)** | D: UI-only 100% and combined 115% readable; restart restored defaults, no live reload claim. |
| P2-05.3 No Camera dependency, turn timers or tactical controls | **PASS** | D and [integration handoff](integration/handoff.md): UI-only runtime and source/package contract; no such features introduced. |
| P2-06.1 Populated slots act at most once with native behavior | **NOT_RUN** | E: controller Favorites and unbound FAV dispatch passed bounded routes; native quick-key-bound press and weapon second press untested. |
| P2-06.2 Native real-time cast/attack, no invented cooldowns | **NOT_RUN** | E: selection/equip/use demonstrated; full casting/attacking behavior with the bar is not independently reported. |
| P2-06.3 Menu suppression and depleted refresh | **PASS (owner-reported, bounded)** | E: combined dispatch retest reported menu suppression and depleted refresh, with log corroboration. |

## Camera MVP and independent installs

The [acceptance matrix](../../compatibility/acceptance-matrix.md) defines full C1–C5; its Phase 1 statuses predate these Phase 2 leaf runs.

| Requirement | Current audit |
| --- | --- |
| C1 elevated follow, zoom, held pitch, yaw/recenter | **NOT_RUN** full criterion: A bounded controls PASS; C owner-reported lifecycle route PASS, but the historical load-pitch FAIL is not separately shown resolved, and the full input matrix remains unverified. |
| C2 collision and usable closer interior | **NOT_RUN** full criterion: C bounded route PASS with opt-in obstruction, plus beam concerns; comprehensive geometry and default configuration remain unverified. |
| C3 melee, bow, aimed magic and activation | **FAIL** overhead ranged aim; B bounded F6 first-person fallback and melee PASS, lock **BLOCKED**, and native interaction range/walls lack separate measurement. Full encounter coverage is NOT_RUN. |
| C4 menu, load, travel, death/reload and scripted handoff | **NOT_RUN** full criterion: C bounded owner route PASS; individual transition coverage and pitch restoration need merged-main regression; the state-flip fix passed the bounded integration regression below. |
| C5 emergency reset and special-state fallback | **NOT_RUN** full criterion: A/B recovery and C bounded route PASS; disable/restart and full special-state matrix remain open. |
| Camera-only | **PASS (owner-reported, bounded)** on A controls and B combat; C is combined and final merged-main camera-only route is NOT_RUN. |
| UI-only | **PASS (owner-reported, bounded)** on D HUD and E controller Favorites with ordinary camera; final merged-main UI-only package and complete hotbar matrix remain NOT_RUN. |
| Combined | **PASS (owner-reported, bounded)** on D, E and C distinct artifact combinations; no single run establishes the final merged-main candidate. |

## Known gaps / blockers

- **BLOCKED:** target lock requires a verified exclusive handoff; TDM's target-lock disable lease prevents it on the current stack. **FAIL:** overhead ranged aim; F6 native first person is the demonstrated precision fallback (B).
- **NOT_RUN:** shout/voice line, native-quick-key-bound press and weapon second-press unequip (E). FAV dispatch and native bound precedence are different paths.
- Minimap partially covers active-effect icons at 1080p; deferred to Phase 3 HUD layout (E). Follower status, compass replacement and broader UI layouts remain Phase 3 gaps.
- Ceiling beams cause frequent obstruction tightening; low furniture may not be selectable from default framing, with F6 available (C). These are usability concerns.
- The earlier C trace exhausted its cap with normal/special alternation. The state-flip and trace-dedup fix was tested at `3ec6730` (128 → 9 trace lines) and carried into merged #10. Obstruction was enabled for the runs; its default remains **OFF**.
- Dialogue audio **FAIL**: silent even in a no-mod baseline. The launcher environment merged in [#13](https://github.com/Sonoran-Solutions/SkyCRPG/pull/13) reduced decoder failures from 19 to 0 in the integration run, but voices remain silent on this Proton host.
- Host input: switch the controller off for native keyboard/mouse gameplay; Deskflow's virtual pointer breaks mouse look. See E's baseline and direct-input isolation. The no-controller FAV failure was followed by a changed candidate and bounded dispatch PASS.

## Phase 3+ owner requests

These are owner requests, not scope decisions: smart proximity targeting with tab-style cycling on controller; mouse-hover targeting on keyboard/mouse; controller right-stick reticle mode; and a fixed camera with occluder fade/see-through instead of pull-in. Source: C.

## Merge record and senior review

Main head `7bad145` includes #13 → `33d7e14`, #9 → `26f23c1`, #12 → `c1f83e7`, #11 → `644095c`, and #10 → `7bad145`. The senior review on PRs #9–#14 requested reconciliation of stale statements, carrying `3ec6730` into #10, the zoom-tracker fix, a merged-main regression, and an owner ruling on F6 fallback versus target lock and overhead aim. The carry was verified by an empty camera-tree diff before the separate zoom-tracker fix; main's camera tree equals tested `integration/p2-camera-wave5@3ec6730` plus zoom-tracker fix `28aae27` (one line in `plugin.cpp` and its tests). The zoom-tracker issue was a real defect and its focused tests **PASS**; the fix is **NOT_RUN in game**.

Exact-head CI **PASS**: [#10 run 40450213582](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40450213582) and [#11 run 40450296976](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40450296976). Main CI for `7bad145`: [run 40451794533](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40451794533) — **PASS**.

## Integration regression run — 2026-10-26

This is not the merged-main gate run. Integration candidate `integration/p2-camera-wave5` source `3ec673026798b053cd47ee283c026d30161ac5c6` (lifecycle + obstruction + slide-classification/trace-dedup fix), Windows build [40446402141](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40446402141) **PASS**; Camera DLL `5a8d48dc997b7a9c59b914a985be03699542d0f4141f2056bc5f57ec57163c49`, archive `8e143a305dd32a00252cd5587893704df426bcb0a7c75b23f3b8c01bc5c1bb05`; UI `ba10da6` (DLL `028c07e7…`). Combined profile, controller off. Deployed INI enabled obstruction and the lifecycle trace for this run only. The launcher used the PR #13 media environment, applied locally for this run and reverted afterwards. Run `fd4aee259c8943c49ec764f8e576f523` quit normally; restoration **PASS**; Camera log `47687df56cd53892b004e815ca741ad7c1fc6dadff0bfc1b2a9023c322005ae6`; records under `/srv/storage/Projects/SkyCRPG/session-p2-20261026/wave7-regression/`.

**Owner-reported PASS:** F6 return after a menu, interior obstruction tightening and clearance without repeated in/out, zoom while moving, save/load camera recovery, hotbar number-key equip, repeated menus, and a load door followed by a first-press F6. **Lifecycle trace:** 9 lines total (load events, five debounced state changes, one player-detach), down from 128 alternating lines; the normal/special flip is resolved in this candidate. **Dialogue audio: FAIL (host/Proton limitation).** With PR #13, the `winegstreamer` transform failures dropped from 19 to 0 and the media converter opened its databases. Voices remained silent because Proton Experimental's media converter has no transcoded entry for Skyrim's xWMA voice streams: the Steam-provided `transcoded_audio.foz` is 16 bytes. The converter therefore dumps the stream (`audiov2.foz`) and substitutes silence. This is not a SkyCRPG module or launcher defect; any fix is a Proton or compatibility-tool choice on this host.

## Merged-main regression — NOT_RUN (owner-waived for GO)

- Record exact merged commit, build URLs, package/DLL/SWF/INI hashes, runtime/dependency/host pins, isolated profiles and copied-save provenance.
- Camera-only: exploration, zoom/recenter, collision/clearance and F6/F8 recovery; melee, F6 ranged precision and activation; menu/load and preference recovery.
- UI-only: HUD, hotbar and ordinary-camera behavior on the final candidate.
- Combined: HUD/resources/prompts and scale, FAV and native-bound actions, menu suppression/depletion, camera recovery and no duplicate input; check dialogue audio and the lifecycle trace fix.
- Record each result and any omissions, run IDs/logs, timeout and cleanup/restoration; do not infer UI-only final-candidate coverage from combined. Record omissions without upgrading them to PASS.

## Owner decision

The owner stated on 2026-10-26:

> When CI passes and everything is merged you are approved to record a phase 2 owner GO and close any remaining phase 2 tickets so phase 3 can start.

**Phase 2 GO (owner decision)** is the owner's ruling on the F6 fallback, target lock and overhead aim. It accepts the following as documented deferrals or limitations, not passes:

- Target lock **BLOCKED**; overhead ranged aim **FAIL**. F6 native first-person fallback is accepted for Phase 2.
- Merged-main camera-only, UI-only and combined regression **NOT_RUN**. Latest evidence is integration `3ec6730` run `fd4aee25` and the leaf runs.
- #10 zoom-tracker fix **NOT_RUN in game**.
- Active-obstruction F8, disable plus restart, native-bound hotkey no-duplicate, weapon second-press unequip, shout recovery, crossbow/concentration and native range/line of sight are **NOT_RUN**.
- Obstruction defaults **OFF**.
- Dialogue audio **FAIL** on the Proton host despite the #13 launcher environment removing decoder failures.
- Minimap/active-effect overlap, beam nuisance and low-furniture selection are deferred.

This decision does not change the evidence statuses above or establish release acceptance.

## Phase 3 entry follow-ups

- Complete the merged-main camera-only, UI-only and combined regression using the checklist above, including the zoom-tracker fix in game.
- Exercise active-obstruction F8, disable plus restart on a copied save, native-bound hotkey no-duplicate, weapon second-press unequip, shout recovery, crossbow and concentration, and native range/line of sight.
- Investigate the Proton dialogue issue and record a verified compatibility-tool or host remedy if available.
- Address the minimap/active-effect overlap, beam nuisance and low-furniture selection.
- Consider the Phase 3+ owner requests above: proximity targeting and controller cycling, mouse-hover targeting, controller right-stick reticle mode, and fixed camera with occluder fade/see-through.
