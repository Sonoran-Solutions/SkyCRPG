# Phase 2 — Camera MVP and playable HUD

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

## Outcome

Exit: camera-only candidate handles exploration, combat, obstruction, handoff and recovery; UI-only candidate supplies a usable core HUD and hotbar. Both remain independent and use the Phase 1 contracts.

## Work items

### P2-01 — Implement the elevated camera controller and player controls

**Linear:** [DYL-156](https://linear.app/devils17/issue/DYL-156/p2-01-implement-the-elevated-camera-controller-and-player-controls).

**Lane:** camera. **Dependencies:** P1-06.

Deliver the basic independent camera experience using the selected Phase 1 route.

**Scope**

- Implement smooth follow, held pitch, bounded distance/zoom, yaw rotation or snap rotation, recenter and a normal-view toggle.
- Support camera-relative keyboard movement and the selected existing directional movement integration; do not reimplement combat movement.
- Persist camera preferences outside saves where possible, clamp invalid settings and avoid changing the UI package.
- Establish the camera state-precedence skeleton and separate obstruction/aiming interfaces before downstream parallel work.

**Acceptance criteria**

- [ ] Camera-only install can explore an exterior route with stable heading, zoom limits and recenter behavior.
- [ ] Keyboard/mouse and the agreed controller camera controls are documented and do not trigger duplicate native actions.
- [ ] Bad or missing config restores safe defaults; toggling off returns control to the underlying game camera.

**Focused validation:** Target camera math/config tests plus the exterior control route; build only the camera target and affected common code.

**Evidence/deliverables:** `docs/evidence/p2/camera-controls.md`, `docs/CONTROLS.md`.

**Out of scope:** No click navigation, detached RTS panning or movement/attack rework.

### P2-02 — Handle camera collision and tight-interior fallback

**Linear:** [DYL-157](https://linear.app/devils17/issue/DYL-157/p2-02-handle-camera-collision-and-tight-interior-fallback).

**Lane:** camera. **Dependencies:** P2-01.

Make the view usable when roofs, walls and corridors prevent the preferred overhead position.

**Scope**

- Implement or configure obstruction-aware distance and pitch adjustment with smoothing/hysteresis.
- Use the existing engine/provider collision path; preserve wall/floor collision and a reachable normal-camera fallback.
- Handle doorways, ceiling changes, stairs and a representative larger chamber without per-cell world edits.

**Acceptance criteria**

- [ ] The recorded tight-interior route never requires walking blind or disabling collision to recover.
- [ ] The view returns smoothly toward the configured exploration position after clearance, without rapid near/far oscillation.
- [ ] Emergency return remains available during obstruction handling and leaves the camera usable.

**Focused validation:** Repeat the three obstruction fixtures from P1, record a short before/after capture, and add a state-transition test only where logic warrants it.

**Evidence/deliverables:** `docs/evidence/p2/obstruction.md`, `docs/compatibility/runtime-matrix.md`.

**Out of scope:** No promise to expose every roof or support every dungeon geometry; universal wall/roof fading is deferred.

### P2-03 — Integrate combat aiming, target lock and world interaction

**Linear:** [DYL-158](https://linear.app/devils17/issue/DYL-158/p2-03-integrate-combat-aiming-target-lock-and-world-interaction).

**Lane:** camera. **Dependencies:** P2-01.

Keep Skyrim's action combat and interactions practical from the elevated view.

**Scope**

- Integrate the chosen TDM/aiming behavior with one camera owner and deterministic aim-mode precedence.
- Provide a closer normal aiming view for bow, crossbow where available, aimed/concentration spells and precise world activation when overhead framing is insufficient.
- Preserve native range/line-of-sight rules, target-lock behavior and crosshair placement; do not aim attacks at decorative HUD targets.

**Acceptance criteria**

- [ ] Melee, bow and targeted magic are usable in the agreed test encounters; aiming fallback clearly corresponds to the actual hit direction.
- [ ] Doors, loot and one small world object can be activated without extending activation range or bypassing walls.
- [ ] Lock-on entry/exit and interrupted aim return to a sensible view; interaction does not consume UI navigation keys twice.

**Focused validation:** Focused ranged, melee and activation fixtures with hit/interaction evidence; no unrelated combat-mod suite.

**Evidence/deliverables:** `docs/evidence/p2/combat-and-activation.md`, `docs/CONTROLS.md`.

**Out of scope:** No combat rebalance, projectile homing implementation, ability cooldown system or tactical target commands.

### P2-04 — Implement camera lifecycle handoff and safe recovery

**Linear:** [DYL-159](https://linear.app/devils17/issue/DYL-159/p2-04-implement-camera-lifecycle-handoff-and-safe-recovery).

**Lane:** camera. **Dependencies:** P2-02, P2-03.

Prevent stuck input or camera state across the events ordinary Skyrim play constantly triggers.

**Scope**

- Handle dialogue, menus, scripted camera scenes, death/reload, fast travel and cell transitions.
- Relinquish camera ownership for first person, mounts, swimming, transformations, furniture and killcams unless explicitly supported by the selected backend; restore only when a valid normal state returns.
- Add explicit disable/reset recovery, version guards and bounded diagnostics; do not unload native DLLs in a running process.

**Acceptance criteria**

- [ ] Supported transitions return control correctly and preserve user preferences without carrying stale actor/camera references through loads.
- [ ] Unsupported special states use a documented safe native/provider view.
- [ ] A camera disable plus restart on a copied save does not make the UI package or base gameplay unusable.
- [ ] Repeated menu/scene toggles leave no capture lock, stuck cursor or runaway logging.

**Focused validation:** Run lifecycle cases C4/C5 and inspect relevant state tests; repeat only changed transitions.

**Evidence/deliverables:** `docs/evidence/p2/lifecycle.md`, `docs/INSTALLATION.md`.

**Out of scope:** No claim of universally safe mid-save uninstallation; record actual supported disable/removal procedure.

### P2-05 — Build the core CRPG HUD and player status layout

**Linear:** [DYL-160](https://linear.app/devils17/issue/DYL-160/p2-05-build-the-core-crpg-hud-and-player-status-layout).

**Lane:** ui. **Dependencies:** P1-06.

Make the UI module immediately playable under the ordinary camera.

**Scope**

- Implement or adapt the charcoal/brass Nordic HUD, player health/magicka/stamina, target/interaction text and reserved party/minimap areas.
- Prefer selected existing HUD/provider widgets; establish anchors, scale and visibility by game/menu state.
- Avoid duplicate native resource bars and keep essential prompts visible while unfinished widgets remain absent.

**Acceptance criteria**

- [ ] UI-only and combined profiles show correct player resources and world interaction text during real-time play.
- [ ] The main combat view remains clear and the HUD can be scaled or reset to safe positions.
- [ ] No SkyCRPG Camera dependency, hidden turn timers or tactical-party controls are introduced.

**Focused validation:** Focused HUD data refresh and 1080p layout smoke; no screenshot-only claim of live data integration.

**Evidence/deliverables:** `docs/evidence/p2/core-hud.md`, `docs/UX_SPEC.md`.

**Out of scope:** No full journal/map/dialogue reskin or generic widget editor.

### P2-06 — Connect an eight-slot hotbar to native favorites and equipment actions

**Linear:** [DYL-161](https://linear.app/devils17/issue/DYL-161/p2-06-connect-an-eight-slot-hotbar-to-native-favorites-and-equipment).

**Lane:** ui. **Dependencies:** P2-05.

Make the CRPG action bar useful without inventing a new combat system.

**Scope**

- Display up to eight native favorite bindings with icons, equipped/selected state, stack counts and relevant native shout recovery if available.
- Use the selected existing hotbar/favorites route for equip, spell selection and consumables; keep native attack/cast inputs.
- Handle empty, depleted and removed favorites, key conflicts, and menu input ownership; provide an accessible controller selection path.

**Acceptance criteria**

- [ ] Each populated slot selects/equips/uses its intended item at most once, respecting native favorite and equipment behavior.
- [ ] Casting or attacking is still real-time Skyrim input; the bar does not imply invented spell cooldowns.
- [ ] Menu navigation cannot fire a consumable or attack behind the menu, and depleted slots refresh correctly.

**Focused validation:** One weapon, one spell, one consumable, one empty/depleted slot, plus menu-focus and controller selection checks.

**Evidence/deliverables:** `docs/evidence/p2/hotbar.md`, `docs/CONTROLS.md`.

**Out of scope:** No rotation automation, macro chains, tactical pause or replacement spell system.

### P2-07 — Phase 2 gate — accept the camera MVP candidate and playable HUD

**Linear:** [DYL-162](https://linear.app/devils17/issue/DYL-162/p2-07-phase-2-gate-accept-the-camera-mvp-candidate-and-playable-hud).

**Lane:** gate. **Dependencies:** P2-02, P2-03, P2-04, P2-06.

Have a usable camera before the inventory work expands the interface.

**Scope**

- Audit camera acceptance C1-C5 and independent install behavior against the recorded evidence.
- Review the working HUD/hotbar in UI-only and combined profiles; list Phase 3 UI gaps explicitly.
- Tag or identify a reproducible internal camera candidate and update the ready queue.

**Acceptance criteria**

- [ ] Camera-only exploration, combat, aiming and lifecycle recovery meet the MVP criteria on the pinned stack.
- [ ] UI-only HUD and hotbar work with the ordinary camera.
- [ ] Remaining blockers are explicit; later feature work does not relabel an untested camera as complete.

**Focused validation:** Reuse leaf evidence, then perform one short camera-only and combined regression route on the integrated commit.

**Evidence/deliverables:** `docs/evidence/p2/gate.md`, `docs/STATUS.md`.

**Out of scope:** No public release or broad compatibility guarantee.
