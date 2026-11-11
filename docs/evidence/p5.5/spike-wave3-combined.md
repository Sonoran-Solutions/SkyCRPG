# P5.5-02 wave 3: combined spike build (shader cut + Stage 1 + player-fade hold, with the fill light)

Disposable spike work on `phase5.5/p55-02-spike-combined`: the shader package at `3528e92` merged with the light package at `0279999` (common base `744609f`). Nothing here is merged into the shipped Camera DLL. Every game row is **NOT_RUN**: no game was launched for this merge. Compilation was not attempted on this host (Windows-only target); the portable tests and the wiring audit say nothing about in-game behaviour.

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

1. **Embershard Mine, cut and fill together** (`iSeeThroughMode=3`, `iRoomSeedMode=0`, `iLightFillMode=1`; the `p55-shader-cave` poses): coc arrival, headings 45/135/225/315 with a screenshot each, F8 off and on. Expect the W4 cross-section on black, now with the floor and the player lit around the feet; `FixedShader` as in W4/W11 (cut > 0, `skipShader` and `created` comparable to row 2), `FixedShaderVoid` sky held in Show Sky cells, `FixedLight … fill=active fillHidden=0 fillCull=0`, `fillAt` z = feet + 192; `FixedLightLevel` `level − levelNoFill` near 0.5 with `neutral=1`; F8 → `event=hide cause=f8`, `event=remove` ~0.5 s later, `event=add` on return; `FixedFade held=1` in every eligible window.
2. **Same, fill off** (`iLightFillMode=0`): the A of the A/B; expect the W4 look exactly and no `FixedLight` line.
3. **Bleak Falls Barrow corridor, cut and fill together** (`p55-shader-bfb-seed1` pose 2048, −9216, −2556, z first; `iSeeThroughMode=3`, `iRoomSeedMode=1`, `iLightFillMode=1`): four headings, screenshots. Expect the seeded room drawn and now lit near the player; ` rooms=hooked ` in the installed line; `FixedRoomPass` lines with `lightPassNear` > 0 and claims from the camera pass only. Compare against the W4 `p55-shader-bfb-seed1` screenshots for anything that changed besides the light (the narrowed gate).
4. **Bannered Mare, Stage 1, fill off (default)** (`p55-shader-mare`, `p55-shader-mare-loft`): must reproduce W11 (P2, F1, S17 PASS) with no light line; this is the "light keys off = shader 3528e92" row.
5. **Bannered Mare, Stage 1, fill on** (A/B, owner look only): whether the lantern over-lights an already lit kit interior; this is the evidence for a dungeon-only default, which this build does not have.
6. **Lifecycle, cut and fill on** (`p55-shader-lifecycle` + the light's L4 steps): inventory and console 2 s each → no `hide` event, fade released (`held=0`) during the menu and re-held after; F6 first person and back → `cause=first-person`, fade released; load door / coc → `cause=load`, `event=add` in the new cell; door-out → exterior: `remove cause=engine-gone` or `exterior`, sky and void back to the engine's (S15), fade held outdoors by design (rig eligible).

## What is NOT_RUN or unverified

- Every game row above. No game was launched.
- The DLL was not compiled here; the MSVC /W4 /WX CI build is the first compile of the combined `fixed_spike.h`/`plugin.cpp` (new code: the conditional suffix `snprintf` in `install`, the three-config `configure`).
- The Hypotheses above: no new vertex-shader permutation from the extra light; no visible change from the narrowed gate in `p55-shader-bfb-seed1`.
- No binary review of the combined build: both packages were reviewed separately; the merge adds no engine access, only call order and log/trace plumbing.
