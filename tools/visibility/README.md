# Rendered visibility from runner screenshots (P5.5-01, DYL-216)

**Status: measure version 3. Batch O2 (four routes) ran on version 2: 7 of 9 fixtures PASS, `mare-on-h0`
FAIL (a low-contrast under-read), `hall-h2` and `cave-h0` indeterminate. Version 3 was made from those frames;
re-evaluated offline it gives 8 PASS and `cave-h0` NOT_RUN (dark). That re-evaluation is not independent
evidence (see "Batch O2"). Fade and clip rows are NOT_RUN (no build).** The probe's geometric clearance rays
(`view_sample` `ray.status`) are diagnostic only. In wave 1 they disagreed with the rendered frames exactly where
it matters (below). This tool measures what reaches the screen instead. It is the final-image technique
("candidate b") of `docs/evidence/p5.5/visibility-instrument.md`. It runs outside the game, uses only the Python
standard library, and needs no probe change.

| File | Purpose |
| --- | --- |
| `png.py` | PNG reader/writer: 8-bit RGB, RGBA and greyscale, non-interlaced, all five row filters, CRC checked. Anything else (palette, 16-bit, interlaced) is refused, not guessed. |
| `rendered_visibility.py` | Window geometry, box mapping, the A/B/A2 measure, calibration, verdicts, overlay crops; CLI. |
| `../tests/test_visibility.py` | 46 unit tests. Synthetic frames (idle animation between A and A2, a player shadow, a dim low-contrast scene, stacked fades, a lighting change), plus wave 1 captures, the `p55-oracle-ext-1` frames and the batch O2 frames when they are on the host. |

## Measurement procedure (runner steps only)

At one settled pose, after the rig has stopped turning:

1. `console player.saa 1`, sleep 2.5 s, `request_view TAG`, `screenshot TAG-a` (frame **A**: player as is).
2. `console player.saa 0`, sleep 2.5 s, `screenshot TAG-b` (frame **B**: player transparent).
3. `console player.saa 1`, sleep 2.5 s, `screenshot TAG-a2` (frame **A2**: restored), `request_view TAG-post`.

The **player mask** is the set of pixels where B differs from **both** A and A2: player pixels that reached the
screen, through every see-through mechanism at once (fade, hide, clip, undrawn room). A and A2 are not compared
with each other inside the box: the restored player is the same silhouette in a different idle-animation pose
(first in-game run, below), so A-A2 is not a noise floor. The noise floor is the same mask test in the ring around
the box. A2 is the restore proof: it must differ from B the way A does.

Every frame follows the same console cycle. The console is a menu, and the spike restores and re-applies its hides
on menus (routes README, disagreement 1). So A is taken after a `player.saa 1` round trip, not before any console.
Nothing between `player.saa 0` and `player.saa 1` can fail fatally (only sleeps and screenshots), and the oracle
routes repeat `player.saa 1` in a final `restore` section. The routes write no save.

### Why `player.saa`

| Option | Judgment |
| --- | --- |
| `player.saa 0/1` (SetActorAlpha) | **Chosen.** Toggles the player alone, leaves the scene untouched, and needs no colour calibration (the difference image is the player). |
| Effect shader marker (`player.pms <EFSH>` / `sms`) | Depends on a vanilla shader's fill colour and on lighting. Also a valid command (`PlayMagicShaderVisuals` PMS, `StopMagicShaderVisuals` SMS at .data 2084580/2084620); kept as the fallback if `saa` misbehaves. |
| `player.disable`/`enable` | Disabling the player is not safe. |
| `setscale` | Changes the geometry being measured. |

Binary check of SAA (SkyrimSE.exe 1.7.104, the script command table in `.data`):

- Entry at 2084760: name `SetActorAlpha`, short `SAA`, opcode 1129h, reference function. Parameters `Float`, `Int(optional)`. Handler 35E010 (ID 22391).
- The handler requires an actor (form type 3Eh) with a process (`[ref+F8h]`) and clamps the value to [0, 1].
- When the optional int is 0 or absent it adds 2.0 (constant 1B5BE44). It stores the result with ID 39961 (`[[process+8]+2C8h] = value`), then calls the actor's virtual at +700h.

**Observed in `p55-oracle-ext-1` (Source: the frames):** `saa 0` removes the player, the equipped weapon and the
player's shadow within 2.5 s; `saa 1` brings them back in the same silhouette (mask IoU of A-B against A2-B 0.86)
but not pixel-identical, because the idle animation moves on (the sword hangs at the side in A and is held across
the body in A2 at `cal-h0`). **Still Hypothesis:** that the stored value (alpha + 2) does not persist past the
session in any way that matters. The routes write no save, and alpha is back to 1 before every cell change.

## Frame geometry

The runner captures the decorated window with `xwd` (`session-p4/autotest/capture/core.py capture_xwd`). Wave 1
captures are 1926x1112 for the deployed 1920x1080 client (`report.json` `resolution`, `iSize W/H`): a 3-pixel
border left, right and bottom, and a 29-pixel top (border plus title bar, RGB 50,150,250). `client_origin` derives
the offset from the two sizes, with equal side and bottom borders. Probe `screen` coordinates are normalised 0..1
with a top-left origin, so pixel = origin + (x * 1920, y * 1080).

Checked on wave 1 frames by eye in zoomed crops:

- `a-open-h0` (p55-a-open-1): head point (962,648) at the helmet/neck, feet point (964,742) at the boots (rendered 748).
- `c-hall-h2` (p55-c-hall-seed0-A): head point (965,654) on the face, but the rendered boots sit near y 766, 24 px below the feet point. So the box extends 0.35 player heights below the feet point (and above the head point, and to each side).

## Measure, noise floor and verdicts (version 3)

All thresholds are in `PARAMS` and are starting points (D55-6 belongs to the owner).

- **Change:** per-pixel max channel difference against a per-pixel threshold = max(scene threshold, min(12, 0.10 x luminance of B at that pixel)).
  - *Scene threshold:* in the ring, set the shadow-like pixels aside; the 99th percentile of min(|A-B|, |A2-B|) over the rest, plus 1, clamped to 3..12. A busy exterior (thatch shimmer, wind) keeps 12; a still interior goes down to 3-6.
  - *Contrast floor:* a change must also be at least 10 % of what is behind it (Weber contrast). It keeps 12 wherever B is brighter than 120, and stops a faint trace through stacked fades from counting as visible in a still scene.
- **Player mask:** pixels where |A-B| and |A2-B| both pass. **Signal** = mask share of a region.
- **Shadow-like pixel:** in the mask, with A and A2 both darker than B in every channel and A closer to A2 than to B. In the ring that is the player's shadow (B removes it too); it is reported as `ring_shadow` and kept out of the noise floor and out of the ring A-B control. A dark-clad player is shadow-like as well (darker than the floor), so the test is **not** applied in the box or the point windows.
- **Noise floor:** the non-shadow-like mask share of the ring (the box grown by one player height on each side, box excluded).
- **Restore:** `true` when A2-B is at least half of A-B in the box; `false` when A2 still looks like B; `null` when A-B is below 0.05 (a hidden player: restore not provable from these frames).
- **Calibration:** a triple at an unobstructed pose with the same rig. The fixed rig keeps the player's screen size constant (761.7 units at pitch 55). It is confirmed only if the box mask minus noise is >= 0.08 and at least 3 of 5 point windows show a mask minus noise >= 0.40, the scene is stable, the player was restored and the box is not dark. Calibration records carry the measure version; a record of another version is refused.
- **Point ratio** = (point mask - noise) / calibration point mask, over a window of 0.07 player heights around each point: >= 0.60 `visible`, >= 0.15 `partly`, otherwise `not_visible`. A dark box (luminance < 28) never returns `not_visible`; a weak signal there is `indeterminate` ("dark").
- **Player verdict** (`verdict`) from the five points: `visible` when at least 4 are visible (the acceptance rule "4 of 5 points"), `not_visible` when none is visible or partly, otherwise `partly`; `indeterminate` when fewer than 4 points could be measured. A point off screen counts as not visible.
- **Box ratio** (`ratio`, `box_verdict`), diagnostic only: (box mask pixels - noise x box pixels) / the same count at calibration. It includes the player's shadow, and both the player's screen area and its shadow change with the camera heading (side-on at `open-h3`, shadow falling behind the figure), so it reads low for a fully visible player (0.65 at `open-h3`).
- **Indeterminate** (never counted as visible) when:
  - a frame is missing, unreadable or of a different size;
  - the view sample is unavailable, or the camera moved or turned between the two view samples (> 2 units / 2 degrees);
  - the head or feet point is off screen, or the box leaves the client;
  - the frame outside the ring changed by more than 0.12 between A and B or A and A2 at threshold 12 (camera or scene moved);
  - more than 0.10 of the frame outside the ring is shadow-like (`lighting_changed`: B was lit differently, which would pass for a shadow in the ring; batch O2 frames show at most 0.02);
  - the ring changed by more than 0.30 (A-B, shadow-like pixels aside) or 0.20 (A-A2);
  - the ring's non-shadow mask share is above 0.10 (`scene_busy`: no trustworthy noise floor);
  - the player's height differs from calibration by more than 25 %;
  - A2 still looks like B (`restored: false`);
  - fewer than 4 points measurable;
  - the calibration is unconfirmed or of another measure version.

### What changed from version 2, and why (batch O2)

| Rule | Version 2 | Version 3 | Why |
| --- | --- | --- | --- |
| Per-pixel threshold | 12, scaled down only when the box luminance is below 28 | scene threshold from the ring noise (3-12) and a 10 % contrast floor per pixel | `mare-on-h0`: a dark-clad player on a dark floor at box luminance 49 differs from the floor by only 1-12 levels over much of the head and right shoulder; at 12 the head read 0.49 and shoulder_r 0.29 (`partly`, FAIL). The ring there has noise 99th percentile 5, so 6 is safe. |
| Luminance-scaled threshold | yes | removed | Replaced by the two rules above, which cover dark rooms too (`bfb1-h0`: threshold 3). The dark rule itself (dark box never `not_visible`) stays. |
| Ring noise | all mask pixels in the ring | shadow-like pixels set aside, reported as `ring_shadow` | `hall-h2`: the player's long shadow on the carpet covers 0.156 of the ring; version 2 called it noise (0.145, `scene_busy`). Without it the ring noise is 0.003. |
| Ring A-B control | all changed pixels | shadow-like pixels set aside | Same cause: a long shadow is not a scene change. |
| `lighting_changed` | none | refuse when > 0.10 of the frame outside the ring is shadow-like | A lighting change that came with B would look like a shadow in the ring and escape the noise floor. |
| Everything else | | unchanged | Mask, calibration, point ratios, 4-of-5 player verdict, restore proof, pose/scale/frame checks. |

The calibration triple is taken in daylight at a scene threshold of 8-10; its point masks are 0.90-1.00 at
every threshold from 3 to 12, so comparing a fixture at a lower threshold against it does not inflate the ratio.

### What changed from version 1, and why

| Rule | Version 1 | Version 2 | Why |
| --- | --- | --- | --- |
| Signal | A-B changed share | B differs from both A and A2 | A pixel that is player in A but not in A2 (idle motion) is not proof; the intersection is. |
| Noise floor | A-A2 changed share in the box | mask share in the ring | In `p55-oracle-ext-1` A-A2 covered 0.25-0.37 of the box, 96-99 % of it inside the player's silhouette: animation, not noise. It refused a good calibration. |
| `noisy` (box A-A2 > 0.25), point noise > 0.40 | refuse | removed | Same cause. Replaced by `scene_busy` (ring mask > 0.10). |
| Main verdict | box ratio | points, 4 of 5 | The box counts the shadow and depends on the box size; the acceptance rule is per point. |
| Restore proof | A2-B >= 0.5 x A-B | unchanged | Worked as intended on the real frames. |
| Stability, pose, off screen, scale, dark, frame checks | | unchanged | Nothing in the run argued against them. |

**B-versus-B consistency** would need a second B frame, a change to the frame protocol (and the routes). It is
not needed now: the ring measures the same false-positive rate with the frames we have (0.015-0.036 in
`p55-oracle-ext-1`, limit 0.10). It stays the option if an exterior with strong wind fails `scene_busy`.

Scene-noise numbers from wave 1 frames taken at the same pose seconds to a minute apart (same rig, same view
sample pose), A-B change share. The "Box" column is a frame pair without any player toggle, so it shows scene
plus idle-animation change, which version 2 no longer treats as noise:

| Pair | Box | Ring | Frame |
| --- | --- | --- | --- |
| Whiterun exterior a-h0 / a-home, a-h0 / a-l4 (wind) | 0.07-0.08 | 0.08-0.10 | 0.05-0.06 |
| Dragonsreach c-hall-h0 / h0b | 0.055 | 0.003 | 0.016 |
| Bannered Mare b-hearth-h0 / h0b, d-bm-ground-h0 / h0b | 0.066-0.070 | ≤ 0.008 | 0.013-0.025 |
| BFB seed 1 c-bfb-h0 / h0b (dark, threshold 5) | 0.146 | 0.006 | 0.001 |
| BFB seed 0 c-bfb-h0 / h0b (grey void) | 0 | 0 | 0 |

## First in-game evidence: `p55-oracle-ext-1`

Run `session-p55/oracle/runs/p55-oracle-ext-1` (runner label `batch-p55-oracle-ext-1-00`, route `p55-oracle-ext`,
spike stage 744609f, probe aeaccbd; runner PASS, postflight ok). Version 1 refused its calibration (`noisy`), so
every row was NOT_RUN. Re-evaluated offline with version 2 into `session-p55/oracle/reeval/p55-oracle-ext-1-v2/`
(the run folder is untouched):

| Tag | Mask box / points | Ring noise | A-A2 box | Version 2 result | Expected |
| --- | --- | --- | --- | --- | --- |
| `cal-h0` | 0.510 / 0.92-1.00 | 0.016 | 0.366 | confirmed, 5 of 5 points | |
| `open-h3` | 0.525 / ratios 0.91-1.07 | 0.036 | 0.253 | **visible**, all 5 visible, restored | visible |
| `open-h2` | 0.289 / head 0.96, chest 1.06, shoulders 0.99/1.04, feet 0.01 | 0.022 | 0.254 | **visible** (4 of 5), feet not visible, box partly 0.51, restored | partly/visible; head visible, feet not |
| `stable-h0` | 0.017 / 0.00-0.01 | 0.015 | 0.034 | **not visible**, restore unprovable | not visible |

It shows: the marker works on 1.7.104 (player, weapon and shadow vanish in B and return in A2), the view samples
before A and after A2 are identical, a player above a roof edge with legs under it reads per point, and a player
under a roof reads not visible. It does **not** show: fades, clipping, hides, undrawn rooms, interiors or dark
scenes (other routes, NOT_RUN); repeatability (one run); or an independent test of version 2, which was designed
after looking at these frames (thresholds unchanged, expected answers written before the run). A fresh run of
`p55-oracle-ext` is the independent check. Silhouette sheets: `reeval/p55-oracle-ext-1-v2/*-masks.png`.

Version 2 results; version 3 gives the same verdicts on these frames (`reeval/O2-v3/ext-1/`).

## Batch O2: version 2 in game, version 3 from its frames

Four routes, runner labels `batch-p55-oracle-{ext,seed1,hide,seed0}-O2-00` (records
`session-p55/oracle/runs/p55-oracle-*-O2/`, aggregate `runs/checkpoint-O2.md`), spike stage 744609f, probe
aeaccbd, measure version 2, calibration confirmed in every route, postflight ok.

What O2 showed about version 2 (independent of the frames it was designed on):
- `p55-oracle-ext-O2`: open-h3 visible, open-h2 visible with feet not visible, stable-h0 not visible: all PASS. This confirms version 2 on a fresh run.
- `bfb0-h0` (undrawn room) not visible, `mare-off-h0` and `bfb1-h0` partly: PASS.
- **Failure:** `mare-on-h0` read partly (head 0.49, shoulder_r 0.29) for a player the frames show fully unobstructed. The player is dark on a dark floor in dim light, so many player pixels differ by less than 12. A measure that under-reads dim interiors fails exactly the places this phase is about.
- `hall-h2` indeterminate: `scene_busy`, 0.145 of the ring passed the mask test. The frames show this is the player's own shadow on the carpet (it leaves the box toward the upper left, and goes with the player in B); A-A2 changes stay on the figure.
- `cave-h0` indeterminate (dark, box luminance 4.0), as predicted.

Version 3, re-evaluated offline on the same frames (`session-p55/oracle/reeval/O2-v3/`). **Re-evaluation, not
independent evidence: version 3 was set on these frames.**

| Run | Fixture | Version 2 (in game) | Version 3 (re-evaluation) | Threshold, ring noise, ring shadow | Point ratios (head, chest, feet, shoulder_l, shoulder_r) |
| --- | --- | --- | --- | --- | --- |
| ext-O2 | open-h3 | visible, PASS | visible, PASS | 11, 0.007, 0.032 | 0.90, 1.03, 1.00, 1.00, 0.98 |
| ext-O2 | open-h2 | visible, PASS | visible, PASS | 12, 0.008, 0.001 | 0.99, 0.97, **0.00**, 1.00, 1.01 |
| ext-O2 | stable-h0 | not visible, PASS | not visible, PASS | 12, 0.029, 0.007 | 0.00, 0.04, 0.00, 0.00, 0.00 |
| seed0-O2 | mare-off-h0 | partly, PASS | visible, PASS | 5, 0.005, 0.058 | 0.82, 0.61, 1.00, 0.96, 0.76 |
| seed0-O2 | bfb0-h0 | not visible, PASS | not visible, PASS | **3**, 0.000, 0.000 | 0.00, 0.00, 0.00, 0.00, 0.00 |
| seed0-O2 | hall-h2 | indeterminate (scene_busy), NOT_RUN | visible, PASS | 4, 0.003, 0.156 | 0.90, 0.96, 1.00, 0.93, 0.99 |
| seed1-O2 | bfb1-h0 | partly, PASS | visible, PASS | 3, 0.009, 0.006 | 0.25, 0.94, 0.89, 0.92, 0.90 |
| hide-O2 | mare-on-h0 | partly, **FAIL** | visible, PASS | 6, 0.006, 0.060 | 0.72, 1.00, 1.00, 0.99, 0.65 |
| hide-O2 | cave-h0 | indeterminate (dark), NOT_RUN | indeterminate (dark), NOT_RUN | 3, 0.000, 0.000 | 0.00 x 5 |

Not-visible readings hold. `stable-h0` keeps threshold 12 (thatch shimmer), and `open-h2` feet read 0.00.
`bfb0-h0` is measured at the lowest threshold, 3, and still reads 0.00 everywhere: a lower threshold does not
invent a player where none is drawn. `mare-off-h0` and `bfb1-h0` move from partly to visible, by the 4-of-5
rule; both allow either answer. The rafter over the chest still reads 0.61 at `mare-off-h0`, and the beam over
the head 0.25 at `bfb1-h0`. `mare-on-h0`'s shoulder_r at 0.65 is close to the 0.60 line.

## Limits

- **Frames between captures** are not measured: no per-frame claim, and no "zero blank frames".
- **Colours that match the background:** a player whose colours match the background within the contrast floor (10 % of the background's luminance) or the scene noise is not seen there. Version 3 narrows this in still scenes but does not remove it.
- **Shadow-like pixels:** the test cannot be used on the body. A hidden player whose shadow falls on a visible surface right where a body point projects would count as visible there. That needs the shadow to land on the camera side of the occluder. None of the O2 fixtures does this.
- **Lighting changes** in B affecting less than 10 % of the frame outside the ring are not caught by `lighting_changed`.
- **Other side effects of `saa`:** anything `saa` changes besides the player (screen-space ambient occlusion around the player, reflections) counts like the player.
- **Dark but undrawn vs dark but visible:** at box luminance 4 (`cave-h0`) every pixel moves by at most 1-2 levels between A and B. A player of rock-like albedo in that light could not differ by more than a few levels either, so "no signal" cannot say "occluded". `bfb1-h0` at luminance 13 was measurable because the lit player differed by up to 20 levels. Two changes would make the dark case decidable; both are route/protocol changes, not made here:
  1. a **dark calibration**: a triple at a known-unobstructed pose in the same dark cell. If it shows a confirmed signal, zero signal at the fixture means not visible.
  2. a **self-lit marker**: an emissive effect shader on the player for the A/A2 frames (`player.pms` with a bright EFSH, the fallback named above). The player is then drawn bright regardless of scene light; a dark-but-visible player gives a strong signal, and an occluded one gives none.
  Until then a dark box with no signal stays `indeterminate`.

It measures how much of the player shows, not why the rest is hidden.

## Commands

```bash
cd /srv/storage/Projects/SkyCRPG/p55-instrument
python3 -B -I -m unittest discover -s tools/tests -p "test_visibility*.py" -v
python3 -B tools/visibility/rendered_visibility.py view PROBE_SLICE.jsonl TAG > view.json
python3 -B tools/visibility/rendered_visibility.py calibrate A.png B.png A2.png --view view.json --view-after post.json > cal.json
python3 -B tools/visibility/rendered_visibility.py measure A.png B.png A2.png --view view.json --view-after post.json --calibration cal.json
python3 -B tools/visibility/rendered_visibility.py overlay A.png --view view.json --out crop.png
python3 -B /srv/storage/Projects/SkyCRPG/session-p55/oracle/mask_sheet.py COLLECTED_DIR OUT_DIR   # silhouette sheets
```

The oracle checkpoint routes, wrapper and evaluator live in `session-p55/oracle` (see its README).
