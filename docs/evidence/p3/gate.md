# DYL-169 / P3-07 — Phase 3 gate

**GO — owner decision 2026-10-28 (with documented deferrals).** This record scopes the evidence to the tested artifacts. The required 15–20 minute combined outdoor-to-dungeon route, and a UI-only smoke on the final candidate, are **NOT_RUN** and waived by the owner for this GO. The checkbox ordinals follow [Phase 3](../../phases/PHASE_3.md), and the U and C IDs follow [MVP scope](../../MVP_SCOPE.md).

A bounded owner report is not full criterion or release acceptance, and tests alone do not establish runtime behaviour. Several leaf files keep superseded status headers or placeholders. Where they conflict, the dated runtime sections cited below take precedence.

## Candidate identity

**Main head `c10cd80`** contains every Phase 3 merge.

| PR | Merge commit | Content |
| --- | --- | --- |
| #15 | `21ca13e` | DYL-166 minimap |
| #16 | `1298ff4` | DYL-165 follower |
| #17 | `945afd1` | DYL-163 inventory integrity |
| #18 | `5ee9cc7` | input-device docs |
| #19 | `5e503a3` | DYL-166 evidence cleanup |
| #20 | `6e7dff6` | DYL-164 grid |
| #22 | `b1bb0a2` | DYL-168 input and layout |
| #21 | `8eef712` | DYL-167 lifecycle |
| #23 | `c10cd80` | never-shipped test probe and CI target selection |

Main push CI:
- `8eef712`: [run 40753002080](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40753002080) **PASS**.
- `c10cd80`: [run 40759295249](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40759295249). Its status is recorded in [STATUS](../../STATUS.md).

**Internal UI candidate:** UI source `9a8692e4b5592edd26ecf8e82733a067862f3b13`, [Windows CI 40638507073](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40638507073) **PASS**.
- `SkyCRPG_UI-0.0.1-9a8692e.zip` `79b79809fcc744b58792192551114c908a4d436c8fff7381f90f22e81029a91c`.
- DLL `c3ef6c3a1fef2e0df1d70a6c2ad864252b6ed1df90ac9cec3a5202819c71ed53`.
- Shipped INI `c7222ebd3f694d074cb6e37dee551f16c6df31ac099742169f0b1410a5a1ba27`.
- Generated movies:
  - `inventorymenu.swf` `2302b306cd3beb6e69a1962a76030f57f1c85a2fcea43ec9938c818e8fb1b3ba`;
  - `skyui/inventorylists.swf` `34e32f04521a28acf16b00e263a479ca233a0814e986a8e0d91bcc46b4024518`;
  - `hudmenu.swf` `54937d9d48d9a5de4c757ca2ce874fff4a41ed72b39048ad2eabd98292a31628`.

**Camera candidate:** source `28aae27` (Phase 2 tree plus the zoom-tracker fix).
- `SkyCRPG_Camera-0.0.1-28aae27.zip` `b97997f62df7c914959938938229384a1d7b3b558d6a1c75681270ec4db156dc`.
- DLL `2ae4d51326604eeaa1ef2dd2b021be41ee53652602ab865b144b84d01ba1fa0d`.

**Source equivalence with main:**
- `git diff 9a8692e c10cd80 -- modules shared cmake CMakePresets.json vcpkg.json` is empty. The root `CMakeLists.txt` differs only by the test-probe option, which defaults OFF.
- `git diff 28aae27 c10cd80 -- modules/camera shared cmake` is empty.

Main's module sources therefore equal the tested candidates. Main-built DLLs embed a different source commit and are not byte-identical; they have not been run in game.

**Host:** Steam Skyrim 1.7.104.0, SKSE 2.3.1, Address Library 13, SkyUI 6.11, Ubuntu 26.04.1 with Proton Experimental 11 and an RTX 3080. Pinned providers are in the [runtime matrix](../../compatibility/runtime-matrix.md). Keyboard/mouse needs the controller switched off on this host.

## Evidence keys

| Key | Leaf | Tested identity and runs used here |
| --- | --- | --- |
| I | [inventory integrity](inventory-integrity.md) (DYL-163, #17) | UI `951dc3e` (CI 40593455538) with Camera `28aae27`. Combined `69f8c8e004e54d97abc60eaa3eb623fb` (controller); UI-only `4885a59564794db7a433d5952180097d` (keyboard/mouse). Earlier fix runs: `5f95c0b048c74bde959ec6f5df90f85c`, `ec0b4e3cd75f47568b46d20190a8f3da`. |
| G | [grid inventory](grid-inventory.md) (DYL-164, #20) | `c1ae156` (CI 40601052249). UI-only `e0abf83561b2494888fdef9e542cf823` (1280×720, 500 entries); combined `07b392d246d54aedbcb44b6871532af1`. Mixed devices. The later repaint fix `4695229` has no dedicated runtime smoke; it is included in the L and R/O/C builds below. |
| F | [follower HUD](follower-hud.md) (DYL-165, #16) | UI-only `1617dd2c7d574a99b9d7fe92376d8776` (`ed22860`, CI 40454404866, Orgnar); UI-only `bf2a8387cdb942628b56b922bf7aefcb` (`8da9625`, CI 40563710877, Serana on a vanilla fixture, controller). |
| M | [minimap](minimap.md) (DYL-166, #15) | Runtime source `9cb576f` (CI 40457081211). UI-only `7571da8e3f4c42fbb4cfd8eaa1a6f5d7`; `bReplaceCompass=0` run `6bf91540692f4083bc8c74973a851950`; provider-absent run `a16a95c6690844f39de2844ba9123d99`; combined `5298264e872545c0ab28f5b07becc551`. |
| L | [input and layout](input-and-layout.md) (DYL-168, #22) | Final layout `46a7809`, UI-only: `87823c897a1a4b9288e7dcc66af77070` (controller, 1080p, owner approved); `1bca86a3f64342ab875a6b1c54c53ff7` (1080p at 115%, controller); `fc323c277e6946c98a102f119b485a67` (720p at 100%; device not stated). Earlier device passes: controller on `0b8c865`, keyboard/mouse on `eef1154`. |
| R / O / C | [menu lifecycle](menu-lifecycle.md) (DYL-167, #21) | **Final candidate** `9a8692e` with Camera `28aae27`, combined. **R:** automated keyboard/mouse route, supervised `auto-dyl167-combined-20261028T181647Z` and unattended `…T181916Z`, 9/9 each. **O:** owner manual session `p3-manual-167` on both devices (dialogue, quit then load, new game, death then reload, combat). **C:** malformed-INI session `p3-ini-167`. |
| T | test probe ([PR #23](https://github.com/Sonoran-Solutions/SkyCRPG/pull/23)) | A never-shipped, opt-in SKSE observer (`tools/testprobe/`). It is installed only into the disposable profile for automated runs and verified removed. Merged probe `aaba0c3`: unattended smoke `auto-dyl167-combined-20261028T200124Z` PASS, 9/9. It is test infrastructure, not part of either module. |

## P3-07 acceptance criteria

| Criterion | Status | Scope and evidence |
| --- | --- | --- |
| 1. UI-only and combined profiles contain live player/follower status, hotbar, player grid inventory and working minimap replacement | **PASS (owner-reported, bounded)**, split across candidates | **UI-only:** status and follower (F), grid (G, L), integrity (I), minimap replacement and fallbacks (M), hotbar via Phase 2 and R. **Combined:** I, G and M on earlier candidates; R, O and C on the final candidate show the HUD, the hotbar slot, the follower panel (Serana) and the minimap after load. No single run on the final candidate exercises every feature in **both** profiles. The final-candidate UI-only smoke is **NOT_RUN**. |
| 2. Camera and UI both meet their feature criteria with documented limited-state fallbacks | **NOT_RUN** full criterion; owner GO with deferrals | The UI feature tickets DYL-163–168 are merged with bounded PASS evidence and documented fallbacks: native menus, the compass when the map is disabled or the provider is absent, missing-portrait silhouettes, INI defaults and clamps, and native favorite and off-thread rulings. Camera feature criteria are unchanged from the [Phase 2 gate](../p2/gate.md): overhead aim **FAIL**, target lock **BLOCKED**, with F6 native first person as the accepted fallback. |
| 3. No open item-loss/duplication, control-lock or navigation-loss defect deferred as cosmetic | **PASS (bounded)** | None are open. The item-loss/trap FAIL on `effea52` was fixed at `e202dfd`, and the first-press refusal at `15f291a` (I). Quantity cancel causes no drop (R), and a held key causes no action (R). Minimap failures kept the compass visible, and the owner ruled "lost navigation is not accepted" (M). Scoped owner rulings, not defects: the native favorite toggle and the off-thread native fallback (I). |

**Focused validation:**
- The single recorded 15–20 minute combined outdoor-to-dungeon route (travel, fight, loot, inspect/equip in the grid, follow the minimap, talk, reload) is **NOT_RUN**. Owner-waived.
- The UI-only smoke on the final candidate is **NOT_RUN**. Owner-waived. The `autotest/routes/ui-smoke.json` route exists but has not run live.
- Unchanged cases reuse the focused evidence above, as the plan allows.

## UI acceptance U1–U6

| ID | Status | Evidence and limits |
| --- | --- | --- |
| U1 player resources and follower status | **PASS (owner-reported, bounded)** | Player resources: Phase 2 core HUD, plus HUD readiness logged after load on the final candidate (R). Follower appear/disappear, live health, reload and cell change, and silhouette fallback: F. Not covered: magicka and stamina for followers, true non-essential follower death, and the two-teammate label (**NOT_RUN**). |
| U2 eight native-action slots | **PASS (bounded), partial** | Auto-FAV slot equip/unequip on keyboard (R, O), and controller Favorites (Phase 2, O). Hotbar presses in combat act once (O). **NOT_RUN:** native-bound no-duplicate, weapon second-press unequip, shout recovery, all eight physical slots and depleted refresh on the final candidate. |
| U3 grid, categories/search/sort, details, equipment, counts/weight/gold | **PASS (owner-reported, bounded)** | G covers grid workflows, a 500-entry inventory (untimed) and long names. L covers the approved layout and the 720p/1080p/115% layouts; its per-action controller items (such as the R3 search pad) are not itemised. **NOT_RUN:** CC-icon fallback; two-handed, left-hand, robe, ring and amulet slot cases; timed open budget. |
| U4 identity and native rules | **PASS (owner-reported, log-confirmed, bounded)** | I covers plain, tempered, enchanted, stolen and owned items, a quest item, a potion stack and a CC form, with zero refusals and a double press dropping once. R adds quantity cancel with no drop. Numeric before/after weight is not recorded. Favorite toggle is native by ruling. |
| U5 minimap replaces compass with safe fallback | **PASS (owner-reported, bounded)** | M covers both profiles, interior/exterior transitions, `bReplaceCompass=0`, provider absent, and no quest-text overlap. The active-effect overlap was resolved in L by owner observation. C shows an invalid `bReplaceCompass` falls back to the native compass. Limits: marker documentation stays open, and survival temperature is hidden with the compass. |
| U6 input/focus/pause, native fallbacks, save/load, both inputs at 720p/1080p with one scaled case | **PASS (bounded)** | R covers keyboard/mouse menu suppression, the held key, quantity cancel and save/load, automated and unattended. O covers dialogue, quit then load, new game, death then reload and combat on both devices. C covers malformed config. L covers 720p, 1080p and 115%. **NOT_RUN:** runtime `iVersion` mismatch, `bResetLayout` at runtime, UI-only disable/restart, ultrawide, load while a modal is open, the full nested transition matrix, and itemised controller actions on the final layout build. The new-game SKSE event was not independently confirmed in the O log. |

## Camera candidate and module independence

The Camera tree has not changed since `28aae27`. No Phase 3 run re-evaluated C1–C5. The combined runs loaded it with its runtime check **PASS** and SmoothCam as the sole transform writer. Camera statuses carry over unchanged from the [Phase 2 gate](../p2/gate.md):
- **FAIL:** C3 overhead ranged aim.
- **BLOCKED:** target lock.
- **NOT_RUN** full criteria: C1, C2, C4 and C5.
- The zoom-tracker fix is loaded in every combined run, but zoom was not specifically exercised.

**Module independence (M1): PASS (bounded).**
- UI-only runs (I, G, F, M, L) need no Camera providers.
- Combined runs (I, G, M, R, O, C) load both.
- CI builds Camera and UI as separate targets and packages, with probe-only changes selecting only the probe.
- The test probe is excluded from release ZIPs and is inert without an explicit `bEnabled=1`.
- Final packaging, disable and removal (M2) remain **NOT_RUN**.

## Release blockers carried to Phase 4

This is the finite list of open items that must be resolved, or explicitly owner-accepted, before a release candidate (DYL-170 onward). It does not reopen Phase 3.

1. **Final-candidate regression.** The 15–20 minute combined outdoor-to-dungeon route and a UI-only smoke on main-built packages, with the camera-only route as well.
2. **Camera aim and lock.** Overhead ranged aim **FAIL**; target lock **BLOCKED** (TDM lock lease). F6 native first person is the accepted fallback until resolved or permanently accepted.
3. **Disable, restart and removal** on a copied save for Camera and UI, including the provider-disable paths (M2, P2-04.3 and P3-05).
4. **Hotbar completeness:** native-bound no-duplicate press, weapon second-press unequip, shout recovery, and all eight slots plus depleted refresh.
5. **Follower:** true non-essential death, and labelling of unsupported multi-follower setups.
6. **Inventory edge cases:** CC-icon fallback; two-handed, left-hand, robe, ring and amulet equipment slots; numeric weight before/after.
7. **Config and layout:** runtime `iVersion` mismatch reset, `bResetLayout` at runtime, ultrawide and other aspect ratios.
8. **Camera coverage:** active-obstruction F8, crossbow and concentration, native range and line of sight, special states, and the zoom-tracker behaviour in game. Obstruction stays default OFF; ceiling beams and low furniture remain usability limits.
9. **Performance budgets** (DYL-172): warm inventory open ≤250 ms at 500 entries, frame overhead and memory growth. All unmeasured.
10. **Dialogue audio FAIL** on the Proton host (no xWMA transcode). Verify on another host or compatibility tool, or document it as a host limitation.
11. **Packaging and rights** (DYL-171/174): an installer that includes the generated SWFs and configuration, attribution, and third-party distribution permissions.
12. **Minimap documentation:** markers and the hidden survival temperature.

## Owner decision

The owner stated on 2026-10-28:

> Can you help me complete linear ticket DYL-169 to complete phase 3? I approve of the current state and you can record my GO approval

**Phase 3 GO (owner decision).** The owner accepts the current state as the Phase 3 exit, including these explicit deferrals, which are not passes:
- the combined outdoor-to-dungeon route and the final-candidate UI-only smoke (**NOT_RUN**);
- the Camera criteria carried over from Phase 2;
- the release-blocker list above.

This decision does not change any evidence status above or establish MVP or release acceptance. Phase 4 (DYL-170 onward) may start.
