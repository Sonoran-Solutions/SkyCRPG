# P5.5-02 wave 2, package "clip" (DYL-217): per-draw clip see-through, `iSeeThroughMode=2`

2026-11-09. Worktree `p55-spike-clip`, branch `phase5.5/p55-02-spike-clip`, base `744609f9fb459aa1e50688b3ab590bb6cbbac05a`. **Disposable: never merged, nothing here enters the shipped Camera DLL.** Experimental scope of PHASE_5_5 (four official masters, Steam Skyrim SE 1.7.104, SKSE 2.3.1, SmoothCam 1.8.1, TDM 2.3.1).

**Status.** The first pass was committed unchanged as `188f366` and compiled under MSVC `/W4 /WX` on the first attempt (CI run 42756521043, as reported by the orchestrator). The independent binary review of `188f366` returned CHANGES, 0 BLOCKER / 0 MAJOR / 5 MINOR, acceptable to run on a disposable profile; the five findings are fixed in this pass (section "Fix pass" below), uncommitted. The Windows build of the fix pass is **NOT_RUN**. **Every in-game row is NOT_RUN**: nothing was launched. The mode is default-off: with `iSeeThroughMode` absent, 0 or 1, no clip runtime is allocated and no hook is installed. Owner decision of 2026-11-10 (as reported by the orchestrator): the shader route's horizontal cut is the chosen look for one-piece meshes; this build stays as the comparison and fallback and will be run in game for comparison.

Each claim is marked **Source** (file:line, binary RVA, Address Library ID, or the shipped shader archive) or **Hypothesis**. RVAs are relative to image base `0x140000000` of `SkyrimSE.exe`, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; IDs are resolved through `session-p4/stage-camera-rc1/Data/SKSE/Plugins/versionlib-1-7-104-0.bin`, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842` (format 5, ID *n* at file offset `96 + 4n`). Disassembly: `objdump -d -M intel` over the stated ranges. Scratch tools (PE and ID reader, RTTI walker, reference scanner, shader-container scanner, a g++ stand-in harness) stayed outside the worktree.

## Answer in short

A per-draw clip **can** be done on 1.7.104 without a renderer dependency, and it is implemented. It is option (a) of the brief, a per-pass near plane perpendicular to the view direction, placed just in front of the top of the player's head. Option (b), an oblique or horizontal plane, **cannot** be done by matrix or device state on this renderer without corrupting depth (reasons below); a horizontal cut that keeps the whole floor needs the shader route (`session-p55/research/shader-discard.md`). The clip package is **not BLOCKED**: the projection-helper argument is resolved and depth passes are reachable.

How it differs from the Disocclusion approach recorded in DYL-199: no code patch in the pass loop (ID 107642 + 0x28F is not touched). Two `BSShader` vtable slots per shader class bracket each pass, and one guarded entry detour on the per-frame buffer upload records the argument that was unresolved in DYL-199.

## Fix pass after the independent review (five MINOR findings on `188f366`)

| # | Finding | Confirmed how | What was done |
| --- | --- | --- | --- |
| M1 | The owner walk read `+0xF8` on every node; the engine reads it only on fade nodes | 2EED1F..2EED37: `AsFadeNode` (vtable `+0x28`) first, `[fade+0xF8]` only when non-null; the player's first-person 3D and the form-type 0x2A case as the reviewer described (section 5) | **The engine function is called** (ID 19750), not mirrored. The adapter now reads no scene-graph field to find an owner. Why the call rather than a mirror: a mirror would put two more vtable slots and a player field into this code as assumptions; the function is the definition of "owner" the engine itself uses (73 callers). Thread: main thread only, as before (`consider` returns before it otherwise). Cost: once per candidate pass (after the bound tests), a walk of the parent chain with one trivial virtual call per node; no lock, no allocation. Guards: ID to RVA (FCS-44 check 6), first 16 bytes and live unwind extent (FCS-45 checks 1 and 2). The floor box is taken from the owner's own 3D root (`Get3D`, as the wave 1 hide does); an owner without 3D counts as no owner. |
| M2 | The entry detour was written whenever the first camera frame came, with no regard to a frame being drawn on another thread | The main-scene frame is on the main thread and begins after the camera update (36564 → 40438 at 6588DE; frame 36555 called at 659319, gate raised at 6565F7). IDs 39039 and 90259 call frame begin and end too (6E4D11/6E4DD4, 11B226C/11B2287); their threads were not established | The install moved from `frameStart` to the first **eligible** frame (`publish`), which excludes loads and the menu state, and the entry is written only when the gate byte reads 0, read immediately before the write; otherwise it stays pending and the next eligible frame tries again (one `clip install deferred` line). Residual risk recorded under "Install and guards". The threads of 39039 and 90259 remain **unverified**. |
| M3 | The cap counted passes, so it could clip a geometry's colour pass and not its depth pass | `Budget::take` was per pass | The cap now counts geometries and the decision holds for the frame (`Admission`): a geometry admitted once is clipped in all its later passes of the frame; once the cap is reached a geometry not yet admitted is refused in all of them. Table of 8,192 slots stamped per frame, no clearing. |
| M4 | The route rule failed on any FCS-42..49 | FCS-47 and FCS-49 (notably check 6) have benign triggers | Route plan and game rows corrected: fail on FCS-43..46 and FCS-48, report FCS-47 and FCS-49 with counts and check numbers. No code change. |
| M5 | Wrong row numbers in the per-frame buffer table | My own operand-level parse reproduces the reviewer's numbers; the old scan had matched `dcl_constantbuffer` sizes | Section 3 rewritten from the operand-level parse. The policy's scanner (`readsViewProjection`) had the same weakness in principle (a declaration of 8 to 11 rows would have counted as a read of that row; no vanilla shader has one) and missed reads through extended operands; it now walks instructions and decodes operands, skips declarations and custom data, and treats anything unparsable as "does not read". Its count over the archive is unchanged: 1,012 of 3,390. |

Nothing in the review was disputed.

## What was built

| File | Content |
| --- | --- |
| `modules/camera/include/fixed_spike_clip_policy.h` (new) | Portable policy, no engine headers: audited IDs, RVAs, offsets and guard bytes; clip settings; the published snapshot; the plan (plane to depth interval, the four replacement matrix columns); the exact viewport depth range for any engine range and bias; selection rules; the vertex-shader scan; per-frame cap; trace format; refusal codes FCS-42..49. |
| `modules/camera/src/fixed_spike_clip.h` (new) | Native adapter included by `fixed_spike.h`: runtime, the three hooks, apply and restore for one pass, owner lookup, guards, gateway, install on the main thread, publish, trace. |
| `modules/camera/src/fixed_spike.h` | Eight delimited `wave 2 clip` edits: include, one `LineBudget`, `lifecycle` deactivates, `beginFrame` calls `clip::frameStart`, `endFrame` deactivates (ineligible frame) or publishes (eligible frame) and traces, `install` asks for the install and names the state in the installed line, `configure` takes the clip settings and adds the trace rows. |
| `modules/camera/include/fixed_spike_policy.h` | `seeThroughMode` 2 is accepted (two lines). |
| `modules/camera/src/plugin.cpp` | Eight clip keys read inside the existing `[FixedCameraSpike]` gate and passed to `fixed::configure`. |
| `modules/camera/config/SkyCRPG_Camera.ini` | Clip keys at the end of `[FixedCameraSpike]`. |
| `modules/camera/tests/fixed_spike_clip_policy_test.cpp` (new), `tests/fixed_spike_policy_test.cpp`, `tests/lock_refresh_wiring_test.py` | Portable tests; mode 2 in the spike's validation test; a sixth wiring section. |

No build file changed: both new headers are reached through `plugin.cpp` (`modules/camera/CMakeLists.txt:1`).

**Off equals 744609f.** `clip::Runtime` is allocated only by `clip::configure`, called only from `fixed::configure` when the validated mode is 2 (`fixed_spike.h`, `if (clip::clipMode(r.config.seeThroughMode))`). Every other clip entry point returns at once on a null runtime (`install`, `deactivate`, `frameStart`, `publish`, `trace`, `tracing`, `status`; wiring audit). With the mode at 0 or 1 the only observable differences from the base are the text `clip=off` in the spike's installed log line and the eight INI reads. In mode 2 the wave 1 hide does not run (`hideScanned(2, …)` is false, unchanged), so nothing is app-culled.

## Binary audit

### 1. How a draw is submitted, and what the pass has at that point

| Fact | Evidence |
| --- | --- |
| Pass loop: ID 107642, RVA 155FF40..15602A8. For each pass of a group it calls the per-pass renderer at 15601CF (`E8`, target 1560340) and follows `pass+0x30`. | Source: disassembly. Agrees with DYL-199 (the published AE call-site offset 0x28F is a call on this build). |
| Per-pass renderer: ID 107644, RVA 1560340..15606A1, `(BSRenderPass* pass, uint32 technique, bool alphaTest, uint32 flags)`. It has **13** direct callers, not one: 98A5A0, 1520EDC, 15216C2, 1521817, 1539CCF, 1539E51, 155E04D, 155FB2C, 15601CF, 156158D, 1561A9E, 1564FD7, 1565108. | Source: every `E8` in `.text` whose target is 1560340. A hook at 15601CF alone misses the other twelve paths. |
| `BSRenderPass`: `+0x00` shader, `+0x08` shader property, `+0x10` geometry, `+0x18` pass enum, `+0x1E` LOD byte, `+0x30` next. | Source: 1560374..1560377 (`[rcx]`, `[rcx+0x10]`), 15603E2, 1560415, 15622A0, 15601D4. |
| Order inside the per-pass renderer: (1) if the shader or technique changed, `shader->RestoreTechnique` then `shader->SetupTechnique(technique)` (vtable `+0x18`, `+0x10`, cached in globals 36939D8/D4); (2) `shader->SetupMaterial` (`+0x20`) if the material changed; (3) `geometry+0x108 = pass+0x1E`; (4) three branches by `geometry+0x130` (skin instance) and `geometry+0x109 & 8`, each of which calls `shader->SetupGeometry(pass, flags)` (vtable `+0x30`, directly at 15605A7 or through the helper 1562270), draws (155F050 or the skin instance's `+0x128`), and calls `shader->RestoreGeometry(pass, flags)` (vtable `+0x38`). | Source: 1560340..15606A0; helper 1562270..1562310 tail-jumps to `[shader vtable+0x30]`. |
| The helper 1562270 has three more callers outside the per-pass renderer (1560A9A in ID 107658; 1560B46 and 1560C02 in ID 107659); each also ends in `[vtable+0x38]` (1560AB3, tail jump at 1560C76). | Source. Setup and Restore are paired on every path found. Indirect calls of slot 6 from elsewhere cannot be enumerated; the adapter does not rely on pairing (see Faults). |
| Draws: 155F050 switches on `geometry+0x150` and draws through wrappers 77263, 77264, 77265, 77275, 77277, 77278, 77282, 77289; **each** calls the dirty-state flush ID 77386 (RVA 1010DE0) before the D3D draw. | Source: 155F050..155F7B6 call list; `E8` callers of 1010DE0. The viewport change below therefore reaches every draw of the pass. |
| The six hooked functions have **no** direct callers; they are reached only through their vtable slots. | Source: `E8`/`E9` scan for 1549550, 154A900, 15673F0, 15689C0, 1557090, 1557F50. |
| Shader vtables (RTTI: `.?AVBS…Shader@@` type descriptor, complete-object locator with offset 0, vtable): Lighting ID 255053 RVA 1B47420, slot 6 = 1549550 (ID 107300), slot 7 = 154A900 (ID 107301); Utility ID 255302 RVA 1B4AB58, slot 6 = 15673F0 (107762), slot 7 = 15689C0 (107768); Effect ID 255194 RVA 1B48AB0, slot 6 = 1557090 (107527), slot 7 = 1557F50 (107528). Slot 2/3 are SetupTechnique/RestoreTechnique, 4/5 SetupMaterial/RestoreMaterial. | Source: RTTI walk; slot use at 15603AA, 15603CA, 156040B, 15605A7, 15605FC. Agrees with the shader memo's IDs, which were treated as leads and re-derived. |
| `SetupGeometry` of Lighting maps two constant buffers at its start (`context->Map`, vtable `+0x70`, 1549609 and 154964C) and unmaps both before it returns (`+0x78`, 154A84D and 154A865), then binds them. Utility maps at 1567469 and 15674B0 and unmaps at 1567FD5, 1567FEE and 156804A. | Source for the call sites. That nothing is left mapped on every branch is **Hypothesis**; the clip does not depend on it (D3D11 allows several resources to be mapped at once, and the per-frame buffer is a different resource). |
| None of the three `SetupGeometry` functions, nor their direct callees, reads the camera matrices of the render state; they read `posAdjust` only (world matrix builder 15170C0: rotation transposed into rows 0..2, `translate - posAdjust` in row 3, 1.0 at `[3][3]`). | Source: no RIP-relative reference into 20CFFB0..20D01D0 and no `lea` of the state base in 1549550..154A900, 15673F0..1568078, 1557090..1557F50, 1568080, 1568300, 15170C0. So the view-projection reaches the vertex shader only through the per-frame buffer. |

### 2. The projection helper and its argument (unresolved in DYL-199, resolved here)

| Fact | Evidence |
| --- | --- |
| ID 77258, RVA 100AE80 (first `.pdata` chunk 100AE80..100B03C, continued to 100B521) is the upload of the **per-frame constant buffer**. It returns at once if the byte at 3336400 (ID 411490) is 0, reads the camera data of the render state, transposes and inverts, fills 0x2D0 bytes on its stack, and copies them into the buffer at 3331F08 (ID 411384) with `context->Map(…, 4 = WRITE_DISCARD, …)` / `Unmap` on the immediate context 3331F30 (ID 411391). | Source: 100AE80..100B520; `Map` = vtable `+0x70` at 100B440, `Unmap` `+0x78` at 100B50D. |
| `RCX` is never read (overwritten at 100B04F). Every caller passes `&3331F40` (ID 411393). | Source. |
| **`EDX` is a flags word**, saved in `EBX` at 100AE9C and used only for three floats of the buffer: bit 1 (`test bl,2`, 100B327) writes 0.0 instead of 1.0 at buffer `+0x2A4`; bit 2 (`test bl,4`, 100B33C) writes 1.0 instead of 0.0 at `+0x2A8`; bit 7 (sign of `bl`, 100B351) writes 1.0 at `+0x2AC`. | Source. Row 42 of the buffer (`+0x2A0`) is read by every Lighting pixel shader (section 3). The field names used by the community for row 42 (gamma, first person, alpha-test reference, colour select) are **Hypothesis**; the adapter does not depend on them. |
| Where the flags come from: they are the **render flags of the scene pass**. `SetCameraData` (ID 77503, RVA 101C660: `(state, NiCamera*, flags)`) looks up or builds the camera's view data (ID 77520), then ID 77521 (RVA 101E7F0) copies it into the render state and **tail-jumps** into 77258 with `EDX = flags` (101EBD6, 101EC1B). The opaque-geometry function ID 106584 (RVA 151F540, `(BSShaderAccumulator*, flags)`, reached from `BSShaderAccumulator` vfunc 43 at 151EF18) calls 77258 four more times with `flags` or `flags | 4` around its batch groups (151F88C, 151F914, 151FA79, 151FB2A), and ID 36555 calls it three times with 0 (656771, 65686E, 656915). | Source. That makes **eight** entries into 77258: seven `E8` (the DYL-199 list) plus the `E9` at 101EC1B, which a call-only scan misses. |
| The flags of the last upload are **not** always the flags passed to the passes that follow: at 151FB21 the buffer is uploaded with `flags` and the next batch range is rendered with `flags | 4` (151FB3F). | Source. So the argument cannot be taken from the pass; it has to be recorded at the upload. That is what the entry detour is for. |
| Frame gate: 3336400 is set to 1 at frame begin (1009E5C), where the per-frame buffer is bound to vertex and pixel slot 12 (1009E9E, 1009EBB), and cleared in the frame end ID 77246 after `Present` (100A012, 100A036). | Source. |

### 3. Who reads the per-frame buffer (shipped `Skyrim - Shaders.bsa`, `shadersfx\shaders011.fxp`)

**Corrected in the fix pass (review finding M5).** The first version of this table came from a token scan that also matched `dcl_constantbuffer cb12[N]`, whose second index is the declared size, not a row. That put a spurious row one past the last row read into every pixel-shader cell (43 or 45 for Lighting, 45 for Utility, 20 for Effect, Grass, Sky and DistantTree, 43/45 for Water), and it missed reads through extended operands (rows 40 and 41 in vertex shaders). The table below is from an operand-level parse: the SHEX stream is walked instruction by instruction (opcode and length from each opcode token, custom-data blocks by their own length), declarations are kept apart, and every operand is decoded (component count, extended tokens, immediates, each index by its representation, relative indices recursively). All 16,044 blobs parse without error. It agrees with the reviewer's parse in every cell.

Buffer layout (16-byte rows; Source: order of the stores in 100AEA4..100B430 and the 0x2D0-byte copy at 100B46C..100B4F5): rows 0-3 view, 4-7 projection, 8-11 view-projection, 12-15 unjittered view-projection, 16-19 previous unjittered view-projection, 20-23 inverse unjittered projection, 24-27 unjittered projection, 28-31 inverse view, 32-35 inverse view-projection, 36-39 inverse projection, 40 and 41 the current and previous position adjustments, 42 the three flag floats (with one more value in its first component), 43 and 44 further parameters. **The upload writes rows 0-44 and no shader reads a row beyond 44.** Which matrix each row group holds follows from the state offsets read (section 4); the names of rows 42-44 used by the community are Hypothesis and nothing here depends on them.

| Class (in load order) | Vertex shaders | reading all of rows 8-11 | other cb12 rows read by vertex shaders | Pixel shaders | cb12 rows read by pixel shaders |
| --- | --- | --- | --- | --- | --- |
| BloodSplatter | 2 | 0 | none | 2 | none |
| DistantTree | 4 | 0 | none | 4 | 12, 13, 15, 16, 17, 19 (2) |
| Grass | 18 | 0 | none | 18 | 12, 13, 15, 16, 17, 19 (14) |
| Particle | 6 | 0 | none | 6 | none |
| Sky | 9 | 0 | none | 9 | 12, 13, 15, 16, 17, 19 (9) |
| Effect | 606 | 599 (the other 7 read 8, 9 and 11 but not 10) | 0-3 (132), 40 (274), 41 (129) | 3,216 | 12, 13, 15, 16, 17, 19 (1,543) |
| Lighting | 127 | **127** | 0-2 (127), 40 and 41 (49) | 6,924 | 12, 13, 15, 16, 17, 19 and 42 (all 6,924); 43 and 44 (4,600) |
| Utility | 339 | **286** (the other 53 read no cb12 row) | 0-2 (48), 40 (141) | 177 | 32-35 and 44 (46) |
| Water | 2,172 | 0 | none | 2,172 | 42 (2,104); 43 and 44 (1,520); 12, 13, 15, 16, 17, 19 (4) |

Declared sizes of cb12, for reference (the source of the old artefact): vertex shaders 12, 41 or 42 rows; pixel shaders 20, 43 or 45 rows. No shader indexes cb12 with a relative (computed) index. 3,390 vertex shaders in the archive (the nine classes plus image-space and others), 1,012 of them reading all of rows 8-11 = 127 + 286 + 599.

Source: scratch operand-level parser; the scanner in the policy header (`readsViewProjection`, rewritten in the fix pass to walk instructions and decode operands the same way) run over the archive gives the same 1,012 of 3,390. Consequences:

- Every Lighting vertex shader takes its view-projection from rows 8-11 of the per-frame buffer; row 10 is the clip-z row. Changing that row and nothing else in the pass moves the near plane.
- No Lighting pixel shader reads a z row (6, 10, 14, 18, 26) or any inverse matrix (rows 20-23, 28-39), so a clipped Lighting pass shades exactly as before. Rows 12, 13, 15 and 16, 17, 19 are the x, y and w rows of the unjittered matrices (motion vectors); the clip changes only z rows.
- 46 Utility pixel shaders read the inverse view-projection (rows 32-35). They belong to draws that reconstruct position from the depth buffer; the selection rules (an owning reference of an architecture or object kind, a bound in front of the plane) are what keeps the clip off such draws, not the shader check. **Hypothesis** that no such draw is owned by a placed reference.
- 53 Utility and 7 Effect vertex shaders do **not** take clip z from the buffer. A clip applied to a draw using one of them would change its depth range without clipping it. The adapter therefore checks the vertex shader actually bound for the pass (below) and leaves such draws alone.
- Grass, sky, particles, distant trees, water and blood splatter cannot be clipped by this mechanism at all.

### 4. Depth range and depth bias

| Fact | Evidence |
| --- | --- |
| Render state: ID 388819, RVA 20CFC00. `+0x00` dirty flags; `+0x70` D3D11 viewport with `MinDepth +0x80`, `MaxDepth +0x84`; `+0xA0` raster depth-bias mode; `+0x340` vertex declaration; `+0x348` current vertex shader; `+0x358` topology; `+0x35C` position adjustment; `+0x368` previous position adjustment; `+0x380` camera data (0x250 bytes): `+0x20` view direction, `+0x30` view, `+0x70` projection, `+0xB0` view-projection, `+0x130` unjittered view-projection, `+0x170` previous unjittered view-projection, `+0x1B0` unjittered projection, `+0x240` view depth range (min, max). | Source: 77258 reads 20CFFB0, 20CFFF0, 20D0030, 20D00B0, 20D00F0, 20D0130, 20CFF5C, 20CFF68; 100BCE6/100BD14 (`+0x340`, `+0x358`); 100FEF7 (`+0x348`); the functions below. CommonLib 7.2.0 `RendererShadowState.h` agrees on every offset used; none was taken from it unchecked. |
| The flush ID 77386: if dirty bit `0x40` (depth bias) is set it copies the view depth range into the viewport's `MinDepth`/`MaxDepth` and subtracts `bias[mode]` from `MaxDepth` (101106C..10110D0, table at 3330200, ID 411359); if bit `0x2` is set it calls `RSSetViewports(1, state+0x70)` (10110D8..10110F3). | Source. Depth bias on this engine is a reduction of the top of the viewport depth range. |
| Bias table: 12 floats copied at start-up from `.rdata` 1A76790 by 102B680: 0, 1.5e-5, 0, 0, 0, 0, 2.6e-5, 1.5e-5, 2.8e-5, 1.7e-5, 3.0e-5, 1.9e-5. | Source. |
| The depth range is **not** always (0, 1): at the start of opaque rendering (15377F0, called from 151F566 when `flags & 0xA == 0`) the engine saves the range and sets max to `fminf(max, 0.999998)` through ID 77511 (RVA 101D5D0, which rewrites the camera data and tail-jumps to the viewport sync at 1009AB0); 1537950 puts it back at the end (151FB4F). ID 77511 does **not** upload the per-frame buffer. | Source: 15377F0..153793F, 1537950..1537A11, constant 1B436B0 = 0.9999979734, import `fminf`. Two consequences for the adapter: the clip must work for any range, and a check that the camera data still equals the last upload must leave the depth range out. |
| `posAdjust` is the NiCamera's world translation, and the state's view direction is column 0 of its world rotation. | Source: ID 77520 copies `camera+0xA0..0xA8` to entry `+0x260` (101E794..101E7C2) and writes `(rot[0][0], rot[1][0], rot[2][0])` at entry `+0x30` (101E571..101E592); ID 77521 stores entry `+0x260` to 20CFF5C (101EAB8..101EAE0). |
| Matrices are stored for row vectors (clip = point × M): clip z is column 2, clip w is column 3. | Source for the world matrix (15170C0, translation in row 3). For the view-projection it follows from 77258 transposing before upload and the vertex shaders reading rows 8-11 as dot products. The adapter does not trust it: at run time the w column of the view-projection must be a unit vector equal to the state's own view direction, or nothing is clipped (FCS-49 check 7). |
| Depth is 0 at the near plane and grows with distance. | **Hypothesis** (not read from the projection builder 101D400). Checked at run time on the live matrices; reversed depth refuses (FCS-49 check 10). |

### 5. Data used for selection

| Fact | Evidence |
| --- | --- |
| `NiAVObject`: world bound `+0xE4` (centre, radius at `+0xF0`). The adapter no longer reads the parent (`+0x30`) or the owner (`+0xF8`) itself (fix pass, M1). | Source: 19 code sites reading `+0xE4/E8/EC/F0` together. The bound was also used in game by the wave 1 hide. |
| Owner of a scene node: ID 19750, RVA 2EECE0..2EED9E, `TESObjectREFR* (NiAVObject*)`. It loads the player (global 3230778, ID 403521) and the player's first-person 3D (player vfunc 0x378 with `dl=1`; for `PlayerCharacter`, vtable ID 208040, that is `mov rax,[rcx+0x900]; ret` at 747B70). Then, for the node and each parent (`+0x30`): the player's first-person 3D returns the player; otherwise `AsFadeNode` (vtable `+0x28`, 2EED25) and, **only if that is non-null**, the owner at `[fade+0xF8]` (2EED2D). If the owner's base (`[ref+0x40]`) has form type 0x2A (ammunition) it returns the owner found from the parent instead, when there is one (2EED55..2EED74). `AsFadeNode` is `return this` for `BSFadeNode` (14E7290) and the shared null stub 1B4CA0 for `NiAVObject`, `NiNode`, `BSGeometry`, `BSTriShape`. | Source: 2EECE0..2EED9D; RTTI vtables. The function takes no lock and allocates nothing: field reads and those trivial virtual calls only. It has 73 direct callers in the engine. |
| `BSGeometry`: skin instance `+0x130`, type `+0x150`. | Source: 1560420 (branch on `+0x130`), 155F05D. |
| `BSGraphics::VertexShader`: DXBC length at `+0x10`, DXBC bytes at `+0x68`. | Source: loader ID 77348 (100FCAB `mov [rax+0x10],ecx`; reads into `+0x48`, `+0x50`, `+0x68`; `CreateVertexShader(+0x68, size)` at 100FD8A); bind ID 77351 stores the object at state `+0x348`. |

### 6. What cannot be verified offline

- That rendering runs on `Main::threadID`. **Hypothesis**, from the single immediate context every draw wrapper uses. Checked on every hook call; a call elsewhere is left native.
- Whether the main view runs a Utility depth pass before the lighting pass, and whether that pass uses one of the 286 Utility vertex shaders that read the buffer. **Hypothesis** (shader memo; the reviewer could not verify it either). The default hooks Lighting and Utility so that both are clipped if it does; `byClass` in the trace answers it in the first run.
- That camera data `+0x20` is the view direction. Derived here from ID 77520/77521 (section 4); the reviewer could not confirm it. It fails closed: if it is not, every main-view pass is rejected with FCS-49 check 7 and nothing is clipped.
- The threads of IDs 39039 and 90259 (other functions that bracket complete frames). See "Install and guards".
- Whether geometry behind a removed piece has been submitted at all (room and portal culling, occlusion planes). The clip does not change culling.
- How shadow, reflection, cube-map and local-map passes set their camera was not traced pass by pass (the shared scene helper 1515230 calls `SetCameraData` at 1515257: **Source** for that helper only). The clip does not depend on it. A draw is clipped only if its vertex shader takes the view-projection from the per-frame buffer, and that buffer is written by one function only: every other reference to the buffer pointer 3331F08 creates it (1012A6C), binds it to slot 12 (1009E9E, 1009EBB, 1010803 and the call after it) or releases it (1008798, 1012DDA); only ID 77258 maps it (**Source**, reference scan). So for any clipped draw the matrices the adapter judged are the matrices the draw uses, and they must look along the published direction with the anchor at the published depth. A view that positions vertices from other constants (53 Utility variants) is never clipped.

## Why this plane and no other

1. Fixed-function D3D11 clips only against the six frustum planes. User clip planes exist only as `SV_ClipDistance` written by the vertex shader; no vanilla shader writes one, and adding one is the shader route.
2. A matrix can move the near clip (clip z ≥ 0) to any plane. But the depth written is `z/w`, so if the surviving fragments are to keep the depth they would have had, the new clip z must be an affine function of the old `z/w`. The only freedom outside the matrix is the viewport's affine depth range. That gives exactly one family of planes: constant `z/w`, perpendicular to the view direction. An oblique plane (through the head facing the camera at another angle, or the horizontal cut height) changes `z/w` non-affinely: clipped geometry would no longer depth-test correctly against the player and the unclipped scene. Scissor is a screen rectangle and cannot tell near from far.
3. For a plane at view depth `n'` with unmodified depth value `d0`: draw with `z' = (z − d0·w) / (1 − d0)` (only column 2 of the stored matrices changes) and raise the bottom of the viewport depth range from `low` to `low + (high − low)·d0`, where (`low`, `high`) is the range the engine's flush will give this draw: rebuilt from the camera data's range with the top lowered by the bias when the depth-bias dirty bit is set, otherwise what the viewport already holds. Near clipping then removes exactly the fragments nearer than the plane, and every other fragment lands on `low + (high − low)·(z/w)`, the value the unclipped draw writes, for every engine range and every bias mode. x, y and w are untouched, so pixel position, TAA jitter and motion vectors are unchanged. (Portable test `plane`: 1,800 points, both jittered and unjittered matrices, ranges (0,1) and (0,0.999998), three bias values, agreement within 2e-6 in double precision.)

So (a) is implemented, (b) is recorded as not available without shader work, and (c) nothing cheaper was found: the engine has no per-draw near plane of its own.

## Design

### The plane

`anchor = feet + (0, 0, fClipHeight)`; the plane is perpendicular to the view direction at view depth `(anchor − camera)·forward − fClipMargin`. With the wave 1 rig (pitch 55°, camera 761.7 from the feet) and the defaults 128 / 24:

- it clears the top of the head by 24 units of view depth;
- directly above the player it removes everything higher than about 157 units over the feet (128 + 24/0.819);
- on a flat floor it meets the ground (0.819·128 + 24)/0.574 ≈ 225 units from the player on the camera side. Whether that line is inside the frame depends on the field of view and SmoothCam's framing: **Hypothesis**, to be seen in the screenshots;
- behind the player it rises 0.70 units per unit of distance, so far walls and the far floor are behind it.

### One clipped pass

Hooks: slots 6 and 7 of the selected shader classes' vtables, and the entry of ID 77258.

1. **Upload hook** (every engine upload): call the original, then remember its flags argument and copy the camera data and both position adjustments as they were at that upload. This is an "epoch". The plan for an epoch is computed lazily, once.
2. **SetupGeometry hook**: call the original first. Then, if a snapshot is active: evaluate the epoch (is this the published view; plane depth from the state's own matrices; `d0`; the four replacement columns), run the selection, and for a selected pass:
   - check, before any write: frame gate is 1; the state's uploaded matrices and position adjustments still equal the epoch's; the bias mode is inside the table; the depth range the flush will produce (as in point 3 above) is finite, inside [0,1] and not empty; the per-frame cap has room;
   - write column 2 of projection, view-projection and the two unjittered matrices, call the original upload with the recorded flags, and **write the four columns back at once**. The state's matrices differ from the engine's only for the duration of that call;
   - store the raised minimum in the camera data's depth range and in the viewport, set dirty bit `0x2`. The engine's own flush applies it at the draw; if it rebuilds the range there, it takes the bottom from the camera data (ours) and lowers the top by the pass's bias as always.
3. The engine draws the pass.
4. **RestoreGeometry hook**: call the original, then upload again (the state is the engine's, so the buffer is the engine's again), put both depth values back if they are still bit-for-bit what was written, set dirty bit `0x2`.

Nothing is left on any reference, node or material. Between passes the only state that differs from native is nothing at all. There is therefore nothing to restore on F6, F8, menus, loads or death: the next frame is simply drawn without clipped passes.

### Selection (by rule, per pass, on the main thread)

In this order, cheapest first; the first that applies ends it, and each is counted in the trace:

| Rule | Effect |
| --- | --- |
| Not the published view (state view direction, plane depth) | never clipped: shadow maps, reflections, cube maps, local map, UI 3D |
| Skinned geometry | never clipped (actors; also skinned statics, see limits) |
| Bound wholly at or behind the plane | not selected. **This is why the floor under and beyond the player, the far walls of the player's own room and everything behind them stay**: the plane cannot remove a fragment behind it, and such pieces are not even touched. |
| Bound farther than `fClipCorridor` from the camera-to-anchor segment (0 = no corridor) | not selected |
| No owning reference by the engine's own lookup (ID 19750; terrain, sky, water), or the owner has no 3D | never clipped |
| Owner is an actor (form type ActorCharacter: NPCs, creatures, the player including the first-person 3D, everything attached under them, and ammunition stuck in them) | never clipped |
| Owner's base is not architecture or a placed object by the wave 1 `kindOf` (lights and anything else excluded there) | never clipped |
| Owner's box (base OBND through `extentOf` on the owner's 3D root, sphere fallback) wholly at or below `feet + fClipFloorBand` | floor: never clipped, even on the camera side |
| The bound vertex shader does not read rows 8-11 of the per-frame buffer | never clipped |
| `iClipMaxPerFrame` geometries already admitted this frame, and this geometry is not one of them | drawn natively, in **every** pass of that geometry this frame. A geometry admitted once is clipped in all of its later passes of the frame, so its depth pass and colour pass never disagree because of the cap. |

A one-piece cave or room mesh is selected because its bound reaches in front of the plane; only its part in front of the plane is removed, the floor around the player and the far side remain. A near wall is cut along the plane: a stump remains on the camera side, lower the farther it is from the player (it cannot hide the feet: at pitch 55° the line from the camera to the feet is 2.04 units up per unit toward the camera, the plane 0.70).

### Threads

| Thread | What it does | What it may touch |
| --- | --- | --- |
| Main, camera frame service (`beginFrame`/`endFrame` in `serviceCameraCall`) | installs the hooks once; publishes the snapshot (`std::atomic<std::shared_ptr<const View>>`: camera position and direction, anchor, plane depth, corridor, feet height, time) after SmoothCam has written the frame; writes trace rows; logs refusals | everything |
| Main, render hooks | everything in "One clipped pass" | render state, scene graph (read), per-frame buffer |
| Any other thread in a hook | calls the original; sets one atomic flag that makes the main thread forget the last upload; if a snapshot is active, also increments one atomic counter (reported once as FCS-47 by the main thread) | nothing else: no engine state, no log, no trace |
| Any thread, lifecycle events | clears `active` | one atomic |

The hooks are **expected** on the main thread (Hypothesis above); `RE::Main::threadID` is compared on every call. A snapshot older than 0.25 s is not used, so if the camera service stops, clipping stops.

### Install and guards

`clip::install` (kDataLoaded, possibly off the main thread) only sets a flag. All guards and all writes run on the main thread in the camera frame service of the first **eligible** frame (`publish` → `commit`), and the entry of the upload is replaced only while the frame gate (3336400) reads 0. An eligible frame is normal play: the spike's state is normal or target lock, which excludes a load in progress (`plugin.cpp:651`, `c.world` requires `!loading`) and the menu state. The camera update runs in `Main::Update` before the frame begins: ID 36564 calls ID 40438 at 6588DE, which reaches the camera update through 40744 → 40449 → 50784; the frame (ID 36555) is called at 659319 and raises the gate at 6565F7 (**Source**: caller chain by reference scan and those call sites; "before" is by address order inside ID 36564, the branches between the two sites were not followed). So on the main thread the gate is 0 at this point, and a 1 means another thread is inside a frame of its own; then nothing is written, one line `clip install deferred` is logged, and the next eligible frame tries again. **Residual risk (M2):** IDs 39039 (RVA 6E4C20) and 90259 (RVA 11B2050) also bracket complete frames (they call the frame begin ID 77245 and frame end ID 77246) and their threads are not established here; a thread that begins such a frame between the last gate read and the end of the 14-byte write (microseconds, once per session, during normal play) could execute a half-written entry. Not eliminated; reduced from "any time after kDataLoaded" to that window.

| Code | Site | Meaning, and what stays in force |
| --- | --- | --- |
| FCS-42 | `configure` | a clip value was out of range; safe value applied |
| FCS-43 | `commit` | `CommunityShaders.dll` or `d3dcompiler_46e.dll` (ENB; module name is **Hypothesis**) is loaded; not installed |
| FCS-44 `check=N` | `commit` | an ID does not resolve to the audited RVA: 1 state, 2 upload, 3 renderer, 4 gate, 5 bias table, 6 owner lookup, 10-12 Lighting/Utility/Effect vtable; not installed |
| FCS-45 `check=N` | `commit` | an engine function is not the audited one: 1 owner lookup first 16 bytes, 2 its live unwind extent; `N = 10·(class+1) + {1 setup slot, 2 restore slot, 3 setup bytes, 4 restore bytes}` for a shader class: another module owns the slot or patched the function; not installed |
| FCS-46 `check=N` | `commit` | upload entry: 1 first 21 bytes differ, 2 live unwind extent differs, 3 gateway allocation, 4 gateway protection; not installed |
| FCS-47 | `frameStart` | a hook ran off the main thread while a snapshot was active; those calls touched nothing and stay native; the mode continues on the main thread. Off-thread calls while inactive (a loading screen drawn by another thread, if that happens) are not reported |
| FCS-48 `cause=N` | `fault` | a clipped pass was still open at 1 the next SetupGeometry, 2 an engine upload, 3 the next camera frame; it was closed and the mode is off for the rest of the session |
| FCS-49 `check=N` | `rejectPass` | the render state differed from the audit; logged once per check, such passes are native: 5 gate, 6 drift (matrices changed since the upload), 7 layout (w column is not the state's view direction, or non-finite), 8 published (plane depth more than 96 units from the published one), 9 depth range (the range the flush would produce is empty or outside [0,1]), 10 convention (depth not increasing, or the four matrices disagree), 11 bias mode outside the table |

FCS-40 and FCS-41 already belong to the wave 1 snap keys (`fixed_spike.h` `Refusal`), so the clip uses 42-49 of the range the brief assigned. No guard bytes contain a base relocation (checked against `.reloc`). ENB and Community Shaders are absent from `session-p55/stage-camera-p55-spike` and from the game directory (Source: directory listings, 2026-11-09). The entry detour is the same construction as the wave 1 caster gateway: 14 bytes of three whole instructions (`mov [rsp+8],rbx`; `push rbp`; `lea rbp,[rsp-220h]`), none RIP-relative, copied to a read-write block, an absolute jump back to entry + 14, sealed execute-read before use; no trampoline is allocated (the plugin's 32-byte trampoline is spent). No other code branches into entry+1..+13 (Source: reference scan).

### Faults

The adapter never assumes a pass that was opened will be closed by its own RestoreGeometry. If a clip is still open at the next SetupGeometry, at an engine upload or at the next camera frame, it is closed there (buffer re-uploaded, depth range put back) and FCS-48 turns the mode off for the session.

### Settings (`[FixedCameraSpike]`, all read only when `bEnabled=1`)

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `iSeeThroughMode` | 0 | 0-2 | 2 = this mode |
| `fClipHeight` | 128 | 0-512 | anchor above the feet |
| `fClipMargin` | 24 | 0-256 | view depth by which the plane sits in front of the anchor |
| `fClipCorridor` | 384 | 0-4096 | corridor radius; 0 = the whole view |
| `fClipFloorBand` | 24 | 0-128 | references wholly below feet + this are floor |
| `iClipMaxPerFrame` | 512 | 1-4096 | cap on clipped geometries per frame; the decision holds per geometry for the frame |
| `iClipShaders` | 3 | 1-7 | 1 Lighting, 2 Utility, 4 Effect |
| `iClipExterior` | 0 | 0/1 | 1 also clips in exterior cells |
| `iClipTrace` | 0 | 0/1 | 6,000 `FixedClip` rows at about 10 Hz |

### Trace legend (`FixedClip`, one row per 0.1 s window, in `SkyCRPG_Camera_FixedSpike.log`)

`installed`, `active`, `latched` (FCS-48); `eye`, `anchor`, `anchorDepth`, `planeDepth`: the published snapshot (nan while inactive; the view direction is in the `FixedRig` row's heading and pitch); `statePlane`, `d0`, `target`: plane depth from the render state, its depth value and the depth-stencil target index of the last clipped pass in the window (nan / -1 if none); `passes`: passes of hooked classes seen while active; `clipped`; `byClass`: clipped passes by class, Lighting, Utility, Effect (a non-zero Utility count in the main view answers whether a depth pass exists there); `peak`: most geometries admitted in one frame; `cap`: `iClipMaxPerFrame`; `uploads`: engine uploads seen; `biased`: clipped passes drawn with a depth bias; `conflicts`: a depth value was not ours at restore; `offMain`; `micros`: time spent applying and restoring; `skip*`: Skin, Bound, Behind, Out (corridor), Owner (none), Actor, Kind, Floor, Shader, Cap (passes of geometries refused by the cap); `rej*`: Stale, Other (another view), Near (plane nearer than 64), Epoch (no upload known), and the FCS-49 checks Gate, Drift, Layout, Pub, Range, Conv, Bias. Skips and rejects are per pass.

## What this mode cannot do

- **Any other plane.** No horizontal cut, no corridor-shaped hole: the corridor only chooses which pieces are cut by the one plane.
- **The camera-side floor of a one-piece mesh** beyond about 225 units from the player (defaults) is removed with the rest of that mesh. Separate floor-level references stay whole.
- **What its vertex shader positions otherwise**: water, grass, particles, sky, distant trees, blood splatter, and the 53 Utility / 7 Effect variants. Effect-shader meshes are not hooked by default (`iClipShaders=4` adds the 599 that qualify). A fog card, light shaft, cobweb with an effect shader, or a water surface between camera and player stays.
- **Skinned geometry that is not an actor** (banners, some animated statics) stays whole in front of the plane.
- **Lights' own models** are excluded by the wave 1 `kindOf` and stay (a lantern can hang in the cut).
- **Shadows.** Shadow passes are not clipped, so removed geometry still casts. The room stays as dark as it was; that is the light package's subject.
- **Culling.** If the engine did not submit what is behind the removed piece, a void shows. Room seeding (`iRoomSeedMode`) is independent and can be combined.
- **Decals drawn with shaders outside the hooked classes** on a clipped surface are not clipped.
- **Look.** A hard edge; the cut shows the inside of single-sided meshes (back faces are culled, so the cut is open, not capped).

## Tests

Commands are those of the `camera-portable` CI job (`.github/workflows/build.yml:272-281`), run from the worktree with `RUNNER_TEMP` in scratch, g++ 15.2.0, Python 3.14.4:

```sh
for test in modules/camera/tests/*_test.cpp; do
  name=$(basename "$test" .cpp)
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$RUNNER_TEMP/$name"
  "$RUNNER_TEMP/$name"
  echo "$name PASS"
done
python3 -B -I modules/camera/tests/lock_refresh_wiring_test.py
```

| Check | Result |
| --- | --- |
| 15 portable C++ tests (14 existing, `fixed_spike_clip_policy_test` new), rerun after the fix pass | **PASS**, exit 0, every test printed `… PASS` |
| Wiring audit, six sections (five existing, `clip wiring` new), rerun after the fix pass | **PASS**, exit 0 |
| `git diff --check` | **PASS** |
| g++ `-fsyntax-only -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion` of `fixed_spike_clip.h` against scratch stand-ins for the Windows, REL and RE declarations it uses | 0 warnings. This is a parse and type check of the adapter only; it is **not** an MSVC or CommonLib build. |
| Policy scanner (operand-level since the fix pass) over the shipped shader archive | 1,012 of 3,390 vertex shaders, as section 3 |
| Scratch operand-level parser over all 16,044 blobs | 0 parse errors; the table of section 3 |
| Hand mutations of the adapter and policy in a scratch copy: seven in the first pass (columns not written back, install at kDataLoaded, no depth restore, no main-thread check, a refusal that does not stop the apply, vertex-shader check dropped, an RVA changed) and six in the fix pass (gate not read before the entry write, install from `frameStart`, owner read from the node, cap not by geometry, a deferred install that gives up, the owner RVA changed) | each failed the wiring audit |
| Windows `/W4 /WX` build | `188f366` (first pass): green, CI run 42756521043, as reported by the orchestrator. Fix pass: **NOT_RUN** |

`fixed_spike_clip_policy_test` covers: codes in 42-49 and distinct; guard-table consistency; settings validation; the snapshot; the depth-preserving plane (clip side, kept side, ranges, bias, both matrix pairs, projection matrices); the range the flush produces with and without the bias bit, and unusable ranges; every reject (stale, orthographic, mirrored, other axis, tilted, moved along the axis, sideways shift accepted, eye offset carried by the matrix, wrong view direction, non-finite, reversed depth, disagreeing matrices, too near); selection (skinned, bad bound, floor under the player, far wall, beyond, ceiling, near wall, one-piece mesh, corridor on and off, floor line at 225, head clearance, owner rules, floor band); the per-geometry cap (an admitted geometry stays admitted, a late one is refused in every pass, new frame, colliding keys, the largest cap); the DXBC scan (four rows, missing row, other buffer, a declaration's size, custom data, an extended operand, relative indices, immediates, instructions that overrun, truncated and malformed containers); the trace row, including that the longest row fits one 768-byte trace row.

The wiring section asserts, among others: no clip runtime outside mode 2; `install` only sets a flag, `frameStart` never installs, and `commit` is called from `publish` alone (eligible frames); guard order, the gate read immediately before the entry write, a deferred install staying pending without a refusal; the gateway sealed before use; originals stored before a slot is written, restore slot first; each hook calls the engine function exactly once; a pass is considered only after its own SetupGeometry; the owner comes from the engine's lookup and the adapter reads no scene-graph field for it; every refusal precedes the first engine write in `apply`; the cap is by geometry; the columns are written once and written back once around the upload; `finish` uploads before it restores depth; the off-thread path contains two atomic operations and nothing else; one site per FCS-42..49; the audited constants.

## Game rows: all NOT_RUN

| Row | Result |
| --- | --- |
| Hooks install under Proton: exactly one `clip installed` line, no FCS-43..46 | NOT_RUN |
| Clip engages in the main view: `clipped > 0`, `peak` below `cap`, no FCS-48; FCS-47 and FCS-49 reported with their counts and check numbers, not failed | NOT_RUN |
| Embershard Mine, coc arrival, four headings: player and floor visible | NOT_RUN |
| Bleak Falls Barrow corridor (2048,-9216,-2556), heading 225, seed mode 1 | NOT_RUN |
| Bannered Mare hearth: cross beams removed, floor kept | NOT_RUN |
| Lifecycle: F6, F8, menu, load door, death; no FCS-48; frames native while ineligible | NOT_RUN |
| Frame cost in Embershard | NOT_RUN |
| Shadows, fog cards, water, decals with the clip on | NOT_RUN |

## First-run protocol (from the independent review)

Run **only** this first, on the disposable profile: Embershard Mine, coc arrival, heading 45, with `iSeeThroughMode=2`, `iClipTrace=1`, `iRigTrace=1`, `iRoomSeedMode=0`, every other clip key at its default.

In `SkyCRPG_Camera.log`:

- exactly one `FixedCameraSpike clip installed shaders=3 …` line (a `clip install deferred` line before it is acceptable);
- no `code=FCS-43`, `FCS-44`, `FCS-45`, `FCS-46` and no `code=FCS-48`;
- `FCS-47` or `FCS-49` lines, if any, are recorded with their check numbers, not failed by themselves.

In the `FixedClip` rows at the pose:

- `installed=1 active=1 latched=0`;
- `uploads>0`, `passes>0`, `clipped>0`, `peak` below `cap`;
- `statePlane` within about 1 unit of `planeDepth`;
- `d0` roughly 0.95 to 0.995;
- `rejDrift`, `rejLayout`, `rejPub`, `rejRange`, `rejConv`, `rejBias` all 0;
- `conflicts=0`, `offMain=0`;
- `byClass`: the second number (Utility) is non-zero if the main view has a depth pass drawn through a buffer-reading Utility shader.

Reading the picture: a void behind the cut (nothing where the player's surroundings should be) means depth and colour disagree (a depth pass that was not clipped, or geometry the engine never submitted), not a crash. Only after this row looks right are the other routes worth their runs.

## Route plan (for the routes package; nothing here was generated or run)

Build name `clip`. Overrides common to every route: `iSeeThroughMode=2`, `iClipTrace=1`, `iRigTrace=1`; the other clip keys at their defaults unless stated. The wave 1 routes named below write `iSeeThroughMode` themselves, so the override must win or the routes need a `clip` variant.

Checker rule for every route (corrected in the fix pass, M4):

- **FAIL** on `code=FCS-43`, `FCS-44`, `FCS-45` or `FCS-46` (not installed), on `code=FCS-48` or `latched=1` in any `FixedClip` row (a bracket fault turned the mode off), and when no `FixedCameraSpike clip installed` line appears or more than one does. `code=FCS-42` is a route configuration error (an out-of-range clip key) and should fail the route as such.
- **REPORT, do not fail:** `code=FCS-47` with the sum of `offMain` over the rows (a hooked call arrived off the main thread while a snapshot was active; it touched nothing and was drawn natively; a loading screen or movie drawn by another thread while the snapshot had not yet been withdrawn is a benign trigger). `code=FCS-49` with each `check=N` seen and the sum of the matching counter over the rows: 5 `rejGate`, 6 `rejDrift`, 7 `rejLayout`, 8 `rejPub`, 9 `rejRange`, 10 `rejConv`, 11 `rejBias`. Each is a fail-closed skip of single passes (check 6, drift, has a benign trigger: the engine changed camera data after its upload and drew before the next one). `conflicts>0` is reported with its sum.
- Whether a reported check matters is decided by the row's own assertion: at a pose where the row requires `clipped>0`, a reported FCS-49 with `clipped=0` in every row is that row's FAIL through `clipped`, with the check number as the cause.
- A `FixedCameraSpike clip install deferred` line is informational (the install waited for a moment with no frame in progress).

| # | Route (base) | Overrides | Looks and assertions |
| --- | --- | --- | --- |
| 1 | `d-cave-dwemer` cave part: EmbershardMine01 coc arrival, headings 45/135/225/315 | `iRoomSeedMode=0` | Screenshot per heading against `batch-p55-d-cave-dwemer-A2-00/d-cave-h0.png` (dark rock, player not visible). At each pose: `active=1`, `clipped>0`, `peak` below `cap`, `statePlane` within 1 unit of `planeDepth`; `rejDrift`, `rejLayout`, `rejPub`, `rejRange`, `rejConv`, `rejBias` expected 0 and reported with their sums and FCS-49 check numbers if not (rule above). Heading 45 of this route is the first-run protocol row. Owner looks at: player visible, floor around the player intact, the cut line at the bottom of the frame, shadows. |
| 1b | same, whole view | `fClipCorridor=0` | Same assertions; compares the corridor with the whole-view cut. |
| 2 | `c-bfb-seed1`: BleakFallsBarrow01 corridor pose, four headings | `iRoomSeedMode=1` | Screenshot `c-bfb-h2` (heading 225) against `batch-p55-c-both-seed1-hide-A2-00/c-bfb-h2.png` (rock and roots). `clipped>0` at heading 225; room trace unchanged in kind from wave 1 (claims present). |
| 3 | `b-mare` hearth part: WhiterunBanneredMare (-117.110,-640,68), four headings | none | Screenshot `b-hearth-h0` against wave 1 hide off (`batch-p55-b-mare-A-00/b-hearth-h0.png`) and hide on (`batch-p55-d-mare-A2-00/d-bm-ground-h0.png`): are the large cross beams gone, is the floor whole (`skipFloor>0` expected for floor-level references), are patrons whole (`skipActor>0`). |
| 4 | `e-lifecycle` and `e-door` | none | At each exit (F6 first person, F8, menu, load door, death and reload): the `FixedClip` rows written while ineligible have `active=0 clipped=0`; after return `active=1` and `clipped>0` again at the same pose; no FCS-48 line in the whole log; screenshots while ineligible show the unclipped scene. |
| 5 | new `f-cave-clip` and `f-cave-base`: Embershard arrival, traces off, the f capture procedure | clip route as 1 with `iClipTrace=0`; base with `iSeeThroughMode=0` | p95 frame time against the base at the same pose and heading; one extra short run with `iClipTrace=1` to read `clipped`, `peak` and `micros` per window. The 60 fps cap hides small deltas (wave 1 lesson); `micros` is the direct number. |
| 6 | optional, after 1-3: Embershard with `iClipShaders=7` | | Does adding Effect remove fog cards and beams; any visible depth error. |

## What can go wrong in the first game run, and how it shows

| What | How it shows |
| --- | --- |
| A guard fails at install | one FCS-43..46 line with its check number; the installed line says `clip=pending` and no later `clip installed` line appears; rendering native |
| The install never finds a moment with no frame in progress | one `clip install deferred` line, never a `clip installed` line, `installed=0` in every row; rendering native |
| The state's view direction never matches the published one (camera root versus NiCamera) | `clipped=0`, `rejOther` ≈ `passes`, no refusal line. This case has no log line; read the trace. |
| Matrices change between upload and pass, or depth is reversed, or the layout is not as audited | FCS-49 with check 6, 10 or 7; `clipped=0` |
| The depth range the flush would produce is empty or outside [0,1] | FCS-49 check 9 |
| The engine's owner lookup returns nothing for statics, or their owner has no 3D | `skipOwner` high, `clipped=0` |
| Vertex shader object layout wrong | `skipShader` high |
| The main view has a depth pass whose vertex shader is one of the 53 Utility variants that do not read the buffer | the lighting pass is clipped but the depth pass is not: the hole shows nothing behind it. `skipShader>0` with `byClass` Utility at 0. |
| A draw path opens a pass and does not close it | FCS-48, mode off for the session, scene native afterwards |
| Rendering is not on the main thread | FCS-47 (reported, not a failure by itself); `offMain` grows; `clipped=0` fails the pose's own assertion |
| A crash in a hook | The offsets read are in section 5 and were confirmed by the independent review; the owner is now found by the engine's own function. The install window is the residual risk recorded under "Install and guards". |
| The depth range is restored late for one draw | would show as geometry drawn at the wrong depth for a frame; `conflicts>0` |
| Cost: two buffer uploads (four matrix inversions each) per clipped pass | `micros` per window; cap `iClipMaxPerFrame` |
| Visual: floor strip missing at the bottom edge, open cross-sections, floating lanterns, fog cards and water uncut, shadows of removed geometry | screenshots; all expected from "What this mode cannot do" |

## Decisions made here (engineering, not owner decisions)

1. **Vtable slots 6/7 instead of the call site at 107642+0x28F.** The call site is one of 13; a call-site patch needs trampoline space the plugin does not have (and a second allocation is forbidden by the wiring audit); wrapping the whole per-pass renderer would put `SetupTechnique` inside the modified window. Slot writes are single pointer stores, need no relocation, and bracket exactly the draw.
2. **Record the upload flags with an entry detour** rather than guess them from the pass (shown wrong at 151FB21/151FB3F).
3. **Apply after the original SetupGeometry**, so the pass's depth-bias mode and bound vertex shader are final when they are read, and the engine's per-geometry constants are computed from untouched state.
4. **State matrices are modified only across the upload call**, never while engine code other than the upload runs.
5. **Depth range through the camera data and the viewport both**, because the engine rebuilds the viewport from the camera data whenever the bias mode changes; and the range is predicted the way the flush computes it (dirty bit, camera data, bias table, float subtraction), not assumed.
6. **Exact for every range and bias** rather than assuming (0,1): the opaque pass runs with max 0.999998.
7. **Vertex-shader check per pass**: found necessary from the shader archive (53 Utility, 7 Effect variants).
8. **Install on the main thread in the camera frame service of an eligible frame, and only while the frame gate is 0** (fix pass, M2), not at kDataLoaded, because the entry detour is a 14-byte write on a function the render path calls every frame.
9. **Off-thread hook calls stay native and do not turn the mode off**; an unbalanced pass does.
10. **Defaults**: plane through head top + 24; corridor 384 (the wave 1 hide radius) because the brief asks for a corridor rule, with 0 available for the whole view; Lighting + Utility; interior only.
11. **Mode 2 does not also run the wave 1 hide.** The two are separable in screenshots this way; combining them is a one-line change in `hideScanned` if the owner wants both.
12. **Owner by the engine's own lookup** (fix pass, M1) rather than a mirror of it: fewer binary assumptions in this code, identical semantics by construction.
13. **The cap is by geometry, for the frame** (fix pass, M3).

## Open questions for the owner

1. Corridor or whole view: is a hole around the sightline (default) or everything in front of the plane (`fClipCorridor=0`) the wanted look? Route 1/1b shows both.
2. Answered on 2026-11-10 (owner decision reported by the orchestrator): the shader route's horizontal cut is the chosen look for one-piece meshes; this build is the comparison and fallback. The camera-side floor loss beyond about 225 units therefore stays a documented limit of this build, to be seen next to the shader result.
3. Should the clip also run outdoors (`iClipExterior=1`: roofs and overhangs between camera and player, never terrain), or is that left to the ext package?
4. Effect-shader meshes (fog cards, beams): clip them too (`iClipShaders=7`) once route 6 shows what that looks like?

## Proposed shared-file changes (not made; outside this package's ownership)

- `docs/evidence/p5.5/fixed-camera-spike.md` (records package): mechanism 3 "Per-draw clip" is no longer BLOCKED on the helper argument; point to this file. The refusal table there should list FCS-42..49.
- `docs/phases/PHASE_5_5.md`, P5.5-02 see-through mechanism 3: "Resolve the projection helper's argument on 1.7.104 first" is done (flags of the scene pass; section 2). The row's game result stays NOT_RUN.
- `briefs/wave2-context.md`: the clip range FCS-40..49 overlaps FCS-40/41 (snap keys). The light (50-59) and ext (60-69) ranges overlap FCS-50, 60 and 61, and codes above 63 do not fit the 64-bit once-mask in `fixed::refuse` (`1 << code`).
- `session-p55/routes`: a `clip` build with the overrides above; checker rows for the `FixedClip` assertions in the route plan, with the fail/report split of the checker rule there (fail FCS-43..46 and FCS-48, report FCS-47 and FCS-49); the first-run protocol row before any other clip route; `FixedClip` lines are a new trace kind (counted as `unparsed_kinds` today, by design).
- When the three native packages are combined: `clip::publish` needs the same `viewer`/`position`/`parent` that `endFrame` already has; the only shared-function signature change is the second parameter of `fixed::configure`.

## Reproduce the offline checks

```sh
E='/home/dq/.local/share/Steam/steamapps/common/Skyrim Special Edition/SkyrimSE.exe'
objdump -d -M intel --start-address=0x14155ff40 --stop-address=0x1415606a1 "$E"   # pass loop and per-pass renderer
objdump -d -M intel --start-address=0x14100ae80 --stop-address=0x14100b521 "$E"   # per-frame upload, flags at 100B327/33C/351
objdump -d -M intel --start-address=0x14151f540 --stop-address=0x14151fb6d "$E"   # opaque pass: uploads with flags and flags|4
objdump -d -M intel --start-address=0x14101e7f0 --stop-address=0x14101ec20 "$E"   # camera data setter, tail jump into the upload
objdump -d -M intel --start-address=0x141010fcc --stop-address=0x1410110ff "$E"   # flush: bias as depth range, RSSetViewports
objdump -d -M intel --start-address=0x1415377f0 --stop-address=0x141537a12 "$E"   # opaque pass depth range 0.999998
objdump -d -M intel --start-address=0x1415170c0 --stop-address=0x14151717e "$E"   # world matrix: row vectors, minus posAdjust
objdump -d -M intel --start-address=0x14100fc10 --stop-address=0x14100ff10 "$E"   # vertex shader object: +10h size, +68h bytecode
objdump -d -M intel --start-address=0x1402eece0 --stop-address=0x1402eed9e "$E"   # owner lookup: AsFadeNode before [fade+F8]
objdump -d -M intel --start-address=0x1406588d0 --stop-address=0x1406588e8 "$E"   # Main::Update: player update (camera) before the frame at 659319
```

Sources read: the binary and Address Library above; `Data/Skyrim - Shaders.bsa` (read-only); CommonLibSSE-NG 7.2.0 local checkout `session-p2-20261026/commonlib-7.2.0` (`RE/R/RendererShadowState.h`, `RE/B/BSRenderPass.h`, `RE/N/NiAVObject.h`, `RE/B/BSGeometry.h`, `RE/B/BSShader.h`) as leads only; `p5-spike/docs/evidence/p5/occlusion-spike.md` and `docs/planning/phase5-targeting-camera-research.md` R4 for the Disocclusion facts (GPL revision `04bb1993171f5604f2d046517154e122f93f0c4c` as cited there; its source was **not** opened in this work and no code was copied); `session-p55/research/shader-discard.md` as leads, each one used here re-derived from the binary or the archive.
