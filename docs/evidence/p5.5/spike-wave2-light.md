# P5.5-02 wave 2 package "light" (DYL-217) — why a seeded room is dark from the fixed camera

Disposable spike work on `phase5.5/p55-02-spike-light`, cut from `744609f`. Nothing here is merged into the shipped Camera DLL. Every game row is **NOT_RUN**: no game was launched for this package. Compilation and portable tests prove nothing about in-game behaviour.

Inputs: `SkyrimSE.exe` 1.7.104 (SHA-256 `846efccf0c1374d7…`), Address Library `versionlib-1-7-104-0.bin` (`8aab3dd251d135b8…`), `Skyrim.esm` (`e198c3b85e5e48e0…`) and `Update.esm` (`b298b0f65fa0127f…`), wave 1 run records under `session-p55/routes/runs/` and screenshots under `session-p4/autotest/runs/`. Scratch scripts (outside the worktree): `/tmp/p55-light/scripts/` (`cells.py` ESM scan, `lum3.py` / `void.py` screenshot luminance), full `.text` disassembly `/tmp/p55-light/text.asm` (`objdump -d -Mintel`), RVA↔ID map from the Address Library (ID n at file offset 96+4n).

## Answer

**The Bleak Falls Barrow corridor is dark because nothing lights that spot, not because the fixed camera removes light.** Confidence: high for "the camera does not cull the room's point lights" (binary), high for "no placed light reaches the corridor pose" (ESM, base radii), medium for "that is what the screenshot shows" (one close-camera comparison at the same pose, different view).

1. **No placed light reaches the pose.** At the corridor pose (2048, −9216, −2556) the nearest placed light is 1014 units away; every light within 2200 units has a base radius of 256 (Source: ESM scan, below). The only lights whose spheres can reach the pose are two shadow-casting omni lights 1761 units away whose reference `XRDS` reads 1744, i.e. the pose is at the very edge of their falloff (**Hypothesis**: `XRDS` is the per-reference radius override; some values it returned, such as −16 and −189 in other cells, show the field is not fully understood by my scan). The cell's own lighting is ambient (66, 60, 40) with directional fade 0, and its directional-ambient term for the +Z face is the darkest of the six: (31, 28, 18) against (59–72, …) for the side faces and (105, 96, 65) for −Z (Source: `XCLL`, inherit flags 0).
2. **The close camera at the same pose was just as dark.** Phase 5 candidate screenshots at the same pose (`batch-fx2-0002-corridor-00/corridor-start.png`, `batch-final2-2204-corridor-00/matched-before.png`): mean luma of rendered pixels 12.7 (median 10.4). The seeded fixed view (`batch-p55-c-bfb-seed1-1-00/c-bfb-h0.png`): 10.3 (median 7.7) on the 63 % of the frame that is not void. Same order; the views show different surfaces. The Bannered Mare (40–48) and Mzulft (21.5) are brighter because lights reach their poses (hearth shadow omni at 278 units with base radius 768; Mzulft's template adds a directional light).
3. **The camera does not take light away from the player's room.** Point lights are culled per frame only by a sphere-against-camera-frustum test and a camera-distance fade that starts at `fLightLODStartFade` (3500 on this host); no room or portal test is involved. Which geometry a light touches is decided from the light's own position. Shadow-casting lights have camera-dependent admission (below), but at this pose no shadow light reaches the player, so it cannot be the cause there.
4. **The void around the island is the fog colour, not black.** The non-rendered area reads (63–68, 80–100, 86–117) and matches the cell fog colour (75, 99, 97) after tone mapping (**Hypothesis**: interior clear colour = fog colour). Fog (near 0, far 12000, power 0.7) adds about 9 percentage points more fog-colour blend at 760 units than at a first-person distance (**Hypothesis**, Skyrim fog formula not audited); fog lightens, it cannot darken.

So there is nothing to *restore*. A fix that makes the corridor readable has to *add* light, which changes the look. That is implemented as a clearly labelled, default-off option (`iLightFillMode=1`); the owner decides whether the look is acceptable.

## Observations (Source: run records and screenshots)

Luminance method: Rec.601 luma of every 4th pixel, HUD strip (y < 120) excluded, the blue-grey void (b − r ≥ 25 and g − r ≥ 15) excluded (`lum3.py`).

| Screenshot | Void share | Mean luma | Median | Note |
| --- | --- | --- | --- | --- |
| BFB seed 1 `c-bfb-h0` | 0.37 | 10.3 | 7.7 | seeded player's room, heading 45 |
| BFB seed 1 `c-bfb-h1` | 0.26 | 14.5 | 11.3 | heading 135 |
| BFB seed 2 / 3 `c-bfb-h0` | 0.37 | 10.1 / 10.3 | 7.5 / 7.7 | same as seed 1 |
| Phase 5 `fx2-0002-corridor/corridor-start` | 0 | 12.7 | 10.4 | close camera, same pose |
| Phase 5 `fx2-0002-corridor/corridor-end` | 0 | 15.9 | 13.5 | after the walk |
| Mzulft `d-dwemer-h0` | 0 | 21.5 | 17.2 | |
| Bannered Mare `b-hearth-h0` / `d-bm-ground-h0` | 0 | 40.4 / 48.2 | 37.8 / 41.4 | |
| Dragonsreach `c-hall-h2` | 0 | 76.7 | 65.1 | |

Room trace, BFB seed 1 (`p55-c-bfb-seed1-1/collected/SkyCRPG_Camera_FixedSpike.log`): (playerRoom, cameraRoom) = (9, 7) in 232 samples, (9, 9) 53, (12, 12) 194, (−1, −1) 52, (3, −1) 8. At the corridor the camera sits in another room's bound most of the time.

## Offline content facts (Source: `cells.py`, Skyrim.esm then Update.esm overrides)

| Cell | XCLL inherit | Ambient | Directional / fade | DALC +Z / −Z | Fog near–far, colour | LIGH refs (point / shadow) | Lights reaching the route pose (base radius) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BleakFallsBarrow01 000371DE | 0 (cell values) | (66,60,40) | (44,47,48) / 0.0 | (31,28,18) / (105,96,65) | 0–12000, (75,99,97) | 57 point, 12 omni-shadow, 3 spot-shadow, all portal-strict | none at the corridor pose; nearest 1014 units |
| Mzulft01 0001523F | 2047 (template 0008810A) | template (40,37,23) | template (79,89,91) / 0.5 | (19,17,10) / (60,56,35) | template 1500–6000 | 48 point, 10 shadow | none by base radius; nearest 651 (radius 256) |
| EmbershardMine01 000B6BE6 | 2047 (template 000E7C5E) | template (31,31,31) | template (65,81,84) / 0.0 | (14,14,14) / (47,47,47) | template 750–6000 | 32 point, 7 shadow | none by base radius; nearest 277 (radius 256) |
| WhiterunBanneredMare 0001605E | 159 (template 0006175D) | template (57,53,49) | (64,73,74) | cell (6,6,5) / (21,19,18), template (27,25,22) / (86,80,75) | 128–2000 | 16 point, 2 omni-shadow | the hearth omni-shadow at 278 (base radius 768); no other by base radius |

LIGH flag meanings used here are from the binary, not from a wiki: ID 17610 (`274390`, building the light from a `TESObjectLIGH`) sets `shadowLight` when `flags & 0x1C00` (0x400 spot, 0x800 hemisphere, 0x1000 omni) and `portalStrict` from bit 0x2000 (`274833-27484A`, `2747B9-2747C5`). The inherit-flag and DALC face meanings are **Hypotheses** (record layout, not audited in the binary).

## Binary audit (1.7.104, file bytes; Address Library IDs resolved and the code read at each RVA)

### Per-frame light selection

| What | Where | Finding |
| --- | --- | --- |
| Light queue and per-frame point-light update | ID 106335 `150D410` | Takes the ShadowSceneNode light lock (`[ssn+1A8]`, `150D439`), moves queued lights, then per active light: dynamic point lights copy the NiLight position (`150DAC5-150DAE7`) and re-assign geometry through ID 106342; every light is then camera-culled by ID 108278 (`150DB02`). Callers: 106337 (all four scene nodes) and seven render paths (13280, 13316, 15691, 19411, 35780, 40744, 52727). |
| Point-light camera cull | ID 108278 `1575D20` | Sphere (NiLight `+A0` position, `+128` radius) against the culling process's six frustum planes (`1575D70-1575DC1`); then, when `BSLight+63` is set, a fade on `|light−camera|² − r²` between `fLightLODStartFade²` and `(fLightLODStartFade+fLightLODRange)²` (globals `33D3D34/38`, written at `412FFD` from Settings `20D6AC0` / `20D6AD8`). Writes `BSLight+5C` = 0 or 0xFF and `+14` dimmer. **No room or portal test.** Host values: 3500 and 500. |
| Per-geometry light list | ID 107784 `1569550` | Shadow lights come only from the four shadow-mask channels (bits of `lightData+1C`, light from `[ssn+230][channel]` via ID 106366 `150FEE0`); point lights from `lightData->lights`, skipping `+5C == 0xFF` and app-culled NiLights (`1569700-156972D`). Order is list order; no camera term. |
| Shadow channels reset | ID 107132 `1537FD0` → ID 106367 `150FF00` | Every frame all four channel owners get `+520 = 0xFF`, then ID 107137 assigns. |
| Shadow-light admission | ID 107137 `15389B0` (chunk `1538AD5-1538ECC`) | Walks `ssn+148` (`activeShadowLights`) **in list order** (ID 106361 `150F8B0`), stops at four (`cmp ebp,4` at `1538B36`), and admits a light only if (a) its `UpdateCamera` (vtable slot 0x10, call `1538B48` `41 FF 90 80 00 00 00`) returns true and (b) ID 76121 `FD97A0` finds a room common to the main camera's visited-room set (`[[434362A0]]+30190`) and the light's own set. Admitted: ID 106342, channel via ID 106365, `+520 = channel`. Not admitted: shadow maps returned (slot 0xC), and the light contributes nothing to any geometry that frame: shadow lights are never linked into a geometry's light list (ID 106400 skips the linker ID 108281 when its third argument `light+528` is set, `1512C3E-1512C45`; only ID 108283 `15765D0` appends to `lightData->lights`), so ID 107784 can draw them through a channel only. |
| Omni / hemisphere shadow light `UpdateCamera` | ID 108625 `1588590` (BSShadowParabolicLight, vtable ID 255534 `1B4F0F0`, slot 0x10 at `1B4F170`) | Sphere against the main camera frustum (`1588764-15887EF`), then `(|light−camera|² − r²)·lodAdjust ≥ shadowDistance²` → `+5C = 0xFF`, dimmer 0 (`1588835-158886B`). |
| Spot shadow light `UpdateCamera` | ID 108618 `1587BC0` (BSShadowFrustumLight, vtable ID 255531 `1B4EFE0`, slot at `1B4F060`) | Frustum-against-frustum test ID 108306 `1577A10` (pure geometry), then the same distance LOD (`1587E61-1587EB6`). |
| Shadow distance | `1434869AC` = `fInteriorShadowDistance²` indoors | Written at `14EBF28/14EBF67` from Setting `20D9038` (`fInteriorShadowDistance:Display`, file default 3000) or `20D9458` (`fShadowDistance:Display`, 8000), selected by `BSShaderManager::State+44` (interior). Host `SkyrimPrefs.ini`: 3000 / 8000. |

### Light-to-room and light-to-geometry association (camera-independent)

| What | Where | Finding |
| --- | --- | --- |
| Update one light | ID 106342 `150E400` | Off if the NiLight is app-culled or its fade < 0.05 (`150E437-150E453`, constant `17ED69C`); sets `BSLight+120` to the graph; shadow lights → ID 106400, point lights → ID 106401. |
| Shadow / point light rooms | IDs 106400 `1512540`, 106401 `1513290` | Each light owns a culling process (`+128`) with **its own NiCamera** placed at the NiLight position (`15126B2-15126DB`, `151340C-151342F`). Portal-strict lights ask `QPointWithin` (vtable slot 0x3F) **at the light's position** (`1512936`, `15129E8`, `151369B`, `1513755`), walk portals with ID 106378, and are added to / removed from each room's light list (`room+180`) by IDs 108279 `1575EF0` / 108280 `1576110`. Non-portal-strict lights test room bounds against their sphere (`[vtable+208]` at `1512BC8`). |
| Geometry under a light | ID 108302 `1577480` | Node bound within the light radius of the light position; a portal-strict light additionally needs the node's room in the light's own visited set (`15775A0-15775DD`). |
| Visited-room set | ID 106378 `15100B0`; IDs 76121 `FD97A0`, 76123 `FD9900`, `FD95C0` | Start rooms and rooms reached are appended to the culling entry's list (`entry+18`, `FD95C0` at `1510323`); a null start room sets `entry+130` instead (`151033F`). ID 76121 returns true when both entries carry `+130`, else when the two lists share a room. |

**Consequences for the brief's questions.** (i) Lights of the player's room are not culled because the camera is in another room: point lights are tested against the camera frustum and distance only. (ii) Shadow lights *are* camera-referenced: frustum, `fInteriorShadowDistance` from the camera, a room shared with the camera's visited set, and the first four in list order. Seeding mode 1 adds the player's room to the camera's visited set (it is a start room of ID 13408), so the player's room's shadow lights stay eligible; with the camera in no room the camera entry carries only the `+130` flag, and (**Hypothesis**, from ID 76121's code) shadow lights bound to rooms are then not admitted. (iii) The four-channel limit is list-ordered, so a fixed camera that sees more rooms can let other rooms' shadow lights take the channels first (**Hypothesis** for any given pose; the light trace counts it as `shadowStarved`). (iv) The room test the spike hooks is not what light selection uses; it is what light *association* uses, at the light's position.

### A wave 1 finding this audit corrects

Wave 1 classified IDs 106400 and 106401 as camera culling passes ("ask about camera + near × forward"). They are the per-light room passes above: the NiCamera they use is the light's own, at the light's position. The spike's caller gate therefore counts per-light queries as `cullCalls`, and admitted shadow lights re-run ID 106342 every frame, so `cullCalls` is not a camera-culling count. The position gate (query within 96 units of the rendered camera) keeps claims rare, but a portal-strict light that happens to sit within 96 units of the rendered camera would, in seeding modes 1-3, be associated with the seeded room. I did **not** change the room hook (it is wave 1 code shared with other packages); see "Proposed shared-file changes".

### Fill-light engine surface (checked before use, at data load)

| Fact | Checked |
| --- | --- |
| `BSShaderManager::State` ID 390951 → `20D68B0`; `shadowSceneNode[0]` is the scene node the engine culls with | address; used as `[1420D68B0]` by IDs 13408, 106400, 107137, 17610 |
| ShadowSceneNode vtable ID 254646 → `1B3DBA0` | address; the live object's vtable is compared on every use (FCS-53) |
| Layout: `activeLights +130`, `activeShadowLights +148`, `lightQueueAdd +160`, `lightQueueRemove +178`, `lightQueueLock +1A8` (owner/count spin lock, compatible with CommonLib `BSSpinLock`) | operands in 106335 / 106326 / 106331 / 106332; lock body `199FA0`; `static_assert`s against CommonLib in `fixed_spike_light.h` |
| `AddLight(NiLight*, const LIGHT_CREATE_PARAMS&)` ID 106326 → `150C810` | requires NiPointLight/NiSpotLight RTTI; takes the light lock; non-shadow path allocates `BSLight` (`0x140`, ctor ID 108276); copies `+0→+46 dynamic`, `+2→+47 portal-strict`, `+3→+61`, `+4→+62`, `+5→+63`, `+20→+130`, `+28→+138`; queues into `+160`. Prologue compared (16 bytes) |
| CommonLib names params `+5` `neverFades`, but it lands in `BSLight+63`, which **enables** the camera-distance fade in ID 108278 (`1575DD9`) | the fill light passes `neverFades = false` so it never fades |
| `RemoveLight(NiLight*)` ID 106331 → `150CFF0` | no-op unless `ssn+300` is true; searches only the active arrays, then queues into `+178` under the lock (ID 106332). Prologue compared (15 bytes) |
| NiPointLight ctor ID 70967 → `EFE4E0` (object 0x150 bytes, as allocated at `2745E6`, `EFE58E`); vtable ID 237259 → `1A4A658` written at `EFE4EE` | address, 14-byte prologue, vtable of the constructed object compared (FCS-55) |
| NiLight fields: ambient `+110`, diffuse `+11C`, radius `+128` (x used), fade `+134` (NiLight ctor `EED140` sets fade 1, colours 1); engine light builder ID 17610 sets ambient to (0,0,0) (`274666`) | operands |
| App-culled NiLight: engine detaches the light from all geometry (ID 108285 `1576870`) and the per-geometry list skips it | `150DAA9`, `1569720` |

## What this package adds

Files: `modules/camera/include/fixed_spike_light_policy.h` (policy, no engine types), `modules/camera/src/fixed_spike_light.h` (adapter), `modules/camera/tests/fixed_spike_light_policy_test.cpp`; small delimited edits to `fixed_spike.h` (refusal codes, include, three calls, configure signature, install log fields), `plugin.cpp` (eight INI reads inside the existing `[FixedCameraSpike]` gate), the INI and the wiring audit. **No hook, no code patch, no vtable write.**

With `iLightFillMode=0` and `iLightTrace=0` (the defaults, or keys absent) no light object is allocated, no engine light state is read and no engine function is called; behaviour equals `744609f`. The only visible difference is three appended fields on the existing `installed …` log line (`light=off fill=off lightTrace=0`); the route checker matches that line by prefix and ` rooms=… `, so it is unaffected.

### Light trace (`iLightTrace=1`, read-only)

About 10 Hz in eligible frames, 6000 lines, into `SkyCRPG_Camera_FixedSpike.log`. On the main thread, under the scene node's light lock (the lock ID 106335 takes; no engine call while it is held), it reads the active point and shadow lights and reports against the player point (feet + 64, the spike's `eyeLift`, **Hypothesis** for chest height):

`FixedLight t frame cell interior eligible eye cam point pointCulled pointDimmed pointReach pointReachLive pointNearest shadow shadowChannel shadowReach shadowReachChannel shadowStarved shadowNearest fillWanted fill fillCreated fillHidden fillCull fillDimmer fillAt adds removes drops sample=state`

`pointReach` = active point lights whose sphere contains the player point; `pointReachLive` = of those, not camera-culled, not hidden, dimmer > 0. `shadowChannel` = shadow lights holding a channel after the last shadow pass; `shadowStarved` = shadow lights that contain the player point but hold no channel (they light nothing). `*Nearest` = smallest (distance − radius), ≤ 0 means inside. Values are those of the last rendered frame. The fill light is reported separately and excluded from the point counts. Fill transitions are traced as events in every frame: `FixedLight … event=add|remove|show|hide|drop-limit|remove-limit cause=<exit or eligible> presence=…`.

### Fill light (`iLightFillMode=1`, **presentation aid: changes the look**)

A dynamic point light (no shadows, not portal-strict, no camera-distance fade, ambient 0, diffuse `fLightFillRed/Green/Blue`, radius `fLightFillRadius`, fade `fLightFillFade`) created once with the engine's own NiPointLight constructor and handed to `AddLight`, exactly as the engine builds a `TESObjectLIGH` light. It is wanted only in **interior cells** in **eligible rig frames**; each wanted frame its NiLight is placed `fLightFillHeight` above the feet (the engine repositions dynamic point lights from the NiLight each frame). It follows the player, not the camera: a camera-side light 760 units away would need a radius that also floods the rock between camera and player.

On every ineligible frame (F8, menu, first person, special state, load, death, Camera off) and outside interiors it is app-culled at once (the engine then detaches it from all geometry) and, once it is in the active list, handed back with `RemoveLight`; the state machine never asks twice while a removal is queued and waits out a queued add. If the engine drops it while wanted (for example a cell reset) it is re-added up to three times per cell (FCS-56 after that); if `RemoveLight` does not take after eight requests it stays hidden (FCS-57). The NiLight is kept for the process lifetime and never freed, because the engine may still hold a reference. It never touches saves (lights are runtime objects).

What it does not do: it does not restore any engine light, does not change shadow admission, does not light exteriors, and does not make one-piece caves visible from inside the rock (Embershard: the camera is inside the mesh).

### Decision: no shadow-focus hook

A "player as reference point" fix for shadow lights would wrap `UpdateCamera` (slot 0x10) of the two shadow-light classes behind a caller gate on return address `1538B4F`, refusing lights whose sphere misses the player so that near lights win the four channels. I did not build it: no shadow light reaches the corridor pose, so it cannot be the cause there, and it would add a vtable hook on a render path for an unproven gain. The trace's `shadowStarved` tells, per pose, whether it would help. If a game row shows `shadowStarved > 0` together with a dark player area, that is the evidence for it; the design above is ready.

## Refusal codes (FCS-51..57; FCS-50 was already the trace writer)

| Code | Site | Effect |
| --- | --- | --- |
| FCS-51 | a light key out of range | safe value applied |
| FCS-52 | `State` (ID 390951) or ShadowSceneNode vtable (ID 254646) not at the audited RVA | light trace and fill light off |
| FCS-53 | the live main scene node is not a ShadowSceneNode | light package off for the session |
| FCS-54 | NiPointLight ctor / vtable, AddLight or RemoveLight differs (ID or prologue) | fill light off, trace stays |
| FCS-55 | NiPointLight allocation / construction failed or wrong class | fill light off |
| FCS-56 | the engine dropped the fill light more than three times in one cell | no further adds until the cell changes |
| FCS-57 | the fill light stayed active after eight `RemoveLight` requests | it stays hidden (contributes nothing) |

## INI reference (`[FixedCameraSpike]`, restart to apply)

| Key | Default | Range / meaning |
| --- | --- | --- |
| `iLightFillMode` | 0 | 0 off, 1 fill light (interiors, eligible frames) |
| `fLightFillHeight` | 192 | 0–1024 above the feet |
| `fLightFillRadius` | 640 | 64–4096 |
| `fLightFillFade` | 0.6 | 0.1–4.0 (the engine switches a light off below 0.05) |
| `fLightFillRed` / `Green` / `Blue` | 1.0 / 0.86 / 0.70 | 0–1 each |
| `iLightTrace` | 0 | 0/1, 6000 lines |

## Tests

Commands, from the worktree root (as CI's `camera-portable` job, `.github/workflows/build.yml:272-281`), g++ 15.2.0:

```
for test in modules/camera/tests/*_test.cpp; do name=$(basename "$test" .cpp)
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$OUT/$name" && "$OUT/$name"; done
python3 -B -I modules/camera/tests/lock_refresh_wiring_test.py
```

| Check | Result |
| --- | --- |
| 15 portable tests (14 existing + `fixed_spike_light_policy_test`) | **PASS**, all exit 0 |
| Wiring audit | **PASS**, six lines: the five existing ones (refusal-code check now 26 codes, one site each across `fixed_spike.h`, `fixed_spike_light.h` and `plugin.cpp`) and `PASS light wiring…` |
| Light policy header under `-Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` | **PASS**, 0 warnings |
| `fixed_spike_light.h` against scratch stand-ins for the engine types it uses (member names and offsets copied from the CommonLib 7.2.0 headers), same warning set, built and run under ASan/UBSan through configure → install → an eligible and an ineligible frame | **PASS**. This checks my syntax, types, shadowing and narrowing; it does **not** prove the real CommonLib or MSVC accept it |
| `tools/tests/test_package.py` (18 tests) | **PASS** |
| `modules/camera/tools/check_preset.py` | **BLOCKED** here, identically on the branch base: it needs a SmoothCam clone at `/tmp/skycrpg-dyl151-audit/` that this host no longer has |
| Windows `/W4 /WX` build | **NOT_RUN** |

`fixed_spike_light_policy_test` covers: key bounds, NaN and out-of-range handling, the "off unless configured" rule, the fade floor; `fillWanted` over every input; presence precedence; the fill state machine (normal add/remove, waiting out a queued add and a queued removal, re-engage after our own removal is not a drop, drop limit and its per-cell reset, removal limit and its reset, idle when never added); `reach`; the census counters (culled, faded, hidden, in reach and live, channel and starved, nearest); and both line formats including NaN/infinity fields and truncation.

## Game rows — all NOT_RUN

| # | Row | Decides | Result |
| --- | --- | --- | --- |
| L1 | Trace at the BFB corridor pose, seed 1, fill off, headings 45 and 135 | whether any light reaches the player in game (`pointReach`, `shadowReach`), whether shadow lights starve (`shadowStarved`) | NOT_RUN |
| L2 | Same with `iLightFillMode=1` | the fill light's look (owner screenshots), `fill=active fillHidden=0`, `pointReachLive` unchanged by the fill (it is excluded) | NOT_RUN |
| L3 | Mzulft01 and EmbershardMine01 coc arrival, fill off and on, heading 45 | the same in a lit Dwemer chamber and in a cave; Embershard is expected to stay unreadable (camera inside the mesh) | NOT_RUN |
| L4 | Lifecycle with fill on: F8 off/on, a menu open/close, a load door or reload, Camera off (F6 path), exterior | an `event=hide` with the matching cause on the first ineligible frame, then `event=remove`, `presence=absent` before re-engage, `event=add`/`show` on return; screenshots after each step show no extra light | NOT_RUN |
| L5 | Frame cost in the BFB corridor, fill off/on | the cost of one dynamic light that re-assigns geometry every frame | NOT_RUN (60 fps cap masks small costs) |

## Route plan

Use the wave 1 route tooling (`session-p55/routes`), one launch per line, traces on (`iRigTrace=1 iRoomTrace=1 iLightTrace=1`), `iRoomSeedMode=1`, `iSeeThroughMode=0`.

1. **BFB corridor, fill off** (≈ 8 min): coc BleakFallsBarrow01, set the corridor pose (z first, per the harness lesson), F7, wait 3 s, screenshot `l-bfb-h0`; DOT, wait 2 s, `l-bfb-h1` (135). Assert in the trace: `FixedLight … cell=000371DE interior=1 eligible=1` lines exist, `fillWanted=0 fill=absent`. Record `pointReach`, `pointReachLive`, `shadowReach`, `shadowStarved`, `pointNearest`, `shadowNearest`.
2. **BFB corridor, fill on** (`iLightFillMode=1`, defaults): same steps, screenshots `l-bfb-fill-h0/h1`. Assert one `event=add cause=eligible`, then `fill=active fillCreated=1 fillHidden=0 fillCull=0` and `fillAt` z = feet + 192 ± 1.
3. **Mzulft01 and EmbershardMine01** (≈ 10 min each), fill off then on, heading 45, screenshots `l-dwemer[-fill]-h0`, `l-cave[-fill]-h0`.
4. **Lifecycle, fill on** (≈ 12 min, Bannered Mare or BFB): F8 → expect `event=hide cause=f8` then `event=remove`; F8 back → `event=add`; open and close the Tween menu → `cause=menu` pair; F6 to first person and back → `cause=first-person`; a load door or `coc` to another interior → `cause=load` or a cell change, then `event=add` in the new cell; walk out to an exterior → `event=hide` and `remove` with `interior=0`. Screenshots after each step. The checker should also assert that no FCS-5x code appears.

Owner judgement: the screenshot pairs of steps 1–3, as the look the option produces.

## What could go wrong in the first game run

- **The fill light does not appear.** Possible if `AddLight` refuses (it returns null for a non-point light: guarded by the vtable check) or the engine never moves our queued light to the active list on this path; the trace then shows `fill=queued-add` forever or repeated `event=add` with `drops` rising and FCS-56. Nothing else is affected.
- **It appears but does not follow the player.** Repositioning relies on ID 106335 copying the NiLight position for dynamic point lights each frame (`150DAC5`); if that loop does not run every frame on this path the light stays where it was added (**Hypothesis** that it runs per frame: torches carried by NPCs depend on it).
- **It lights too much or too little.** The colour, fade and radius are first guesses; the shader's use of radius versus the NiPointLight attenuation constants was not audited (**Hypothesis**: radius-based falloff, as for `TESObjectLIGH` lights, which also leave the attenuation at the ctor defaults).
- **It displaces a real light on busy geometry.** The per-geometry list is order-based with a limit; a geometry already at the limit drops the last light, which may be ours (harmless) or, if the list is re-sorted, a real one (**Hypothesis**: not observed in the code read, which appends).
- **Frame cost.** A dynamic light re-runs room association and geometry assignment every frame (IDs 106342 → 106401 → 108302) over every room in the graph.
- **Spin lock contention.** The trace and the presence check take the scene node's light lock on the main thread for a short scan at up to 10 Hz. The engine takes the same lock on the render path; a long hold elsewhere would stall our frame, not deadlock (we call nothing while holding it).
- **Removal never completes** if `ssn+300` is false on some path: the light stays hidden (FCS-57) and leaves with the next scene reset.
- **The trace is wrong about channels.** `shadowChannel` reads `BSShadowLight+520` from the previous rendered frame; it is reset every frame by ID 106367, so it is current to within one frame.

## Proposed shared-file changes (not edited)

- `docs/evidence/p5.5/fixed-camera-spike.md` (records package): in "2. Room visibility / What the binary does", replace "Three culling functions call it … with camera world position + frustum near × camera forward" by: "ID 13408 is the main camera's culling pass. IDs 106400 and 106401 are per-light room passes (shadow and point lights): they query `QPointWithin` at the light's own position through a NiCamera each light owns (`15126B2`, `151340C`). `cullCalls` therefore also counts light queries, including every admitted shadow light every frame." Same correction for the "culling callers" rows of the binary audit table.
- `modules/camera/src/fixed_spike.h` / `fixed_spike_policy.h` (orchestrator's call; I did not change the room hook): narrow the claim gate to ID 13408 only (keep the three-function guard for install), so that no light is ever associated with a seeded room. With the 96-unit position gate the current risk is small: only a portal-strict light within 96 units of the rendered camera.
- `docs/DECISIONS.md` (owner): a row for the fill light as a look decision under D043, if the owner accepts the look.

## Open questions for the owner

1. The dark corridor is the cell's real lighting. Is a player-following fill light an acceptable look in dungeons, or should dark places stay dark (the vanilla look)?
2. If yes: always on indoors, or only where the trace finds no light reaching the player (`pointReachLive = 0`)?
3. Is a warm "lantern" tint (default) wanted, or neutral white?
