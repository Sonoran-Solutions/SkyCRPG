# Phase 5 — Targeting, lock-on and fixed-height camera (post-MVP proposal)

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Research and feasibility](../planning/phase5-targeting-camera-research.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

**Status: proposal, 2026-10-28, revised after the senior review of PR #24.** This phase starts only after the Phase 4 gate (DYL-175) records owner ACCEPT; it does not change the four-phase MVP commitment in [ROADMAP](../ROADMAP.md). No Linear tickets exist yet; IDs below are plan IDs. Items marked **owner decision** need a ruling before the corresponding ticket is ready. All feasibility statements come from the research document; nothing in this phase has in-game evidence.

## Outcome

Exit: the Camera candidate lets the player activate the nearest usable object from the elevated view on both input devices, cycle candidates, select by pointer or stick reticle, lock on to an enemy with a visible indicator and land ranged attacks from the elevated camera, and keeps its configured height past positively identified thin overhead geometry (beams, lintels, doorframes) which becomes see-through instead of pulling the camera in. Existing C1–C5 behaviour, F6/F8 recovery and module independence remain intact; every new feature is individually disable-able and defaults are recorded.

## Principles carried over

- SmoothCam stays the only normal third-person transform writer. Nothing in this phase takes `RequestCameraControl`. Native SmoothCam collision stays active for genuine enclosure.
- External providers stay external and pinned by hash; the Camera module talks to them through exported APIs only. Any GPL derivative (for example a BTPS fork) ships as a separate package with corresponding source and only after the licensing boundary assessment in D5-1.
- One input consumer per event. New bindings refuse natively mapped keys/buttons exactly as the existing toggle/recenter bindings do. The controller mapping contract (D5-2) is agreed before any worker implements a binding.
- State precedence is unchanged: disable → invalid world → special → menu → first person → precision → **target lock** → obstruction → normal. Lock and cursor mode release on every higher state; native menus and dialogs always take precedence over the Camera's own overlay.
- A technical failure never substitutes a different product behaviour. If a spike or implementation cannot deliver what the owner asked for, the item stops at HOLD and returns to the owner with the tested alternatives; it does not quietly ship compression, hiding or clipping in place of see-through.
- Each work item ends with a bounded owner route and a PASS/FAIL/BLOCKED/NOT_RUN table in `docs/evidence/p5/`.

## Decisions required before tickets are ready

| ID | Decision | Default proposed by engineering |
| --- | --- | --- |
| D5-1 | BTPS as an external Camera-lane dependency, and whether a GPL fork may be made. | **Conditional yes, in three separate permissions.** (a) Evaluate and ship a declared configuration for **stock** BTPS 0.9.0 (GPL-3.0-or-later, source revision and licence pinned) as an optional external dependency. (b) Request an upstream target-injection API or an explicit exception from the author. (c) A fork is **not** pre-approved: it needs the upstream answer, a written assessment of the actual API/linking boundary with the MIT Camera module, and documented corresponding-source obligations. |
| D5-2 | Controller binding budget for lock toggle, cycle and cursor mode; whether TDM's short-press POV lock toggle is re-enabled. | Short-press POV (R3) = lock toggle, hold = native POV, with the hold duration tested; cycle and cursor-mode on an explicitly approved chord or context-specific action. No spare button is assumed to exist. |
| D5-3 | Keyboard/mouse cursor behaviour. | Toggle mode first; normal mouse-look and combat behaviour are preserved until the player enters cursor mode. Hold-right-mouse-to-rotate is investigated later as an option. |
| D5-4 | Lock indicator for Camera-only installs. | TrueHUD if a verified 1.7.104 build exists; otherwise an **original minimal Camera-owned indicator**. Lock-on without a visible target indicator is rejected as a default, because the player must see which enemy will receive a predicted projectile. UI-only independence is unaffected either way. |
| D5-5 | Scope of see-through. | **Stage 1 only** in this phase: thin beams, lintels and similar overhead occluders. Stage 2 (walls, ceilings, room visibility) needs its own feasibility evidence, visual review and frame-time measurements, and a separate owner approval even if the spike succeeds. |
| D5-6 | Fate of the DYL-157 compression policy. | Keep it as an optional recovery setting, **default OFF**. Native SmoothCam collision remains active; aggressive pull-in is never silently enabled. |

## Work items

### P5-01 — Enable TDM target lock under the held-pitch camera

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-2, D5-4.

Turn the BLOCKED Phase 2 target-lock item into a working feature by configuring the existing provider instead of writing a second camera writer.

**Scope**

- Replace the unconditional `RequestDisableTargetLock` lease with an **acknowledged** handoff. TDM processes the disable request only in its unpaused update, so: take the lease when a higher-priority state begins; keep it until `GetTargetLockState()` reports unlocked; fail closed (keep the lease, no lock state) whenever the TDM interface or lock-state information is unavailable; release only in eligible third person; require a fresh lock input after any interruption, never resume a previous lock.
- Extend the TDM settings fragment: `uTargetLockMode` relaxed, `fTargetLockYawAdjustSpeed=0`, `fTargetLockPitchAdjustSpeed=0`, `bResetCameraWithTargetLock=0`, `bTargetLockEnableLockBehindTarget=0`, arrow and missile aim type Predict, scroll-wheel switching off, and the D5-2 toggle/switch bindings. Keep the merge-not-replace rule.
- Guard live settings: extend the existing menu-close re-read of TDM's settings file to the lock keys above; if a value differs from the declared fragment, either refuse the lock state (suspend the policy and log once) or re-apply the fragment, per a recorded choice. The zero-speed behaviour is an **orientation-preservation hypothesis**: TDM still writes player pitch and a compensating camera-relative pitch each frame, and the trace must show the net world pitch/yaw unchanged before the policy depends on it.
- Add the `targetLock` state to the Camera policy: held pitch and user yaw stay active, obstruction is suppressed, F6/F8 keep precedence, lock is dropped on menu/load/death/special by the existing barrier; expose `GetCurrentTarget` and the lease state to the lifecycle trace.
- Resolve the indicator per D5-4 and pin any new provider in the dependency lock.

**Acceptance criteria**

- [ ] Trace evidence shows the camera's world pitch and user yaw unchanged frame to frame while locked (hypothesis confirmed), and a drawn bow and an aimed spell hit a hostile at medium range from the elevated view in a bounded route; if confirmed, this covers the Phase 1 overhead ranged FAIL for the locked case only.
- [ ] Lock toggles on/off and switches targets with the decided inputs without moving the camera or the zoom; mouse look and wheel zoom still work while locked; a visible indicator shows the locked actor in Camera-only.
- [ ] On quick menu open/close, dialogue, loading, death, F6 and F8, no target remains locked after the interruption, the lease is only released after TDM reports unlocked, and a new lock needs a fresh press; disabling the feature restores the original always-held lease.

**Focused validation:** policy tests for the lease/acknowledgement/precedence table including the paused-menu race; one Camera-only route with melee, bow and spell versus two hostiles, target death and auto-retarget, rapid menu open/close, dialogue, a load door, F6 and F8, on controller and keyboard.

**Evidence/deliverables:** `docs/evidence/p5/target-lock.md`, `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini`, `docs/CONTROLS.md`, `docs/compatibility/dependency-lock.md`.

**Out of scope:** No corrected free-aim (unlocked) ranged fire from the elevated view; F6 remains that fallback. Cursor-aimed projectiles are a separate follow-up (see below).

### P5-02 — Proximity activation and cycling through stock BTPS

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-1(a), D5-2.

Make the bench in the Bannered Mare selectable from the default framing without F6, with the nearest object as the default choice.

**Scope**

- Pin stock BTPS 0.9.0 (licence and source revision recorded) and JContainers for the 1.7 line in the dependency lock with hashes; preflight them like the minimap trio. Verify in game that BTPS, SmoothCam 1.8.1, TDM 2.3.1 and the Camera DLL load together.
- Choose and record the selection behaviour explicitly. Default: **strict nearest-first**: specify every weight, bonus and tie-break (angle and type weights at or near zero, value weight zero, crosshair and previous-selection bonuses zero, deterministic tie-break) and demonstrate it. If a weight turns out to be hard-coded, record the item as **proximity-biased** instead and state that another object may win on type, angle or history; do not claim nearest-first.
- Ship the declared BTPS configuration delta: the scoring above, cycle keys per D5-2, widget placement that does not collide with the SkyCRPG HUD, hide-during-lock through its TDM integration.
- Document the interaction with native prompts and QuickLoot-style mods; no SkyCRPG hook touches the crosshair pick in this item.

**Acceptance criteria**

- [ ] A fixture with a nearer object off-axis and a farther object directly in front of the camera selects the nearer one under the recorded behaviour; the Bannered Mare bench case passes without F6; doors, containers, items and an NPC are selectable in a tight interior route.
- [ ] Cycling steps through visible candidates in a stable order on keyboard and controller and returns to the closest; the widget shows the current candidate.
- [ ] Disabling or removing BTPS restores native activation; Camera-only without BTPS still loads and behaves as before.

**Focused validation:** the off-axis fixture; one Camera-only interior/exterior activation route with low furniture, stacked loot, doors and an NPC; one provider-absent startup.

**Evidence/deliverables:** `docs/evidence/p5/proximity-activation.md`, `docs/compatibility/dependency-lock.md`, `docs/INSTALLATION.md`, `docs/CONTROLS.md`.

**Out of scope:** No original selection scoring, no fork, no auto-loot, no change to activation range rules.

### P5-03 — Cursor mode: pointer hover and controller reticle

**Lane:** camera. **Dependencies:** P5-02, D5-1(b) answered. **Decisions:** D5-1, D5-3.

Add the one targeting behaviour nothing provides: pick by pointer on PC and by right-stick reticle on controller, and feed that pick into the same activation path.

**Scope**

- Obtain a target-injection path: upstream API or exception first (D5-1(b)); a fork only after D5-1(c) is granted. Without either, this item is BLOCKED and the cursor overlay is not built.
- Focus ownership: the Camera's `focused()` policy currently suspends gameplay behaviour for any menu flagged cursor/menu-context/modal. Define a **narrowly scoped exception** for the Camera's own overlay, matched by its own menu name and flags, never by category; every native menu and dialog keeps precedence and immediately suspends cursor mode.
- Selection ownership: the Camera module owns the selected target and pushes it through the injection API. **Cycling latches** the chosen target until meaningful pointer or stick movement (threshold recorded), target invalidation (out of range, unloaded, dead, menu) or mode exit; hover never overwrites a latched target below that threshold.
- Cursor mode itself: a toggle on both devices; a cursor/reticle overlay owned by Camera; screen-point to world ray and a hull pick with the native activation radius/length rules; actors, furniture, doors, containers and items as candidates; the look hook consumes right-stick/mouse deltas while the mode is active.
- Exit rules: leaving cursor mode restores mouse and right-stick camera control in the same frame and hides the overlay; menus, loading, death, first person, special states and F8 exit the mode; if BTPS (or the injection provider) is absent or reports unavailable, cursor mode refuses to enable and says why in the log.

**Acceptance criteria**

- [ ] On keyboard/mouse, hovering an object shows the selection and Activate acts on it; on controller, the reticle moves with the right stick and Activate acts on the reticle target; a cycled target stays selected under small pointer drift and is replaced only by the recorded movement threshold, invalidation or exit.
- [ ] No duplicate activation and no camera rotation from cursor input; every tested exit (toggle, menu, F6, F8, load, death) restores yaw control and hides the cursor; a native menu opened over the overlay takes focus and the overlay never re-captures input behind it.
- [ ] Provider-absent (no BTPS or no injection API) cursor mode refuses to enable; nothing half-works.

**Focused validation:** pointer and stick routes over the same interior fixture as P5-02; latch tests with cycling then small and large pointer movements; menu/F6/F8/load interruptions while the cursor is shown; performance sample of the per-frame pick.

**Evidence/deliverables:** `docs/evidence/p5/cursor-mode.md`, `docs/CONTROLS.md`, `docs/UX_SPEC.md`, licensing assessment and fork repository/notices if D5-1(c) was granted.

**Out of scope:** No click-to-move, no RTS panning, no inventory drag-and-drop from the world cursor, **no projectile aiming** (moved to the follow-up below).

### P5-04 — Spike: fixed-height camera and occluder handling on 1.7.104

**Lane:** camera. **Dependencies:** P4-06. **Decisions:** D5-5.

Measure before building. Three unverified mechanisms decide whether R4 is a bounded feature or a renderer project. The spike ends with an explicit **GO / REVISE / HOLD** outcome; P5-05 cannot start on REVISE or HOLD without an owner decision.

**Scope**

- Camera caster detour: hook the engine camera caster the pinned SmoothCam calls, log hit object, layer, normal and whether the goal point is in open space inside the current room for the Bannered Mare beams, a dungeon corridor and an exterior; prove that discarding a positively identified thin-occluder hit keeps SmoothCam stable (no second transform writer, F8 still works) and that enclosure hits are never discarded.
- Fade mechanism: on a known beam reference try `UpdateMaterialAlpha`, `NiAlphaProperty` injection, and `SetAppCulled`; record which renders **transparent** (visual context kept), which only hides, whether the engine resets it, and the cost. Evaluate Disocclusion's **actual** approaches rather than an approximation: its per-draw near-plane clip through the `RenderPassImmediately` call site (SE and AE relocation IDs exist; confirm the AE call-site offset on 1.7.104), its SE-only plane hooks versus `UseLegacyStrip`, and its `QPointWithin` room-seeding modes 1–3 with their measured cost.
- Culling: hold the camera above a low ceiling and record what disappears; measure seeding modes 1, 2 and 3 in one large interior for draw-call and frame-time cost.
- One evaluation session of Camera Studio as an alternative isometric provider, recorded as reference only.

**Acceptance criteria**

- [ ] An evidence table states PASS/FAIL per mechanism with screenshots and frame-time deltas on the pinned stack, separating "transparent", "hidden" and "clipped" results.
- [ ] A written outcome: **GO** (fixed height plus transparent thin occluders demonstrated safely within budget: P5-05 may start on stage 1), **REVISE** (only hide or clip demonstrated, or budget exceeded: owner chooses between those behaviours, deferral, or renderer work; P5-05 blocked until then) or **HOLD** (caster detour unsafe or culling unsolved: P5-05 blocked, owner decision required). Stage 2 is reported separately and needs its own approval regardless of the stage 1 outcome.
- [ ] No spike code is merged into the shipped Camera DLL; disposable builds are hashed and retained like earlier diagnostic candidates.

**Focused validation:** disposable Camera builds on an isolated profile; bounded 20-minute sessions per fixture; restoration verified.

**Evidence/deliverables:** `docs/evidence/p5/occlusion-spike.md`, `docs/DECISIONS.md` entry recording the outcome and the owner's response to it.

**Out of scope:** No Community Shaders feature, no provider fork, no new camera controller.

### P5-05 — Fixed-height camera with see-through overhead occluders (stage 1)

**Lane:** camera. **Dependencies:** P5-04 outcome **GO**. **Decisions:** D5-5, D5-6.

Deliver the CRPG feel the owner asked for within what P5-04 proved. Three layers stay distinct and are never conflated:

1. **Native SmoothCam collision**: unchanged, always active for genuine enclosure (walls, floors, rock ceilings, outside-room goals).
2. **DYL-157 adaptive compression**: optional recovery setting, default OFF (D5-6); it engages only when layer 1 kept a hit, never as a substitute for layer 3.
3. **Thin-occluder handling (new)**: opt-in, applied only to geometry positively identified as safe by the spike's criteria.

**Scope**

- Implement the caster-detour filter behind an opt-in setting: while the policy is in normal/targetLock state, discard camera hits classified as thin overhead occluders (downward-facing normal, goal point in open space inside the current room, object in the allowed layers); keep every other hit.
- Implement see-through with the **transparent** mechanism P5-04 selected: detect references between camera and player with a small ring of rays at a bounded rate, fade with enter/exit hysteresis, restore on every lifecycle exit, cap the per-frame set, never touch the player, actors or the floor.
- If during implementation the selected mechanism cannot be made transparent on the shipped build, stop and return to the owner (HOLD); do not ship hide or clip under this item.
- Stage 2 (walls/ceilings with a culling strategy) is **not** part of this item; it needs its own ticket, evidence and owner approval.

**Acceptance criteria**

- [ ] On the Bannered Mare beam route the camera height and zoom stay at the configured preference while beams become see-through with visual context retained; no pull-in, no near-plane pop, no oscillation over 30 seconds at the boundary.
- [ ] Corridors, low rock ceilings and doorways still collide safely (no blind walking, nothing disappears from room culling); F6/F8 and lock interruptions restore every faded object; with the feature off, behaviour equals the Phase 4 candidate.
- [ ] Frame-time overhead of detection plus fade stays within the Phase 4 budget on the recorded host, measured on the same route.

**Focused validation:** policy/hysteresis tests; one Camera-only interior route covering beams, doorframes, stairs, a dungeon corridor and a large hall; the Phase 4 performance capture repeated once.

**Evidence/deliverables:** `docs/evidence/p5/fixed-height-camera.md`, `modules/camera/config/SkyCRPG_Camera.ini`, `docs/CONTROLS.md`, `docs/VALIDATION.md`.

**Out of scope:** No universal cutaway, no exterior roof removal, no change to SmoothCam's own files, no stage 2.

### P5-06 — Phase 5 gate: input matrix, regression and owner playtest

**Lane:** gate. **Dependencies:** P5-01, P5-02, P5-03, P5-05 (or their recorded HOLD/BLOCKED dispositions).

**Scope**

- Verify the combined binding table on keyboard/mouse and controller: lock, cycle, cursor mode, F6, F7, F8, hotbar, map and native POV with no duplicate actions.
- Run Camera-only, UI-only and combined regressions on the final candidate, including provider-absent starts for BTPS, JContainers and TrueHUD and the disable-every-new-feature profile.
- Record the owner's playtest of the targeting and camera feel and the finite list of defects.

**Acceptance criteria**

- [ ] Every new feature can be turned off independently and the MVP acceptance IDs C1–C5 still pass on the candidate.
- [ ] No control lock, stuck cursor, invisible geometry, stranded lock state or stale lease remains in the recorded routes.
- [ ] Owner records ACCEPT or specific revisions.

**Evidence/deliverables:** `docs/evidence/p5/gate.md`, `docs/STATUS.md`, `docs/compatibility/runtime-matrix.md`, `docs/RELEASE_CHECKLIST.md`.

## Follow-ups outside this phase

- **Cursor-aimed projectiles:** when cursor mode hovers a hostile with a bow or missile spell drawn, aim the projectile at the hovered point (the same launch-velocity hook pattern TDM uses). Depends on P5-01 PASS and P5-03 PASS; separate ticket and owner approval.
- **Stage 2 see-through (walls, ceilings, room seeding):** separate project after reviewing P5-05's visual result and frame-time measurements.
- **Hold-right-mouse-to-rotate cursor scheme (D5-3 option).**

## Sequencing

```
Phase 4: DYL-175 owner ACCEPT
  → owner rules on D5-1 … D5-6
  → P5-01 (TDM lock-on) ‖ P5-02 (stock BTPS) ‖ P5-04 (occlusion spike)
  → P5-03 (cursor mode) after P5-02 and the D5-1(b) answer
  → P5-04 outcome GO → P5-05 (stage 1); REVISE/HOLD → owner scope decision
  → P5-06 gate
```

P5-01, P5-02 and P5-04 are independent in intent, but the Camera source (`plugin.cpp`, policy headers), the shared configuration (`SkyCRPG_Camera.ini`, the TDM fragment), the dependency lock and `docs/CONTROLS.md` need one coordinated owner: assign disjoint files and have the integrator merge in a fixed order. Skyrim gameplay sessions remain serialized on the shared host (one process per profile, bounded jobs, restoration verified). Estimated relative effort: P5-01 small–medium (the acknowledged handoff adds policy work), P5-02 small, P5-03 medium–large, P5-04 medium, P5-05 large, P5-06 medium.

## Risks specific to this phase

| Risk | Impact | Mitigation |
| --- | --- | --- |
| JContainers or TrueHUD has no 1.7.104 build. | P5-02 blocked / no TrueHUD reticle. | Verify first; D5-4 requires an original indicator instead of an invisible lock; BTPS-NG style repacks are unverified and not accepted. |
| TDM disable request is not processed while paused. | Target stays locked across a menu; stale state. | Acknowledged lease (release only after `GetTargetLockState()` unlocked), fail closed, fresh input after interruption; paused-menu race in the policy tests and the route. |
| Live MCM edits change TDM lock behaviour. | Camera writes return silently. | Re-read on menu close; refuse lock state or re-apply the fragment; recorded choice. |
| BTPS weights are not all configurable. | Nearest-first cannot be guaranteed. | P5-02 records proximity-biased behaviour honestly or requests an upstream change; no false nearest-first claim. |
| BTPS declines an injection API. | P5-03 blocked. | D5-1(b)/(c) sequence; licensing assessment before any fork; cursor mode is not built on a conflicting second pick hook. |
| TDM switching inputs conflict with yaw/zoom. | Lock usable but target switching awkward. | Fragment disables colliding inputs; owner chooses dedicated keys; worst case switching only by re-toggling. |
| Engine caster detour destabilises SmoothCam. | Fixed height not achievable this way. | Spike outcome HOLD; P5-05 blocked; owner decision, no silent fallback. |
| Only hide or clip is achievable, not transparency. | The requested behaviour is not met. | Spike outcome REVISE; owner chooses between hide, clip, deferral or renderer work; nothing substituted automatically. |
| Room culling when the camera leaves a room. | Blank screen in dungeons if stage 2 is forced. | Stage 1 keeps native collision for enclosure; stage 2 is a separate approved project. |
| No free controller buttons. | Features unusable on controller. | D5-2 is required before tickets are ready; chords only with explicit approval; mapping contract agreed before implementation. |
