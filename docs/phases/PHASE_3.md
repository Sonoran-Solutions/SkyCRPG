# Phase 3 — UI MVP and combined gameplay

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

## Outcome

Exit: UI-only and combined candidates have player/follower status, hotbar, native-weight grid inventory, minimap replacing compass, menu focus and layout handling. A playable combined dungeon route is demonstrated.

## Work items

### P3-01 — Implement authoritative inventory snapshots and item actions

**Linear:** [DYL-163](https://linear.app/devils17/issue/DYL-163/p3-01-implement-authoritative-inventory-snapshots-and-item-actions).

**Lane:** inventory. **Dependencies:** P2-07.

Make a visual grid safe for real items, including visually identical items with different state.

**Scope**

- Implement or adapt the chosen inventory model, maintaining identity for stacks and distinct enchanted/tempered/stolen instances.
- Route equip/unequip, consume/use/read, favorite and drop/quantity operations through supported native game/menu actions; reacquire item state before a mutation.
- Refresh after engine changes, protect quest-item rules, preserve native weight/capacity and invalidate stale references on reload.

**Acceptance criteria**

- [ ] A fixture set of plain duplicates, enchanted/tempered items, stolen items, stacked consumables, quest items and modded forms preserves identities and quantities.
- [ ] Native carry weight and encumbrance behavior remain unchanged; displayed data is not the source of inventory truth.
- [ ] Repeated or stale UI events cannot duplicate, delete or act on the wrong item; failed actions refresh or report a safe failure.

**Focused validation:** Targeted item-model/transaction tests plus before/after in-game quantities for the fixture set; keep a copied save for recovery.

**Evidence/deliverables:** `docs/evidence/p3/inventory-integrity.md`, `docs/ARCHITECTURE.md`.

**Out of scope:** No Tetris storage capacity, item rotation, nested bags, economy changes or new item records.

### P3-02 — Build the grid inventory and equipment presentation

**Linear:** [DYL-164](https://linear.app/devils17/issue/DYL-164/p3-02-build-the-grid-inventory-and-equipment-presentation).

**Lane:** inventory. **Dependencies:** P3-01.

Deliver the classic CRPG inventory layout shown by the concept while keeping the scope shippable.

**Scope**

- Add a scrollable uniform icon grid with categories, sort, search, counts, equipped/favorite/quest indicators, details and carry weight/gold.
- Provide equipment slots with a static silhouette or a proven reusable character preview; do not build a new 3D portrait/preview engine for MVP.
- Use P3-01 actions and readable quantity/confirmation dialogs; retain a path to the vanilla/SkyUI menus outside the player-inventory replacement.

**Acceptance criteria**

- [ ] The player can find, inspect, equip, consume, favorite and drop items through the grid, with accurate weight and counts.
- [ ] Long names, missing custom icons and large inventories use safe fallbacks and bounded rendering.
- [ ] A scrollable/virtualized dataset of 500 total inventory entries remains usable in the performance fixture; any lower cap must be a recorded scope decision.

**Focused validation:** In-game grid operations and representative item tooltips plus layout checks at the baseline resolution; validate integrity through P3-01 evidence.

**Evidence/deliverables:** `docs/evidence/p3/grid-inventory.md`, `docs/UX_SPEC.md`.

**Out of scope:** No grid barter, container, crafting or follower-trade overhaul in MVP; those remain usable through native/SkyUI screens.

### P3-03 — Add follower status portraits through the selected provider

**Linear:** [DYL-165](https://linear.app/devils17/issue/DYL-165/p3-03-add-follower-status-portraits-through-the-selected-provider).

**Lane:** ui. **Dependencies:** P2-07.

Provide the classic party-strip feel without taking over follower AI.

**Scope**

- Integrate Party Sheet or the Phase 1 provider/limited native adapter for one recruited vanilla humanoid follower; handle additional followers only through an explicitly tested provider.
- Show name, portrait or neutral silhouette, health and supported status values; treat unloaded or unavailable data as unknown.
- Handle recruitment, dismissal, unload and death with bounded updates and no arbitrary portrait-asset bundling.

**Acceptance criteria**

- [ ] The required vanilla follower appears and disappears correctly, with live status and a safe missing-portrait fallback.
- [ ] No stale/dead actor handle is dereferenced after transitions; unsupported multi-follower configurations are labeled untested.
- [ ] Portraits are status displays and do not issue movement, combat or dialogue commands.

**Focused validation:** Recruit/dismiss, leave/re-enter cell, injury and death or equivalent disposable-fixture states; one provider-disabled check.

**Evidence/deliverables:** `docs/evidence/p3/follower-hud.md`, `docs/compatibility/dependency-lock.md`.

**Out of scope:** No multi-follower framework, autonomous party AI, party switching or custom portrait generation pipeline.

### P3-04 — Integrate the minimap and replace the Skyrim compass safely

**Linear:** [DYL-166](https://linear.app/devils17/issue/DYL-166/p3-04-integrate-the-minimap-and-replace-the-skyrim-compass-safely).

**Lane:** ui. **Dependencies:** P2-07.

Deliver the approved upper-right minimap layout by integrating a working existing mod.

**Scope**

- Install/configure the selected minimap provider with a matching frame or the closest supported styling, upper-right anchoring, scale and toggle.
- Hide the horizontal compass only while minimap replacement is enabled and its provider is available; restore native navigation on disable or provider failure.
- Test exterior roads and interior floor plans; preserve provider exploration/marker rules and show only markers actually supplied.

**Acceptance criteria**

- [ ] UI-only and combined profiles display a working minimap without a duplicate compass.
- [ ] Exterior/interior transitions retain usable navigation and quest text does not overlap the map.
- [ ] Missing/disabled provider restores compass navigation; enemy and companion markers are required only if the chosen provider supports them and are documented accurately.
- [ ] The supplied configuration/add-on respects source and asset permissions and does not bundle unrelated third-party binaries.

**Focused validation:** Outdoor/interior transition, map on/off, provider absent and menu overlap cases on the locked version.

**Evidence/deliverables:** `docs/evidence/p3/minimap.md`, `docs/compatibility/dependency-lock.md`, `docs/UX_SPEC.md`.

**Out of scope:** No custom map renderer, navigation mesh extraction, wallhack enemy detection or guarantee of the exact concept-frame artwork.

### P3-05 — Handle UI menu focus, pause semantics and save/load lifecycle

**Linear:** [DYL-167](https://linear.app/devils17/issue/DYL-167/p3-05-handle-ui-menu-focus-pause-semantics-and-saveload-lifecycle).

**Lane:** integration. **Dependencies:** P3-02, P3-03, P3-04.

Prevent the overlay from breaking the rest of Skyrim's menus or firing actions behind them.

**Scope**

- Enforce one input owner across gameplay, inventory, quantity dialogs, console, dialogue, crafting, container/barter, map and journal.
- Use the selected native paused inventory behavior; release held inputs and cursor capture on close/load/disable.
- Persist presentation preferences separately from authoritative game state, restore provider/compass state, and test native menu fallbacks.

**Acceptance criteria**

- [ ] Opening or closing any covered menu leaves native controls working and causes no unintended attack/item use.
- [ ] Save/reload and new-game transitions clear stale item/follower state; configuration recovery works.
- [ ] Player inventory can coexist with native/SkyUI container, barter, magic, crafting, journal and map screens without losing items or becoming trapped.
- [ ] The UI never globally pauses action combat outside its documented menu behavior.

**Focused validation:** Focused menu-lifecycle matrix including nested quantity dialogs and one save/reload; reuse inventory integrity fixtures.

**Evidence/deliverables:** `docs/evidence/p3/menu-lifecycle.md`, `docs/CONTROLS.md`, `docs/INSTALLATION.md`.

**Out of scope:** No Skyrim Souls/unpaused-menu support promise; it needs a later explicit compatibility task.

### P3-06 — Finish controller navigation, UI scaling and readable layouts

**Linear:** [DYL-168](https://linear.app/devils17/issue/DYL-168/p3-06-finish-controller-navigation-ui-scaling-and-readable-layouts).

**Lane:** ui. **Dependencies:** P3-02, P3-03, P3-04.

Make the interface operable with the supported input modes and common display sizes.

**Scope**

- Implement keyboard/mouse and controller focus paths through grid, equipment, categories, action dialogs and hotbar without requiring pointer emulation.
- Verify 1280x720 and 1920x1080 at default scale, plus one scaled-text case; support a basic anchored ultrawide layout check if the test host permits it.
- Keep status distinguishable by icon/text as well as color, readable focus indicators, long-name handling and a reset-layout command.

**Acceptance criteria**

- [ ] Every MVP inventory/hotbar action is reachable with both supported input modes.
- [ ] No invisible focus trap or overlapping quantity/confirmation prompt; controller cancellation reliably backs out.
- [ ] Party, quest, minimap and bottom bar stay within safe areas at the two required resolutions.

**Focused validation:** One end-to-end controller inventory route, keyboard route and screenshots of the required resolutions; no exhaustive resolution Cartesian product.

**Evidence/deliverables:** `docs/evidence/p3/input-and-layout.md`, `docs/UX_SPEC.md`, `docs/CONTROLS.md`.

**Out of scope:** No touchscreen-native interface, localization rollout or full accessibility certification.

### P3-07 — Phase 3 gate — accept the UI MVP candidate and combined vertical slice

**Linear:** [DYL-169](https://linear.app/devils17/issue/DYL-169/p3-07-phase-3-gate-accept-the-ui-mvp-candidate-and-combined-vertical).

**Lane:** gate. **Dependencies:** P3-05, P3-06.

Finish feature implementation before the last phase, leaving Phase 4 for actual stabilization.

**Scope**

- Verify UI acceptance U1-U6, the camera candidate and module independence.
- Run the combined outdoor-to-dungeon route: travel, fight, loot, inspect/equip through the grid, follow the minimap, talk and reload.
- Record an internal UI candidate and the finite list of release blockers.

**Acceptance criteria**

- [ ] UI-only and combined profiles contain live player/follower status, hotbar, player grid inventory and working minimap replacement.
- [ ] Camera and UI both meet their feature criteria with documented limited-state fallbacks.
- [ ] No open item-loss/duplication, control-lock or navigation-loss defect is deferred as cosmetic.

**Focused validation:** One recorded 15–20 minute combined route plus a UI-only smoke; use prior focused evidence for unchanged cases.

**Evidence/deliverables:** `docs/evidence/p3/gate.md`, `docs/STATUS.md`, `docs/compatibility/runtime-matrix.md`.

**Out of scope:** No new feature expansion or full gameplay overhaul.
