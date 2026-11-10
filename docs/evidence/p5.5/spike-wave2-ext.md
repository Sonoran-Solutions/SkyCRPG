# P5.5-02 wave 2 package "ext" (DYL-217) — see-through for roofs, overhangs and canopies in exterior cells

2026-11-09. Worktree `p55-spike-ext`, branch `phase5.5/p55-02-spike-ext`, base `744609f`. **Disposable: never merged, nothing enters the shipped Camera DLL.** Experimental scope only (D045): Skyrim.esm, Update, Dawnguard, HearthFires, Dragonborn on Steam Skyrim SE 1.7.104, SKSE 2.3.1, SmoothCam 1.8.1, TDM 2.3.1.

**Status.** Source, portable tests, wiring audit and offline binary and data checks are complete. **Not compiled on Windows (NOT_RUN). Every in-game row is NOT_RUN.** No commit, push, branch change, game or harness launch, download or external post was made. Compilation and portable tests prove no in-game behaviour.

Each claim is **Source** (file:line, binary RVA, ESM record, recorded game log) or **Hypothesis**. RVAs are relative to image base `0x140000000` of `SkyrimSE.exe` (SHA-256 prefix `846efccf0c1374d71f892907`). Address Library `versionlib-1-7-104-0.bin` SHA-256 `8aab3dd2…9423842` (ID n at file offset 96+4n). The AE IDs apply (`cmake/CommonLib.cmake` builds AE only). CommonLibSSE-NG pin `7a60f4de` (read from `session-20261026/commonlib-camera-audit`, which is that commit).

## What was built

All of it sits behind `[FixedCameraSpike] bEnabled=1` **and** the new `iExteriorSeeThrough` (default 0).

| File | Change |
| --- | --- |
| `modules/camera/include/fixed_spike_ext_policy.h` (new) | Portable rule, no engine headers: settings, oriented boxes from OBND × world transform, slab test, cover column, overhang test and open-below probes, canopy crossing, large-structure test, classification, exterior ray targets, cell range and grid-slot mapping, counters, the `ext-summary` line, own refusal codes. |
| `modules/camera/src/fixed_spike_ext.h` (new) | Native adapter: settings hook, kinds, the loaded-grid walk, exterior rays and probes, the scan, claim with the exterior cap, apply loop, traces, refusals FCS-62..64. |
| `modules/camera/src/fixed_spike.h` | Delimited: include of the ext policy, `ExtState ext` in `Runtime`, two forward declarations, **one dispatch line in `endFrame`**, include of `fixed_spike_ext.h` after the namespace. |
| `modules/camera/include/fixed_spike_policy.h` | `Reason` gains `extCover`, `extOverhang`, `extCanopy` and their trace names (appended; existing values unchanged). |
| `modules/camera/src/plugin.cpp` | Three INI reads and `fixed::configureExterior(e)` inside the existing `bEnabled` gate, after `fixed::configure`. |
| `modules/camera/config/SkyCRPG_Camera.ini` | `iExteriorSeeThrough=0`, `fExteriorMaxSpan=1024`, `iExteriorMaxHidden=16`, documented. |
| `modules/camera/tests/fixed_spike_ext_policy_test.cpp` (new), `tests/lock_refresh_wiring_test.py` (extended) | Portable tests and a sixth audit block. |

### Off equals 744609f

With `iExteriorSeeThrough` absent or 0, `exteriorScanned(0,·)` is false, so `endFrame` takes the unchanged wave 1 branch: interior hide when configured, otherwise in an exterior with `iSeeThroughMode=1` `restoreAll(Exit::cell)` and the one `FCS-31` notice per cell entry (`fixed_spike.h` `endFrame`, dispatch order asserted by the audit). The only other runtime effects of the change with the key at 0: `configureExterior` runs once at load and logs one `FixedCameraSpike exterior see-through mode=0 …` note line, and `Runtime` carries the unused `ExtState`. The three new `Reason` values are never produced. With `bEnabled=0` nothing changes at all (no runtime). **In-game regression: NOT_RUN.**

## What the binary and the data say (all checked offline; method at the end)

| Fact | Evidence |
| --- | --- |
| All loaded exterior cells share **one** havok world, so a ray from the player's cell returns hits in neighbouring cells | `TESObjectCELL::GetbhkWorld` (ID 18995, `2BE9B0`): interior → `ExtraHavok` from the extra list at `+48`; exterior → `mov rax,[21A0598]; mov rax,[rax]`, one global regardless of the cell. **Source.** |
| `TES` singleton | ID 403450 → `32303B8` (`.data`, BSS part); loaded and passed as `this` to `TES::GetCell` at `20134A`→`201351` and `402706`→`402721`. CommonLib `RELOCATION_ID(516923, 403450)`. **Source.** |
| `TES::GetCell(NiPoint3)` and the position-to-cell step | ID 13322 → `1A3C60`: returns `[this+C0]` (interior cell) if set; else `cvttss2si`, floor correction, `sar 0xC` on x and y, call ID 13319, and returns the cell only if `[cell+44]` is 6 or 7. **Source.** The ext code does not call it; it repeats the arithmetic (`cellIndex`) and uses it as the self-check. |
| Grid lookup | ID 13319 → `1A3A00`: `half = uGridsToLoad >> 1` (global `2079EB0`, ID 359676, value 5 in the file), `slot = cell − [TES+B0] / [TES+B4] + half`, bounds `[0, length)`, then `[TES+78]` (gridCells) passed to `227120` (ID 15946): `length = [grid+10]`, slot = `[grid+18] + (x·length + y)·8`. Matches CommonLib `TES::gridCells` (`+78`), `currentGridX/Y` (`+B0/+B4`), `interiorCell` (`+C0`), `GridArray::length` (`+10`), `GridCellArray::cells` (`+18`), `GetCell(x,y)` = `cells[x*length+y]`. **Source.** |
| Cell attached state | `[cell+44]`, values 6 and 7 accepted as loaded by 13322; CommonLib `cellState` `+44`, `kAttached = 7`, `IsAttached()` compares to 7 (pin `src/RE/T/TESObjectCELL.cpp:130-133`). That 7 means "attached" specifically is CommonLib's naming (**Hypothesis** for the name only; a cell in state 6 is skipped, fail closed). |
| Exterior cell coordinates | `2BE9D0` (next to GetbhkWorld) reads `[cell+68]` → `[rax]`, `[rax+4]`: `cellData.exterior->cellX/cellY` (CommonLib AE `RUNTIME_DATA` at `+68`). **Source.** Not used by the ext code (slots are computed from positions). |
| App-culled roots are skipped by the parent's cull, for every node type | ID 70267 (`EE0F30`): `test byte [rcx+F4],1; jne ret; else process->vfunc[B0/8](obj)`. NiNode::OnVisible (vtable ID 237098 slot 0x34 = `EE2CC0`, ID 70307) calls `EE0F30` for each child (`EE2D26`); BSMultiBoundNode::OnVisible (vtable ID 238470 slot 0x34 = `FF2DB0`, ID 76690) calls it at `FF2EE9`. 13 direct callers in total. **Source.** |
| Trees | `BSTreeNode` vtable ID 252919 → `1B2FBB8`; its slot 0x34 `14E93D0` is its own OnVisible (an optional distance test on `[process+121]`, `[20D67F8]`, then `BSLeafAnimNode::OnVisible` `14E89D0`, then flags on `[this+180]`). It is reached only after the parent's `EE0F30` app-cull test, so **app-culling a tree's 3D root stops it being drawn, like any static** (Source for the call path; the drawn result is NOT_RUN). Distant tree LOD is a different object: `BGSDistantTreeBlock` instances in `BSMultiStreamInstanceTriShape` geometry with their own `hidden` byte (CommonLib `BGSDistantTreeBlock.h`), not under the reference's 3D. **Hypothesis:** the engine hides the LOD instances of loaded cells independently of the full tree's visibility, so hiding the 3D shows neither (nothing reappears); if it does not, a billboard would show where the tree was, a look defect, not a hazard. |
| What the sightline rays can hit | Skyrim.esm COLL records (scratch parse of the `COLL` group): `L_LOS` (layer 41, `0008878A`) collides with layers 0, 1 static, 2 animstatic, 4 clutter, 5, 6, 7, 8 biped, 9 trees, 10 props, 13 terrain, 14, 16, 17 ground, 32, 38-41, 43, 44, 53 — **not** 3 transparent, 26/28 transparent-small, 27 invisible wall, 12 trigger, 22 actor zone. **Source** for the data; **Hypothesis** that the runtime filter matrix is built from these records unchanged. Tree trunks (layer 9) are hit; **Hypothesis:** canopies have no collision, so rays alone cannot find them (hence the box test for trees). |
| `bhkWorld::PickObject`, `FindCollidableRef`, world scale, read lock | As wave 1 (`fixed-camera-spike.md` binary audit): slot 0x33 of vtable ID 238918 (`1A7CDA8`) re-read as `104BE80`; `FindCollidableRef` ID 26003 → `404740` (resolves; used in shipped thin occluder and Selection). |

**Wave 1's in-game probe data agree with the box model of the stable.** At the stable pose the probe's rays hit `0627E3`/`0627E4` at heading 45, `0627E2`/`0627E3`/`0627E4` at 135 and 315, `0627E2`/`0627E3` at 225, plus a short wall `0627E7` and hay bales `0007807B`/`FE02081F` (`session-p55/routes/runs/p55-a-open-1/collected/probe-slice.jsonl`, samples `a-h0`..`a-l4`). All three are `WRStableStr01` (`0627E1`, OBND x −215..215, y −98..98, z −9..422) placed 191.3 units apart along their local y (ESM DATA: rot z 0.0873; origins `(18841.8,−10813.9)`, `(18825.1,−11005.2)`, `(18808.4,−11196.4)`). The stable pose is exactly the origin of `0627E3`. Crossing the 19 exterior targets with the three boxes gives the same pairs (scratch `/tmp/p55-ext/dbg.cpp`):

| Heading | Under the player (cover) | Neighbour crossed high (overhang candidate; highest crossing above feet, ground reach) | Neighbour crossed only low |
| --- | --- | --- | --- |
| 45 | 0627E3 | 0627E4 (429, 184) | 0627E2 (81) |
| 135 | 0627E3 | 0627E2 (429, 184) | 0627E4 (88) |
| 225 | 0627E3 | 0627E2 (429, 184) | 0627E4 (82) |
| 315 | 0627E3 | 0627E4 (429, 184) | 0627E2 (88) |

So "hide only what the player stands under" would leave the neighbouring roof segment over the line of sight at every heading. That is why the rule has an overhang branch.

**Sizes (Source: scratch scan of 4,258 placed STAT/MSTT/TREE references in the 3×3 cells around the five fixture areas, OBND × XSCL; `/tmp/p55-ext/sizes.py`).** Statics: median widest span 287, p90 1374. Target pieces: `WRStableStr01` 430×196×431, `MrkMarketStallRoof01` 155×135×138, `MrkFrontWALLintGateRoof01` 875×867×434, `WRWallMainGateHouse01` 480×277×614, dock pieces 512–793 wide and ~510 tall, bridges `MrkTreasuryBridge02` 860×575×276. One-piece structures: `WRHouseStables01` 851×829×576, `Farmhouse05` 903×855×936, `FarmInn01` 1944, `WRWallStr01` 1024×211×1022, `RockCliff01` 1596, `DragonBridge01` 5154×1058×3010, mountain cliffs 2400–12700; `MountainCliffSlope*` and many markers have an all-zero OBND. Trees: median 227, p95 606, max 1025 (`TreePineForestSnowL01`); pines reach 2500 tall with roots to −1187.

## The rule (`iExteriorSeeThrough` 1 or 2)

Every 0.1 s, in eligible frames, in an exterior cell (`scanExterior`, main thread, from `endFrame`):

1. **Cells.** From `TES::gridCells`, only cells whose 4096-unit square comes within 1536 units of the ground-plane box around the camera, the feet and the 128-unit ring (`cellsAround`; at most 3×3, at the wave 1 framing 2×2). The grid is trusted only if it holds the player's parent cell at the slot the engine's own arithmetic gives for the feet (`exteriorCells`); otherwise that scan walks nothing (`gridSkips`), and after 10 such scans in a row FCS-63 is logged once. Cells not attached (`cellState != 7`) are skipped.
2. **Rays** (`castExteriorRays`, world read lock, `L_LOS` with the player's group, as wave 1). From the rendered camera to 19 targets: head +120, chest +80, hip +40, eight points 64 units out at +80, eight points 128 units out at +24 (`exteriorTargets`); each marched past up to 4 hits. For every distinct hit reference the highest hit point is kept (at most 48 references). Then, for at most 12 of them whose highest hit could be an overhang, three **open-below probes**; then the five wave 1 floor probes. At most 19·4 + 12·3 + 5 = 117 picks per scan. Hit pointers are only compared.
3. **Walk** the selected cells (`ForEachReference`, at most 8192 visits), turning hit pointers into live references; in mode 1 the walk stops as soon as every hit pointer is found. Mode 2 also looks at every `TREE` reference whose bound sphere reaches within 128 units of the camera-to-feet segment (`nearFan`).
4. **Kinds** (`extKindOf`): plain `REFR` with base Static or MovableStatic = structure; Tree = tree; everything else (actors, doors, containers, activators, furniture, flora, items, lights, markers of other types) is never touched. Terrain is not a reference (`rayNoRef`).
5. **Classify** (`classifyExterior`), in this order:
   - a structure that no ray hit is not a candidate (silent);
   - no usable OBND → skip `no-bounds`;
   - neither a ray hit nor (trees) cover or canopy → silent;
   - a floor probe hit it → skip `floor-contact` (the dock, bridge deck, rock or step the player stands on);
   - **large** → skip `large`: a structure whose widest local span ≥ `fExteriorMaxSpan` (1024), or ≥ 1536 tall, or ≥ 640 in both spans and ≥ 512 tall (a building shell); a tree ≥ 1536 wide;
   - **cover** → hide (`ext-cover`): the vertical column above the feet, from 160 units (above the head and above clutter such as a hay bale, 148 at most beside the player) up to the camera height, crosses the reference's oriented box (no margin on entry; 48 units while hidden);
   - **overhang** → hide (`ext-overhang`): a ray hit it at least 192 units above the feet within 384 units of them in the ground plane, and the ground under that hit is **open to the player**: one of three horizontal probes at 144 units over the feet, from the player toward the point under the hit (straight and 32 units to either side), ending 48 units short of it, reaches its end without any hit;
   - **canopy** (mode 2, trees) → hide (`ext-canopy`): a camera-to-target segment crosses the tree's box at or above 160 units over the feet (`pierces`);
   - otherwise skip `beside`: a wall, a rock face, the roof of a building the player stands behind (its walls block all three probes), a tree trunk on a ray.
6. **Dwell, caps, hide, restore**: wave 1's table and machinery. 0.2 s to enter, 0.6 s to leave; at most `iExteriorMaxHidden` (16, max 32) tracked at once, nearest kept (a strictly nearer candidate evicts the farthest, cause `evicted`); at most `iHideMaxPerFrame` new hides per scan, nearest first; a root already app-culled is never taken; `SetAppCulled(true)` on the 3D root; a hidden piece that becomes a floor-probe hit is restored in that scan (`underfoot`); one whose handle died or whose root changed is restored (`unload`). Exit thresholds are looser while hidden (cover +48, overhang clear −24 and reach +64).

### The bound

At any moment the number of hidden exterior references is at most `iExteriorMaxHidden` (default 16, hard limit 32) and at most the number of tracked slots. Each one is either (a) a structure hit this scan by a ray from the camera to a point within 128 units of the player, whose box lies over the player's head or whose hit lies 192+ units above the feet within 384 units and over ground the player can reach horizontally, or (b) in mode 2, a tree whose box covers the player or is crossed above the head by one of those 19 segments. Nothing is selected by height or radius alone, nothing large, nothing the player stands on, no terrain. A hidden reference that stops qualifying is restored after 0.6 s.

Per-route expectations below use this bound plus the data predictions.

### Roofs of buildings the player is not under

In an exterior the player is never inside a building (interiors are other cells). Whole building shells are one reference and fail the size test. Where a building is assembled from pieces (Markarth), a roof piece hit above the head is hidden only if the probes reach the ground under it, which the building's own walls prevent. `MrkMillRoof01` (roof only, z 136..422) behind its walls is the portable test case. **Hypothesis:** in game the walls' collision blocks the 144-unit probes as it blocks the box model; buildings with open ground floors (arcades, porches) will open up, which is the intended look.

### Trees: decision

Trees are **supported, opt-in (mode 2)**, and **off in mode 1**. Reason: app-culling a tree root is honoured by the same cull test as any static (binary, above); distant LOD is a separate object. But canopies are very likely without collision (**Hypothesis**), so trees are taken by box geometry rather than by rays, which is a weaker bound (a pine's box is about 950 wide and 2500 tall, so a tree whose trunk is up to ~475 units from the sightline counts). Mode 2 is therefore a separate look for the owner to judge, not the default.

## Traces (`iHideTrace=1`, shared 8000-line budget)

- `FixedHide … event=ext-active cell= mode= maxHidden= maxSpan= hidden=` once per exterior cell entry (instead of the FCS-31 `inactive` line).
- `event=hide|restore … reason=ext-cover|ext-overhang|ext-canopy cause=… cutZ=… feetZ=… model=` through the wave 1 `traceHide`; outdoors `cutZ` is where the cover column starts (feet + 160).
- `event=skip … reason=ext cause=kind|trees-off|no-bounds|floor-contact|large|beside minZ maxZ spanX spanY feetZ model` (32-entry de-duplication shared with wave 1).
- `event=ext-summary` each second while scanning outdoors: `cell mode hidden tracked maxHidden scans cells visited rays rayHits rayNoRef rayFailed rayCap matched unmatched probed open probeCap trees treeTests hides restores extCap gridSkips visitCap noWorld skipKind skipTreesOff skipNoBounds skipFloorContact skipLarge skipBeside feetZ eyeZ exits=<13 causes in Exit order: disable,f8,first-person,menu,load,cell,unload,death,special,hook-silent,expired,underfoot,evicted>`. `unmatched` counts hit references not found in the walked cells (persistent references live in the worldspace's persistent cell, which is not walked): they are never hidden.

## INI (`[FixedCameraSpike]`, restart to apply)

| Key | Default | Range / meaning |
| --- | --- | --- |
| `iExteriorSeeThrough` | 0 | 0 off (wave 1 behaviour outdoors), 1 structures (cover, overhang), 2 also trees |
| `fExteriorMaxSpan` | 1024 | 512–2048: widest local span of a structure that is still a candidate |
| `iExteriorMaxHidden` | 16 | 1–32 exterior references hidden at once |

Independent of `iSeeThroughMode`; uses `iHideMaxPerFrame` and `iHideTrace`.

## Refusal codes

| Code | Site | Effect |
| --- | --- | --- |
| FCS-62 | an exterior value out of range | safe value applied |
| FCS-63 | the grid is unreadable, does not hold the player's cell at its computed slot, or the fan spans more than 3×3 cells, for 10 scans in a row | those scans walk nothing; hidden references expire |
| FCS-64 | `bhkWorld` slot 0x33 is not `104BE80` | no exterior rays; nothing hidden outdoors |

**Range collision (for the orchestrator).** The brief assigns ext FCS-60..69, but `744609f` already uses FCS-60 (`restoreConflict`) and FCS-61 (`offMain`), and the base refusal mask is one 64-bit word (`1 << code`), so codes 64+ cannot go into it. The ext codes are 62–64 with their own mask (`ExtRefusal`, bit = code − 60). The same problem applies to the other packages: clip FCS-40..49 overlaps `snapMapped` 40 / `snapUnbound` 41, light FCS-50..59 overlaps `traceWriter` 50.

## Tests (run here, g++ 15.2.0, exactly as CI's `camera-portable` job, `.github/workflows/build.yml:272-281`)

```
for test in modules/camera/tests/*_test.cpp; do name=$(basename "$test" .cpp)
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$RUNNER_TEMP/$name"
  "$RUNNER_TEMP/$name"; echo "$name PASS"; done
python3 -B -I modules/camera/tests/lock_refresh_wiring_test.py
```

| Check | Result |
| --- | --- |
| 15 portable tests (14 existing + `fixed_spike_ext_policy_test`) | **PASS**, all exit 0 |
| Wiring audit | **PASS**, six lines; the new one: `PASS ext wiring: off equals wave 1 (FCS-31 branch); in-gate INI; ray-hit structures and fan-bounded trees only; shared table, caps and restores; grid trusted by the player's cell; own FCS-62..69 codes, one site each` |
| Ext policy header and its test under `-Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` | **PASS**, 0 warnings |
| `fixed_spike.h` + `fixed_spike_ext.h` syntax-only against scratch stand-ins (wave 1's stub extended with `TES`, `GridArray`/`GridCellArray` and `IsAttached` copied from the pinned headers), same warning set | **PASS**, 0 warnings. Checks my syntax, types and conversions only; it does **not** prove the real CommonLib or MSVC accept it |
| Windows `/W4 /WX` build | **NOT_RUN** |

`fixed_spike_ext_policy_test` covers: settings bounds; mode 0 never scans and the wave 1 `hideScanned` is unchanged; reason names; refusal bits; box validity, world↔local, extents, slab test (through, miss, grow, parallel, inside, rotated); cover at the real stable geometry (under `0627E3` only, exit grow, never the short wall, a hay bale, a wall beside, or anything above the camera); the large test on 15 ESM bases; overhang thresholds and the three probes; classification order (silent, floor, large, cover, overhang only when open, `beside` for a roof behind walls, trees off/on, canopy, trunk, exit hysteresis); the 19 targets and the fan bound; position-to-cell and grid slots (including overflow); the summary line fits a trace row with ten-digit counters.

## Game rows — all NOT_RUN

| # | Row | Decides |
| --- | --- | --- |
| X1 | Off: `iExteriorSeeThrough=0` with `iSeeThroughMode=1` at the stable — one FCS-31 `inactive` line per exterior entry, no `ext-` line, nothing hidden | off = wave 1 |
| X2 | Install: one `exterior see-through mode=1` note, no FCS-62/63/64; `ext-active` once per cell entry | guards on the live image |
| X3 | Stable, four headings: the player visible in each screenshot; `hidden` 1–3, never more than 4 | the target case (D043 look) |
| X4 | Markarth gate and street: arches and gate roof over the player hidden, building fronts beside the player kept | city exteriors, rule 2 |
| X5 | Bridge, pier, cave mouth: large structures skipped with `skip … cause=large`; the deck the player stands on never hidden | row 14 |
| X6 | Forest, mode 1 then mode 2: mode 1 hides no tree (`skipTreesOff` only); mode 2 hides trees within the cap | trees |
| X7 | Restore: leaving the cell, console load, F8, F6, menu, quickload — `hidden=0` after each, the matching `exits` counter up, then re-hidden after the dwell | presentation only |
| X8 | Frame cost: forest pose, mode 0 vs 2, traces off | Phase 4 budget |
| X9 | Side effects: shadows of hidden roofs, LOD billboards where a tree was, NPC pathing under a hidden stable roof | unverified engine behaviour |

## Route plan (poses are Source; the expected hides are box-model predictions from the ESM, **Hypothesis** until run)

Common: wave 1 candidate stack and harness; INI `bEnabled=1 iRigTrace=1 iHideTrace=1 iSeeThroughMode=0 iRoomSeedMode=0`, `iExteriorSeeThrough` as stated; `tgm`; collect `SkyCRPG_Camera.log`, `SkyCRPG_Camera_FixedSpike.log`, the probe slice. First assertions in every route: `FixedCameraSpike exterior see-through mode=N`, no `FCS-62`, `FCS-63`, `FCS-64` (any of them makes the affected rows BLOCKED). On **every** `ext-summary` line: `hidden <= maxHidden`, `tracked <= maxHidden`, `rayFailed = 0` (else note the pick budget), `conflicts` absent from the base summary. Probe view samples at each screenshot (player points, hits). **Teleports:** wave 1 lost a run to an intermediate `setpos` spot; set z first to the target z + 256, then x, then y, then z exactly, wait 3 s, and assert the probe's player position within 32 units of the target before any sample (else that row is NOT_RUN for a fixture defect).

### e1. Whiterun stable (G3/X1/X3/X7) — about 12 min

Pose: `coc WhiterunExterior01`, then the stable pose (18825.100, −11005.162, −4580.337), `setangle z 5` (**Source:** `p5-final-exterior`; recorded feet (18824.98, −11005.16, −4591.27) in `p55-a-open-1` sample `a-h0`).

1. Mode 0 control (separate launch, `iSeeThroughMode=1`): screenshot `x-off-h0`; exactly one `inactive cause=exterior code=FCS-31` line, no `ext-` line, no `event=hide` (X1). Expected to match wave 1 `a-h0` (roof only).
2. Mode 1: wait 3 s; screenshots at headings 45, 135, 225, 315 (`DOT` ×3, 1.5 s apart, then `F7`), each after 1.5 s: `x-st-h0..h3`. Assert per heading: a `hide … reason=ext-cover` line for `000627E3`; one `ext-overhang` hide for the neighbour in the table above (`000627E4` at 45 and 315, `000627E2` at 135 and 225) unless all three probes were blocked (then a `skip … cause=beside` line for it and the screenshot decides); **after the 0.2 s dwell `hidden` between 1 and 3 on every summary line at this pose, never above 4**; no hide of `0627E7`, `07807B`, `FE02081F` (short wall and hay bales: below the 160 column and the 192 overhang); probe player points mostly clear where they were 5/5 blocked in wave 1 (diagnostic only).
3. Open-ground control: hold `W` 6 s as `p55-a-open-1` did (**Source** for the resulting pose: sample `a-open-h0` at (18045.9, −10626.5, −4603.5)); assert `hidden=0` there within 1 s after the dwell (expected skips: `WRHouseStables01` `large`, its box covers that spot). Walk back with `S` 6 s; stable hides return.
4. Restores (X7), each followed by 3 s and a summary; assert `hidden=0` right after, the named `exits` counter (by index) up, and the stable hides back after the step:
   - leave the cell: `setpos z −4324`, `setpos x 22921`, `setpos y −11005`, `setpos z −4580` (cell [5,−3]; the destination is **Hypothesis**, used only to change cell; asserted by `exits[cell]` up and the probe cell id ≠ `0000961B`), then the stable pose again;
   - console load: `coc WhiterunBanneredMare`, 5 s, then `coc WhiterunExterior01` and the stable pose: `exits[load]` (or `cell`) up; real door activation stays NOT_RUN (wave 1: the door is not offered from the fixed view);
   - F8 off 2 s, on: `exits[f8]`;
   - F6 to first person and back: `exits[disable]` or `exits[first-person]` ("feature off");
   - Tab menu 2 s: `exits[menu]`;
   - quicksave, `DOT`, quickload: `exits[load]`, `hidden=0` until the dwell, then the hides of the loaded heading.
   Every restore appears as `event=restore` lines whose count equals the preceding `hidden`; no `FCS-60`.

### e2. Markarth: gate and street (X4) — about 10 min

Pose A (**Source:** catalogue `r12-primary-p3`, XTEL of `MarkarthMainGateExterior` 0001C389 → 0001C38B): `cow MarkarthWorld -43 1`, then (−173399.531, 5407.912, −3347.921), yaw 266.46. Box model: the pose is under `MrkFrontWALLintGate01` (0BACE4, 704×381×690, not large) and `MrkFrontWALLintGateRoof01` (0BACE6, 875×867×434, not large); large neighbours `MrkFrontWALLintMts03`, `MrkFrontWALLintBldg01a/b`, `02a/b`, `MrkVendorSq01/02` must only ever appear as `skip … cause=large`. Four headings, screenshots `x-mk-gate-h0..h3`; **assert `hidden <= 4`**, hides only from the two gate pieces or overhangs whose probes were open, and every hide has a model.

Pose B (**Source:** `r12-primary-p1`, XTEL 000793A4 → `MarkarthSilverBloodInnDoorRef`): (−174538.469, 4971.030, −3306.261), yaw 39.27. Box model: the pose is under `MrkSilverfishInnPart01` (0B793A, 448×385×663, not large), which is part of the inn's front; **owner review item**: if it is hidden the inn front opens around the door (`ext-cover`), which may be the wrong look for rule 2; the trace shows which. Large: `MarkarthCliff02`, `MrkTempleCrag01`. Assert `hidden <= 4`. Then walk `W` 4 s and `S` 4 s (the street; terraces and the vendor-square bridge `MrkVendorSqBridge01` 0B7954 lie 406 units away, a Hypothesis for what the walk reaches) and record hides and restores.

### e3. Bridge, pier, cave mouth (X5) — about 10 min

- **Dragon Bridge** (**Source:** `r14-primary-p2`, heading marker 000DD62E): `cow Tamriel -25 19`, pose (−100852.789, 81007.234, −12576.031), yaw 327.55. `DragonBridge01` (0E37A8) is 5154 wide: whenever a ray hits it, `skip … cause=large`, never a hide. The catalogue records that neither row 14 pose is under the bridge; **no Source pose under the bridge exists**, so "bridge overhead" is NOT_RUN until a reviewed pose is authored (P5.5-01). Expected `hidden=0`.
- **Four Shields exit** (**Source:** `r14-primary-p1`): `cow Tamriel -25 20`, (−100896.203, 83853.023, −11926.207), yaw 298.6: `FarmInn01` covers the door spot in the box model and is large → `skip … large` only; expected `hidden=0`.
- **Solitude pier** (**Source:** catalogue supplement `pier-p1`, XTEL 000A17A1): `cow Tamriel -17 23`, (−66260.906, 94442.180, −13656.059), yaw 253.5. The deck under the player must never be hidden: dock references hit by rays appear as `skip … cause=floor-contact` if a floor probe hits them; `ShipLargeDaintySload01` is large. Expected `hidden <= 2`. A walk under the pier is not a Source pose (catalogue: "a reviewed walk under the pier is a derived route") — NOT_RUN.
- **Cave mouth** (**Source:** `r14-backup-p1` = `r13-primary-p1`): `cow Tamriel 2 -15`, (8849.191, −58119.750, 1446.459), yaw 321.0. `RockCliffMineEntrance02Dirt02` (062836, 1776 wide) covers the spot and is large: the rock overhang of this mouth is **kept** by design (one large cliff piece). Expected `hidden=0`; screenshot records whether the player is visible anyway.

### e4. Forest (X6, X8) — about 12 min

Pose (**Source:** `r13-primary-p2`, XTEL 000B6F24): `cow Tamriel 3 -14`, (16197.991, −53979.906, 1348.916), yaw 344.0. Box model: `TreePineShrub02` 06A49E and 06A49D cover the pose; `RockCaveEntrance01`, `RockCliff01` are large.

- Mode 1: four headings; trees appear only as `skip … cause=trees-off`; expected `hidden=0`.
- Mode 2 (restart): four headings; expected hides `ext-cover` for the two shrub pines and `ext-canopy` for pines crossing the sightline; **`hidden <= 16` (the cap); `extCap` recorded**; screenshots judge whether the look is acceptable or hides too much (owner).
- Cost (X8): traces off, 60 s standing and 60 s with a `DOT` every 1.5 s, mode 0 then mode 2, the Phase 4 capture as in wave 1 route f. The 60 fps cap masks small costs (wave 1 lesson).

## What could go wrong in the first game run

1. **Not compiled on Windows.** New engine surface: `RE::TES::GetSingleton`, `TES::gridCells/currentGridX/currentGridY/interiorCell`, `GridCellArray::GetCell/length/cells`, `TESObjectCELL::IsAttached`, `FormType::Tree`. All declared in the pin (`include/RE/T/TES.h:77,211-222`, `include/RE/G/GridCellArray.h:26-34`, `include/RE/T/TESObjectCELL.h:212,254`), reached through `RE/Skyrim.h`.
2. **Overhang probes blocked by clutter or posts** (all three) leave the neighbouring stable segment drawn at that heading: the player partly hidden by roof again. Trace: `skip … beside` for 0627E2/E4, `open` low against `probed`.
3. **The inn front at Markarth** (`MrkSilverfishInnPart01`) or similar façade pieces whose box spans the space in front of a door can be taken as `ext-cover`: an open façade. The rule cannot tell a façade overhang from a porch roof by box alone.
4. **Box over-approximation**: a piece whose OBND is much larger than its mesh (an L-shaped piece, a tilted rock) can count as covering the player where its mesh is not overhead. Tilted references are handled by the full rotation, but the OBND box itself may be loose.
5. **Trees (mode 2) may hide more than the canopy actually blocks** (box ~950 wide for a pine); the cap (16) bounds it.
6. **Grid self-check at cell borders.** A one-frame mismatch between the player's parent cell and the position is only counted; if the parent cell lags for more than 1 s (10 scans) FCS-63 appears and the exterior rule pauses. A persistent reference's base cell is not walked: such hits are `unmatched` and never hidden.
7. **Cost.** Up to 117 picks per 0.1 s scan, and a walk of up to 4 cells; mode 2 dereferences every reference in those cells. Unmeasured (X8).
8. **Shadows and LOD.** A hidden roof's shadow may remain or vanish; a hidden tree's LOD billboard might reappear (Hypothesis above). Wave 1's open question about engine uses of the app-culled bit (AI line of sight, activation) applies outdoors too.
9. **Restore latency in menus** and **hook-silent grace** as wave 1 (items 7 and 8 there).
10. **The probe instrument** reports collision, not rendering: hidden roofs still block its rays (wave 1 lesson 10). Screenshots and the rendered-visibility check decide.

## Proposed shared-file changes (not edited)

- `docs/STATUS.md`: P5.5-02 wave 2 "ext" source complete on `phase5.5/p55-02-spike-ext` (exterior see-through behind `iExteriorSeeThrough`, default 0); portable tests PASS; Windows build and all game rows NOT_RUN.
- `docs/phases/PHASE_5_5.md`, "Things the request did not list": add that exterior cells share one havok world and the loaded grid holds their references, so exterior see-through is bounded by sightline hits rather than a cell scan; that many large exterior structures (house shells, city walls, cliffs, whole bridges such as Dragon Bridge) are one reference and can only be handled by a per-draw or shader mechanism; and that tree canopies have no collision (Hypothesis).
- Refusal-code ranges in `briefs/wave2-context.md`: FCS-60/61 are taken and the base mask stops at 63; reassign or keep separate masks per package (as done here).
- P5.5-01 fixtures: a reviewed standing pose **under** Dragon Bridge and **under** the Solitude pier are needed for row 14; none exists in the catalogue.

## Open questions for the owner

1. Mode 1 hides the stable segment over the player **and** the neighbouring segment the view crosses. Is losing the whole local roof the look you want, or only the piece directly overhead (then the player is still partly covered at every heading)?
2. Large one-piece structures (house shells, city walls, cave-mouth cliffs, Dragon Bridge) are never hidden by this mechanism. Accept that outdoors those stay blocking (and rely on another mechanism later), or should the size limit be raised for some types?
3. Trees: mode 2 hides whole trees whose box covers or crosses the view. Try it in the first run, or keep exteriors to structures only until the owner has seen mode 1?
4. A façade piece over a door (Markarth inn front) may open up when the player stands in the doorway. Acceptable, or should cover require the piece to be roof-like (bottom above the head)?

## Method (offline, read-only)

`objdump -d -Mintel` ranges of the pinned executable and Python over its PE sections, `.pdata` and the Address Library (`/tmp/p55-ext/bin.py`; ID→RVA, RVA→ID, rel32 callers, rip-relative loads); Skyrim.esm `COLL` records parsed directly (`/tmp/p55-ext/coll.py`); base and reference records from the P5.5-01 scratch database `/tmp/p55-fixtures.sqlite` with `XSCL` read from the masters (`/tmp/p55-ext/sizes.py`, `/tmp/p55-ext/poses.py`). Scratch files are outside the worktree and not part of the change.
