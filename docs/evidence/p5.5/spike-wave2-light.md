# P5.5-02 wave 2 package "light" (DYL-217) — why a seeded room is dark from the fixed camera

Disposable spike work on `phase5.5/p55-02-spike-light`, cut from `744609f`; first pass committed as `7afd6b4`, this document describes `7afd6b4` plus the fix pass after the independent binary review (see "Review fix pass"). Nothing here is merged into the shipped Camera DLL. Every game row is **NOT_RUN**: no game was launched for this package. Compilation and portable tests prove nothing about in-game behaviour.

Inputs: `SkyrimSE.exe` 1.7.104 (SHA-256 `846efccf0c1374d7…`), Address Library `versionlib-1-7-104-0.bin` (`8aab3dd251d135b8…`), `Skyrim.esm` (`e198c3b85e5e48e0…`) and `Update.esm` (`b298b0f65fa0127f…`), wave 1 run records under `session-p55/routes/runs/` and screenshots under `session-p4/autotest/runs/`. Scratch scripts (outside the worktree): `/tmp/p55-light/scripts/` (`cells.py` first ESM scan, `rooms.py` room markers / portals / lights with additive radius, `lum3.py` / `void.py` screenshot luminance), full `.text` disassembly `/tmp/p55-light/text.asm` (`objdump -d -Mintel`), RVA↔ID map from the Address Library (ID n at file offset 96+4n).

## Answer

**The Bleak Falls Barrow corridor is dark because nothing lights that spot, not because the fixed camera removes light.** Confidence: high for "the camera does not cull the room's point lights" (binary), high for "no placed light *of the player's room* reaches the corridor pose" (ESM radii with the reference extra, room markers and portal links), medium for "that is what the screenshot shows" (one close-camera comparison at the same pose, different view).

1. **No light of the player's room reaches the pose.** A light's radius is the base `LIGH` radius **plus** the reference's `XRDS` (Source: ID 20173 `2FFD70`, called by the light builder ID 17610 at `2746C2`: for a base of form type 0x1F it returns `(float)LIGH+E4 + ExtraRadius` (`2FFE3C-2FFE60`, extra read by `16C300`); the result is written to NiLight `+128` at `2746E6`; negative `XRDS` values shrink the radius). With that, exactly one light's sphere contains the player point (feet + 64): reference `0003960F` (base `0002EDFC` BleakFallsDraugrTorch01, omni shadow, portal-strict) at (1218.7, −10769.2, −2534.9), radius 256 + 1743.9 = 1999.9, the point about 239 units inside. It is one reference (Skyrim.esm defines it, Update.esm overrides it; my first scan listed it twice). It still cannot light the player: it is in room marker `0003923A`, the player in `00039233`; the two boxes share the x = 1920 face (`XMBO` half extents; 3923A spans x 0–1920, 39233 x 1920–3456) and no portal joins them (`XPOD` pairs: `0003924B` 39238↔3923A, `00039247` 39233↔39234, `00039246` 39232↔39233), and a portal-strict light lights only nodes under rooms its own portal walk reached (ID 108302 at `15775A0-15775DD`, the walk in ID 106401 `151358E-1513845`). The nearest light of the player's own room is the omni shadow light `000395F1` (radius 256 + 924.7 = 1180.7), 1343 units from the player point, 163 outside its sphere; the nearest point light of that room (`000310E6`, radius 436.5) is 560 outside. The cell's own lighting is ambient (66, 60, 40) with directional fade 0, and its directional-ambient term for the +Z face is the darkest of the six: (31, 28, 18) against (59–72, …) for the side faces and (105, 96, 65) for −Z (Source: `XCLL`, inherit flags 0).
2. **The close camera at the same pose was just as dark.** Phase 5 candidate screenshots at the same pose (`batch-fx2-0002-corridor-00/corridor-start.png`, `batch-final2-2204-corridor-00/matched-before.png`): mean luma of rendered pixels 12.7 (median 10.4). The seeded fixed view (`batch-p55-c-bfb-seed1-1-00/c-bfb-h0.png`): 10.3 (median 7.7) on the 63 % of the frame that is not void. Same order; the views show different surfaces. The Bannered Mare (40–48) and Mzulft (21.5) are brighter because lights reach their poses (hearth shadow omni at 278 units with base radius 768; Mzulft's template adds a directional light).
3. **The camera does not take light away from the player's room.** Point lights are culled per frame only by a sphere-against-camera-frustum test and a camera-distance fade that starts at `fLightLODStartFade` (3500 on this host); no room or portal test is involved. Which geometry a light touches is decided from the light's own position. Shadow-casting lights have camera-dependent admission (below), but at this pose no shadow light that can light the player reaches it (the one sphere that contains the point belongs to a room without a portal to the player's room), so admission cannot be the cause *at the pose*. See the caveat below for the rest of the room.
4. **The void around the island is the fog colour, not black.** The non-rendered area reads (63–68, 80–100, 86–117) and matches the cell fog colour (75, 99, 97) after tone mapping (**Hypothesis**: interior clear colour = fog colour). Fog (near 0, far 12000, power 0.7) adds about 9 percentage points more fog-colour blend at 760 units than at a first-person distance (**Hypothesis**, Skyrim fog formula not audited); fog lightens, it cannot darken.

**Caveat (reviewer's, recorded here):** `000395F1` is in the player's room and lights other parts of the visible island. Whether it keeps one of the four shadow-mask channels under the fixed camera is unknown (NOT_RUN); if it loses its channel, the rest of the room is darker than vanilla even though the pose itself is not. The light trace now lists shadow lights by reference ID (`FixedLightShadow … 000395F1:…`) so route L1 can show it.

So, at the pose, there is nothing to *restore*. A fix that makes the corridor readable has to *add* light, which changes the look. That is implemented as a clearly labelled, default-off option (`iLightFillMode=1`); the owner decides whether the look is acceptable.

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

## Offline content facts (Source: `cells.py` and `rooms.py`, Skyrim.esm then Update.esm overrides, one row per reference)

Radius = base `LIGH` radius + reference `XRDS` (ID 20173, above). "Room" = the room-marker boxes (`XMBO` half extents, `DATA` position; all rotations here are 0) that contain the point; the census point is the route pose + 64.

| Cell | XCLL inherit | Ambient | Directional / fade | DALC +Z / −Z | Fog near–far, colour | LIGH refs | Lights whose sphere contains the census point |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BleakFallsBarrow01 000371DE | 0 (cell values) | (66,60,40) | (44,47,48) / 0.0 | (31,28,18) / (105,96,65) | 0–12000, (75,99,97) | 71: 57 point, 11 omni-shadow, 3 spot-shadow, all portal-strict | one, `0003960F` (r 1999.9, 239 inside) in room 3923A; the point is in room 39233 and no portal joins them: **none can light it** |
| Mzulft01 0001523F | 2047 (template 0008810A) | template (40,37,23) | template (79,89,91) / 0.5 | (19,17,10) / (60,56,35) | template 1500–6000 | 57: 47 point, 8 omni-shadow, 2 spot-shadow | point light `000261AC` (r 256 + 482.2 = 738.2, 96 inside), same room 66411: **lit** |
| EmbershardMine01 000B6BE6 | 2047 (template 000E7C5E) | template (31,31,31) | template (65,81,84) / 0.0 | (14,14,14) / (47,47,47) | template 750–6000 | 39: 32 point, 7 omni-shadow | point light `0008F94C` (r 256 + 647.8 = 903.8, 79 inside), same room B6FBA: **lit** (the camera is inside the mesh, so it is not seen) |
| WhiterunBanneredMare 0001605E | 159 (template 0006175D) | template (57,53,49) | (64,73,74) | cell (6,6,5) / (21,19,18), template (27,25,22) / (86,80,75) | 128–2000 | 18: 16 point, 2 omni-shadow | hearth omni-shadow `000C8FBD` (r 768 − 188.9 = 579.1, 319 inside); no room markers in this cell: **lit** |

LIGH flag meanings used here are from the binary, not from a wiki: ID 17610 (`274390`, building the light from a `TESObjectLIGH`) sets `shadowLight` when `flags & 0x1C00` (0x400 spot, 0x800 hemisphere, 0x1000 omni) and `portalStrict` from bit 0x2000 (`274833-27484A`, `2747B9-2747C5`). The inherit-flag and DALC face meanings are **Hypotheses** (record layout, not audited in the binary).

## Binary audit (1.7.104, file bytes; Address Library IDs resolved and the code read at each RVA)

### Per-frame light selection

| What | Where | Finding |
| --- | --- | --- |
| Light queue and per-frame point-light update | ID 106335 `150D410` | Takes the ShadowSceneNode light lock (`[ssn+1A8]`, `150D439`), moves queued lights, then per active light: dynamic point lights that are not camera-culled (`+5C != 0xFF`, checked at `150DABC`) copy the NiLight position (`150DAC5-150DAE7`) and re-assign geometry through ID 106342; every light is then camera-culled by ID 108278 (`150DB02`). Callers: 106337 (all four scene nodes), ID 106385 ResetScene (`1511E97`) and seven render paths (13280, 13316, 15691, 19411, 35780, 40744, 52727). |
| Point-light camera cull | ID 108278 `1575D20` | Sphere (NiLight `+A0` position, `+128` radius) against the culling process's six frustum planes (`1575D70-1575DC1`); then, when `BSLight+63` is set, a fade on `|light−camera|² − r²` between `fLightLODStartFade²` and `(fLightLODStartFade+fLightLODRange)²` (globals `33D3D34/38`, written at `412FFD` from Settings `20D6AC0` / `20D6AD8`). Writes `BSLight+5C` = 0 or 0xFF; the `+14` dimmer is recomputed only for a light inside the frustum: for a culled light the value read at `1575DC3` is stored back unchanged (`1575DCC` → `1575E62`/`1575ED9`). **No room or portal test.** Host values: 3500 and 500. |
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

**Consequences for the brief's questions.** (i) Lights of the player's room are not culled because the camera is in another room: point lights are tested against the camera frustum and distance only. (ii) Shadow lights *are* camera-referenced: frustum, `fInteriorShadowDistance` from the camera, a room shared with the camera's visited set, and the first four in list order. Seeding mode 1 adds the player's room to the camera's visited set (it is a start room of ID 13408), so the player's room's shadow lights stay eligible; with the camera in no room the camera entry carries only the `+130` flag, and (**Hypothesis**, from ID 76121's code) shadow lights bound to rooms are then not admitted. (iii) The four-channel limit is list-ordered, so a fixed camera that sees more rooms can let other rooms' shadow lights take the channels first (**Hypothesis** for any given pose; the light trace counts it as `shadowStarved`, room-aware since the fix pass). (iv) The room test the spike hooks is not what light selection uses; it is what light *association* uses, at the light's position.

### A wave 1 finding this audit corrects, and the change made for it

Wave 1 classified IDs 106400 and 106401 as camera culling passes ("ask about camera + near × forward"). They are the per-light room passes above: the NiCamera they use is the light's own, at the light's position, and they are called only from the per-light update ID 106342 (`150E4A6`, `150E4D7`). The independent review confirmed this. Consequences in `7afd6b4` and earlier: `cullCalls` counted every portal-strict light query (admitted shadow lights re-run ID 106342 every frame), and a portal-strict light within 96 units of the rendered camera, while seeding was active, got the seeded room as an extra start room.

**Change (fix pass, clearly delimited in `fixed_spike_policy.h` "room seeding" and one line in `hookRoom`):** `RoomCaller` now has `camera` (ID 13408, return addresses `1A9981`, `1A9A0E`) and `light` (IDs 106400 / 106401, the other four sites). Claims are made only for `camera`; light passes at the camera are answered vanilla and counted in `gatedNear`. The install guard still checks all three functions (address, unwind extent, six call sites), unchanged. The room trace keeps `cullCalls` (both passes, the wave 1 meaning, which the route checker uses) and adds `cameraPassCalls` and `lightPassCalls`.

**Do the wave 1 seeding results stand? Yes for what was drawn (high confidence); the counts are mixed (certain).** Which rooms are drawn is decided only by the start rooms of ID 13408 (the camera culling process; light passes write into each light's own culling entry and never into the camera's), so the drawn island in the seed 1-3 screenshots came from camera-pass claims. A light-pass claim could only have attached a light near the camera to the seeded room, which adds light rather than removing it; whether any happened is unknown because the wave 1 `claims` and `cullCalls` counters did not separate the passes.

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

Files: `modules/camera/include/fixed_spike_light_policy.h` (policy, no engine types), `modules/camera/src/fixed_spike_light.h` (adapter), `modules/camera/tests/fixed_spike_light_policy_test.cpp`; small delimited edits to `fixed_spike.h` (refusal codes, the include after the room helpers, three calls, the grace flag, configure signature, install log fields), `plugin.cpp` (eight INI reads inside the existing `[FixedCameraSpike]` gate), the INI and the wiring audit. The light package adds **no hook, no code patch, no vtable write**. Separately, the fix pass narrows the wave 1 room hook's claim gate (above), in `fixed_spike_policy.h`, `fixed_spike.h` and `fixed_spike_policy_test.cpp`.

With `iLightFillMode=0` and `iLightTrace=0` (the defaults, or keys absent) no light object is allocated, no engine light state is read and no engine function is called; the light package behaves as `744609f`. With room seeding on, the narrowed claim gate is a deliberate behaviour change from `744609f` (light passes no longer claimed for). The only visible difference is three appended fields on the existing `installed …` log line (`light=off fill=off lightTrace=0`); the route checker matches that line by prefix and ` rooms=… `, so it is unaffected.

### Light trace (`iLightTrace=1`, read-only)

About 10 Hz in eligible frames, 6000 lines, into `SkyCRPG_Camera_FixedSpike.log`. First, before any lock, it finds the rooms of the cell's portal graph whose **vanilla** `QPointWithin` (the saved original, never a seeded claim) contains the player point (feet + 64, the spike's `eyeLift`, **Hypothesis** for chest height), at most 8. Then, on the main thread, under the scene node's light lock (the lock ID 106335 takes; no engine call while it is held), it reads the active point and shadow lights:

`FixedLight t frame cell interior eligible eye cam rooms playerRooms point pointCulled pointDimmed pointReach pointLit pointLitLive pointNearest shadow shadowChannel shadowReach shadowLit shadowLitChannel shadowStarved shadowNearest fillWanted fill fillCreated fillHidden fillCull fillDimmer fillAt adds removes drops sample=state`

`rooms` = the cell has a portal graph with rooms; `playerRooms` = how many contain the point. `*Reach` = bare sphere test (kept for comparison; it ignores rooms). `*Lit` = the room-aware test: the sphere contains the point and, for a portal-strict light (`BSLight+47`) in a cell with rooms, one of the light's own rooms (`BSLight+D8`, the list ID 108279 appends to) is a room containing the point (pointer comparison only). `pointLitLive` = of the lit point lights, not camera-culled, not hidden, dimmer > 0. `shadowChannel` = shadow lights holding a channel after the last shadow pass; `shadowStarved` = shadow lights that are `Lit` but hold no channel (they light nothing). `*Nearest` = smallest (distance − radius) among lights whose room condition holds, ≤ 0 means inside. Not modelled: spot cones and occlusion, so `Lit` is an upper bound on "lights the player". Values are those of the last rendered frame.

Once a second it also writes `FixedLightShadow t frame total listed <ref>:reach=,r=,ch=,cull=,strict=,room=,lit=,hidden= …` for the 8 nearest active shadow lights, each named by the placed reference whose 3D holds its NiLight (NiLight parent → `GetUserData`, the walk ID 19750 `2EECE0` does: parent at `+30`, node user data at `+F8`; 0 when none), so one light, for example `000395F1`, can be followed by ID. The fill light is reported separately and excluded from the point counts. Fill transitions are traced as events in every frame: `FixedLight … event=add|remove|show|hide|drop-limit|remove-limit cause=<exit or eligible> presence=…`.

### Fill light (`iLightFillMode=1`, **presentation aid: changes the look**)

A dynamic point light (no shadows, not portal-strict, no camera-distance fade, ambient 0, diffuse `fLightFillRed/Green/Blue`, radius `fLightFillRadius`, fade `fLightFillFade`) created once with the engine's own NiPointLight constructor and handed to `AddLight`, exactly as the engine builds a `TESObjectLIGH` light. It is wanted only in **interior cells** in **eligible rig frames**; each wanted frame its NiLight is placed `fLightFillHeight` above the feet (the engine repositions dynamic point lights from the NiLight each frame). It follows the player, not the camera: a camera-side light 760 units away would need a radius that also floods the rock between camera and player.

On every ineligible frame (F8, menu, first person, special state, load, death, Camera off) and outside interiors it is app-culled at once (the engine then detaches it from all geometry) and, once it is in the active list and has been unwanted for 0.5 s, handed back with `RemoveLight`; the state machine never asks twice while a removal is queued and waits out a queued add. The 0.5 s delay means a menu tap does not free and rebuild the engine's `BSLight` and its 0x30230-byte culling process (allocated by ID 106401 at `1513344`). A camera update that skips a frame for no identified reason gets wave 1's grace (`restoreNow`, three frames): the fill light is then left exactly as it was, neither hidden nor moved. If the scene node is refused after the light was created (FCS-53), the light is hidden on the next frame and no further engine call is made; it stays registered, hidden, until the next scene reset. If the engine drops it while wanted (for example a cell reset) it is re-added up to three times per cell (FCS-56 after that); if `RemoveLight` does not take after eight requests it stays hidden (FCS-57). The NiLight is kept for the process lifetime and never freed, because the engine may still hold a reference. It never touches saves (lights are runtime objects).

What it does not do: it does not restore any engine light, does not change shadow admission, does not light exteriors, and does not make one-piece caves visible from inside the rock (Embershard: the camera is inside the mesh).

### Decision: no shadow-focus hook

A "player as reference point" fix for shadow lights would wrap `UpdateCamera` (slot 0x10) of the two shadow-light classes behind a caller gate on return address `1538B4F`, refusing lights that cannot light the player so that the player's lights win the four channels. I did not build it: no shadow light that can light the player reaches the corridor pose, so it cannot be the cause there, and it would add a vtable hook on a render path for an unproven gain.

Decision rule, restated after the review: the hook is justified only by a game row in which **`shadowStarved > 0`** (room-aware: the light's sphere contains the player point *and* it is not portal-strict, or the cell has no rooms, or one of its rooms contains the player point), the `FixedLightShadow` line names that light with `lit=1 ch=255`, and the screenshot shows the player area darker than the same pose in vanilla. A light the player's room does not have (`room=0`, such as `0003960F` at the BFB corridor) can never trigger it. A starved light of the player's room that does not contain the point (for example `000395F1` at the corridor) is the reviewer's caveat: it shows as `lit=0 room=1 ch=255` in the list and can only be judged from screenshots.

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
| 15 portable tests (14 existing + `fixed_spike_light_policy_test`), rerun in the fix pass | **PASS**, all exit 0 |
| Wiring audit, rerun in the fix pass | **PASS**, six lines: the five existing ones (refusal-code check 26 codes, one site each across `fixed_spike.h`, `fixed_spike_light.h` and `plugin.cpp`; the room hook check now on `RoomCaller::camera`) and `PASS light wiring…` (now also: grace flag ordering, the include after `within`, player rooms found before the lock and outside the census, the delayed-remove call) |
| Light policy header under `-Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` | **PASS**, 0 warnings |
| `fixed_spike_light.h` against scratch stand-ins for the engine types it uses (member names and offsets copied from the CommonLib 7.2.0 headers), same warning set, built and run under ASan/UBSan through configure → install → an eligible frame with a scene node holding one point and two shadow lights (one sharing the player's room, one not) → a held (grace) frame → an ineligible frame; the printed `FixedLight` and `FixedLightShadow` lines were read | **PASS**. This checks my syntax, types, shadowing and narrowing; it does **not** prove the real CommonLib or MSVC accept it |
| `tools/tests/test_package.py` (18 tests) | **PASS** |
| `modules/camera/tools/check_preset.py` | **BLOCKED** here, identically on the branch base: it needs a SmoothCam clone at `/tmp/skycrpg-dyl151-audit/` that this host no longer has |
| Windows `/W4 /WX` build | `7afd6b4`: PASS (CI run 42753164263, reported by the orchestrator). Fix pass: **NOT_RUN** |

`fixed_spike_light_policy_test` covers: key bounds, NaN and out-of-range handling, the "off unless configured" rule, the fade floor; `fillWanted` over every input; presence precedence; the fill state machine (normal add/remove after the removal delay, a menu tap inside the delay keeps the light, a broken or backward clock, waiting out a queued add and a queued removal, re-engage after our own removal is not a drop, drop limit and its per-cell reset, removal limit and its reset, idle when never added); `lightsPoint` over every input and `anyShared` (null rooms never match); `reach`; the census counters (culled, faded, hidden, bare reach, room-aware lit and live, other-room portal-strict lights, the no-rooms cell, channel and starved, nearest among the player's room's lights) including the BFB `0003960F` case (`shadowReach=1`, `shadowLit=0`, `shadowStarved=0`); the shadow list (nearest 8 kept in order, NaN last); and all line formats including the widest line and the full shadow list fitting one 768-byte trace row. `fixed_spike_policy_test` now checks that only ID 13408's two call sites are the camera pass, the other four are light passes, and that a light pass at the camera is never claimed for in any mode.

## Game rows — all NOT_RUN

| # | Row | Decides | Result |
| --- | --- | --- | --- |
| L1 | Trace at the BFB corridor pose, seed 1, fill off, all four headings | expected `rooms=1 playerRooms>=1 pointLit=0 shadowReach=1 shadowLit=0 shadowStarved=0` (the ESM prediction); whether `000395F1` holds a channel (`FixedLightShadow`), heading by heading | NOT_RUN |
| L2 | Same with `iLightFillMode=1` | the fill light's look (owner screenshots), `fill=active fillHidden=0`, `pointLitLive` unchanged by the fill (it is excluded) | NOT_RUN |
| L3 | Mzulft01 and EmbershardMine01 coc arrival, fill off and on, heading 45 | the same in a lit Dwemer chamber and in a cave; Embershard is expected to stay unreadable (camera inside the mesh) | NOT_RUN |
| L4 | Lifecycle with fill on: F8 off/on, a menu open/close (one held ≥ 1 s, one tap), a load door or reload, Camera off (F6 path), exterior | an `event=hide` with the matching cause on the first ineligible frame; `event=remove` only after ≥ 0.5 s unwanted (none for the tap: `event=show` with the same `adds`); `presence=absent` before re-engage after a removal, `event=add`/`show` on return; screenshots after each step show no extra light | NOT_RUN |
| L5 | Frame cost in the BFB corridor, fill off/on | the cost of one dynamic light that re-assigns geometry every frame | NOT_RUN (60 fps cap masks small costs) |

## Route plan

Use the wave 1 route tooling (`session-p55/routes`), one launch per line, traces on (`iRigTrace=1 iRoomTrace=1 iLightTrace=1`), `iRoomSeedMode=1`, `iSeeThroughMode=0`.

1. **BFB corridor, fill off** (≈ 8 min): coc BleakFallsBarrow01, set the corridor pose (z first, per the harness lesson), F7, wait 3 s, screenshot `l-bfb-h0`; DOT, wait 2 s, `l-bfb-h1` (135). Repeat for headings 225 and 315 (two of the four put the camera inside `0003960F`'s room 3923A). Assert in the trace: `FixedLight … cell=000371DE interior=1 eligible=1 rooms=1` lines exist with `playerRooms>=1`, `fillWanted=0 fill=absent`, and at least one `FixedLightShadow` line per heading. Record `pointLit`, `pointLitLive`, `shadowReach`, `shadowLit`, `shadowStarved`, `pointNearest`, `shadowNearest`, and the `000395F1` and `0003960F` entries (`ch`, `room`, `lit`) per heading.
2. **BFB corridor, fill on** (`iLightFillMode=1`, defaults): same steps, screenshots `l-bfb-fill-h0/h1`. Assert one `event=add cause=eligible`, then `fill=active fillCreated=1 fillHidden=0 fillCull=0` and `fillAt` z = feet + 192 ± 1.
3. **Mzulft01 and EmbershardMine01** (≈ 10 min each), fill off then on, heading 45, screenshots `l-dwemer[-fill]-h0`, `l-cave[-fill]-h0`.
4. **Lifecycle, fill on** (≈ 12 min, Bannered Mare or BFB): F8 → expect `event=hide cause=f8` then `event=remove`; F8 back → `event=add`; open and close the Tween menu → `cause=menu` pair; F6 to first person and back → `cause=first-person`; a load door or `coc` to another interior → `cause=load` or a cell change, then `event=add` in the new cell; walk out to an exterior → `event=hide` and `remove` with `interior=0`. Screenshots after each step. The checker should also assert that no FCS-5x code appears.

Owner judgement: the screenshot pairs of steps 1–3, as the look the option produces.

## What could go wrong in the first game run

- **The fill light does not appear.** Possible if `AddLight` refuses (it returns null for a non-point light: guarded by the vtable check) or the engine never moves our queued light to the active list on this path; the trace then shows `fill=queued-add` forever or repeated `event=add` with `drops` rising and FCS-56. Nothing else is affected.
- **It appears but does not follow the player.** Repositioning relies on ID 106335 copying the NiLight position for dynamic point lights each frame (`150DAC5`); if that loop does not run every frame on this path the light stays where it was added (**Hypothesis** that it runs per frame: torches carried by NPCs depend on it). The copy is skipped while the light is camera-culled (`+5C == 0xFF`, `150DABC`), so after it re-enters the frustum it lights from its old position for one frame.
- **It lights too much or too little.** The colour, fade and radius are first guesses; the shader's use of radius versus the NiPointLight attenuation constants was not audited (**Hypothesis**: radius-based falloff, as for `TESObjectLIGH` lights, which also leave the attenuation at the ctor defaults).
- **It displaces a real light on busy geometry.** The per-geometry list is order-based with a limit; a geometry already at the limit drops the last light, which may be ours (harmless) or, if the list is re-sorted, a real one (**Hypothesis**: not observed in the code read, which appends).
- **Frame cost.** A dynamic light re-runs room association and geometry assignment every frame (IDs 106342 → 106401 → 108302) over every room in the graph.
- **Spin lock contention.** The trace and the presence check take the scene node's light lock on the main thread for a short scan at up to 10 Hz. The engine takes the same lock on the render path; a long hold elsewhere would stall our frame, not deadlock (we call nothing while holding it).
- **Removal never completes** if `ssn+300` is false on some path: the light stays hidden (FCS-57) and leaves with the next scene reset.
- **The room-aware census reads a light's room list (`BSLight+D8`) under the scene node's light lock.** ID 106335 changes it under that lock, but ID 107137's admission path also re-runs ID 106342 for admitted shadow lights; if that ever runs on another thread while our main-thread census reads, the read could see a list being reallocated (pointer comparisons only, nothing dereferenced; **Hypothesis** that both run on the main thread). Trace-only (`iLightTrace=1`).
- **The trace is wrong about channels.** `shadowChannel` reads `BSShadowLight+520` from the previous rendered frame; it is reset every frame by ID 106367, so it is current to within one frame.

## Proposed shared-file changes (not edited)

- `docs/evidence/p5.5/fixed-camera-spike.md` (records package): in "2. Room visibility / What the binary does", replace "Three culling functions call it … with camera world position + frustum near × camera forward" by: "ID 13408 is the main camera's culling pass. IDs 106400 and 106401 are per-light room passes (shadow and point lights), called only from ID 106342: they query `QPointWithin` at the light's own position through a NiCamera each light owns (`15126B2`, `151340C`). In wave 1, `cullCalls` and `claims` therefore also counted light queries; the drawn-room results stand because only ID 13408's start rooms decide drawing." Same correction for the "culling callers" rows of the binary audit table and its `RoomCaller` description.
- The narrowed claim gate is applied in this worktree (fix pass); the orchestrator carries it to the other builds.
- `docs/DECISIONS.md` (owner): a row for the fill light as a look decision under D043, if the owner accepts the look.

## Open questions for the owner

1. The dark corridor is the cell's real lighting. Is a player-following fill light an acceptable look in dungeons, or should dark places stay dark (the vanilla look)?
2. If yes: always on indoors, or only where the trace finds no light reaching the player (`pointReachLive = 0`)?
3. Is a warm "lantern" tint (default) wanted, or neutral white?

## Review fix pass (after the independent binary review of `7afd6b4`: CHANGES, 0 BLOCKER / 1 MAJOR / 4 MINOR)

| Finding | Confirmed against | What changed |
| --- | --- | --- |
| MAJOR 1: `XRDS` is additive; one light's sphere contains the BFB point; the census ignored portal-strict rooms | ID 20173 `2FFD70` (`2FFE3C-2FFE60`), call `2746C2`, store `2746E6`; `0003960F` is one reference; rooms 3923A / 39233 from `XMBO` boxes; portal pairs from `XPOD`; `BSLight+47` (read by 106400 at `151257D`, written by AddLight `150C9F0`) and `BSLight+D8` (appended by 108279 at `1575EF0`) | Census room-aware (`*Lit` fields, player rooms from the vanilla room test before the lock), `FixedLightShadow` list by reference ID, document tables rescanned with base + `XRDS` and room membership, decision rule restated, 11 omni-shadow (not 12) |
| MINOR 1: no grace for one ineligible frame | `fixed_spike.h` ineligible path, `restoreNow` | `frameLight` takes `hold = !restoreNow(...)`; removal delayed 0.5 s after the light is hidden |
| MINOR 2: FCS-53 left the light unhidden | `serviceFill` returned before `show(false)` | `serviceFill` hides the light first when `!fillReady` |
| MINOR 3: room hook claimed for light passes | callers of 106400 / 106401 (`150E4A6`, `150E4D7`, both in ID 106342) | claim gate narrowed to ID 13408; `cameraPassCalls` / `lightPassCalls` added, `cullCalls` kept; wave 1 statement above |
| MINOR 4: document | `1575DC3-1575E62`, `1511E97`, `150DABC` | dimmer, ResetScene caller and one-frame lag corrected |

Unverified after the fix pass: the room-box containment is my reading of `XMBO` as half extents with zero rotation (it agrees with the reviewer's room assignment); that ID 106401's portal walk would not reach 39233 from 3923A rests on the `XPOD` pairs, not on a run; every game row is still NOT_RUN; the fix pass has not been built with MSVC.
