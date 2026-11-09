# Phase 5.5 — Fixed isometric camera, playable everywhere (proposal)

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Phase 5](PHASE_5.md) · [Phase 5 research](../planning/phase5-targeting-camera-research.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

**Status: proposal, 2026-11-09. No tickets exist and nothing here is implemented.** It follows the Phase 5 owner ACCEPT ([D041](../DECISIONS.md)) and sits before any Phase 6. It is a camera-only phase: no UI, Selection or packaging scope except where the camera forces it. The owner's three scope answers of 2026-11-09 are recorded as [D042–D044](../DECISIONS.md); the remaining decisions below are engineering proposals until the owner rules on them. Feasibility statements are taken from the Phase 5 research, the DYL-199 spike record and the DYL-201 evidence; anything not measured in game is marked **unverified**.

## Outcome

Exit: with `[FixedCamera]` enabled, the camera keeps one pitch, one height and one distance for the whole session, turns only in 90° steps on a player input, and is **never** moved by collision. In every location type in the coverage matrix, and in the measured share of all vanilla and DLC interiors agreed under D55-6, the player character, the floor around them and nearby actors stay visible because the geometry between camera and player is faded, cut away or not drawn, and the room the player stands in is still rendered. Play needs no first-person fallback for visibility. With the feature off, behaviour equals Phase 5 candidate 8.

## Where the camera stands today

Source: `modules/camera/src/plugin.cpp`, the policy headers, `docs/evidence/p5/fixed-height-camera.md` and the DYL-199 spike record on `phase5/dyl-199-occlusion-spike`.

| Area | Today | Gap for a fixed camera |
| --- | --- | --- |
| Pitch | Held at `fPitchDegrees=55`; the player cannot change it. | None; the hold already exists. |
| Yaw | Free orbit on mouse X / right stick; F7 recenters. | No locked heading and no snap turn. |
| Distance | Wheel zoom, bounded `fMinZoom`–`fMaxZoom`, persisted. | No fixed distance. |
| Collision | SmoothCam's native hull cast pulls the camera in on every wall, ceiling and column. The DYL-201 caster detour passes only three curated beam forms (D033). | The camera still drops whenever real enclosure is hit: z swings of 288 → 552 while walking the Bannered Mare, and a 200-unit pop at one column. |
| Visibility | Three beam forms fade to alpha 0.35 through private material clones, capped at 8 references. | Character-in-view is **FAIL** at the corridor, rock-ceiling and doorway routes. The clone mechanism refuses shared shaders and will not scale to walls or ceilings. |
| Room culling | Not handled. The spike's room-seeding and low-ceiling culling rows are **NOT_RUN**. | A camera outside the player's room may draw nothing. This has never been measured on 1.7.104 and is the largest unknown. |
| Location awareness | None: no interior/exterior, storey, stair or doorway logic. | Upper floors, roofs and overhangs need a rule for what is "above the player". |
| Special states | Mount, swim, furniture, bleedout, beast forms, killmoves and scripted cameras hand back to native. | Each needs a decision: hold the fixed view or hand off. |
| Measurement | The probe reports player position, cell, camera state and frame stats. It has no camera position, no line of sight and no player screen position. | "Player visible" cannot be asserted by a route today; evidence is screenshots and the thin-occluder trace. |

Two spike results matter for the design. `SetAppCulled` on a reference both hid it **and** stopped it from blocking the camera (the native caster skips culled objects), giving a full-height view in that run. The per-draw near-plane clip was refused because its projection helper takes an argument the reference source does not pass; it remains untested, not disproven.

## What this phase changes in principle

These are recorded so that the plan does not quietly contradict earlier documents:

- [VISION](../VISION.md) principle 1 says playability wins over holding an overhead angle. In fixed mode the angle is held and playability must come from visibility. The principle still applies with the feature off.
- [MVP scope](../MVP_SCOPE.md) defers "universal roof/wall cutaways", and D5-5 limited Phase 5 to thin beams with walls and ceilings needing separate approval. This phase is that separately approved Stage 2, for fixed mode only.
- D028 says the automatic classifier must not choose what the camera passes through. In fixed mode nothing chooses: collision is ignored for everything. A classifier is only ever used for what to draw, where a wrong answer costs a visual error rather than a camera inside a wall.
- D012 keeps SmoothCam as the only third-person transform writer. D55-1 below proposes keeping that; a replacement controller needs an owner ruling.
- True orthographic projection stays deferred. "Isometric" here means a fixed perspective view.

## Principles

- The fixed mode is opt-in (`[FixedCamera] bEnabled=0`) through the whole phase. Whether it becomes the default is a gate question.
- A technical failure never substitutes a different product behaviour. If a location type cannot be made visible, it is recorded as FAIL with the tested alternatives and returned to the owner. The camera does not quietly pull in, tilt or switch to first person there.
- Rules, not lists. The Phase 5 curated-form list cannot cover the game or any mod. What is faded or cut is decided from geometry relative to the player and camera, with form lists only as exceptions.
- Presentation only. Nothing changes actor or player collision, navmesh, activation rules, saves or reference persistence. Every visual change is restored on every lifecycle exit.
- State precedence is unchanged (disable → invalid world → special → menu → first person → precision → target lock → obstruction → normal). Fixed mode is a property of the normal and target-lock states. F8 remains the emergency exit and F6 remains available as a player choice.
- Process carried from Phase 5: a binary-checking independent review precedes the first game run of any native change; a fix on a path shared by a feature flag is rerun in both flag states; every route asserts the probe state it claims rather than only sending keys.

## Decisions

### Taken by the owner on 2026-11-09

| ID | Decision |
| --- | --- |
| D042 | Fixed means pitch, height and distance never change and there is no free orbit. The view turns in 90° steps on a player input. |
| D043 | Where walls, ceilings or upper floors block the view, the look (fade, cutaway or clip) is whichever the spike proves per location type. The owner reviews screenshots of each look before the feature is built. |
| D044 | Acceptance needs both a hand-picked location matrix with an owner playtest and an unattended sweep of every vanilla and DLC interior that measures player visibility. |

### Required before tickets are ready

| ID | Decision | Default proposed by engineering |
| --- | --- | --- |
| D55-1 | Who writes the camera transform in fixed mode. | **SmoothCam stays the only writer.** The existing caster detour (ID 33007) reports "no hit" for every cast while fixed mode is eligible, so SmoothCam never pulls in. Taking `RequestCameraControl` and owning the transform is evaluated in the spike only as the fallback if SmoothCam's interpolation or offsets fight a fixed rig, and needs an owner ruling under D012. |
| D55-2 | The four headings and the snap-turn inputs. | Four diagonal headings (NE, SE, SW, NW) so two far walls are seen and two near walls are cut, with the spike comparing against cardinal headings in screenshots. Controller: a right-stick flick left/right, because the stick's yaw axis is unused in fixed mode and no new button is needed. Keyboard: two keys proposed after a binding audit, returned to the owner as a tested proposal. F7 becomes "return to the default heading". |
| D55-3 | Whether any zoom remains. | One configured distance, no wheel zoom, matching "100% fixed". An optional small set of fixed zoom steps can be added later if the owner wants it. |
| D55-4 | What the freed look inputs do. | A bounded "peek": holding the stick or mouse shifts the framing a limited distance toward that direction and springs back. This is the proposed answer to enemies outside the frame on the near side. Decided after the spike shows how much of the scene is visible. |
| D55-5 | Which special states keep the fixed view. | Keep it for sneak, combat, drawn bow/spell, target lock, surface swimming, mount, beast forms, bleedout and dialogue. Hand off for killmoves, scripted cameras, furniture/crafting and menus, and return to the same heading afterwards. Underwater and dragon riding are decided on spike evidence. |
| D55-6 | Acceptance thresholds for "playable". | Proposed in [Acceptance measures](#acceptance-measures); the owner approves or changes the numbers before the gate, not after. |
| D55-7 | Content in scope. | Skyrim.esm, Dawnguard, Hearthfire and Dragonborn on the pinned 1.7.104 host. Creation Club content and mods are untested and stated as such. |
| D55-8 | Framing. | Perspective projection with field of view, pitch and distance tuned once in the spike and then frozen. A longer, narrower view looks more isometric but puts the camera outside more rooms; the spike records that trade with screenshots. |

## Coverage matrix

The matrix is the hand-picked half of D044. Locations are examples to be confirmed as fixtures in P5.5-01; each row names the problem it exists to catch.

| # | Location type | Example fixtures | What usually breaks |
| --- | --- | --- | --- |
| 1 | Small single-storey interior | Riverwood Trader, Alvor's house | Camera is outside the building shell; near walls and roof fill the screen. |
| 2 | Multi-storey house or inn | Bannered Mare loft, Breezehome, Candlehearth Hall | The floor above hides the player; on the upper floor, the lower floor must stay visible through the stairwell. |
| 3 | Great hall or palace | Dragonsreach, Blue Palace, Understone Keep | Tall ceilings, balconies, columns, several rooms and portals in view. |
| 4 | Nordic ruin | Bleak Falls Barrow | Low corridors, doorframes, puzzle chambers, traps that must be seen before they fire. |
| 5 | Cave or mine | Embershard Mine, Brittleshin Pass | Floor, walls and ceiling are one mesh, so hiding a reference removes the floor too. |
| 6 | Dwemer ruin | Alftand, Mzulft, Nchuand-Zel | Pipes and gantries overhead, lifts, very tall chambers, moving parts. |
| 7 | Blackreach and other interior worldspaces | Blackreach, Soul Cairn, Apocrypha, Forgotten Vale | Huge volumes, unusual fog, geometry that moves. |
| 8 | Fort, keep and prison | Helgen Keep, Fort Greymoor | Spiral stairs, cells, stacked floors joined by narrow shafts. |
| 9 | Tower and spiral stair | Western Watchtower, Frostflow Lighthouse, College towers | The player is always directly below more of the same staircase. |
| 10 | Sewer and tunnel | The Ratway, Riften canal level | Tunnels narrower than the cut, walkways above the player. |
| 11 | Ship, shack, tent and cellar | The Katariah, Hearthfire cellar, Falmer tents | Very low ceilings and tiny rooms. |
| 12 | City exterior | Markarth, Windhelm alleys, Solitude gate, Riften | City walls, arches, overhangs and upper walkways between camera and player. |
| 13 | Wilderness exterior | Falkreath forest, a mountain path, a cliff base | Tree canopy, cliffs and rock faces on the camera side. |
| 14 | Bridges, piers and cave mouths | Dragon Bridge, Riften docks, any cave entrance | The player walks under a structure that is not a roof. |
| 15 | Water | River crossing, lake swim, a shipwreck | Camera above the surface with the player below it. |
| 16 | Vertical combat | A dragon fight, archers on a ledge, a giant camp | Threats above or outside the frame; tall actors. |
| 17 | Transitions | Load doors, trapdoors, ladders, Dwemer lifts, fast travel | First frame after a load, before any cutaway has been computed. |
| 18 | Handed-off states | Mount, werewolf, Vampire Lord, crafting, lockpicking, killmove, the Helgen cart | Leaving and re-entering the fixed view without a jump or a lost heading. |

### Things the request did not list

These came out of the survey and are folded into the work items:

1. **Room culling.** Skyrim interiors draw by room and portal. A camera outside the player's room can render an empty screen. Unverified on 1.7.104; first question for the spike.
2. **One-piece meshes.** Caves and many Dwemer and Nordic pieces include floor and ceiling in one model. Hiding per reference cannot work there; a per-draw clip or a shader discard is needed.
3. **Fog.** Interior fog is set per cell for a close camera. A camera several hundred units away may see the player through fog. Unverified.
4. **Light and shadow.** Hidden ceilings may change which lights and shadow casters the engine selects, and leave a black void above interiors. Unverified.
5. **Sound.** If the audio listener follows the camera, a distant fixed camera changes volume and occlusion of sounds near the player. Unverified.
6. **Threats outside the frame.** A fixed pitch shows less ground on the camera side. Enemies there, above (dragons, ledges) or on another floor can attack unseen.
7. **Things hidden by the cut.** Wall levers, pull chains, shelves, chests on upper ledges and enemies on a balcony may sit above the cut line and disappear with the ceiling.
8. **Aiming across height.** Target lock covers locked ranged attacks; unlocked aim up or down a staircase from a fixed view is untested.
9. **First frame after a load.** The player must not arrive behind an uncut wall.
10. **Mods and Creation Club.** New architecture cannot be on any list; only rule-based handling covers it, and it stays untested.
11. **Cost.** Seeding extra rooms and re-rendering faded geometry adds draw calls; the Phase 4 frame budget still applies.
12. **Minimap heading.** With snap turns the UI minimap may need to follow the camera heading. The UI module stays independent, so this is a note for Phase 6, not scope here.

## Acceptance measures

Proposed under D55-6. P5.5-01 builds the instrument; the numbers are placeholders for the owner to set once the baseline is measured.

- **Rig hold.** In every fixed-mode sample the camera position equals player position plus the configured offset for the current heading, within a recorded tolerance. Any pull-in is a FAIL, not a degraded pass.
- **Player visible.** Rays from the camera to a fixed set of points on the player (head, chest, feet, both shoulders) are tested against what is actually drawn, so faded or hidden geometry does not count as blocking. Proposed: at least 4 of 5 points clear in 98% of samples per route, and no interval longer than 1 s with the player fully hidden.
- **Surroundings visible.** The same test to a ring of floor points around the player. Proposed: at least 75% of the ring clear in 95% of samples, so the player can see where they are walking and who is next to them.
- **Room drawn.** A screenshot check that the frame is not empty or void where the floor should be. Proposed: zero blank frames.
- **Sweep.** Every interior cell in scope is entered, sampled at its arrival pose and along a short walk, in each of the four headings. Proposed: 97% of cells pass the three measures above; every failing cell is listed with its location type and a disposition.
- **Owner route.** The owner plays one fixture from each matrix row without using F6 for visibility and records ACCEPT or specific revisions.

## Work items

### P5.5-01 — Visibility instrument, fixtures and baseline

**Lane:** test infrastructure. **Dependencies:** none; starts at kickoff. **Decisions:** D55-6, D55-7.

**Scope**

- Extend the never-shipped test probe with camera position and orientation, the player's projected screen box, and the ray tests defined above against rendered geometry. Keep it out of every shipped archive, as today.
- Enumerate the interior cells in scope from the masters with the existing offline scanner and generate sweep routes in batches that fit the 20-minute route limit. Measure the time per cell first and state the real size of the sweep before committing to it.
- Author one fixture per matrix row from reviewed player poses, following the route-fixture rules learned in Phase 5.
- Run the matrix once on candidate 8 with fixed height on and off. This baseline says where the current camera fails and by how much, and it is the comparison for every later run.

**Acceptance criteria**

- [ ] The probe's visibility numbers agree with screenshots on at least three fixtures where the answer is obvious (open ground, behind a wall, under a beam).
- [ ] The baseline table exists for all 18 rows, with NOT_RUN where a fixture could not be built and why.
- [ ] The sweep's measured duration and batch plan are recorded.

**Evidence/deliverables:** `docs/evidence/p5.5/visibility-instrument.md`, `docs/evidence/p5.5/baseline.md`, probe README.

**Out of scope:** No change to the shipped Camera DLL.

### P5.5-02 — Spike: fixed rig and enclosure see-through on 1.7.104

**Lane:** camera. **Dependencies:** none; runs beside P5.5-01. **Decisions:** D043, D55-1, D55-2, D55-8.

Measure before building, as P5-04 did. Disposable builds only. The spike ends with **GO / REVISE / HOLD per location type**, because one mechanism is unlikely to win everywhere.

**Scope**

- **Rig.** Lock yaw to a heading in the existing `look` hook, fix the distance, and make the caster detour report no hit for every SmoothCam cast. Record whether SmoothCam stays stable with no collision at all, across a load door and through F6/F8. Only if it does not, test the `RequestCameraControl` route.
- **Room culling.** Hold the rig above a low ceiling and outside a small building and record what is drawn. Then test `BSMultiBoundRoom::QPointWithin` room seeding (player's room only; rooms on the sightline; rooms within a radius) for correctness and frame cost. The hook target was checked against the binary in the DYL-199 spike; its in-game rows were NOT_RUN. This row decides whether the phase is feasible in interiors at all.
- **See-through mechanisms**, each on a kit interior, a cave and a Dwemer chamber:
  1. Per-reference hide (`SetAppCulled`) for everything above a cut height over the player or between camera and player. Proven to hide; expected to fail on one-piece meshes.
  2. Per-reference fade with the Phase 5 material clones, to find where shared shaders make it refuse.
  3. Per-draw clip. Resolve the projection helper's argument on 1.7.104 first; if it cannot be resolved, record the row as BLOCKED.
  4. A world-space discard in the pixel shader (everything above a plane through the player's head, or inside a cone from camera to player). Record what renderer hooking it needs and whether that is acceptable without a new dependency.
- **Side effects.** For the best candidate, capture fog, lighting and shadows, sound near the player, water, and the black backdrop above interiors.
- **Framing.** Screenshots at the proposed pitch, distance and field of view, at diagonal and cardinal headings, and during a snap turn.

**Acceptance criteria**

- [ ] A table per location type: mechanism, result (Transparent / Hidden / Clipped / FAIL / BLOCKED), screenshots, frame-time delta, side effects.
- [ ] A written outcome per location type and a recommended combination, with the open risks stated.
- [ ] The owner has seen the looks and ruled on D043's choice per type and on D55-1, D55-2, D55-4 and D55-8.
- [ ] No spike code is merged into the shipped Camera DLL.

**Evidence/deliverables:** `docs/evidence/p5.5/fixed-camera-spike.md`, `docs/DECISIONS.md` entries.

**Out of scope:** No Community Shaders dependency, no copied GPL code, no production feature.

### P5.5-03 — Fixed rig: locked pitch, height and distance with snap turns

**Lane:** camera. **Dependencies:** P5.5-02 rig rows PASS; D55-1, D55-2, D55-3 ruled.

**Scope**

- `[FixedCamera]` settings: enable, heading set, default heading, distance, pitch, snap-turn duration, bindings. Validation and refusal follow the existing INI rules.
- Yaw lock and snap turn in the `look` hook; the turn is a short eased rotation that cannot be interrupted into a non-90° heading. Heading survives menus, loads and handed-off states.
- Collision off for every cast while fixed mode is eligible, replacing the curated-beam path in that mode. With fixed mode off, DYL-201 behaves as shipped.
- Interaction with existing features: the DYL-157 compression policy is suppressed; target lock keeps working with its zero yaw and pitch adjustment; cursor mode keeps its own input ownership; F7 returns to the default heading.
- Movement stays camera-relative through TDM, so "up" on the stick is always away from the camera at the current heading.

**Acceptance criteria**

- [ ] Rig hold passes on every matrix fixture: zero pull-in samples, including walking into walls, under beams, through doorways and up stairs.
- [ ] Snap turns land on exactly four headings on both devices, do not fire during menus, and cause no duplicate native action.
- [ ] Load, death and reload, F6 and return, F8 off and on, and each handed-off state return to the same heading and distance.
- [ ] With `bEnabled=0`, the candidate-8 routes for camera controls and fixed height still pass.

**Evidence/deliverables:** `docs/evidence/p5.5/fixed-rig.md`, `modules/camera/config/SkyCRPG_Camera.ini`, `docs/CONTROLS.md`.

**Out of scope:** Visibility. After this item the rig is correct and the view is often blocked; that is expected.

### P5.5-04 — Enclosure see-through and room visibility

**Lane:** camera. **Dependencies:** P5.5-02 outcome GO for at least kit interiors; P5.5-03.

**Scope**

- Implement the mechanism or combination the owner approved, driven by rules: geometry above a cut height over the player's head, and geometry between the camera and the player, is faded, hidden or clipped; the floor, the player, actors and everything below the cut stay drawn.
- Storey rule: the cut height follows the floor the player is standing on, with hysteresis on stairs so the view does not flicker between storeys.
- Room visibility: the selected seeding mode, so the player's room and the rooms visible from it are drawn when the camera is outside them.
- Enter and exit hysteresis, a per-frame cap, and restoration on every lifecycle exit, as in DYL-201. No change is ever left on a reference across a save.
- Exceptions list for forms that must never be cut (for example a lever above the cut line) or must always be cut, kept small and justified per entry.

**Acceptance criteria**

- [ ] Player visible, surroundings visible and room drawn pass on matrix rows 1–4, 10 and 11.
- [ ] No flicker over 30 seconds standing at a storey boundary, a doorway and a room portal.
- [ ] Everything changed is restored after F6, F8, menu, load door, death and feature off, checked by the probe, not only by eye.
- [ ] Frame-time overhead stays within the Phase 4 budget on the recorded host for a small interior and a great hall.

**Evidence/deliverables:** `docs/evidence/p5.5/see-through.md`, INI, `docs/VALIDATION.md`.

**Out of scope:** Exteriors and one-piece caves unless the approved mechanism already covers them; those are P5.5-05.

### P5.5-05 — Coverage hardening by location type

**Lane:** camera. **Dependencies:** P5.5-04. Iterative; one pass per group of matrix rows.

**Scope**

- **One-piece and unusual interiors** (rows 5–7): caves, mines, Dwemer chambers, Blackreach and the DLC worldspaces, using the per-draw or shader mechanism if the spike selected one.
- **Vertical interiors** (rows 8 and 9, and whatever row 2 left open): stairwells, spiral stairs, shafts, lifts and balconies; what remains visible below and above the player.
- **Exteriors** (rows 12–14): overhangs, arches, city walls, bridges, piers, cave mouths, tree canopy and cliffs. Roofs of buildings the player is not inside are left alone.
- **Water** (row 15): surface swimming and the underwater decision from D55-5.
- **Fog, light and sound** corrections found by the spike, each as its own small change with before and after captures.
- Run the full interior sweep, group failures by cause, fix the causes that cover many cells, and record the rest.

**Acceptance criteria**

- [ ] Every matrix row has a PASS or a FAIL with the tested alternatives and an owner disposition; none is NOT_RUN.
- [ ] The sweep meets the D55-6 threshold, and the failing cells are listed by name with their cause.
- [ ] No fix for one location type regresses another: the whole matrix is rerun after each pass.

**Evidence/deliverables:** `docs/evidence/p5.5/coverage.md`, `docs/evidence/p5.5/sweep.md`.

**Out of scope:** Mod and Creation Club content.

### P5.5-06 — Playing under a fixed view

**Lane:** camera. **Dependencies:** P5.5-03; runs beside P5.5-05. **Decisions:** D55-4, D55-5.

**Scope**

- Threat awareness: the peek control if approved, and a check that enemies on the camera side, above the player and on other floors can be seen or located in time to react.
- Aiming: melee, locked bow and spell across a height difference, and the unlocked case recorded honestly as PASS or as a limitation.
- Activation: objects above the cut line and behind cut walls; Selection must not offer what the player cannot see, or must show it.
- Special states per D55-5, each with an enter and return check.
- Transitions (row 17): the correct view on the first frame after a load door, fast travel and a lift.

**Acceptance criteria**

- [ ] A bounded combat route with a melee enemy, an archer on a ledge and one airborne or tall enemy completes without F6 on both devices.
- [ ] The special-state table has a result for every state in D55-5.
- [ ] No transition shows a blocked or empty first second.

**Evidence/deliverables:** `docs/evidence/p5.5/fixed-view-play.md`, `docs/CONTROLS.md`.

**Out of scope:** Cursor-aimed projectiles, click-to-move and party commands.

### P5.5-07 — Phase 5.5 gate

**Lane:** gate. **Dependencies:** P5.5-03 to P5.5-06 or their recorded dispositions.

**Scope**

- Full matrix and full sweep on the final candidate, with the probe measures and screenshots.
- Regression with fixed mode off (equals candidate 8) and on, in camera-only and combined profiles, and the Phase 4 performance capture on a small interior, a great hall, a cave and an exterior.
- Owner playtest across one fixture per matrix row on both devices.

**Acceptance criteria**

- [ ] Every acceptance measure meets the owner-approved threshold, or its shortfall is listed as a carried limitation the owner accepted by name.
- [ ] No control lock, stranded heading, permanently hidden geometry or blank view remains in the recorded routes.
- [ ] Owner records ACCEPT or specific revisions, and rules on whether fixed mode becomes the default.

**Evidence/deliverables:** `docs/evidence/p5.5/gate.md`, `docs/STATUS.md`, `docs/RELEASE_CHECKLIST.md`.

## Sequencing

```
Phase 5: DYL-202 owner ACCEPT (D041)
  → P5.5-01 (instrument, fixtures, baseline) ‖ P5.5-02 (spike)
  → owner reviews the spike: looks per location type, D55-1 … D55-8
  → P5.5-03 (fixed rig)
  → P5.5-04 (see-through and room visibility)
  → P5.5-05 (coverage hardening, sweep) ‖ P5.5-06 (play under a fixed view)
  → P5.5-07 gate
```

P5.5-01 and P5.5-02 touch disjoint files (probe and routes versus a disposable Camera build) and can run in parallel; game sessions stay serialized on the one host. P5.5-03 to P5.5-06 all edit `plugin.cpp` and the Camera INI and need one integrator and a fixed merge order. Relative effort: P5.5-01 medium, P5.5-02 large and decisive, P5.5-03 small to medium, P5.5-04 large, P5.5-05 large and open-ended until the sweep converges, P5.5-06 medium, P5.5-07 medium.

The phase can stop early with a useful result. If the spike returns HOLD on room culling, the rig (P5.5-03) still ships as an exterior-and-large-hall mode and the interior problem goes back to the owner with the measured alternatives.

## Risks specific to this phase

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Rooms are not drawn when the camera is outside them. | Blank or partial interiors; the phase is not achievable indoors. | First spike row; the seeding hook target is already checked against the binary; HOLD and an owner decision if no mode works within budget. |
| No mechanism handles one-piece cave and Dwemer meshes. | Caves and mines stay blocked, a large share of dungeons. | Per-draw clip and shader discard are evaluated before any production work; per-type outcome lets kit interiors proceed while caves are decided. |
| The shader route needs a renderer dependency. | Conflicts with the "no Community Shaders dependency" boundary. | Recorded as a spike finding and an owner decision, never adopted silently. |
| SmoothCam misbehaves with collision removed. | Jitter or drift in the rig. | Spike rig row; fallback to owning the transform under an owner ruling (D55-1). |
| Rule-based cutting hides something the player needs. | Missed levers, loot or enemies above the cut. | Cut height margin, exceptions list, Selection check in P5.5-06, sweep screenshots. |
| Fog, lighting or sound change with a distant camera. | The game looks or sounds wrong even when visible. | Measured in the spike; each correction is its own change with captures. |
| Extra rooms and faded geometry cost frames. | Budget exceeded in large interiors. | Cost measured per seeding mode in the spike; cap and hysteresis; budget rows at the gate. |
| The sweep is too long to run repeatedly. | Coverage is claimed from a stale run. | Duration measured in P5.5-01; rerun only the affected location types between passes, the full sweep at the gate. |
| "100%" cannot be reached. | The exit is never met. | Thresholds are set by the owner from the baseline; every failing cell is named, so the remaining gap is a decision rather than an unknown. |
| Snap-turn bindings collide with native controls. | Duplicate actions or an unusable controller. | Binding audit and the existing refusal of natively mapped inputs; owner approves the tested proposal. |
