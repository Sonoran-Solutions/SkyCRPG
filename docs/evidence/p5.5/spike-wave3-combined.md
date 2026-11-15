# P5.5-02 wave 3: combined spike build (shader cut + Stage 1 + player-fade hold, with the fill light)

Disposable spike work on `phase5.5/p55-02-spike-combined`: the shader package at `3528e92` merged with the light package at `0279999` (common base `744609f`). Nothing here is merged into the shipped Camera DLL. Every game row is **NOT_RUN**: no game was launched for this merge. Compilation was not attempted on this host (Windows-only target); the portable tests and the wiring audit say nothing about in-game behaviour.

**Update 2026-11-15.** This file was written for the first merge (`d55f885`) and the text below is kept as written; its "NOT_RUN" statements are superseded where a pointer says so. The combined build has since run in game and moved to head `46c5b3c` (shader `1fd65e0` merged at `2ce7a9b`; CI 43606729447): see "Current defaults" and "Game runs (combined build)". The shader-side runs and features are in `spike-wave2-shader.md` ("Game runs"); the owner's decisions are D049-D064 in `docs/DECISIONS.md`.

Package evidence this file builds on: `spike-wave2-shader.md` (shader cut, void colour, sky hide, Effect cut, Stage 1, fade hold) and `spike-wave2-light.md` (fill light, stealth-neutral wraps, light trace, narrowed room-hook gate). In-game status before the merge (orchestrator ledger): shader `3528e92` batch W11 clean (fade hold P2/F1 PASS on the Mare loft, Stage 1 S17 PASS, no refusal); light `0279999` batch W4 lifecycle L4 PASS; the W4 shader look on Embershard was "very dark overall", which is why the two are combined.

## What was merged

The merge applied cleanly except four files; everything else (the light's new files, its `fixed_spike_policy.h` room-gate hunks and `fixed_spike_policy_test.cpp`) merged automatically and is unchanged from `0279999`.

| File | Resolution |
| --- | --- |
| `modules/camera/config/SkyCRPG_Camera.ini` | Both blocks kept in one `[FixedCameraSpike]` section: the shader's (fade hold, shader cut, passes two to four) first, then the light's. No key or comment changed. |
| `modules/camera/src/fixed_spike.h` | Union of every call of both sides (orders below). `configure` takes `shader::Config` then `light::Config`. Trace writer rows are the sum of both sides' terms. The `installed …` line is the shader's line byte for byte; the light's three fields are appended only when the light package exists (`light::lights` non-null, i.e. `iLightFillMode=1` or `iLightTrace=1`). |
| `modules/camera/src/plugin.cpp` | Shader INI reads, then the light's nine reads, then one `fixed::configure(f,shaderCut,l,settings,…)`. |
| `modules/camera/tests/lock_refresh_wiring_test.py` | Both sides' sections kept, with three merged expectations: the unguarded-token list has both `fixed::shader::Config shaderCut;` and `fixed::light::Config l;`; the spike refusal enum has 29 codes (base 19 + FCS-15 + FCS-51..59) with `fadeSetting == 15`; the shader's eligible-frame assertion and the light's `configure` assertion now accept the combined order and signature. A new "combined wiring" section is added (below). No check was deleted. |

## Install order (kDataLoaded, `fixed::install`)

`installCaster` → `installRoomHook` (only with `iRoomSeedMode` 1-3) → `shader::install` (no-op unless `iSeeThroughMode=3`; it only asks, the commit runs in the first main-thread frame) → `installFade` (FCS-15 on a failed check) → `light::installLight` (no-op unless a light key is on) → `active = true` → the installed line → the `player fade hold=` line.

Why: the shader and the fade hold keep their `3528e92` positions, so their behaviour and log lines are unchanged. The light's install only checks IDs and prologues and, with `iLightFillMode=1`, patches the two light-level summers (IDs 106362, 107776); it touches nothing the other two read or write (shader: Lighting/Utility/Effect vtable slots 6/7, the void clear-colour call site, the sky root's hidden bit; fade: one Setting value), so its place relative to them is free; it goes last so that the shader build's sequence is untouched, and before `active` as on the light branch. The light's stealth gateway and the caster gateway and the shader's void relay are separate `VirtualAlloc` blocks; none uses the trampoline.

## Frame order (`endFrame`, main thread)

Eligible frame:

1. lifecycle requests taken, `restoreAll` for any, `light::noteExits(requests)` (a load request ends any light hold);
2. cell change handling, rig eligibility, `publishView`, hide scan, `traceRig(true)`, `traceRoom` (+ `FixedRoomPass` once a second with `iRoomTrace=1`);
3. shader: `noteCell`, `publish` (the plane from the transform SmoothCam has just written), `trace`;
4. light: `frameLight(true, keep, …, "eligible")` — after the rig has decided the frame is eligible (its duty is "keep") and after the room trace (both read the room graph with the vanilla test; the light's census uses `within`, never the hook's claim);
5. fade: `observeFade(); settleFade(true); traceFade();` — **last**, as on `3528e92`: the hold must be in place for the next native camera update and must reflect the final decision of this frame.

Ineligible frame: `shader::deactivate(); shader::trace()` → `observeFade(); settleFade(false); traceFade();` (put the engine's value back at once) → `dropSnaps` → `restoreNow`/`restoreAll` → `light::frameLight(false, fillDuty(…, exiting, cause, …, silenceHeld()), …)` → `traceRig(false)`. The shader's own sequence is unchanged from `3528e92`; the light's call stays after the hide restore with the `exiting` flag it needs (grace frames hold the light as it is), as on `0279999`.

There is no data flow between the shader and the light calls; the order between steps 3 and 4 only fixes the order of trace lines within a frame. With the light keys off `frameLight` returns at its first statement, so the trace is the shader build's.

`lifecycle()` (any thread) is the shader build's: `shader::deactivate()`, and on the main thread `restoreAll` + `settleFade(false)`. The light takes lifecycle exits only through the request bits read in `endFrame` (step 1), as on `0279999`.

## Refusal codes

| Series | Codes | Once-mask |
| --- | --- | --- |
| Base spike (`fixed::Refusal`) | 1, 2, 3, 10-14, 20-23, 30, 31, 40, 41, 50, 60, 61 | 64-bit, `fixed::refuse` |
| Fade hold (shader branch, base enum) | 15 | same |
| Light (light branch, base enum) | 51-59 | same |
| Shader cut (`fixed::shader::Refusal`) | 66-79 (first pass), 80-88 (Effect, void), 89-90 (sky), 91-92 (Stage 1) | its own 32-bit mask from 66 |

29 codes in the base enum, all distinct and below 64; 27 shader codes, 66..92; the two sets are disjoint (checked by the combined wiring section). Note for later merges: the ext package was given FCS-60..69 in wave 2, which overlaps 60/61 (base) and 66-69 (shader); it is not in this build.

## Trace capacity

Rows reserved = rig 12000 + room (12000 + `FixedRoomPass` 1500) + hide 8000 + shader (3 or 5 kinds × `shader::traceLines`) + `FixedFade` 1500 (with `bHoldPlayerFade=1`) + light (`FixedLight`/`FixedLightShadow`/events 6000 + `FixedLightLevel` 1500), each term only when its trace can write. The writer starts if any of rig, room, hide, shader or light trace is on. Every `LineBudget` of both runtimes is matched to one term (wiring audit).

## INI defaults in the combined file

| Key | Default | From |
| --- | --- | --- |
| `bEnabled` | 0 | base |
| `iRoomSeedMode` | 0 | base; identical on both branches |
| `iSeeThroughMode` | 0 | base; 3 selects the shader cut |
| `bHoldPlayerFade` | 1 | shader (base rig feature, active in every see-through mode) |
| `fShaderCutHeight` / `fShaderCutRadius` / `iShaderCutMaxPerFrame` / `iShaderCutShaders` | 160 / 0 / 4096 / 3 | shader |
| `bShaderCutEffect`, `bVoidColor`, `bVoidHideSky`, `bShaderCutEffectWhole` | 1 | shader (owner decisions 2026-11-10, used only in mode 3) |
| `sVoidColor` | 0,0,0 | shader |
| `iCutStyle` / `bCutKeepFarGallery` / `bCutStyleShadows` | 1 / 0 / 1 | shader (Stage 1 by default in mode 3) |
| `iShaderCutTrace` | 0 | shader |
| `iLightFillMode` | 0 | light |
| `fLightFillHeight` / `Radius` / `Fade` / `Red,Green,Blue` | 192 / 640 / 0.6 / 1.0, 0.86, 0.70 | light |
| `bLightFillStealthNeutral` | 1 | light |
| `iLightTrace` | 0 | light |

**Fill light default: off (0), in every interior, kit or dungeon.** The fill light has no cell-type gate: `fillWanted` is `mode == 1 && eligible && interior && ready`, so with 1 it lights the Bannered Mare exactly as it lights Embershard. The owner's 2026-11-10 decision ("dungeons get the fill light as a regular feature with a setting to turn it off") is not implemented as a dungeon-only rule in either branch, and the spike rule is that every new key is default-off; a dungeon-only default would be new behaviour that neither package reviewed. Routes turn it on per run.

**Room seeding vs the light's narrowed gate.** All three commits ship `iRoomSeedMode=0`, so 0 wins with no conflict: the room hook is not installed and the gate question does not arise at the defaults. The narrowed gate itself (claims only for the camera pass, ID 13408; light passes IDs 106400/106401 answered vanilla and counted as `lightNear`) came in through the light branch's `fixed_spike_policy.h` and is **unconditional** in the combined build, independent of the light keys: the light evidence designates it a correction of wave 1 for every build, and making it depend on the light keys would create an untested third variant. Consequence: with `iRoomSeedMode` 1-3 the combined build claims for fewer callers than `3528e92` (route `p55-shader-bfb-seed1`); drawing is decided by the camera pass only, so the drawn rooms should not change, but which rooms portal-strict lights belong to near the camera can (Hypothesis: no visible change; compare screenshots).

### Current defaults (head `46c5b3c`, 2026-11-15)

The table and the two paragraphs above describe `d55f885` and are superseded for these keys (INI of this branch; the struct defaults in code stay as they were, the project pattern):

| Key | Now | Since (commit, decision) |
| --- | --- | --- |
| `iRoomSeedMode` | 1 | `7ac3001`; orchestrator's call overnight, confirmed by the owner (D054). The narrowed-gate paragraph above no longer describes the defaults. |
| `iLightFillMode` | 2 (dungeon-only gate) | gate `5663b14`; default `1753a5d` (owner “2.0 plus door fix”, D056). The "Fill light default: off" paragraph above is superseded: dungeons now get the fill by default, kit and settled cells do not. |
| `fLightFillFade` | 2.0 | `1753a5d` (D056); radius 640, height 192, colour and stealth-neutral unchanged |
| `bFramePass`, `fFrameRadiusInner/Outer` | 1, 500/900 | Stage 2 `cc2bbf2`; falloff `ce737a4` (D051) |
| `iCutStacks` / `iCutFill` | 1 / 0 | `9573098` / `12a702d`; caps parked (D052) |
| `iCutTilt` | 1 | `bc4ca54` (D053) |
| `iDoorVoid` / `fDoorVoidSide` | 1 / 320 | `03dcdfc` (D056) / `247cb61` (D057) |
| `iDoorVoidLights` / `iDoorVoidGlow` | 0 / 1 | `e7b88b0` kept off (D058) / `22ce7fc` (D059) |
| `fShaderCutFollowDown` | 480 | `dcd4021` (D060) |

## Equivalence to each side

With the light keys off (`iLightFillMode=0`, `iLightTrace=0`): no light runtime, `installLight`/`frameLight`/`noteExits`/`silenceHeld` return at once, nothing is patched; the installed line, `FixedShader*`, `FixedFade`, `FixedRig`, `FixedRoom`, `FixedHide` lines are the shader build's byte for byte. Two exceptions, both inherited from the light branch's room-gate change: (a) with `iRoomTrace=1` one extra `FixedRoomPass` line per second (its own kind and budget; the route checker counts unknown kinds and never calls them malformed); (b) with `iRoomSeedMode` 1-3 the narrowed gate above (`FixedRoom` keeps its format; `viewerCalls`, `gatedNear`, `claims` can be lower than on `3528e92`).

With `iSeeThroughMode` not 3 and the shader keys off (`bHoldPlayerFade=0`): no shader runtime, no fade hold, so behaviour is the light build's. Log differences only: the installed line carries `shaderCut=off` after `seeThrough=` and omits the light fields when the light package is off (the light build printed `light=off fill=off lightTrace=0`); a `player fade hold=off …` line follows it. The route checker matches the installed line by prefix and ` rooms=… `, so both are accepted.

## Interactions (Source: code of both packages; nothing measured in game)

- **Does the cut or Stage 1 select the fill light?** No. The fill light is an `NiPointLight` handed to `ShadowSceneNode::AddLight`; it has no geometry and no owning reference, so it never reaches the Lighting/Utility/Effect `SetupGeometry`/`RestoreGeometry` hooks, and the selection rules exclude ownerless geometry and lights anyway. It sits at feet + 192, above the default plane (feet + 160): it has no body to cut, and it lights whatever is drawn below the plane (floor, walls up to 160, the player, actors) and nothing above it is drawn. Cut faces are not capped, so the plane edge is not lit as a surface.
- **Shadow passes.** The fill light is created with `shadowLight = false` (and not portal-strict, no distance fade): it has no shadow map, holds none of the four shadow-mask channels, and so never produces a Utility other-view pass. The cut's other-view path (`fxOtherView`, and Stage 1's `bCutStyleShadows` rule for geometry removed whole) applies only to the scene's own shadow lights, as on `3528e92`.
- **Per-draw light count.** The fill light adds one point light to draws within its 640 radius. Hypothesis: in SE the Lighting vertex shader permutation does not depend on the point-light count (the count selects pixel shaders), so no new patched vertex shader is needed; if one were, the audited-hash check would refuse it (FCS-75) and those passes would be drawn natively, uncut. Watch `created` and `skipShader` on `FixedShader` (and any FCS-75 line) in the first run, fill on against fill off.
- **Sky hide and void colour vs the stealth wraps.** No shared state: the void colour replaces the main view's clear colour at one call site, the sky hide flips the sky root's hidden bit in Show Sky interiors, the stealth wraps patch the entries of the two light-level summers and subtract the fill light's own ID 108292 term. Neither package reads or writes the other's state, and neither file names the other's namespace (checked: no `shader::` in the light files, no `light::` in the shader files).
- **Fade hold vs the light's menu/Tween logic.** No interaction: the fade hold writes one Setting (`fCharControllerCheckHeightOffset:Camera`) on eligible frames and puts it back on ineligible ones and on main-thread lifecycle exits; the light reads `UI::numPausesGame` and the camera state id on ineligible frames to decide hold/off. Both run on the main thread inside `endFrame`; neither reads what the other writes. During a pausing menu the fade is released (engine value) while the fill light is held as it was; both are what each package did alone.
- **Lighting of the void and the dark-cave look.** The void stays the configured colour (black): the clear colour is not lit. The fill light only brightens the geometry around the player, which is what W4 found missing on Embershard.

## Wiring audit: combined section

`PASS combined wiring`: install order caster → room hook → `shader::install` → `installFade` → `light::installLight` → `active`, each once; the installed line is the shader format with the light suffix appended only behind `light::lights`; eligible frame `publishView` → `traceRoom` → `shader::noteCell` → `shader::publish` → `shader::trace` → `light::frameLight(true…)` → fade settle, with nothing of either package after the fade; ineligible frame `noteExits` → `shader::deactivate` → fade back → restore → `light::frameLight(false…)` → `traceRig(false)`; `configure` builds the shader then the light runtime before the writer starts; writer rows contain every term once and every `LineBudget` of both runtimes is accounted for; spike codes and shader codes disjoint (spike below 64, shader 66..92, light 51..59 complete, 15 present); one `[FixedCameraSpike]` section with both key sets at the defaults above; no conflict markers left.

## Tests (this host, from the repository root, CI flags)

| Test | Result |
| --- | --- |
| 17 portable Camera tests, `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include` (includes `fixed_spike_policy_test`, `fixed_spike_shader_policy_test`, `fixed_spike_fade_policy_test`, `fixed_spike_light_policy_test`) | **PASS** |
| `python3 -B -I modules/camera/tests/lock_refresh_wiring_test.py` | **PASS**, 12 lines (both sides' sections and the combined one) |
| MSVC /W4 /WX build of the DLL | **NOT_RUN** here (needs the Windows CI job) |

## First-run protocol (NOT_RUN)

One launch per row, traces on (`iRigTrace=1 iRoomTrace=1 iShaderCutTrace=1 iLightTrace=1`), fill defaults, `bLightFillStealthNeutral=1`. In every row: the installed line starts `FixedCameraSpike installed rig=1 caster=own-detour ` and contains `shaderCut=installed` when mode 3 and ` light=ready fill=ready lightTrace=1` when the fill is on; `player fade hold=installed`; no `FCS-` line at all; profile restored at the end.

*Answered by game runs, see "Game runs (combined build)": rows 1-6 ran as batch W12 on `d55f885` (8 of 8 clean, no FCS line; the Mare fill off and on, the lifecycle and a stealth route included) and again as W14 on `a26be55` with the frame pass. The protocol below is kept as written.*

1. **Embershard Mine, cut and fill together** (`iSeeThroughMode=3`, `iRoomSeedMode=0`, `iLightFillMode=1`; the `p55-shader-cave` poses): coc arrival, headings 45/135/225/315 with a screenshot each, F8 off and on. Expect the W4 cross-section on black, now with the floor and the player lit around the feet; `FixedShader` as in W4/W11 (cut > 0, `skipShader` and `created` comparable to row 2), `FixedShaderVoid` sky held in Show Sky cells, `FixedLight … fill=active fillHidden=0 fillCull=0`, `fillAt` z = feet + 192; `FixedLightLevel` `level − levelNoFill` near 0.5 with `neutral=1`; F8 → `event=hide cause=f8`, `event=remove` ~0.5 s later, `event=add` on return; `FixedFade held=1` in every eligible window.
2. **Same, fill off** (`iLightFillMode=0`): the A of the A/B; expect the W4 look exactly and no `FixedLight` line.
3. **Bleak Falls Barrow corridor, cut and fill together** (`p55-shader-bfb-seed1` pose 2048, −9216, −2556, z first; `iSeeThroughMode=3`, `iRoomSeedMode=1`, `iLightFillMode=1`): four headings, screenshots. Expect the seeded room drawn and now lit near the player; ` rooms=hooked ` in the installed line; `FixedRoomPass` lines with `lightPassNear` > 0 and claims from the camera pass only. Compare against the W4 `p55-shader-bfb-seed1` screenshots for anything that changed besides the light (the narrowed gate).
4. **Bannered Mare, Stage 1, fill off (default)** (`p55-shader-mare`, `p55-shader-mare-loft`): must reproduce W11 (P2, F1, S17 PASS) with no light line; this is the "light keys off = shader 3528e92" row.
5. **Bannered Mare, Stage 1, fill on** (A/B, owner look only): whether the lantern over-lights an already lit kit interior; this is the evidence for a dungeon-only default, which this build does not have.
6. **Lifecycle, cut and fill on** (`p55-shader-lifecycle` + the light's L4 steps): inventory and console 2 s each → no `hide` event, fade released (`held=0`) during the menu and re-held after; F6 first person and back → `cause=first-person`, fade released; load door / coc → `cause=load`, `event=add` in the new cell; door-out → exterior: `remove cause=engine-gone` or `exterior`, sky and void back to the engine's (S15), fade held outdoors by design (rig eligible).

## Game runs (combined build, 2026-11-11 to 2026-11-14)

The sections above describe the merge at `d55f885` before it ran. The combined build has run in game since; this section records it. Sources: the orchestrator ledger `session-p55/ledger.md`, run folders `session-p55/routes/runs/<route>-<batch label>/check.md`, sheets in `session-p55/logs/sheets/` (all outside the repository). Shader-side features (door void, glow cards, ramp follow-down, tilt, frame pass, caps) and their shader-route runs are recorded in `spike-wave2-shader.md` under "Game runs"; only what the combined build adds, or ran only on it, is here. Host: Steam Skyrim SE 1.7.104 under Proton Experimental `20261027b`, then `20261105c` from 2026-11-10 (every batch below until PB2) and `20261112` from 2026-11-14 (PB2). Verdict words as in that file: PASS, FAIL, BLOCKED, NOT_RUN, **OWNER** for a row that records numbers and leaves the look to the owner; a "look" is the orchestrator's reading of stills, not an owner acceptance. Nothing is merged to main.

### Batches

| Batch | Head (CI) | Routes | Result |
| --- | --- | --- | --- |
| W12 | `d55f885` (43000209724) | 8 | Clean, no FCS line. Embershard fill off against on at the same pose: `passes=2485 cut=308 shaders=4,0,0 created=0` in both; Bleak Falls Barrow `cut=1883 shaders=7,0,0 created=0` in both: **the permutation Hypothesis above holds**, the extra point light changes no shader selection. S1 S2 S3 S11 S14 S18 F1 PASS on the cave and BFB pairs; L1 PASS (`bfb-nofill`), L2 OWNER (fill added and active). Mare hearth S17 and F1 PASS with the light off and on. Lifecycle with the light on: S8 S13 S15 F1 and L4 PASS (light held through the menu, removed on exits, fade value restored). Stealth: `player.getlightlevel` 6.46 at the BFB pose with the fill on and `neutral=1`, the same as the light package's fill-off value; trace `level` 0.5001 against `levelNoFill` 0.0000. Look: floor, props and the player clearly brighter around the player on a black void; both rooms still have hard cut edges. |
| W14 | `a26be55` (43081646453), Stage 2 merged | 8 | 8 of 8 clean. All rows PASS or OWNER (S4, S5, S19, L2, L6); S21 PASS on the six posed routes; lifecycle S8 S13 S15 F1 L4 PASS; frame pass counters over all runs `skipShared` 0, `skipTargets` 0, `mismatch` 0, `offMain` 0; only benign FCS-75, 86, 90. Stealth trace identical to W12 (0.5001 with fill, `levelNoFill` 0.0000, lit sample 2.944/2.444): the frame pass changes no light value. Sheets `w14-{cave,bfb,mare}-fill-vs-nofill.png`. |
| S2, F2, G2 | `59dc1bf`, `b300603`, `94574cf` | 2 + 2 + 2 | S2 (stack rule): Mare routes S22 and S23 PASS with the fill (census 4/0). F2: `comb-cave-caps` `filled=0`, S24 FAIL as on the shader side (alpha-test and write-mask gates). G2: `comb-cave-fill` **BLOCKED** at the main menu (the console did not open after 3 presses, host focus loss) and `comb-cave-caps` **NOT_RUN**: the combined caps pair never ran. |
| L2, X2, Y2 | `ef901a7`, `2bd51ef` | 4, 3, 3 | Fill light with the tilt (L2) and with the seed default (X2, Y2): clean, no FAIL, BLOCKED or NOT_RUN row; S25, S23, S22, S21 PASS or OWNER as on the shader side. |
| D2 | `76bdc09` | 5 | Tilt default: flat cells have tilt rows and `band=tilt`, kit cells `tilted=0`, the pinned flat record `comb-cave-fill` has no tilt rows. No FCS code. Sheet `d2-comb-bfb-tilt.png`. |
| WK2 | `76bdc09` | 1 | Embershard walk end with the fill light at its then defaults (fade 0.6, radius 640), see below. |
| FS1-FS3 | `76bdc09` | 8 + 7 + 4 | Fill light sweep, below. |
| GC1 | `dd23f7e` (43317691546) | 6 | Dungeon gate, below. |
| DD2, DS1, GD1, RP1 | `1753a5d` (43329763324), `b5f2b84` (43349589177), `2f4bafe` (43374916333), `dded309` (43433155241) | 6, 4, 5, 4 | Default confirmations, below. |
| SM1 | `46c5b3c` (43606729447) | 1 | **BLOCKED** before any launch: Steam had updated Proton to `20261112` and the harness pin refused. Nothing started. |
| PB2 | `46c5b3c` | 4 | Proton `20261112` baseline, below. |

### Fill light brightness sweep (FS1-FS3, 2026-11-12, owner “Yes option 1”, D055)

19 of 19 routes ran (cave 8, Bleak Falls Barrow 7, Bannered Mare 4), postflight ok, no FAIL or BLOCKED, on `76bdc09` (fill switched on per route, tilt on, every other key at its default). Variants are fade/radius: a 0.6/640 (the old default), b 1.2/640, c 2.0/640, d 3.0/640, e 1.2/1024, f 2.0/1024; each cell also has a no-fill reference. Metric S27: mean luma of the player's screen box.

| Cell and pose | no fill | a | b | c | d | e | f |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Embershard walk end, h0-end | 6.1 | 12.0 | 17.9 | 25.3 | 34.7 | about b | about c |
| Embershard walk end, h3-end | 7.7 | 13.7 | 19.3 | 27.3 | 35.7 | about b | about c |
| Bleak Falls Barrow corridor, h0 | 11.0 | 17.7 | 24.8 | 35.2 | 52.5 | not recorded | not recorded |

Radius barely matters; fade is the knob. Whole-frame brightness against no fill: Embershard c 1.44-1.90x, d 1.66-2.34x; Bleak Falls Barrow c about 2.1x, d about 2.7x; Bannered Mare (a lit inn) a 1.15x, c 1.48x, d 1.70x, which is why a dungeon-only gate is needed for a default-on. Look (orchestrator): the player is clearly visible at c and d at both dark Embershard poses; d reads as a strong warm torch pool. The owner picked c plus the door fix (D056). Sheets `fs-cave-sweep.png`, `fs-bfb-sweep.png`, `fs-mare-sweep.png`. Also in FS1: the new S28 ramp record at variant a, held-floor lag up to 69 units (41 of 90 rows above 8), model ceiling margin down to -26, real ray margin above the plane 16.2 at the h0 end: the latent ramp risk of `embershard-walkway.md` confirmed, worse than that note's 42-46 (fixed by D060, below).

WK2 (before the sweep; `comb-cave-walk`, fill at fade 0.6): player-box luma at the walk end, mean/max, no fill h0 6.1/16, h1 11.6/113, h2 31.9/251, h3 7.7/28; fill h0 12.0/48, h1 17.5/130, h2 37.6/252, h3 13.7/53 (about double at the dark poses); whole frame 1.13-1.28x and centre 1.13-2.25x brighter. Look: faintly visible at h0 and h3, still dim: the defaults were not enough at this pose. Sheet `wk-walk-end-nofill-vs-fill.png`.

### Dungeon-only gate (GC1, DD2, DS1, GD1; S29)

`iLightFillMode=2` (combined `5663b14`, review notes `c4bf09a`; binary review PASS WITH NOTES: `GetLocation` 2BB180 direct and null-safe, 36 keyword ID/name pairs match Skyrim.esm): the fill is lit only where the cell's location (and its parents) carries a dungeon keyword, a settled keyword wins per level, no location means off. New trace kind `FixedLightGate`; 301 of 759 interior cells are dungeon by the ESM parse; the Mine is in neither list. Fort Greymoor's military-fort type counts as dungeon, which the ledger marks as the owner's call with no answer recorded.

- **GC1** (`dd23f7e`): 6 of 6 ran, postflight ok. Resolve line: fill-gate keywords 36 of 36 found (all by Skyrim.esm id), 17 dungeon. S29 PASS: Embershard (dungeon, `LocTypeDungeon`) lit, Bleak Falls Barrow (dungeon) lit, Fort Greymoor (`LocTypeMilitaryFort`, dungeon) lit, Bannered Mare (`LocTypeInn`) hidden, Dragonsreach (`LocTypeCastle`) hidden. `gate-life`: lit in Embershard, hidden outside (`cause=cell`), lit again on return, hidden in the inn and never shown. The first S29 evaluation FAILed on the hide cause (`cell`, set at the load, before the gate is read), a harness over-assertion: S29 now accepts `cell` and `engine-gone`, and the re-check is PASS (4 steps); the stored `check.md` of `p55-comb-gate-life-GC1` still shows the first FAIL. FCS-90 informative as in 200+ earlier runs; no other FCS.
- **DD2** (`1753a5d`, defaults `iLightFillMode=2`, `fLightFillFade=2.0`, door void on), with the shader's DD1: 14 of 14 ran, no FAIL or BLOCKED; S29 PASS on `gate-embershard` and `gate-bfb` (lit), `gate-mare` (hidden) and `gate-life` (4 steps); L2 lit at feet + 192. Look: Embershard arrival much brighter around the player and the door. Sheets `dd-embershard-old-vs-new-defaults.png`, `dd-bfb-old-vs-new-defaults.png`.
- **DS1** (`b5f2b84`, door lane 320): `gate-embershard`, `gate-bfb` (lit), `gate-mare` (hidden) and `lifecycle`, S29 PASS.
- **GD1** (`2f4bafe`, glow cards hidden by default): `comb-cave-walk`, the three gate routes and `comb-lifecycle`, 5 of 5 ran; S29 PASS on the three gate routes; the Embershard routes (`comb-gate-embershard`, `comb-cave-walk`) hide 2 glow cards, other cells 0.

### Ramp follow-down on the combined routes (RP1)

RP1 (`dded309`, 4 of 4 ran: `comb-cave-walk`, `comb-fsw-cave-ramp`, `comb-gate-embershard`, `comb-lifecycle`): S28 on `comb-fsw-cave-ramp`, FS1 (old rule) to RP1 (`fShaderCutFollowDown=480`): first descent held-floor lag max 62.5 to 23.8, model ceiling margin -19.5 to +19.3; second descent lag 78.3 to 20.2, margin -35.3 to +22.8; ray margin above the plane at the descent poses 16 to 30.6 and 18.3 to 30.5. Full numbers and the shader routes are in `spike-wave2-shader.md`. Ramp fix confirmed (D060).

### Combined-merge review and the foreign-light fix (2026-11-14, owner “Yes, review it”, D063)

An independent read-only review of the combined build's merged code (all 15 combined merges) returned **PASS WITH NOTES**: conflict resolutions correct, nothing dropped or duplicated; INI keys equal the keys read, with the intended defaults; locks at `+1A8` sequential and never nested, no engine call under them; patched and guarded byte ranges disjoint; `SilenceHold` copies independent; the Loading Menu only affects door lights; trace sum and refusal ranges right; CI 43433155241 clean. LOW: the fill light was not excluded by identity from the door-light match (only geometrically unreachable, and only with `iDoorVoidLights=1`). INFO: culled door lights still count in the stealth light level. Cosmetic: stale comments (`plugin.cpp:390`, `fixed_spike.h:859/900`), light struct defaults differing from the INI (project pattern). Fixes: shader `1fd65e0` (`foreignLight` atomic, the door-light match skips it; CI 43606696628), combined merge `2ce7a9b` and `46c5b3c` (the light package sets `shader::foreignLight` at `createFill`; stale comments fixed; wiring PASS; CI 43606729447). Not applied: struct defaults, an optional order assertion. No behaviour change at the defaults (`iDoorVoidLights=0`). The first two merges had their own reviews (`d55f885` PASS 0/0/2 MINOR, `a26be55` PASS 0/0/0); the ledger records no separate review of the later merges (for example `ef901a7`, `2f4bafe`) before this one.

### Proton 20261112 baseline (PB2) and a brightness comparison

PB2 (`46c5b3c`, `comb-cave-walk`, `comb-gate-embershard`, `comb-gate-mare`, `comb-lifecycle`): 4 of 4 ran, postflight ok, no FAIL or BLOCKED, FCS-75 and FCS-90 informative only. Frame brightness against the earlier runs on `20261105c` on the same routes (`comb-gate-embershard`, `comb-gate-mare`, and the shader routes): ratios 0.99-1.006. The perf-pair harness fix of that day is in `spike-wave2-shader.md`.

### Defaults confirmed in game (DD2, GD1, RP1, PB2)

The defaults in "Current defaults" above ran as the combined build's INI: DD2 first (fill gate and fade 2.0, door void on), GD1 (glow cards on), RP1 (follow-down 480), PB2 (the head `46c5b3c` on the new Proton). Each batch recorded no FAIL or BLOCKED row. The look of each default is the owner's.

## What is NOT_RUN or unverified

- **Superseded by "Game runs (combined build)":** every game row above was NOT_RUN when this was written; W12, W14 and the later batches ran them. What is still NOT_RUN or unverified after those runs is listed at the end of this section.
- (As written 2026-11-11) Every game row above. No game was launched.
- The DLL was not compiled here; the MSVC /W4 /WX CI build is the first compile of the combined `fixed_spike.h`/`plugin.cpp` (new code: the conditional suffix `snprintf` in `install`, the three-config `configure`).
  - Answered: CI built the union with MSVC `/W4 /WX` (CI 43000209724 for `d55f885`, success) and every later head; the game ran it from W12.
- The Hypotheses above: no new vertex-shader permutation from the extra light; no visible change from the narrowed gate in `p55-shader-bfb-seed1`.
  - Answered in part: no new permutation (W12: `created=0`, `shaders` equal with the fill on and off). The narrowed-gate comparison on `p55-shader-bfb-seed1` is not recorded as a separate A/B; the seed default and `bfb-seed1` ran on later builds (Y1/Y2, DD1, GD1) with no refusal.
- No binary review of the combined build: both packages were reviewed separately; the merge adds no engine access, only call order and log/trace plumbing.
  - Answered: the first two merges were reviewed (`d55f885` PASS 0/0/2 MINOR; `a26be55` PASS 0/0/0) and the whole combined merged code was reviewed on 2026-11-14 (PASS WITH NOTES, one LOW fixed in `46c5b3c`).

Still NOT_RUN or open after the runs:

- The combined caps pair (G2: **BLOCKED** at the main menu, `comb-cave-caps` **NOT_RUN**), and the frame cost of caps; caps are parked (D052).
- The combined build was run on Proton `20261112` only in PB2 (4 routes); the smoke batch SM1 was **BLOCKED** by the host pin.
- The owner's look of each default (fill 2.0 gated to dungeons, door void, glow, tilt, ramp) is not recorded as accepted.
- Fort Greymoor's military-fort type counts as dungeon for the fill gate: the ledger marks it as the owner's call, no answer recorded.
- Window placement on monitor 1 and the mute were not achieved or verified (D064); the batches ran on the usual placement.
