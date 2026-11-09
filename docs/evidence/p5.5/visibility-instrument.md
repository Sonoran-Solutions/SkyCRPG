# P5.5-01 wave 1a — view and visibility observations in the test probe (DYL-216)

[Phase 5.5](../../phases/PHASE_5_5.md) · [probe README](../../../tools/testprobe/README.md#view-samples-p55-01-view-protocol-1-runtime-not_run)

**Status, 2026-11-09: code and portable tests only. Every in-game row is NOT_RUN.** The experimental scope of D045 applies: the change lives only in the never-shipped test probe. Its output is **diagnostic** until the oracle-validation checkpoint below passes; geometric clearance never passes an acceptance measure by itself. The probe still installs no hooks, sends no input and changes no game state.

Worktree `p55-instrument`, branch `phase5.5/p55-01-visibility-instrument`, base `da16672`. Pinned host: SkyrimSE.exe 1.7.104 SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`, Address Library 13 (`versionlib-1-7-104-0.bin`, format 5, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`), CommonLibSSE-NG 7.2.0 at `7a60f4de794095d7b0f8928d1b930a52e9a7da83`.

## What was built

| File | Change |
| --- | --- |
| `tools/testprobe/src/view_policy.h` (new) | Portable rules: `@view` grammar, INI bounds, projection, pose angles, ring and ray planning, treatment aggregation, ray status folding, transmittance, cadence coalescing, JSON shape. |
| `tools/testprobe/src/plugin.cpp` | Windows adapter: `view_sample` event, request routing, optional view timer, binary and layout guards, Havok casts, skeleton and treatment reads. |
| `tools/testprobe/src/json_line.h` | `Object::Append` (header + body in one line). |
| `tools/testprobe/tests/view_policy_test.cpp` (new) | Portable test (picked up by the existing CI glob). |
| `tools/testprobe/README.md`, `SkyCRPG_TestProbe.ini` | New "View samples" section, settings, schema rows. |

`bViewSamples` gates everything. With the key absent or not exactly `1` (and with `bEnabled=0`), the probe reads six more INI keys and otherwise behaves exactly as the branch base: no `@view` namespace (an `@view x` file stays a raw `player_state` tag), no timer, no new `probe_loaded` fields. The sample INI sets `bViewSamples=1` and `iViewIntervalMs=0`, so a runner using it gets `@view` requests but no periodic samples.

### `view_sample` fields (view protocol 1)

The README is the contract; this is the summary the route generator uses. Order is stable.

| Field | When | Content |
| --- | --- | --- |
| `view_protocol`, `seq` | always | `1`; process-wide sequence in file order |
| `requested`, `tag` | always / requests | label of `@view <label>` |
| `available`, `reason` | always | `reason` null when available; codes below |
| `frame`, `since_last_ms`, `since_last_frames`, `coalesced_ticks`, `hops`, `queued_ms` | always | cadence; `frame` is the engine update counter (ID 411489) or null |
| `current_thread_id`, `captured_main_thread_id`, `main_thread_id` | always | as `player_state` |
| `camera_state` / `frame_end` | with those reasons | diagnostics |
| `camera` | available | `state`, `pos`, `yaw_deg`, `pitch_deg`, `fov_deg`, `vfov_deg`, `world_fov_deg`, `ortho`, `node` (`world_root_camera`), `under_camera_root`, `camera_root_pos`, `camera_root_delta`, `rotation_forward_delta_deg` |
| `player` | available | `pos`, `heading_deg`, `cell {form_id, editor_id}`, `interior` |
| `offset` | available | `{x,y,z,distance}` = rendered camera − player |
| `points` | available | `head`, `chest`, `feet`, `shoulder_l`, `shoulder_r`, each `{name, world, screen {x,y} (top-left origin, 0..1), on_screen, ray}` |
| `ring` | available | `ring_0..ring_{n-1}`, same shape |
| `ray` | per point | `status` (`clear`/`faded_only`/`blocked`/`indeterminate`), `transmittance`, `hits [{ref, base, layer, distance, treatment (opaque/faded/hidden/unknown), alpha, reason}]`, `truncated`, `reason`, `skipped_player`, `casts` |
| `player_box`, `counts`, `casts`, `on_main_thread`, `config` | available | screen box of player points; status counts; cast total; thread; effective settings |

Unavailable reasons, each with its own code and one log line per process: `view_busy`, `invalid_view_request`, `enqueue_failed`, `outside_task_drain`, `generation_changed`, `not_loaded`, `off_main_thread`, `player_unavailable`, `game_paused`, `camera_unavailable`, `camera_state`, `guard_binary`, `world_camera_unavailable`, `guard_layout`, `cell_unavailable`, `world_changed`, `frame_advanced`, `sample_failed`. Ray-level indeterminate reasons: `invalid_geometry`, `no_havok_world`, `cast_budget`, `point_unavailable`, `skeleton_cap`, `truncated`. Unknown-treatment reasons: `unresolved`, `node_cap`, `non_lighting_blend`, `invalid_alpha`, `no_geometry`, `actor` (actor hits are not walked; only the root's app-culled flag is read).

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
| World lock | 68233 → CE3250, 68239 → CE3510 | owner fast path `[lock]==GetCurrentThreadId → lock inc [lock+4]`; else CAS on `count & 0FFFFFFFh`, spinning with Sleep after 10000 tries. A pending writer's high bits stop new readers, so a nested read under a waiting writer can deadlock. Unlock `lock dec [rcx+4]` | binary; matches `docs/evidence/p5/proximity-activation.md` F5 |
| Engine practice | job "Pick reference" 39058 → 40620 → 26127 | makes the `+198h` virtual call (PickObject slot) and takes LockForRead inside the main job list | binary |
| Hit → reference | FindCollidableRef 26003 → 404740 | 77988 (collidable owner → NiAVObject), `lock inc` on the object, 19750 walks `parent` (+30h) to the reference, `lock xadd` release | binary |
| Hit → object | FindCollidableObject 15870 → 2236D0 | system group 1 entities: position lookup returning `[x+30h]`; otherwise tail-jump to 77988 (1051DE0) | binary |
| Layer, app-culled | native caster at 5577D3/5577EB | `mov edi,[collidable+2Ch]; and edi,7Fh`; object from 77988; `test byte [obj+F4h],1` | binary |
| Cell world | GetbhkWorld 18995 → 2BE9B0 | `test byte [cell+40h],1` → interior: extra data at `+48h`; exterior: global | binary (guarded) |
| Scale | 188105 → 17FDE5C = 0.0142875; 187407 → 17F423C = 69.99125 | constants in `.rdata` | binary |
| Ray layer | Skyrim.esm COLL `L_LOS` 0008878A, BNAM 41 | collides with 0,1,2,4,5,6,7,8,9,10,13,14,16,17,32,38,39,40,41,43,44,53; every COLL CNAM list is symmetric; no Update/Dawnguard/HearthFires/Dragonborn override | Skyrim.esm data |

Decision: the probe calls `hkpWorld::castRay` (61399) itself while holding `bhkWorld::worldLock` for read, which is what PickObject does internally, and resolves the hit (layer, reference, base, scene object, reference root) **before** releasing the lock. Using PickObject instead would release the lock before the collidable could be resolved, and it can fail under the engine's pick budget. The cost: the probe's casts do not count against that budget (unmeasured). The ray filter is group 0, layer 41 (`L_LOS`): it sees statics, anim statics, clutter, props, trees, terrain, ground and actors, and not invisible walls, the character controller, water or transparent layers. Whether the runtime collision filter equals the ESM matrix is a **Hypothesis**. The player is excluded by reference identity, and the probe re-casts past each hit 1 unit along the ray (re-casts from inside a convex shape do not re-hit it; triangle meshes may report an exit face, **Hypothesis**).

### Scene graph, materials, threads

| Item | ID → RVA | What the binary shows |
| --- | --- | --- |
| NiNode vtable | 237098 → 1A46540 | slot 3 (AsNode) `mov rax,rcx`; slot 7 (AsGeometry) `xor eax,eax`; slot 2Ah (GetObjectByName, EE23A0) iterates children data `+118h`, count WORD `+122h` (free index) |
| Geometry vtables | BSTriShape, BSDynamicTriShape | slot 7 returns `this`, slot 3 returns 0 |
| Names, parents | EE0A40, 2EECE0 | name compared at `+10h`; parent walk at `+30h` |
| `lastUpdatedFrameCounter` | NiAVObject::UpdateWorldData (NiNode slot 30h → EEBF40) | stores the global at 33363F4 (ID 411489) into `node+104h` |
| Frame counter | 411489 → 33363F4 | 28 RIP-relative references in `.text`: 27 `mov`/`cmp` reads and one write, `inc dword [rip]` at 100A02B, right after a renderer virtual call. "Once per frame" is a **Hypothesis**. |
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
3. Camera: the NiCamera and PlayerCamera are process-lifetime objects. The camera update runs on `Main::threadID` (Phase 5 finding), and in steady play the drain runs inside the job list's "UI" job. That the main thread is inside the job list, and so not updating the camera, while that job runs is a **Hypothesis** from the job table, not proven. Backstop: the frame counter is read before and after the sample, and any change gives `frame_advanced`.
4. Scene-graph reads (player skeleton, hit-reference subtrees) take a reference count on each node before reading it and are bounded (1024 and 256 nodes). Actor hits are not walked at all (root app-culled flag only, else `unknown`/`actor`). No engine lock protects scene-graph structure. The "UI" job shares its job-list segment with Process lists, Update low list helpers, Script clear and Wait For Shadow Culling (between "Quests/Scenes update sync" and "Shadow Culling sync"), and I found no proof either way that those jobs never attach or detach children of the player or of static references. This is the **residual crash risk** of the default mode. Equipping during a view request and samples during cell streaming are the likeliest triggers. Mitigation for the first game run: request samples only at settled poses; if a crash appears, switch to `bViewMainThreadOnly=1` and report gameplay rows as BLOCKED.
5. Guards: on the first sample, the seven called engine entries (61399, 26003, 15870, 36609, 18995, 68233, 68239) must have exactly the audited bytes at the audited RVA, and ID 411489 must resolve to 33363F4, or every sample is `guard_binary` (logged with the ID). The NiCamera/PlayerCamera accessor offsets are checked once on live objects (`guard_layout`), and plain member offsets have `static_assert`s.

## Unverified and NOT_RUN

| Item | Status |
| --- | --- |
| MSVC `/W4 /WX` build of the probe | NOT_RUN. A g++ `-fsyntax-only -Wall -Wextra -Wshadow -Wconversion -pedantic` check of the new adapter block against hand-written stubs of the CommonLib declarations passed; that is not a CommonLib compile. |
| Any `view_sample` in game (pose, projection against screenshots, rays, treatments, thread distribution, `frame_advanced` rate, guards passing live) | NOT_RUN |
| Rig hold, player visible, surroundings visible, room drawn measures | NOT_RUN; diagnostic only until the checkpoint |
| Per-sample CPU cost and frame-time impact | NOT_RUN |
| Hypotheses marked above (frame counter cadence, frustum tangents, camera X axis, UI job concurrency, alpha bit 0, ESM layer matrix = runtime filter, triangle two-sidedness, pick budget meaning) | Unverified |

## Rendered visibility (design)

The senior review's point stands: geometric clearance is not rendered visibility. A ray asks "which collision surfaces lie on this line and what does the probe know about their drawing". The screen is decided by render geometry, culling, rooms, materials, textures, fog and post-processing. Three candidates were compared.

**(a) Accumulated transmittance from the ray hits (built here).** Each faded hit multiplies by `1 − alpha`, hidden hits are free, opaque hits block.
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

## Oracle-validation checkpoint (to run with the Camera spike build)

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
| V10 | Thread/consistency soak | `iViewIntervalMs=250`, 10 min across load doors | no crash; rates of `frame_advanced` and `on_main_thread` recorded; guards pass | — |
| V11 | Cost | `@perf` capture with and without periodic view samples | frame-time delta recorded against the Phase 4 budget | — |
| V12 | Strict mode | `bViewMainThreadOnly=1` in steady play | `off_main_thread` (confirms the thread finding) | — |

The checkpoint repeats when the set of mechanisms changes (PHASE_5_5).

## Capture cadence

A `view_sample` is one moment in one frame (`frame`). It says nothing about the frames between samples. A periodic sample is queued only after the previous one was emitted; ticks in between are counted in `coalesced_ticks`, and `since_last_ms` and `since_last_frames` state the real gap. A sample can wait several frames in the queue (`queued_ms`). Claims therefore scale with cadence: "no interval longer than 1 s with the player fully hidden" holds only if every gap between consecutive available samples is well under 1 s (for example a 250 ms interval with no gap above 500 ms). Unavailable samples count as gaps, never as visible. "Zero blank frames among the frames captured" can be claimed only for frames that were captured as images. The view sample cannot claim it. A per-frame claim needs per-frame image or depth evidence, candidate (c), in the spike build.

## Proposed shared-file changes

- None required for CI: `.github/workflows/build.yml` already builds and runs `tools/testprobe/tests/*_test.cpp`, and `tools/testprobe/**` selects the MSVC probe job.
- `docs/phases/PHASE_5_5.md`, "Where the camera stands today", Measurement row, after the first runtime pass: "The probe reports the rendered camera pose, the player's projected screen box and geometric-clearance rays (`view_sample`); rendered visibility is not yet measured."
- `docs/DECISIONS.md` (proposed engineering note, not an owner decision): "View samples run in the probe's UI task drain off the main thread during play; `bViewMainThreadOnly` is the fallback if a crash is traced to them."

## Commands and results

| Command | Result |
| --- | --- |
| `for t in tools/testprobe/tests/*_test.cpp; do g++ -std=c++20 -Wall -Wextra -Werror -pedantic "$t" -o /tmp/p55-probe1/$(basename $t .cpp) && /tmp/p55-probe1/$(basename $t .cpp); done` (g++ 15.2.0) | PASS: all nine tests, including `view_policy_test` |
| Mutation check: faded folded to clear; truncation ignored; behind-camera projected | each mutant aborts `view_policy_test` (exit 134) |
| `g++ -std=c++20 -fsyntax-only -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -pedantic` on the extracted adapter block with CommonLib stubs | no diagnostics (not a substitute for the MSVC build) |
| Binary and ESM reads described above | as recorded; no game launch |
