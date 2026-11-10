# Rendered visibility from runner screenshots (P5.5-01, DYL-216)

**Status: tool and offline tests complete; every in-game use is NOT_RUN.** The probe's geometric clearance rays
(`view_sample` `ray.status`) are diagnostic only. In wave 1 they disagreed with the rendered frames exactly where
it matters (below). This tool measures what reaches the screen instead. It is the final-image technique
("candidate b") of `docs/evidence/p5.5/visibility-instrument.md`. It runs outside the game, uses only the Python
standard library, and needs no probe change.

| File | Purpose |
| --- | --- |
| `png.py` | PNG reader/writer: 8-bit RGB, RGBA and greyscale, non-interlaced, all five row filters, CRC checked. Anything else (palette, 16-bit, interlaced) is refused, not guessed. |
| `rendered_visibility.py` | Window geometry, box mapping, the A/B/A2 measure, calibration, verdicts, overlay crops; CLI. |
| `../tests/test_visibility.py` | 29 unit tests. Synthetic frames, plus wave 1 captures when they are on the host. |

## Measurement procedure (runner steps only)

At one settled pose, after the rig has stopped turning:

1. `console player.saa 1`, sleep 2.5 s, `request_view TAG`, `screenshot TAG-a` (frame **A**: player as is).
2. `console player.saa 0`, sleep 2.5 s, `screenshot TAG-b` (frame **B**: player transparent).
3. `console player.saa 1`, sleep 2.5 s, `screenshot TAG-a2` (frame **A2**: restored), `request_view TAG-post`.

Pixels that change between A and B inside the player's projected box are player pixels that reached the screen,
through every see-through mechanism at once (fade, hide, clip, undrawn room). A against A2 is the noise floor
(TAA, idle animation, wind, flicker). It is also the restore proof: A2 must differ from B the way A does.

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

**Hypothesis until run:** that alpha 0 renders the player (and the equipped weapon) fully transparent within 2.5 s; whether the player's shadow disappears with it; that `saa 1` leaves no visible difference from the pre-command state; that the stored value (alpha + 2) does not persist past the session in any way that matters. The routes write no save, and alpha is back to 1 before every cell change.

## Frame geometry

The runner captures the decorated window with `xwd` (`session-p4/autotest/capture/core.py capture_xwd`). Wave 1
captures are 1926x1112 for the deployed 1920x1080 client (`report.json` `resolution`, `iSize W/H`): a 3-pixel
border left, right and bottom, and a 29-pixel top (border plus title bar, RGB 50,150,250). `client_origin` derives
the offset from the two sizes, with equal side and bottom borders. Probe `screen` coordinates are normalised 0..1
with a top-left origin, so pixel = origin + (x * 1920, y * 1080).

Checked on wave 1 frames by eye in zoomed crops:

- `a-open-h0` (p55-a-open-1): head point (962,648) at the helmet/neck, feet point (964,742) at the boots (rendered 748).
- `c-hall-h2` (p55-c-hall-seed0-A): head point (965,654) on the face, but the rendered boots sit near y 766, 24 px below the feet point. So the box extends 0.35 player heights below the feet point (and above the head point, and to each side).

## Measure, noise floor and verdicts

All thresholds are in `PARAMS` and are starting points (D55-6 belongs to the owner).

- **Change:** per-pixel max channel |A-B| >= 12, scaled down with box luminance below 28 (minimum 3). A player in a dark room is dark too.
- **Signal** = fraction of box pixels changed A-B; **noise** = the same for A-A2; the verdict uses signal minus noise.
- **Calibration:** a triple at an unobstructed pose with the same rig. The fixed rig keeps the player's screen size constant (761.7 units at pitch 55). It is confirmed only if the box signal is >= 0.08 and at least 3 of 5 point windows show >= 0.40, the scene is stable and the box is not dark.
- **Ratio** = (signal - noise) / calibration signal, for the box and for a window of 0.07 player heights around each point: >= 0.60 `visible`, >= 0.15 `partly`, otherwise `not_visible`. A dark box (luminance < 28) never returns `not_visible`; a weak signal there is `indeterminate` ("dark"). The per-point verdicts are what "4 of 5 points visible" needs; the box verdict can be inflated by the player's own shadow.
- **Indeterminate** (never counted as visible) when:
  - a frame is missing, unreadable or of a different size;
  - the view sample is unavailable, or the camera moved or turned between the two view samples (> 2 units / 2 degrees);
  - the head or feet point is off screen, or the box leaves the client;
  - the frame outside the ring changed by more than 0.12 between A and B or A and A2 (camera or scene moved);
  - the ring changed by more than 0.30 (A-B) or 0.20 (A-A2);
  - the box noise is above 0.25;
  - the player's height differs from calibration by more than 25 %;
  - A2 still looks like B (`restored: false`);
  - the calibration is unconfirmed.

Noise limits come from wave 1 frames taken at the same pose seconds to a minute apart (same rig, same view sample pose):

| Pair | Box | Ring | Frame |
| --- | --- | --- | --- |
| Whiterun exterior a-h0 / a-home, a-h0 / a-l4 (wind) | 0.07-0.08 | 0.08-0.10 | 0.05-0.06 |
| Dragonsreach c-hall-h0 / h0b | 0.055 | 0.003 | 0.016 |
| Bannered Mare b-hearth-h0 / h0b, d-bm-ground-h0 / h0b | 0.066-0.070 | ≤ 0.008 | 0.013-0.025 |
| BFB seed 1 c-bfb-h0 / h0b (dark, threshold 5) | 0.146 | 0.006 | 0.001 |
| BFB seed 0 c-bfb-h0 / h0b (grey void) | 0 | 0 | 0 |

**What it cannot see:** frames between captures (no per-frame claim, no "zero blank frames"); a player whose colours match the background (less signal, so a lower ratio); anything `saa` itself changes. It measures how much of the player shows, not why the rest is hidden.

## Commands

```bash
cd /srv/storage/Projects/SkyCRPG/p55-instrument
python3 -B -I -m unittest discover -s tools/tests -p "test_visibility*.py" -v
python3 -B tools/visibility/rendered_visibility.py view PROBE_SLICE.jsonl TAG > view.json
python3 -B tools/visibility/rendered_visibility.py calibrate A.png B.png A2.png --view view.json --view-after post.json > cal.json
python3 -B tools/visibility/rendered_visibility.py measure A.png B.png A2.png --view view.json --view-after post.json --calibration cal.json
python3 -B tools/visibility/rendered_visibility.py overlay A.png --view view.json --out crop.png
```

The oracle checkpoint routes, wrapper and evaluator live in `session-p55/oracle` (see its README).
