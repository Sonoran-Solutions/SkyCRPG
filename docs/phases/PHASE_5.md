# Phase 5 — Targeting, lock-on and fixed-height camera (post-MVP proposal)

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Research and feasibility](../planning/phase5-targeting-camera-research.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

**Status: proposal, 2026-10-28.** This phase starts only after the Phase 4 gate (DYL-175) records MVP acceptance; it does not change the four-phase MVP commitment in [ROADMAP](../ROADMAP.md). No Linear tickets exist yet; IDs below are plan IDs. Items marked **owner decision** need a ruling before the corresponding ticket is ready. All feasibility statements come from the research document; nothing in this phase has in-game evidence.

## Outcome

Exit: the Camera candidate lets the player activate the right nearby object from the elevated view on both input devices, cycle candidates, select by pointer or stick reticle, lock on to an enemy and land ranged attacks from the elevated camera, and keeps its configured height past thin overhead geometry (beams, doorframes, lintels) which becomes see-through instead of pulling the camera in. Existing C1–C5 behaviour, F6/F8 recovery and module independence remain intact; every new feature is individually disable-able and defaults are recorded.

## Principles carried over

- SmoothCam stays the only normal third-person transform writer. Nothing in this phase takes `RequestCameraControl`.
- External providers stay external and pinned by hash; the Camera module talks to them through exported APIs only. New GPL providers (BTPS, a BTPS fork if needed) ship as separate packages with corresponding source.
- One input consumer per event. New bindings refuse natively mapped keys/buttons exactly as the existing toggle/recenter bindings do.
- State precedence is unchanged: disable → invalid world → special → menu → first person → precision → **target lock** → obstruction → normal. Lock and cursor mode release on every higher state.
- Each work item ends with a bounded owner route and a PASS/FAIL/BLOCKED/NOT_RUN table in `docs/evidence/p5/`.

## Decisions required before tickets are ready

| ID | Decision | Default proposed by engineering |
| --- | --- | --- |
| D5-1 | Accept BTPS (GPL-3, needs JContainers + MCM Helper) as an external Camera-lane dependency, and a GPL fork of it if the target-injection API cannot be upstreamed. | Yes; fork only after an upstream request is declined or unanswered for the phase. |
| D5-2 | Controller binding budget for lock toggle, cycle and cursor mode; whether TDM's short-press POV lock toggle is re-enabled. | Short-press POV = lock toggle (hold = native POV); cycle and cursor-mode buttons chosen by the owner from unmapped inputs or an explicitly approved chord. |
| D5-3 | Keyboard/mouse cursor behaviour: toggle key, or CRPG-style hold-right-mouse-to-rotate with the pointer free otherwise. | Toggle first; hold-to-rotate evaluated as an option in P5-03. |
| D5-4 | Lock reticle source for Camera-only installs: TrueHUD as an optional external dependency, or an original minimal indicator. | TrueHUD if a 1.7.104 build exists; otherwise crosshair-hide only, with the indicator deferred. |
| D5-5 | Scope of see-through: thin overhead occluders only (stage 1) versus walls and ceilings generally (stage 2, depends on the culling spike). | Stage 1 in this phase; stage 2 only on spike PASS. |
| D5-6 | Fate of the DYL-157 compression policy once fixed height ships. | Keep as the fallback for true enclosure; it stays default OFF unless the owner prefers it. |

## Work items

### P5-01 — Enable TDM target lock under the held-pitch camera

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-2, D5-4.

Turn the BLOCKED Phase 2 target-lock item into a working feature by configuring the existing provider instead of writing a second camera writer.

**Scope**

- Replace the unconditional `RequestDisableTargetLock` lease with a policy-driven one: hold the lease only while a higher-priority state (menu, first person, special, invalid world) is active; release it in eligible third person.
- Extend the TDM settings fragment: `uTargetLockMode` relaxed, `fTargetLockYawAdjustSpeed=0`, `fTargetLockPitchAdjustSpeed=0`, `bResetCameraWithTargetLock=0`, `bTargetLockEnableLockBehindTarget=0`, arrow and missile aim type Predict, scroll-wheel switching off, and the owner-decided toggle/switch bindings. Keep the merge-not-replace rule.
- Add the `targetLock` state to the Camera policy: held pitch and user yaw stay active, obstruction is suppressed, F6/F8 keep precedence, lock is dropped on menu/load/death/special by the existing barrier; expose `GetCurrentTarget` to the lifecycle trace.
- Resolve the reticle source per D5-4 and pin any new provider in the dependency lock.

**Acceptance criteria**

- [ ] With lock on, the camera's world pitch and user yaw are unchanged frame to frame (trace evidence), and a drawn bow and an aimed spell hit a hostile at medium range from the elevated view in a bounded route; this closes the Phase 1 overhead ranged FAIL for the locked case.
- [ ] Lock toggles on/off and switches targets with the decided inputs without moving the camera or the zoom; mouse look and wheel zoom still work while locked.
- [ ] Lock releases on dialogue, menus, loading, death, F6 first person and F8, and the camera recovers through the existing barrier; disabling the feature restores the original lease behaviour.

**Focused validation:** policy tests for the lease/precedence table; one Camera-only route with melee, bow and spell versus two hostiles, target death and auto-retarget, menu and F6/F8 interruptions, controller and keyboard.

**Evidence/deliverables:** `docs/evidence/p5/target-lock.md`, `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini`, `docs/CONTROLS.md`, `docs/compatibility/dependency-lock.md`.

**Out of scope:** No corrected free-aim (unlocked) ranged fire from the elevated view; F6 remains that fallback until P5-03's optional cursor-aimed projectiles.

### P5-02 — Proximity activation and cycling through BTPS

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-1, D5-2.

Make the bench in the Bannered Mare selectable from the default framing without F6.

**Scope**

- Pin BTPS 0.9.0 and JContainers for the 1.7 line in the dependency lock with hashes; preflight them like the minimap trio. Verify in game that BTPS, SmoothCam 1.8.1, TDM 2.3.1 and the Camera DLL load together.
- Ship a declared BTPS configuration delta: proximity-weighted scoring under the held pitch (tune `fMaxAngleDif` and distance weight), cycle keys per D5-2, widget placement that does not collide with the SkyCRPG HUD, hide-during-lock through its TDM integration.
- Document the interaction with native prompts and QuickLoot-style mods; no SkyCRPG hook touches the crosshair pick in this item.

**Acceptance criteria**

- [ ] From the default elevated framing, Activate selects the nearest bench, door, container, item and NPC in a tight interior route; the Bannered Mare bench case passes without F6.
- [ ] Cycling steps through visible candidates in a stable order on keyboard and controller and returns to the closest; the widget shows the current candidate.
- [ ] Disabling or removing BTPS restores native activation; Camera-only without BTPS still loads and behaves as before.

**Focused validation:** one Camera-only interior/exterior activation route with low furniture, stacked loot, doors and an NPC; one provider-absent startup.

**Evidence/deliverables:** `docs/evidence/p5/proximity-activation.md`, `docs/compatibility/dependency-lock.md`, `docs/INSTALLATION.md`, `docs/CONTROLS.md`.

**Out of scope:** No original selection scoring, no auto-loot, no change to activation range rules.

### P5-03 — Cursor mode: pointer hover and controller reticle

**Lane:** camera. **Dependencies:** P5-02. **Decisions:** D5-1, D5-3.

Add the one targeting behaviour nothing provides: pick by pointer on PC and by right-stick reticle on controller, and feed that pick into the same activation path.

**Scope**

- Obtain a target-injection API from BTPS (upstream request first, GPL fork second per D5-1): "while an external provider supplies a reference, use it as the selection".
- Implement cursor mode in the Camera module: a toggle on both devices; a cursor/reticle overlay owned by Camera (native cursor via a non-pausing menu, or an original drawn reticle); screen-point to world ray and a hull pick with the native activation radius/length rules; actors, furniture, doors, containers and items as candidates; the look hook consumes right-stick/mouse deltas while the mode is active.
- Release the mode on menus, loading, death, first person, special states and F8; never leave the cursor visible over gameplay after a lifecycle exit.
- Optional stretch, only if P5-01 is PASS: when cursor mode hovers a hostile with a bow or missile spell drawn, aim the projectile at the hovered point (same vfunc hook pattern TDM uses), closing the unlocked ranged gap.

**Acceptance criteria**

- [ ] On keyboard/mouse, hovering an object shows the selection and Activate acts on it; on controller, the reticle moves with the right stick and Activate acts on the reticle target; cycling still works in cursor mode.
- [ ] No duplicate activation and no camera rotation from cursor input; leaving cursor mode restores yaw control and hides the cursor in every tested exit.
- [ ] Provider-absent (no BTPS) cursor mode degrades to a documented safe state rather than a half-working overlay.

**Focused validation:** pointer and stick routes over the same interior fixture as P5-02; menu/F6/F8/load interruptions while the cursor is shown; performance sample of the per-frame pick.

**Evidence/deliverables:** `docs/evidence/p5/cursor-mode.md`, `docs/CONTROLS.md`, `docs/UX_SPEC.md`, fork repository and notices if D5-1 chose a fork.

**Out of scope:** No click-to-move, no RTS panning, no inventory drag-and-drop from the world cursor.

### P5-04 — Spike: fixed-height camera and occluder handling on 1.7.104

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-5.

Measure before building. Three unverified mechanisms decide whether R4 is a bounded feature or a renderer project.

**Scope**

- Camera caster detour: hook the engine camera caster the pinned SmoothCam calls, log hit object, layer, normal and whether the goal point is in open space for the Bannered Mare beams, a dungeon corridor and an exterior; prove that discarding a hit keeps SmoothCam stable (no second transform writer, F8 still works).
- Fade mechanism: on a known beam reference try `SetAppCulled`, `UpdateMaterialAlpha`, and `NiAlphaProperty` injection; record which renders see-through, whether the engine resets it, and the cost; evaluate the Disocclusion near-plane approach only if 1.7.104 render offsets can be found.
- Culling: hold the camera above a low ceiling and record what disappears; measure the `BSMultiBoundRoom::QPointWithin` workaround's draw-call cost in one large interior.
- One evaluation session of Camera Studio as an alternative isometric provider, recorded as reference only.

**Acceptance criteria**

- [ ] An evidence table states PASS/FAIL per mechanism with screenshots and frame-time deltas on the pinned stack.
- [ ] A written recommendation selects stage 1 geometry (thin overhead occluders) and either green-lights or defers stage 2 (walls/ceilings), with the room-bounds finding attached.
- [ ] No spike code is merged into the shipped Camera DLL; disposable builds are hashed and retained like earlier diagnostic candidates.

**Focused validation:** disposable Camera builds on an isolated profile; bounded 20-minute sessions per fixture; restoration verified.

**Evidence/deliverables:** `docs/evidence/p5/occlusion-spike.md`, `docs/DECISIONS.md` entry for D5-5.

**Out of scope:** No Community Shaders feature, no provider fork, no new camera controller.

### P5-05 — Fixed-height camera with see-through overhead occluders

**Lane:** camera. **Dependencies:** P5-04. **Decisions:** D5-5, D5-6.

Deliver the CRPG feel the owner asked for within whatever P5-04 proved.

**Scope**

- Implement the caster-detour filter behind an opt-in setting: while the policy is in normal/targetLock state, discard camera hits classified as thin overhead occluders (downward-facing normal, goal point in open space inside the current room); keep collision for walls, floors and enclosure.
- Implement occluder see-through with the mechanism P5-04 selected: detect references between camera and player with a small ring of rays at a bounded rate, fade or hide with enter/exit hysteresis, restore on every lifecycle exit, cap the per-frame set, never touch the player, actors or the floor.
- Make the DYL-157 compression policy the fallback for true enclosure per D5-6; the two never fight (compression only engages when the detour kept a hit).
- Stage 2 (walls/ceilings with the culling workaround) only if P5-04 green-lit it, as a separate opt-in.

**Acceptance criteria**

- [ ] On the Bannered Mare beam route the camera height and zoom stay at the configured preference while beams become see-through; no pull-in, no near-plane pop, no oscillation over 30 seconds at the boundary.
- [ ] Corridors, low rock ceilings and doorways still collide safely (no blind walking, nothing disappears from room culling); F6/F8 and lock interruptions restore every faded object.
- [ ] Frame-time overhead of detection plus fade stays within the Phase 4 budget on the recorded host, measured on the same route.

**Focused validation:** policy/hysteresis tests; one Camera-only interior route covering beams, doorframes, stairs, a dungeon corridor and a large hall; the Phase 4 performance capture repeated once.

**Evidence/deliverables:** `docs/evidence/p5/fixed-height-camera.md`, `modules/camera/config/SkyCRPG_Camera.ini`, `docs/CONTROLS.md`, `docs/VALIDATION.md`.

**Out of scope:** No universal cutaway, no exterior roof removal, no change to SmoothCam's own files.

### P5-06 — Phase 5 gate: input matrix, regression and owner playtest

**Lane:** gate. **Dependencies:** P5-01, P5-02, P5-03, P5-05.

**Scope**

- Verify the combined binding table on keyboard/mouse and controller: lock, cycle, cursor mode, F6, F7, F8, hotbar, map and native POV with no duplicate actions.
- Run Camera-only, UI-only and combined regressions on the final candidate, including provider-absent starts for BTPS and TrueHUD and the disable-every-new-feature profile.
- Record the owner's playtest of the targeting and camera feel and the finite list of defects.

**Acceptance criteria**

- [ ] Every new feature can be turned off independently and the MVP acceptance IDs C1–C5 still pass on the candidate.
- [ ] No control lock, stuck cursor, invisible geometry or stranded lock state remains in the recorded routes.
- [ ] Owner records ACCEPT or specific revisions.

**Evidence/deliverables:** `docs/evidence/p5/gate.md`, `docs/STATUS.md`, `docs/compatibility/runtime-matrix.md`, `docs/RELEASE_CHECKLIST.md`.

## Sequencing

P5-01, P5-02 and P5-04 are independent and can start together once Phase 4 closes; P5-01 and P5-02 touch different provider fragments and different policy states, P5-04 is disposable. P5-03 follows P5-02 (needs the injection API) and benefits from P5-01 for the optional projectile stretch. P5-05 follows P5-04. The gate runs last. Estimated relative effort: P5-01 small, P5-02 small, P5-03 medium–large, P5-04 medium, P5-05 large, P5-06 medium.

## Risks specific to this phase

| Risk | Impact | Mitigation |
| --- | --- | --- |
| JContainers or TrueHUD has no 1.7.104 build. | P5-02 blocked / lock reticle missing. | Verify first; BTPS-NG style repacks are unverified; D5-4 allows crosshair-hide only. |
| BTPS declines an injection API. | P5-03 needs a GPL fork. | D5-1 pre-authorises the fork path; corresponding-source obligations as for the minimap fork. |
| TDM switching inputs conflict with yaw/zoom. | Lock usable but target switching awkward. | Fragment disables colliding inputs; owner chooses dedicated keys; worst case switching only by re-toggling. |
| Engine caster detour destabilises SmoothCam. | R4 falls back to compression policy. | Spike proves it before implementation; feature remains opt-in. |
| Static fade not achievable without renderer work. | R4 ships as hide/cutaway instead of fade. | Spike decides; hide with hysteresis is still an improvement over pull-in. |
| Room culling when the camera leaves a room. | Blank screen in dungeons if stage 2 is forced. | Stage 1 keeps collision for enclosure; stage 2 gated on spike PASS. |
| No free controller buttons. | Features unusable on controller. | D5-2 is required before tickets are ready; chords only with explicit approval. |
