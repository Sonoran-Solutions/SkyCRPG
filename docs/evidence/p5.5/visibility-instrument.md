# P5.5-01 wave 1a — view and visibility observations in the test probe (DYL-216)

[Phase 5.5](../../phases/PHASE_5_5.md) · [probe README](../../../tools/testprobe/README.md#view-samples-p55-01-view-protocol-1-runtime-not_run)

**Status, 2026-11-09: code and portable tests only. Every in-game row is NOT_RUN.** Wave 1a was committed as `59613f2`; the orchestrator's `61610d9` (two MSVC fixes) built in CI run 42719928362 (success). Wave 2b (below, "Review fixes") answers the independent binary review of `59613f2` (CHANGES, 0 BLOCKER / 2 MAJOR / 8 MINOR) and was committed as `5cb2d16`; wave 2c answers the re-review of `5cb2d16` (CHANGES, 0 BLOCKER / 1 MAJOR / 5 MINOR). The experimental scope of D045 applies: the change lives only in the never-shipped test probe. Its output is **diagnostic** until the oracle-validation checkpoint below passes; geometric clearance never passes an acceptance measure by itself. The probe still installs no hooks, sends no input and changes no game state.

**Update, wave 2 (2026-11-09, evening):** the probe (aeaccbd, reviewed PASS, CI 42725445914) ran in 22 game launches. Its view samples work (0.2-0.8 ms, off the main thread, no `guard_binary`). Its clearance rays disagree with the rendered frames where it matters. The rendered-visibility check (candidate b) is now built outside the game: see "Wave 2: rendered-visibility check" below. Clearance rays remain diagnostic only.

**Update, first in-game run of the check (`p55-oracle-ext-1`):** the marker technique works on 1.7.104, but measure version 1 refused its calibration because it took A-A2 as the noise floor and the restored player is in a different idle-animation pose. Measure version 2 (player mask = B differs from both A and A2; noise from the ring around the box; player verdict from the points) re-evaluated the same frames offline: calibration confirmed, `open-h3`, `open-h2` and `stable-h0` PASS. Version 2 was designed after looking at these frames, so a fresh run is the independent check. See "First in-game run" below.

**Update, batch O2 (four oracle routes, measure version 2):** calibration confirmed in every route. The fresh exterior run confirmed version 2 independently: open-h3, open-h2 and stable-h0 PASS. Also PASS: bfb0-h0, mare-off-h0 and bfb1-h0. **FAIL:** `mare-on-h0` read partly for a plainly visible, dark-clad player on a dark floor (low-contrast under-read). `hall-h2` was indeterminate (`scene_busy`: the player's own shadow on the carpet) and `cave-h0` indeterminate (dark). Measure version 3 (scene-adaptive threshold with a contrast floor; shadow kept out of the ring noise) re-evaluates the O2 frames to 8 PASS and cave-h0 NOT_RUN. That is a re-evaluation on the frames version 3 was made from, not independent evidence. See "Batch O2" below.

Worktree `p55-instrument`, branch `phase5.5/p55-01-visibility-instrument`, base `da16672`. Pinned host: SkyrimSE.exe 1.7.104 SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`, Address Library 13 (`versionlib-1-7-104-0.bin`, format 5, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`), CommonLibSSE-NG 7.2.0 at `7a60f4de794095d7b0f8928d1b930a52e9a7da83`.

## Wave 2: rendered-visibility check

### Wave 1: rays against frames

Probe ray status per player point (head, chest, feet, shoulder_l, shoulder_r) against the screenshot taken right after the same view sample. Frame readings are the orchestrator's from the ledger, except where marked "mine".

| Run label | Frame (session-p4/autotest/runs/batch-LABEL-00/screenshots) | Rays | Frame | Agree? |
| --- | --- | --- | --- | --- |
| p55-a-open-1 | a-open-h0 (open ground, 45) | 5 clear | player visible | yes |
| p55-a-open-1 | a-h0 (stable, 45) | 5 blocked | only the thatch roof | yes |
| p55-a-open-1 | a-open-h2 (open ground, 225) | head clear, 4 blocked | head and shoulders above the thatch edge, legs under it (mine) | shoulders no |
| p55-b-mare-A | b-hearth-h0 (hide off) | 5 blocked | player seen through the undrawn roof, rafters crossing | **no** |
| p55-d-mare-A2 | d-bm-ground-h0 (hide on) | 5 clear | player clearly visible | yes |
| p55-c-bfb-seed0-1 | c-bfb-h0 (seed 0) | 5 blocked | near-empty grey view, room not drawn | yes, for the wrong reason (the rays hit the ceiling collision; the frame is empty because the room is not drawn) |
| p55-c-bfb-seed1-1 | c-bfb-h0 (seed 1) | 5 blocked | room drawn through its back-face-culled ceiling, dark, player near the centre | **no** |
| p55-c-hall-seed0-A | c-hall-h2 (Dragonsreach, 225) | 5 clear | player fully visible | yes |
| p55-d-cave-dwemer-A2 | d-cave-h0 (Embershard) | 5 blocked | dark rock, player not visible | yes |

The rays fail exactly where the engine does not draw geometry that still has collision: roofs and ceilings seen from outside are back-face culled. They also cannot tell an undrawn room from a blocked view. This is why clearance stays diagnostic.

### What was built (all offline; standard-library Python; no probe or native change)

| Path | Purpose |
| --- | --- |
| `tools/visibility/png.py` | PNG reader/writer: 8-bit RGB/RGBA/greyscale, non-interlaced, all five filters, CRC checked; other formats refused |
| `tools/visibility/rendered_visibility.py` | xwd window geometry, box mapping, A/B/A2 measure, calibration, verdicts, overlay; CLI |
| `tools/visibility/README.md` | Procedure, `saa` binary check, geometry, thresholds and their source, limits |
| `tools/tests/test_visibility.py` | 46 tests (synthetic scenes with idle animation, a shadow, a dim low-contrast scene, stacked fades, a lighting change; wave 1 captures, the `p55-oracle-ext-1` and batch O2 frames when present) |
| `session-p55/oracle/` | `write_oracle_routes.py`, `gen/routes` + `gen/specs` (4 routes), `run_oracle_route.sh`, `evaluate_oracle.py`, `mask_sheet.py` (silhouette sheets), `test_oracle.py` (16 tests), README; `reeval/p55-oracle-ext-1-v2/` (version 2 re-evaluation of the first run) |

**Procedure:** at a settled pose the route takes frame A after a `player.saa 1` round trip, frame B after `player.saa 0`, and frame A2 after `player.saa 1`, each 2.5 s after its console command, with a view sample before A and after A2. Measure version 3: pixels where B differs from both A and A2 are player pixels on screen, through any mechanism (the player mask). The per-pixel threshold comes from the scene's own noise in the ring (3-12) with a floor of 10 % of B's luminance. The same mask test in the ring, with the player's shadow set aside, is the noise floor. A2 is the restore proof (it must differ from B the way A does). A calibration triple at the unobstructed open-ground pose gives the full-visibility signal; the fixed rig keeps the player's screen size constant. Verdicts per point and for the player (4 of 5 points): visible / partly / not visible / indeterminate; a box ratio is reported beside them as a diagnostic (thresholds and indeterminate rules: tools/visibility/README.md). Version 1 used A-B for the signal and A-A2 in the box as the noise floor; the first in-game run disproved that (below). Version 2 used a fixed threshold of 12 and counted the player's shadow in the ring as noise; batch O2 disproved that (below).

**`player.saa` in the binary:** script command entry at .data 2084760 (`SetActorAlpha`, short `SAA`, params Float + optional Int), handler 35E010 (ID 22391). It needs an actor with a process, clamps to [0, 1], adds 2.0 when the optional int is absent, and stores through ID 39961 at `[[process+8]+2C8h]`, then calls actor virtual +700h. Observed in `p55-oracle-ext-1` (Source: frames): `saa 0` removes the player, the weapon and the player's shadow within 2.5 s, and `saa 1` brings them back in the same silhouette, in a later idle-animation pose. That the stored alpha + 2 has no lasting effect is still a **Hypothesis**. Restoration is proven per triple (A2 differs from B as A does), repeated in a final `restore` section, and no save is written.

**Offline validation done (wave 1 frames have no B frame):**

- PNG reader on the runner's real captures (1926x1112 RGB) and on synthetic files for every filter, RGBA, grey and the refusals.
- Window geometry: border 3 px, title 29 px, from report.json `resolution` and the pixels.
- Box mapping checked by eye on zoomed crops:
  - a-open-h0: head point at the helmet/neck, feet point 6 px above the rendered boots.
  - c-hall-h2: head point on the face, boots 24 px below the feet point, so the box reaches 0.35 player heights below it.
- Noise floor from wave 1 pairs at identical poses (wind in the exterior: frame 0.05-0.06; interiors <= 0.025; dark seeded room: box 0.15 at threshold 5). The limits are set above those.
- Verdict logic on synthetic triples: visible, opaque occluder, lower-body occluder (per point), single fade visible and eight stacked 0.35 fades not visible, scene change, camera moved, not restored, missing frame, unconfirmed calibration, off screen, scale mismatch, wind noise, dark visible, and dark with no signal (indeterminate, never not visible).

### First in-game run: `p55-oracle-ext-1`

Run record `session-p55/oracle/runs/p55-oracle-ext-1/` (runner label `batch-p55-oracle-ext-1-00`, route `p55-oracle-ext`: spike stage 744609f, probe aeaccbd, settings of a-open-1). Runner PASS, postflight ok. Version 1 result: calibration NOT confirmed (`noisy: A-A2 changed 0.366 of the box`; `marker_not_confirmed: 0 of 5 points`), every row NOT_RUN. That record is left as it is.

**Why A and A2 differ inside the box (Source: the frames, `reeval/p55-oracle-ext-1-v2/*-masks.png`):**

- The A-B and A2-B silhouettes match: IoU 0.86 at `cal-h0` and `open-h3`, 0.78 at `open-h2`; point windows 0.92-1.00 in both.
- 96-99 % of the A-A2 changes lie inside that silhouette. The ring changes A-A2 by only 0.02-0.04, and the frame outside the ring by 0.02-0.04.
- No single image shift explains the A-A2 difference (best shift still 0.35-0.52 mismatch). Mean luminance in the box is about the same in A and A2. Blend factor (A2-B)/(A-B) is 0.90-0.96. So this is not a camera move and not partial alpha: the player has moved, an idle animation ~13 s later. At `cal-h0` the sword hangs at the side in A and is held across the body in A2.
- In B the player's shadow disappears with the player, so A-B in the box includes the shadow.

**Version 2 measure:** the player mask is the set of pixels where B differs from both A and A2. The noise floor is the mask share in the ring. The main verdict comes from the five points (4 of 5 visible = visible); the box ratio is a diagnostic. The version 1 rules `noisy` (box A-A2 > 0.25) and point noise > 0.40 are removed (same cause); `scene_busy` (ring mask > 0.10) replaces them. The restore proof and every other indeterminate rule are unchanged. A B-versus-B noise check would need a second B frame, which is a frame-protocol change; it is not needed, so the routes are unchanged.

**Re-evaluation with version 2** (offline, `session-p55/oracle/reeval/p55-oracle-ext-1-v2/oracle.md`):

| Fixture | Expected (written before the run) | Version 2 | Status |
| --- | --- | --- | --- |
| `cal-h0` | calibration | confirmed: box mask 0.510 (ring 0.016), 5 of 5 points 0.90-0.98 | |
| `open-h3` | visible; all points visible | visible; point ratios 0.91-1.07; restored; box 0.65 (side-on, shadow behind) | PASS |
| `open-h2` | partly or visible; head visible, shoulders visible/partly, feet not visible | visible (4 of 5); head 0.96, chest 1.06, shoulders 0.99/1.04, feet 0.01; restored; box partly 0.51 | PASS |
| `stable-h0` | not visible; all points not visible | not visible; points 0.00-0.01; restore unprovable (nothing on screen) | PASS |

**What this run shows:** the marker works in game on 1.7.104 (player, weapon and shadow out in B, back in A2); the view samples before A and after A2 matched at every pose; a player with the legs under a roof edge reads per point (head visible, feet not); a player under a roof reads not visible; the restore proof reports `true` where the player is on screen.

**What it does not show:** independence: version 2 was written after looking at these frames (thresholds unchanged, expected answers set before the run), so a fresh `p55-oracle-ext` run is the real check. Repeatability (one run). Any interior, dark scene, undrawn room, back-face-culled roof, hide, fade or clip fixture (routes seed0, seed1, hide not run; no fade or clip build). The `plainly visible` row also needs `hall-h2`.

### Batch O2: version 2 in game, version 3 from its frames

Records (read only): `session-p55/oracle/runs/p55-oracle-{ext,seed1,hide,seed0}-O2/`, runner labels `batch-p55-oracle-{ext,seed1,hide,seed0}-O2-00`, aggregate `runs/checkpoint-O2.md`. Spike stage 744609f, probe aeaccbd, measure version 2. Runner PASS for every route, postflight ok; wrapper exits 0, 0, 1 (hide: the FAIL) and 2 (seed0: hall-h2 NOT_RUN).

**What O2 showed about version 2:**
- `p55-oracle-ext-O2` reproduced the expected exterior answers on fresh frames: open-h3 visible, open-h2 head visible and feet not, stable-h0 not visible. This is the independent confirmation that ext-1 could not give.
- The undrawn room (`bfb0-h0`) read not visible, and both back-face-culled roof fixtures read partly: PASS, with rays 5/5 blocked at all three.
- **Failure at `mare-on-h0`** (Bannered Mare hearth, hide on): partly (head 0.49, shoulder_r 0.29) for a player who is unobstructed in A and A2 and absent in B (orchestrator's crops; my zoomed crops agree). The player is dark brown on a dark floor in dim light (box luminance 49). Over much of the head the player differs from the floor by 1-9 levels, below the fixed threshold of 12. The ring's own noise there is small (99th percentile 5), so the threshold, not the scene, made the under-read.
- **`hall-h2` scene_busy (0.145):** the frames show the player's long shadow on the carpet, leaving the box toward the upper left and gone in B. In A and A2 it is darker than B in every channel, and A matches A2 there. It was not NPCs or fire: the non-shadow part of the ring mask is 0.003.
- **`cave-h0`:** dark (luminance 4.0). Every pixel moves by at most 1-2 levels between A and B.

**Version 3** (tools/visibility/README.md, "What changed from version 2"):
- The per-pixel threshold is the ring's non-shadow noise (99th percentile + 1, clamped to 3..12), with a per-pixel floor of 10 % of B's luminance. The luminance-scaled threshold of versions 1-2 is gone; the dark rule stays.
- Shadow-like ring pixels (A and A2 both darker than B in every channel, A closer to A2 than to B) are reported as `ring_shadow` and kept out of the noise floor and the ring A-B control. They are not discounted on the body, because a dark-clad player passes the same test.
- New `lighting_changed`: more than 0.10 of the frame outside the ring shadow-like (O2 maximum 0.02).

**Version 3 re-evaluation** (`session-p55/oracle/reeval/O2-v3/`; **re-evaluation, not independent evidence**):

| Run | Fixture | Version 2 in game | Version 3 re-evaluated | Threshold / ring noise / ring shadow |
| --- | --- | --- | --- | --- |
| ext-O2 | open-h3 | visible PASS | visible PASS | 11 / 0.007 / 0.032 |
| ext-O2 | open-h2 | visible PASS (feet not) | visible PASS (feet 0.00) | 12 / 0.008 / 0.001 |
| ext-O2 | stable-h0 | not visible PASS | not visible PASS (points ≤ 0.04) | 12 / 0.029 / 0.007 |
| seed0-O2 | mare-off-h0 | partly PASS | visible PASS (chest 0.61 under a rafter) | 5 / 0.005 / 0.058 |
| seed0-O2 | bfb0-h0 | not visible PASS | not visible PASS (0.00 at threshold 3) | 3 / 0.000 / 0.000 |
| seed0-O2 | hall-h2 | indeterminate (scene_busy) | visible PASS | 4 / 0.003 / 0.156 |
| seed1-O2 | bfb1-h0 | partly PASS | visible PASS (head 0.25) | 3 / 0.009 / 0.006 |
| hide-O2 | mare-on-h0 | partly **FAIL** | visible PASS (head 0.72, shoulder_r 0.65) | 6 / 0.006 / 0.060 |
| hide-O2 | cave-h0 | indeterminate (dark) | indeterminate (dark) | 3 / 0.000 / 0.000 |
| ext-1 | open-h3, open-h2, stable-h0 | (v1 NOT_RUN; v2 re-eval PASS) | PASS, PASS, PASS | 10-12 |

**No false visibility, checked on the frames:**
- stable-h0 keeps threshold 12 (thatch shimmer) and reads ≤ 0.04.
- open-h2 feet read 0.00.
- bfb0-h0 is measured at the lowest threshold, 3, and reads 0.00 at every point.
- A synthetic floor-coloured wall at threshold 3 reads not visible.
- A synthetic player behind eight stacked 0.35 fades reads not visible; this is the contrast floor's job, and without it the trace would count.

**Limits:**
- **Shadow on a body point:** a hidden player's shadow landing on a visible surface exactly at a body point would count there.
- **Small lighting changes:** a lighting change in B covering less than 10 % of the frame is not refused.
- **Close to the line:** mare-on-h0 shoulder_r (0.65) sits near the 0.60 line.
- **Dark cases:** dark-but-occluded cannot be told from dark-but-visible by the `saa` triple. Either of two route changes, not made here, would decide it:
  - a dark calibration triple at a known-unobstructed pose in the same cell;
  - an emissive marker (`player.pms` with a bright effect shader) so the player is drawn bright regardless of scene light.

**In-game validation of the technique:**
- Version 2: one independent exterior confirmation (ext-O2), one interior FAIL (mare-on-h0), two NOT_RUN.
- Version 3: offline re-evaluation only; the next fresh batch is its independent check.
- Fade and clip rows NOT_RUN.

### Checkpoint as it stands

Run with `session-p55/oracle/run_oracle_route.sh` (exact commands in session-p55/oracle/README.md). Expected answers are the orchestrator's reading of wave 1 frames, not owner judgments. "O2 (v2)" is the in-game batch O2 judged by measure version 2; "re-eval (v3)" is the same frames judged offline by version 3, which was made from them.

| Mechanism | Fixture (route) | Expected | O2 (v2) | re-eval (v3) |
| --- | --- | --- | --- | --- |
| plainly visible | open-h3 (ext), hall-h2 (seed0) | visible, all points visible | NOT_RUN (open-h3 PASS; hall-h2 scene_busy) | PASS |
| partly occluded | open-h2 (ext) | partly or visible; head visible, feet not visible | PASS | PASS |
| plainly blocked | stable-h0 (ext) | not visible | PASS | PASS |
| undrawn room | bfb0-h0 (seed0) | not visible | PASS | PASS |
| drawn through a back-face-culled roof/ceiling | mare-off-h0 (seed0), bfb1-h0 (seed1, dark) | visible or partly | PASS | PASS |
| hidden geometry | mare-on-h0 (hide) | visible | **FAIL** (partly) | PASS |
| one-piece mesh, camera inside rock | cave-h0 (hide) | not visible; likely indeterminate (dark, luminance 4) | NOT_RUN (dark) | NOT_RUN (dark) |
| single fade, stacked fades | none: build 744609f has no fade mechanism | | NOT_RUN | NOT_RUN |
| clipped geometry | none: wave 2 clip package not built | | NOT_RUN | NOT_RUN |

## What was built (wave 1a)

| File | Change |
| --- | --- |
| `tools/testprobe/src/view_policy.h` (new) | Portable rules: `@view` grammar, INI bounds, projection, pose angles, ring and ray planning, treatment aggregation, ray status folding, transmittance, cadence coalescing, JSON shape. |
| `tools/testprobe/src/plugin.cpp` | Windows adapter: `view_sample` event, request routing, optional view timer (opt-in), binary guard, Havok casts, cached bone reads and per-reference treatment. |
| `tools/testprobe/src/json_line.h` | `Object::Append` (header + body in one line). |
| `tools/testprobe/tests/view_policy_test.cpp` (new) | Portable test (picked up by the existing CI glob). |
| `tools/testprobe/README.md`, `SkyCRPG_TestProbe.ini` | New "View samples" section, settings, schema rows. |

`bViewSamples` gates everything. With the key absent or not exactly `1` (and with `bEnabled=0`), the probe reads seven more INI keys and otherwise behaves exactly as the branch base: no `@view` namespace (an `@view x` file stays a raw `player_state` tag), no timer, no new `probe_loaded` fields. The sample INI sets `bViewSamples=1`, `iViewIntervalMs=0` and `bViewPeriodicOffMain=0`, so a runner using it gets `@view` requests but no periodic samples. A nonzero interval without `bViewPeriodicOffMain=1` (or `bViewMainThreadOnly=1`) creates no timer and writes one `view_sample` with `periodic_opt_in_missing`.

### `view_sample` fields (view protocol 1)

The README is the contract; this is the summary the route generator uses. Order is stable.

| Field | When | Content |
| --- | --- | --- |
| `view_protocol`, `seq` | always | `1`; process-wide sequence in file order |
| `requested`, `tag` | always / requests | label of `@view <label>` |
| `available`, `reason` | always | `reason` null when available; codes below |
| `frame`, `since_last_ms`, `since_last_frames`, `coalesced_ticks`, `hops`, `queued_ms`, `sample_ms` | always | cadence and cost; `frame` is the engine update counter (ID 411489, a label) or null |
| `current_thread_id`, `captured_main_thread_id`, `main_thread_id` | always | as `player_state` |
| `camera_state` | with that reason | diagnostic |
| `camera` | available | `state`, `pos`, `yaw_deg`, `pitch_deg`, `fov_deg`, `vfov_deg`, `world_fov_deg`, `ortho`, `node` (`world_root_camera`), `under_camera_root`, `camera_root_pos`, `camera_root_delta`, `rotation_forward_delta_deg` |
| `player` | available | `pos`, `heading_deg`, `cell {form_id, editor_id}`, `interior` |
| `offset` | available | `{x,y,z,distance}` = rendered camera − player |
| `points` | available | `head`, `chest`, `feet`, `shoulder_l`, `shoulder_r`, each `{name, world, screen {x,y} (top-left origin, 0..1), on_screen, ray}` |
| `ring` | available | `ring_0..ring_{n-1}`, same shape |
| `ray` | per point | `status` (`clear`/`faded_only`/`blocked`/`indeterminate`), `transmittance`, `hits [{ref, base, layer, distance, surfaces, treatment (opaque/faded/hidden/unknown/volume), alpha, reason}]` (one entry per distinct reference and hit class), `truncated`, `reason`, `skipped_player`, `casts` |
| `player_box`, `counts`, `casts`, `on_main_thread`, `config` | available | screen box of player points; status counts; cast total; thread; effective settings |

Unavailable reasons, each with its own code and one log line per process: `periodic_opt_in_missing`, `view_busy`, `invalid_view_request`, `enqueue_failed`, `outside_task_drain`, `generation_changed`, `not_loaded`, `off_main_thread`, `player_unavailable`, `game_paused`, `camera_unavailable`, `camera_state`, `guard_binary`, `world_camera_unavailable`, `cell_unavailable`, `world_changed`, `sample_failed`. Ray-level indeterminate reasons: `invalid_geometry`, `no_havok_world`, `cast_budget`, `point_unavailable`, `skeleton_cap`, `truncated`. Unknown-treatment reasons: `unresolved`, `node_cap`, `non_lighting_blend`, `invalid_alpha`, `no_geometry`, `actor` (actor hits are not walked; only the root's app-culled flag is read), `detection` (L_DETECTION hits), `unresolved_culled` (a reference-less hit whose own node is app-culled). Opaque landscape hits carry reason `landscape`.

## Binary audit

Method: Address Library 13 parsed as format 5 (dense `u32` RVAs after a 96-byte header; checked against the known IDs 33007→5574F0, 61402→B59D80, 50784→8F8680 from the DYL-201 audit). `objdump -d -M intel` on the pinned executable for function bodies; a RIP-relative xref scan of `.text`, `.pdata` for function bounds and reverse ID lookup; `.rdata`/`.data` reads for constants, vtables and the job table. Scratch scripts were under `/tmp/p55-probe1/` (not committed). Nothing was executed except the analysis scripts; no game binary was modified.

### Rendered camera and projection

| Item | ID → RVA | What the binary shows | Source |
| --- | --- | --- | --- |
| World camera | `Main::WorldRootCamera` 36609 → 65C480 | `mov rax,[rip→3230740]; mov rax,[rax+128h]; ret` (entry bytes guarded) | binary |
| Same object is the rendered world camera | Main code at 657CF9..657D07 | writes a 16-byte NiRect to `[[3230740]+128h]+174h` (the port) | binary |
| SmoothCam writes it | SmoothCam `camera.cpp` `SetPosition`/`UpdateInternalWorldToScreenMatrix`, `thirdperson.cpp` ~L690-730, rev `e0afd09` | writes the same translate to `cameraRoot` (local and world) and to the NiCamera child of `cameraRoot` (world), sets its world rotation, then calls ComputeToScreenMatrix | source, facts only |
| ComputeToScreenMatrix | 70641 → EF1310 | reads frustum `+150..+164`, ortho byte `+168`, writes 16 floats `+110..+14C` (`worldToCam`) | binary |
| Port | WindowPointToRay 70630 → EF0050 | reads `+174` left, `+178` right, `+17C` top, `+180` bottom; frustum `+150..+168` | binary |
| Projection | WorldPtToScreenPt3 70640 → EF1180 | `w = row3·(p,1)`; `andps` with abs mask at 17ED6C0, fails only if `|w| ≤ tol`; `inv = 1.0/|w|` (1.0 at 1B5BE28); `x = row0·inv · (R−L)·0.5 + (L+R)·0.5`, `y = row1·inv · (T−B)·0.5 + (T+B)·0.5` (0.5 at 1B5BE14); `z = row2·inv` | binary |
| PlayerCamera FOV | singleton 400802 → 31A5478 | FOV setter writes `+13C` and `+140` together (3758EC, 3758FB); Main reads `+13C` and passes it to 1537A20 (657CDA, 659113) | binary |
| `cameraRoot` | TESCamera `+20` | used by the shipped Camera module (`thin_occluder.h:316`), whose in-game trace measured z values through it (DYL-201) | runtime evidence, earlier |

Decision: the probe reports the world root NiCamera as the rendered camera (`node:"world_root_camera"`), projects with its own `worldToCam` and port using the engine arithmetic above, and reports per sample whether that NiCamera is a child of `cameraRoot` and how far `cameraRoot` is from it. The engine projection divides by `|w|`, so a point behind the camera is mirrored onto the screen; the probe instead reports it as not in front (`screen:null`, `on_screen:false`). Yaw and pitch come from the matrix's view axis (row 3), which is exact for the projection; the node rotation's X axis is reported as `rotation_forward_delta_deg` because "the camera looks down local +X" is a Gamebryo convention I did not prove in this binary (**Hypothesis**). Frustum planes as tangents at unit distance is likewise a **Hypothesis**; `world_fov_deg` lets the first run check it.

### Havok rays

| Item | ID → RVA | What the binary shows | Source |
| --- | --- | --- | --- |
| bhkWorld vtable | 238918 → 1A7CDA8 | slot 27h → 104B470 `mov rax,[rcx+10h]; ret` (GetWorld1); slot 33h → 104BE80 | binary |
| `bhkWorld::PickObject` | 77860 → 104BE80 | early out: when a float at 3336520 exceeds 20D1758/20D175C (unless byte 3336528 is set) it sets `pickFailed` (+C0) and returns false, a per-frame pick budget (**Hypothesis** for the meaning). Otherwise `GetWorld2()->[+430h]+C598h` → LockForRead 68233, only if byte 20D18D0 ≠ 0 (file value 1); then all-hits collectors (+B0/+B8, sorted by 10A0670) or closest (+A8) or, with none, `hkpWorld::castRay(world,input,output)` 61399; unlock 68239 | binary |
| `hkpWorld::castRay` | 61399 → B598C0 | profiling markers, kd-tree/broadphase cast; no BSReadWriteLock call (entry bytes guarded) | binary |
| World lock | 68233 → CE3250, 68239 → CE3510 | owner fast path `[lock]==GetCurrentThreadId → lock inc [lock+4]`; else CAS on `count & 0FFFFFFFh`, spinning with Sleep after 10000 tries, so a reader waits only while a writer **holds** the lock (high bit set). LockForWrite 68234 → CE32D0 only CASes `[lock+4]` from 0 to 80000001h in a Sleep loop and sets no pending bit: readers can starve writers, and a nested read cannot deadlock behind a waiting writer (wave 1a said the opposite; corrected after the review). Unlock `lock dec [rcx+4]` | binary; matches `docs/evidence/p5/proximity-activation.md` F5 |
| Engine practice | job "Pick reference" 39058 → 40620 → 26127 | makes the `+198h` virtual call (PickObject slot) and takes LockForRead inside the main job list | binary |
| Hit → reference | FindCollidableRef 26003 → 404740 | 77988 (collidable owner → NiAVObject), `lock inc` on the object, 19750 walks `parent` (+30h) to the reference, `lock xadd` release | binary |
| Hit → object | FindCollidableObject 15870 → 2236D0 | when `[collidable+2Ch] & FFFF0000h == 10000h` and type is entity: position lookup through the TES singleton and cell grid (2236E8..22374D) returning `[x+30h]`; otherwise tail-jump to 77988 (1051DE0). The land heightfield body is built with filter info `0x10011` (system group 1, layer 17: `mov dword [rbp-78h],10011h` at 2AEAA5 in ID 18750). The probe avoids FindCollidableObject for system-group-1 collidables (`landscape`) and calls it only for ordinary hits without a reference | binary |
| Layer, app-culled | native caster at 5577D3/5577EB | `mov edi,[collidable+2Ch]; and edi,7Fh`; object from 77988; `test byte [obj+F4h],1` | binary |
| Cell world | GetbhkWorld 18995 → 2BE9B0 | `test byte [cell+40h],1` → interior: extra data at `+48h`; exterior: global | binary (guarded) |
| Scale | 188105 → 17FDE5C = 0.0142875; 187407 → 17F423C = 69.99125 | constants in `.rdata` | binary |
| Ray layer | Skyrim.esm COLL `L_LOS` 0008878A, BNAM 41 | collides with 0,1,2,4,5,6,7,8,9,10,13,14,16,17,32,38,39,40,41,43,44,53; every COLL CNAM list is symmetric; no Update/Dawnguard/HearthFires/Dragonborn override | Skyrim.esm data |

Decision: the probe calls `hkpWorld::castRay` (61399) itself while holding `bhkWorld::worldLock` for read, which is what PickObject does internally, and resolves the hit (layer, reference, base, scene object, reference root) **before** releasing the lock. Using PickObject instead would release the lock before the collidable could be resolved, and it can fail under the engine's pick budget. The cost: the probe's casts do not count against that budget (unmeasured). The ray filter is group 0, layer 41 (`L_LOS`). Compared with the camera layer `L_CAMERA` (39, CNAM 0,1,2,3,8,9,13,14,16,17,20,26,28,30,40,41,43,44), L_LOS does not see `L_TRANSPARENT` (3), `L_DEBRIS_LARGE` (20), `L_TRANSPARENT_SMALL` (26), `L_TRANSPARENT_SMALL_ANIM` (28) or `L_CHARCONTROLLER` (30), and it does see clutter (4), weapons (5), projectiles (6), spells (7), props (10), cloud traps (16), cone projectiles (38), the query layers (40, 41, 43, 44) and `L_DETECTION` (53); neither sees invisible walls (27) or water (11). So `clear` is not "the camera caster is clear". Hits on 7, 16, 38, 39, 40, 41, 43 and 44 are reported as `volume` and never block: 16 is the only one of them with the COLL sensor flag (GNAM 02); the rest are chosen from their names and purpose (spell and cone-projectile bodies, query layers), an engineering reading (**Hypothesis**), not a render test. The reviewer scanned every NIF in the two vanilla mesh archives and found no body on any of these layers, so they are created at run time. `L_DETECTION` (53) collides only with 41; what its bodies stand for is unknown, so its hits are `unknown`/`detection`, which always blocks (wave 2b had it as a volume). Projectiles (6), weapons (5), traps (14) and clutter stay ordinary hits because they are drawn objects. Whether the runtime collision filter equals the ESM matrix is a **Hypothesis**. The player is excluded by reference identity, and the probe re-casts past each hit 1 unit along the ray (re-casts from inside a convex shape do not re-hit it; triangle meshes may report an exit face, **Hypothesis**).

### Scene graph, materials, threads

| Item | ID → RVA | What the binary shows |
| --- | --- | --- |
| NiNode vtable | 237098 → 1A46540 | slot 3 (AsNode) `mov rax,rcx`; slot 7 (AsGeometry) `xor eax,eax`; slot 2Ah (GetObjectByName, EE23A0) iterates children data `+118h`, count WORD `+122h` (free index) |
| Geometry vtables | BSTriShape, BSDynamicTriShape | slot 7 returns `this`, slot 3 returns 0 |
| Names, parents | EE0A40, 2EECE0 | name compared at `+10h`; parent walk at `+30h` |
| `lastUpdatedFrameCounter` | NiAVObject::UpdateWorldData (NiNode slot 30h → EEBF40) | stores the global at 33363F4 (ID 411489) into `node+104h` |
| Frame counter | 411489 → 33363F4 | 28 RIP-relative references in `.text`: 27 `mov`/`cmp` reads and one write, `inc dword [rip]` at 100A02B (a chunk of 1009F70, which `Main` ID 36555 calls at 656BB1), right after a renderer virtual call. The reviewer found that 36555 waits for the UI job before reaching it, so the counter cannot change during a sample; I confirmed the call chain, not the wait. "Once per frame" is a **Hypothesis**. |
| Material type | BSShaderMaterial slot 7 | BSLightingShaderMaterial/Base/Envmap → `mov eax,2`; BSEffectShaderMaterial → `mov eax,1` |
| Material alpha | BSLightingShaderMaterialBase slot 4 (ComputeCRC32, 1523A30) | reads `+70h`, `+78h`, float `+80h`, `+88h`, consistent with CommonLib (`materialAlpha` +80h); the DYL-201 audit and its in-game fade use the same offset |
| Shader material | 105544 entry `48 8b 79 78` | `mov rdi,[rcx+78h]` |
| Alpha property | factory 70687 → EF2BE0, flags `+30h` | earlier audit; "bit 0 = blend enable" is a **Hypothesis** supported by the DYL-201 flags `0x10ED` producing the in-game fade |
| Player 3D | PlayerCharacter vtable 208040 slot 6Fh → 747B70, 70h → 747AF0 | `Get3D1(true)` → `[this+900h]`; `Get3D1(false)` → `Get3D2` (2EBCE0) |
| SKSE UI drain | SKSE 2.3.1 `Hooks_UI.cpp` site 1169D20+AB5 = 116A7D5 | `call 116E030` inside ID 82082 |
| Callers of 82082 | — | `Main::Update` 36564 at 65930C (skipped when 60E990(0) is true); job **"UI"** 39042 (6E4E80), pointer at `.data` 20B4EF0 in the named job table; init paths (36548 from WinMain 36544) and others |
| Job table | `.data` 20B4E00..20B4FF8 | name/function pairs: Detection, Pick reference, Auto aim, Player Anim sync, Actor animation, … , Quests/Scenes update sync, **UI**, Process lists, Update low list helper 1–3, Script clear, Wait For Shadow Culling, Shadow Culling sync, Cell animations, Ragdoll post physics, World update sync, … , IO update sync, IO. All "sync" entries use the same function 35401. |
| SKSE task drain | 65941E (in 36564), 65A3DC | the second site is reached from 39092, in the same job-function range |

Probe history (Source: `session-p4/autotest/runs/*/evidence/probe-slice.jsonl`, 359 runs): of the unpaused periodic `player_state` samples, 6590 ran on `Main::threadID` and **all** of them were within 2 s of a menu or load event; 22034 samples further than 2 s from such an event all ran on other threads. Paused samples: 10915 on main, 187 not. `skse_task_post_load` (ordinary `AddTask`) ran off main 606 of 606 times.

## Thread-safety decision

1. A main-thread-only policy gives **no samples in steady play**: the UI drain never runs on `Main::threadID` there (history above). The probe has no hook, and adding one would break its contract. So the default (`bViewMainThreadOnly=0`) samples in the same UI task drain as `player_state`, on whatever thread runs it. `bViewMainThreadOnly=1` keeps a strict mode: a drain on another thread hops through the main task queue (whose helper only re-queues, so neither queue requeues into its own drain), at most 600 times or 5 s, then reports `off_main_thread`.
2. Havok: every cast holds the world read lock exactly as PickObject does, and the probe never holds it across another engine call that takes it. Casts from job threads under that lock are engine practice ("Pick reference" job).
3. Camera: the NiCamera and PlayerCamera are process-lifetime objects. What the binary shows, from the reviewers' passes and my checks: in `Main::Update` (36564) the call at 658B17 (to ID 36581, which the review identifies as the path to the camera update; I confirmed the call target, not the path) comes before 6590E0. From 6590E0 on, the main thread runs the scene render in 36555 until its wait at 6568F3 (`call EC1DD0` with `ecx=0, edx=2`; I confirmed the call, the reviewer identified it as the wait for the job list). So the camera update has already finished when the UI job starts and the main thread is not updating the camera during a sample. What remains unproven: that the job list runs its segments strictly in table order, so that the UI job cannot start before the camera update of the same frame. Wave 1a's `frame_advanced` backstop could not fire (the counter's writer runs after the main thread waits for the UI job) and was removed; `frame` is only a label.
4. Scene-graph reads take a reference count on each node before reading it and are bounded (1024 skeleton nodes, 256 per hit reference). The player skeleton is searched only when `Get3D(false)` returns a new root, the load generation changes, or a held bone no longer reaches the held root through at most 64 parent links; otherwise the four held bone nodes' `world.translate` is all that is read. The cache and the parked holds live in `Probe` behind their own mutex (no namespace-scope static), and `kPreLoadGame` retires the cache. Actor hits are not walked (root app-culled flag only, else `unknown`/`actor`); landscape, volume and detection hits resolve no scene object at all. A reference count taken on the job thread can become the last one if the engine drops its own meanwhile, so the probe's release would run that node's destructor on the job thread. Superseded bone holds are therefore parked and released on `Main::threadID` (load messages, observed on main 606/606, or a menu event that arrives there), up to 10; beyond that, and for per-sample hit holds, the release happens on the job thread. No engine lock protects scene-graph structure. The "UI" job shares its job-list segment with Process lists, Update low list helpers, Script clear and Wait For Shadow Culling (between "Quests/Scenes update sync" and "Shadow Culling sync"), and I found no proof either way that those jobs never attach or detach children of the player or of static references. This is the **residual crash risk** of the default mode, together with destructors on the job thread. Equipping during a view request and samples during cell streaming are the likeliest triggers. Usage rule (README): default mode only for `@view` requests at settled poses on the disposable profile; periodic samples through movement, cell borders or load doors only with the explicit `bViewPeriodicOffMain=1` opt-in, which the reviewer did not endorse. If a crash appears, switch to `bViewMainThreadOnly=1` and report gameplay rows as BLOCKED.
6. Cost: the sample runs while SKSE holds its UI queue lock and the main thread waits for the UI job, so its whole duration adds to that frame (not a deadlock). `sample_ms` reports it per sample; the bound is 256 casts plus the walks above.
5. Guards: on the first sample, the seven called engine entries (61399, 26003, 15870, 36609, 18995, 68233, 68239) must have exactly the audited bytes at the audited RVA, and ID 411489 must resolve to 33363F4, or every sample is `guard_binary` (logged with the ID). Wave 1a also compared the NiCamera/PlayerCamera accessor offsets on live objects (`guard_layout`); that was a tautology (the accessors are `this` + a CommonLib constant) and was removed. Layout assurance comes from the binary audit: plain member offsets (including `TESCamera::cameraRoot` +20h and the collidable filter info +2Ch) have `static_assert`s, and in a flat build so do `NiCamera` `worldToCam` +110h, `viewFrustum` +150h, `port` +174h and `PlayerCamera::worldFOV` +13Ch.

## Unverified and NOT_RUN

| Item | Status |
| --- | --- |
| MSVC `/W4 /WX` build | `61610d9`: CI run 42719928362 **success**. `5cb2d16` (wave 2b): CI run 42723206076 **success**. Wave 2c changes: NOT_RUN in CI; g++ stub check only (not a CommonLib compile). |
| Any `view_sample` in game (pose, projection against screenshots, rays, treatments, merging, landscape/volume classification, thread distribution, guards passing live, `sample_ms`) | NOT_RUN |
| Rig hold, player visible, surroundings visible, room drawn measures | NOT_RUN; diagnostic only until the checkpoint |
| Rendered-visibility check | version 2 in game (batch O2): 7 PASS, 1 FAIL (mare-on-h0), 2 NOT_RUN. Version 3: offline re-evaluation of O2 only (8 PASS, cave-h0 NOT_RUN), not independent; fade and clip NOT_RUN |
| Per-sample CPU cost and frame-time impact | NOT_RUN |
| Hypotheses marked above (frame counter cadence, frustum tangents, camera X axis, UI job concurrency, alpha bit 0, ESM layer matrix = runtime filter for a group-0 layer-41 ray, triangle two-sidedness, pick budget meaning, the volume-layer list) | Unverified |
| Also not claimed (reviewer could not verify either): that job lists run strictly in table order; whether SmoothCam or TDM detour any of the seven guarded entries (a detour would give `guard_binary`, not a crash) | Unverified |

## Rendered visibility (design)

The senior review's point stands: geometric clearance is not rendered visibility. A ray asks "which collision surfaces lie on this line and what does the probe know about their drawing". The screen is decided by render geometry, culling, rooms, materials, textures, fog and post-processing. Three candidates were compared.

**(a) Accumulated transmittance from the ray hits (built here).** Each faded reference multiplies by `1 − alpha` once, however many of its surfaces the ray crosses; hidden and volume hits are free, opaque hits block.
- Needs: this probe; the spike's mechanism must leave its marks where the probe reads them (app-culled bit, material alpha with a blending property).
- Blind to: per-draw clipping (collision and material unchanged, so a clipped wall still reads opaque, a false *blocked*); undrawn rooms (no occluder, so a void reads *clear*, a false *pass*, the dangerous case); render geometry without collision (banners, cobwebs, hanging lights, LOD, particles, fog); collision without render geometry; hulls coarser than meshes; texture alpha (grates, foliage); shader discards.
- Cost: bounded casts per sample; any time, any cadence.

**(b) Final-image evidence from the runner's screenshots.** The runner already captures the Skyrim window with `xwd` (`session-p4/autotest/capture/core.py` `capture_xwd`, `detect_blank`; `compare.py` `color_mask`, `bbox_of`, region `compare`). The probe supplies where to look (`player_box`, per-point `screen`, ring `screen`). Two variants:
- *Marker colour:* the route applies a saturated effect shader to the player with a console command (for example `player.pms <EFSH>`; the shader is to be chosen from Skyrim.esm in the spike) and measures the marker's pixel fraction in `player_box` against an open-ground calibration at the same camera pose and heading.
- *Two-frame difference:* at a settled pose, capture A, set the player's alpha to 0 with a console command (for example `player.saa 0`, then `saa 1`; behaviour on the pinned host unverified), capture B, then capture A' at alpha 1. Changed pixels in `player_box` between A and B, minus the noise measured between A and A', are the player pixels that reached the screen, through every mechanism at once. This needs no colour calibration.
- *Floor present:* for "room drawn", the ring's screen points and the region below `player_box` are tested against `detect_blank`-style statistics and against the same region of an open-ground calibration, to catch voids.
- Needs from the runner: view request and screenshots at one settled pose, a time pause or a short capture sequence, and console steps that are restored. Needs from the probe: `player_box`, ring `screen`, `on_screen`. Nothing from the Camera spike.
- Blind to: frames between captures; anything the console marker itself changes; it measures the visible fraction, not why something is hidden.
- Cost: seconds per pose, so it fits the fixture matrix but not every frame of the sweep.

**(c) Depth or render-target readback inside a DLL.** It reads the depth buffer at the projected points, or a per-frame stencil, each frame. That needs a renderer hook (Present or the scene draw), which breaks the probe's no-hook contract. It could live only in the disposable Camera spike build, with a new dependency review, and readback stalls under DXVK. It is the only candidate that could support "zero blank frames" per frame.

Recommendation per mechanism:

| Mechanism | Per-sample measure | Validated by | Why |
| --- | --- | --- | --- |
| Fade (DYL-201 material alpha or a successor) | (a) transmittance, with a threshold set from (b) | (b) two-frame difference on single and stacked fades | (a) reads the exact alpha; (b) calibrates what transmittance means on screen, including the 0.65^8 case |
| Hide (`SetAppCulled`) | (a) `hidden` → clear | (b) on a hidden-wall fixture | the flag is what the renderer and the native caster skip |
| Per-draw clip | (b) | spike trace of clipped references joined to hit `ref`s; (b) is the oracle | (a) cannot see it; a later probe extension could import the spike's clip list |
| Undrawn room | (b) floor-present check; later a probe room-visibility field (`Main::IsRoomVisible` on the player's room, not built) | an undrawn-room fixture with seeding off | (a) reports such a view clear, so (a) alone must never pass "room drawn" or "surroundings visible" in interiors |

Rule for using (a): a ray may count as visible only for mechanisms whose (b) validation passed, and in interiors only together with a (b) floor-present result at the same pose.

## Oracle-validation checkpoint (wave 1a design; superseded by "Checkpoint as it stands" above for the fixtures)

Each test runs at a settled pose with one `@view` request, then screenshots per (b). It PASSes when every listed expectation holds; any other result is FAIL with the JSON line and images kept. All are NOT_RUN.

| # | Fixture | Setup | Expected `view_sample` | Expected image (b) |
| --- | --- | --- | --- | --- |
| V1 | Plainly visible: open ground (e.g. outside Riverwood) | rig on, no mechanism | `available`; 5/5 points `clear`, `on_screen:true`; ring ≥ 75 % clear; `under_camera_root:true`; `rotation_forward_delta_deg` < 1 | player fraction = calibration; `player_box` within 3 % of screen of the difference-image bbox |
| V2 | Plainly blocked: behind a solid wall | mechanism off | occluded points `blocked` with an `opaque` hit whose `ref` is the wall | player fraction ≈ 0 |
| V3 | Single fade | DYL-201 beam or the spike fade on one wall | `faded_only`, transmittance 0.65 (alpha 0.35) | fraction consistent with the V1 calibration × measured factor |
| V4 | Stacked fades | ≥ 2 and, if buildable, 8 faded layers | product of `1 − alpha`; 8 × 0.35 → 0.0319 | fraction falls monotonically; sets the transmittance threshold that rejects 0.65^8 |
| V5 | Hidden | spike `SetAppCulled` on the wall | `clear` with `hidden` hits | fraction = V1 calibration |
| V6 | Clipped | spike per-draw clip (if not BLOCKED) | (a) `blocked` (known blind spot, recorded) | fraction > 0: (b) decides |
| V7 | Undrawn room | camera outside a small room, seeding off | (a) any; recorded | floor-present check FAILs (void detected) |
| V8 | Unavailable paths | open a menu; switch to first person; request across a load door; set `iViewMaxHits=1` before two walls | `game_paused`; `camera_state` 0; `generation_changed`/`world_changed`; ray `indeterminate` `truncated` | none counted as clear |
| V9 | Rig hold | walk 10 s with the rig on | `offset` constant within the recorded tolerance; `camera_root_delta` ≈ 0 | — |
| V10 | Thread/consistency soak (opt-in, not default) | `iViewIntervalMs=250` with `bViewPeriodicOffMain=1`, 10 min across load doors; run only after V1–V9 and only if the orchestrator accepts the risk | no crash; `on_main_thread` rate and `sample_ms` distribution recorded; guards pass | — |
| V13 | Merging and classification | a closed faded mesh crossed on entry and exit; terrain in an exterior; a layer-13 mountain or glacier static; a spell or cloak volume on an actor between camera and player | one hit with `surfaces` ≥ 2 and transmittance `1 − alpha`; terrain `opaque`/`landscape` with `ref:null`; volume hit does not block | — |
| V11 | Cost | `@perf` capture with and without periodic view samples | frame-time delta recorded against the Phase 4 budget | — |
| V12 | Strict mode | `bViewMainThreadOnly=1` in steady play | `off_main_thread` (confirms the thread finding) | — |

The checkpoint repeats when the set of mechanisms changes (PHASE_5_5).

## Capture cadence

A `view_sample` is one moment in one frame (`frame`). It says nothing about the frames between samples. A periodic sample is queued only after the previous one was emitted; ticks in between are counted in `coalesced_ticks`, and `since_last_ms` and `since_last_frames` state the real gap. A sample can wait several frames in the queue (`queued_ms`). Claims therefore scale with cadence: "no interval longer than 1 s with the player fully hidden" holds only if every gap between consecutive available samples is well under 1 s (for example a 250 ms interval with no gap above 500 ms). Unavailable samples count as gaps, never as visible. "Zero blank frames among the frames captured" can be claimed only for frames that were captured as images. The view sample cannot claim it. A per-frame claim needs per-frame image or depth evidence, candidate (c), in the spike build.

## Review fixes (wave 2b)

| Finding | Confirmed? | Change |
| --- | --- | --- |
| M1 culled chain marks the whole reference hidden | Yes: `chainCulled` set `hidden` before the opaque test | Only a culled **root** hides a reference (`rootCulled`); a culled node below it marks only its subtree's parts culled and the parts decide (hidden only if every drawn part is culled). Actors: root flag only. Tests: culled proxy + opaque sibling → opaque; culled proxy + faded sibling → faded. |
| M2 repeated hits per reference | Yes | `AddHit` merges by reference key (form ID, else tagged object/collidable address); `surfaces` counts raw surfaces; merges use no `iViewMaxHits` slot and no extra factor. Per-ray cap 48 casts. Tests: two faces of one 0.35 wall → 0.65; eight references each crossed twice → 0.65^8 with 8 entries; ninth → full. |
| m1 parameter named like `part()` | Renamed (`sample`); MSVC `/W4 /WX` of `59613f2`+fixes passed in CI, so it was not an error there. Rechecked both files for parameters/locals named like members. |
| m2 lock not writer-preferring | Yes (LockForWrite CE32D0 disassembled) | Comment and audit row corrected; no-nesting kept. |
| m3 `frame_advanced` cannot fire | Accepted (call chain 36555 → 1009F70 → 100A02B confirmed; the wait is the reviewer's) | Check, reason and `frame_end` removed; `frame` documented as a label. |
| m4 layout guard tautology | Yes | `ViewLayoutGuard` and `guard_layout` removed; `static_assert`s added (flat-build member offsets, `cameraRoot`, filter info). |
| m5 terrain grid walk | Yes for system group 1 | Those hits are `opaque`/`landscape` without FindCollidableObject or a walk. Wave 2b also shortcut layer 13 and justified sparing layer 17 with "floors use `L_GROUND`"; both corrected in wave 2c (N2). |
| m6 what layer 41 sees | Yes | Documented above and in the README; `volume` treatment for non-drawn layers, never blocking. |
| m7 destructors on the job thread | Yes | Residual-risk text; bone holds parked and released on the main thread (cap 10); hit holds stay per sample. |
| m8 frame-time cost under the queue lock | Yes | Stated; `sample_ms` added. |
| Bone caching | Done | Search once per root/generation; holding the root prevents address reuse; retire on root change or generation change. |
| Periodic timer | Done | Refused (`periodic_opt_in_missing`, `probe_loaded.view_periodic:"refused"`) unless `bViewPeriodicOffMain=1` or `bViewMainThreadOnly=1`; README usage rule. |

## Review fixes (wave 2c)

| Finding | Confirmed? | Change |
| --- | --- | --- |
| N1 a volume and a solid surface of one reference merged | Yes: the key was the form ID regardless of class | Hits carry a class (ordinary, volume, landscape, detection) and only merge within it (`SameHit`). Identity is a nonzero form ID, or the address of a reference-less object that the probe now holds for the whole sample; form ID 0, a bare collidable or no object never merges. Tests: volume then unknown/opaque of the same form ID → two records, `blocked`; volume then volume → one record; landscape then ordinary → two; form ID 0 and unidentified → separate; held object merges only with itself. |
| N2 layer-13 arm | Yes (2AEAA5 in ID 18750 writes `0x10011`; FindCollidableObject's grid test at 2236D9..2236E6) | `LandscapeHit` is the system-group test only; layer 13 statics are ordinary hits. The layer-17 reason is corrected: not shortcutting it is right because the group test covers the heightfield, not because of vanilla floors. |
| N3 bone cache lifetime | Yes | Cache and parked holds moved into `Probe` with `viewBonesMutex`; `kPreLoadGame` retires the cache (released there when on main, as observed); each held bone must reach the held root within 64 parent links or the skeleton is searched again. |
| N4 `L_DETECTION` as volume | Yes (COLL 0010AA40 CNAM = {L_LOS}) | Chose `unknown`/`detection`: it always blocks, so it can never give a wrong clear. Walking the owning reference could report `hidden` from a culled root while the detection body stands for something drawn elsewhere. |
| N5 thread item 3 | Accepted | Rewritten with what the binary shows and what stays unproven (table order). |
| N6 reference-less culled node | Yes | `unknown`/`unresolved_culled` (`FixedTreatment`); test added. |
| m7 | Unchanged by design | Per-sample hit holds (now also reference-less objects) release on the drain thread at the end of the sample; documented. |

## Proposed shared-file changes

- Wave 2: CI does not run `tools/tests/test_visibility.py` (the workflow runs only `test_ci_scope.py` and `test_package*.py`, and `tools/ci_scope.py` does not select on `tools/visibility/**`). Proposed: a Linux step `python -B -I -m unittest discover -s tools/tests -p "test_visibility*.py"` on changes to `tools/visibility/**` or `tools/tests/test_visibility*.py`. The wave 1 frame tests skip themselves when the captures are absent.

- None required for CI: `.github/workflows/build.yml` already builds and runs `tools/testprobe/tests/*_test.cpp`, and `tools/testprobe/**` selects the MSVC probe job.
- `docs/phases/PHASE_5_5.md`, "Where the camera stands today", Measurement row, after the first runtime pass: "The probe reports the rendered camera pose, the player's projected screen box and geometric-clearance rays (`view_sample`); rendered visibility is not yet measured."
- `docs/DECISIONS.md` (proposed engineering note, not an owner decision): "View samples run in the probe's UI task drain off the main thread during play; `bViewMainThreadOnly` is the fallback if a crash is traced to them."

## Commands and results

| Command | Result |
| --- | --- |
| `for t in tools/testprobe/tests/*_test.cpp; do g++ -std=c++20 -Wall -Wextra -Werror -pedantic "$t" -o /tmp/p55-probe1/$(basename $t .cpp) && /tmp/p55-probe1/$(basename $t .cpp); done` (g++ 15.2.0) | PASS: all nine tests, including `view_policy_test` (wave 1a and again after wave 2b) |
| `gh run view 42719928362 --json status,conclusion` | `completed` / `success` (Windows MSVC build of `61610d9`) |
| `gh run view 42723206076 --json status,conclusion` | `completed` / `success` (Windows MSVC build of `5cb2d16`) |
| Wave 2c: the same g++ loop | PASS: all nine tests; mutants (class ignored in merging; reference-less culled node not unknown) abort `view_policy_test` |
| Mutation check: faded folded to clear; truncation ignored; behind-camera projected | each mutant aborts `view_policy_test` (exit 134) |
| `g++ -std=c++20 -fsyntax-only -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -pedantic` on the extracted adapter block with CommonLib stubs | no diagnostics (not a substitute for the MSVC build) |
| Binary and ESM reads described above | as recorded; no game launch |
| Wave 2: `python3 -B -I -m unittest discover -s tools/tests -p "test_visibility*.py" -v` | 29 tests OK (synthetic scenes plus wave 1 captures on this host) |
| Wave 2: `cd session-p55/oracle && python3 -B test_oracle.py` | 14 tests OK |
| Measure v2: `python3 -B -I -m unittest discover -s tools/tests -p "test_visibility*.py"` | 38 tests OK (includes 4 on the `p55-oracle-ext-1` frames) |
| Measure v2: `cd session-p55/oracle && python3 -B test_oracle.py` | 16 tests OK (includes the re-evaluation of `p55-oracle-ext-1` and a held-lock refusal) |
| Measure v2: `python3 -B evaluate_oracle.py runs/p55-oracle-ext-1/collected --json reeval/p55-oracle-ext-1-v2/oracle.json` | exit 0: calibration confirmed; open-h3, open-h2, stable-h0 PASS |
| Measure v2: `python3 -B mask_sheet.py runs/p55-oracle-ext-1/collected reeval/p55-oracle-ext-1-v2` | silhouette sheets and `silhouettes.json` (numbers above) |
| Measure v3: `python3 -B -I -m unittest discover -s tools/tests -p "test_visibility*.py"` | 46 tests OK (includes 4 on `p55-oracle-ext-1` and 4 on batch O2 frames) |
| Measure v3: `cd session-p55/oracle && python3 -B test_oracle.py` | 16 tests OK |
| Measure v3: `evaluate_oracle.py runs/p55-oracle-<run>/collected` for ext-O2, seed0-O2, seed1-O2, hide-O2, ext-1 into `reeval/O2-v3/<run>/` | exits 0, 0, 0, 2 (cave-h0 NOT_RUN), 0; table above |
| Measure v3: `evaluate_oracle.py --aggregate reeval/O2-v3/*/oracle.json` | exit 2: every mechanism with a fixture PASS except camera inside rock (NOT_RUN); fade, clip NOT_RUN |
| Wave 2: `python3 -B write_oracle_routes.py --replace` | 4 routes `ready`, runner schema valid, tightest section margins 289/135/376/280 s |
| Wave 2: `run_batch.py <4 oracle routes with sandbox paths> --simulate success` (output in /tmp) | executor runs setup and steps up to the first `request_view`, which the simulator's fake probe does not answer (BLOCKED there); control flow only, not evidence |
