# P5.5-02 wave 2, package "shader" (DYL-217): world-space horizontal cut in the shader stage, `iSeeThroughMode=3`

2026-11-09. Worktree `p55-spike-shader`, branch `phase5.5/p55-02-spike-shader`, base `744609f9fb459aa1e50688b3ab590bb6cbbac05a`. **Disposable: never merged, nothing here enters the shipped Camera DLL.** Experimental scope of PHASE_5_5 (four official masters, Steam Skyrim SE 1.7.104, SKSE 2.3.1, SmoothCam 1.8.1, TDM 2.3.1).

**Second pass (2026-11-10, base `4a3b722`).** Owner decision after batch W2S: void colour pure black to start, fog cards and light shafts above the cut removed, ENB or Community Shaders still refuse everything. Built: the void colour override and the Effect-class cut, both behind new keys that default on in `iSeeThroughMode=3` only. Everything about it is in **"Second pass: void colour and Effect-class cut"** below; the sections before it describe the first pass and stay valid except where that section says otherwise. Its game rows are **NOT_RUN**.

**Third pass (2026-11-10, base `6397406`).** Batch W3 showed the void dark but not black. Root cause: in interiors flagged Show Sky the engine draws the exterior sky dome over the void. Built: the sky dome hidden while the cut is active (`bVoidHideSky`), and Effect meshes centred in the upper half under the plane or higher clipped whole (`bShaderCutEffectWhole`, the wedge). See **"Third pass: what paints the void, and the wedge"** below. Its game rows are **NOT_RUN**.

**Stage 2 (2026-11-11, base `3528e92`).** Owner go "Start Stage 2": one full-screen frame pass of our own after the main world render and before image space (soft cut edge, radial falloff, vignette, exact void colour), behind `bFramePass` (default on in mode 3). Everything about it is in **"Stage 2: the frame pass"** below. Its game rows are **NOT_RUN**.

**Status.** Source, portable tests, wiring audit, offline binary audit and offline shader-archive audit are complete and uncommitted. The Windows `/W4 /WX` build is **NOT_RUN** (no MSVC here). **Every in-game row is NOT_RUN**: nothing was launched, no shader was created on a device. The mode is default-off: with `iSeeThroughMode` absent or 0..2 no shader-cut runtime is allocated and no hook is installed.

Each claim is marked **Source** (file:line, binary RVA, Address Library ID, the shipped shader archive, or a URL and revision) or **Hypothesis**. RVAs are relative to image base `0x140000000` of `SkyrimSE.exe`, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; IDs are resolved through `session-p4/stage-camera-rc1/Data/SKSE/Plugins/versionlib-1-7-104-0.bin`, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842` (ID *n* at file offset `96 + 4n`). Shader archive: `Data/Skyrim - Shaders.bsa`, SHA-256 `fd1ba630f443353ed21ed4ca0380968f9c7091c8434518ea3cc79bc6d38c18e7`, one uncompressed file `shadersfx\shaders011.fxp` (67,768,128 bytes at archive offset 0x66). Disassembly: `objdump -d -M intel`. Scratch tools (ID resolver, PE reader, a scanner for vtable calls on a global COM object, a DXBC disassembler, a reference checksum, a ctypes wrapper for vkd3d-shader) stayed outside the worktree.

## Answer in short

The horizontal cut the owner asked for **is implemented**, as candidate (c) of the brief: a **user clip distance written in the vertex stage**. For a selected draw the adapter binds a patched copy of the vertex shader the engine had bound, and one 16-byte constant buffer. The copy also writes `SV_ClipDistance0 = cutHeight - worldHeight`, so the rasteriser drops everything of that draw above the plane, in **every** pass of the draw: colour, depth-only and any other pass that uses a patched shader. No pixel shader is touched, so passes that bind no pixel shader are cut like the rest.

Size of the shader side, measured on the archive: **195 vertex shaders (84 distinct bytecodes), 27 tokens added to each**, against 6,924 Lighting pixel shaders for a pixel discard. Size of the engine side: two vtable slots on each of two shader classes. **No engine code is patched, no engine object and no render-state field is written.**

What is delivered and what is not:

| Target of the brief | State |
| --- | --- |
| Everything above a horizontal plane over the player's head is not drawn, the whole floor, walls up to that height, the player and actors stay | Implemented. In-game look NOT_RUN. |
| Other rooms' geometry between camera and player does not block | Implemented for what is drawn with a patched shader (same cut). Whether the engine submitted what lies behind is room culling: unchanged, combine with `iRoomSeedMode`. NOT_RUN. |
| Depth pre-pass and shadow passes handled consistently | Depth passes of the published view are cut with the same plane (Utility class hooked, 125 shaders patchable). Shadow maps are another view and are deliberately **not** cut: lighting stays as authored. Whether the main view has a Utility depth pass at all is Hypothesis; the trace answers it (`byClass`). |
| Soft band `fShaderCutBand` | **Not implemented, key not added.** A vertex-stage cut has a hard edge. What a fade would take is under "What a fade would take". |
| Fail closed, exact restoration, trace | Implemented; see Guards and Faults. |

## What was built

| File | Content |
| --- | --- |
| `modules/camera/include/fixed_spike_shader_policy.h` (new) | Portable policy, no engine headers: audited IDs, RVAs, offsets, vtable byte offsets and guard bytes; settings; the published snapshot and the view test; the plane constant; selection rules; DXBC container checksum; the vertex-shader patcher; bytecode hash; trace format; refusal codes FCS-66..79 with their own once-mask. |
| `modules/camera/include/fixed_spike_shader_audit.h` (new, generated) | 84 FNV-1a-64 hashes: the bytecodes the patcher may touch. Hashes only; no shader bytecode is stored in the repository. |
| `modules/camera/src/fixed_spike_shader.h` (new) | Native adapter included by `fixed_spike.h`: runtime, the two hooks per class, selection, owner lookup, lazy creation of patched shaders, bind and restore of one pass, guards, install on the main thread, publish, trace. |
| `modules/camera/src/fixed_spike.h` | Delimited `wave 2 shader` edits: include, one `LineBudget`, `lifecycle` deactivates, `beginFrame` calls `shader::frameStart`, `endFrame` deactivates (ineligible frame) or publishes (eligible frame) and traces, `install` asks for the install and names the state in the installed line, `configure` takes the shader settings and adds the trace rows. |
| `modules/camera/include/fixed_spike_policy.h` | `seeThroughMode` 3 is accepted (two lines). 2 stays invalid on this branch. |
| `modules/camera/src/plugin.cpp` | Five keys read inside the existing `[FixedCameraSpike]` gate and passed to `fixed::configure`. |
| `modules/camera/config/SkyCRPG_Camera.ini` | Keys at the end of `[FixedCameraSpike]`. |
| `modules/camera/tests/fixed_spike_shader_policy_test.cpp` (new), `tests/fixed_spike_policy_test.cpp`, `tests/lock_refresh_wiring_test.py` | Portable tests; mode 3 in the spike's validation test; a sixth wiring section. |
| Second pass | `src/fixed_spike_shader_void.h` (new), `include/fixed_spike_shader_effect_audit.h` (new, generated), and edits to the files above: see "Second pass". |

No build file changed: the new headers are reached through `plugin.cpp` (`modules/camera/CMakeLists.txt:1`). No Windows header is added: device calls go through vtable byte offsets (below).

**Off equals 744609f.** `shader::Runtime` is allocated only by `shader::configure`, called only from `fixed::configure` when the validated mode is 3. Every other entry point returns at once on a null runtime (`install`, `deactivate`, `frameStart`, `publish`, `trace`, `tracing`, `status`; wiring audit). With the mode at 0 or 1 the only observable differences from the base are the text `shaderCut=off` in the spike's installed log line and the five INI reads. In mode 3 the wave 1 hide does not run (`hideScanned(3, …)` is false, unchanged), so nothing is app-culled.

## Binary audit (SkyrimSE.exe 1.7.104)

Everything below was read from the binary in this work. The clip package's document and the shader memo were used as leads only; the coordinator's note of the independent review (what it confirmed, and that the clip table of per-frame-buffer rows above 19 is wrong) was taken into account: **nothing here depends on that table**; the only use of the per-frame buffer is an operand-level match on rows 8..11 in vertex shaders (shader audit, below).

### 1. Vertex shader objects, creation and binding

| Fact | Evidence |
| --- | --- |
| `LoadVertexShader` ID 77348, RVA 100FC10. Stream entry: magic `0x11223344`, u32 bytecode size, then the object is allocated as `size + 0x68`; the loader stores the size at object `+0x10` (100FCAB), reads 4 bytes into `+0x00`, 8 into `+0x48`, 0x14 into `+0x50`, 4 onto the stack, then `size` bytes of DXBC into `+0x68` (100FCED..100FCF9). | Source: 100FC10..100FDBC. |
| It then calls `ID3D11Device::CreateVertexShader(device, object+0x68, size, nullptr, &object+0x08)` as `[vtbl+0x60]` on the device global ID 411348 (RVA 3330190) at 100FD8A. A second creator at 100FE5B has the same shape. | Source. So the object keeps its bytecode (`+0x68`, length `+0x10`) and its device shader (`+0x08`). |
| `SetVertexShader` ID 77351, RVA 100FEE0: sets dirty bit 0x400 in render state `+0x00`, stores the object at state `+0x348` (20CFF48), and tail-jumps to `ID3D11DeviceContext::VSSetShader(context, object->+0x08, nullptr, 0)` as `[vtbl+0x58]` on the context global ID 411391 (RVA 3331F30). No caching. | Source: 100FEE0..100FF05. |
| This is the **only** `VSSetShader` call on the engine's context, and it has two callers: `BSShader::BeginTechnique` (ID 108328, RVA 1579120, call at 15791F5 = +0xD5) and 11B34B6. | Source: scan of every vtable call made through the global 3331F30 (363 references); `E8`/`E9` scan for 100FEE0. |
| `BeginTechnique` is called only from the classes' `SetupTechnique` (Lighting 1547D9A in vtable slot 2 = 1547D20; Utility 1566790 in slot 2 = 1566710; others) and from image-space code (158AA93…). None of the callers lies inside `SetupGeometry`/`RestoreGeometry` of Lighting (1549550, 154A900) or Utility (15673F0, 15689C0). | Source: caller list of 1579120. |
| Dirty bit 0x400 is consumed by the state flush ID 77386 (RVA 1010DE0) at 10111D2: it looks up or builds the input layout from the current vertex shader object's descriptor (`+0x48`) and the vertex declaration (state `+0x340`). It does not rebind the shader. | Source: 10111C9..1011255. The patched copy has the byte-identical input signature (ISGN chunk copied), so the layout the engine picked for its own shader fits the copy. |
| State `+0x348` is read by many `SetupGeometry`-level functions (constant tables of the current shader). It is never written here. | Source: every RIP-relative reference to 20CFF48: one writer (100FEF7), about sixty readers. |

### 2. Where a draw happens, relative to the hooks

| Fact | Evidence |
| --- | --- |
| Per-pass renderer ID 107644, RVA 1560340: technique change → `RestoreTechnique` `[vtbl+0x18]` / `SetupTechnique` `[vtbl+0x10]` (15603AA, 15603CA); material `[vtbl+0x20]`; then three branches, each `SetupGeometry` (`[vtbl+0x30]` directly at 15605A7, or through helper 1562270 which tail-jumps to `[vtbl+0x30]` at 1562310), draw (155F050 or the skin instance's `+0x128`), `RestoreGeometry` `[vtbl+0x38]` (156052B, 1560586, 15605FC, 1560668). RCX = shader, RDX = pass, R8D = flags. | Source: 1560340..15606A0, 1562270..1562310. |
| Between `SetupGeometry` and `RestoreGeometry` the non-skinned branches call only 1555400, 1578AD0, 1578930 and the draw (155F050). `BeginTechnique` has no caller in 1555400, 1578930..1578E10 or 155F050..155F7B6. | Source: 15605A7..15605FC, 156064F..1560668; the caller list of 1579120 (next section). |
| `BSRenderPass +0x00` shader, `+0x10` geometry; `BSGeometry +0x130` skin instance. | Source: 1560374, 1560377, 1560420. |
| Lighting vtable ID 255053 RVA 1B47420: slot 6 = 1549550, slot 7 = 154A900. Utility vtable ID 255302 RVA 1B4AB58: slot 6 = 15673F0, slot 7 = 15689C0. First 16 bytes of each function as in `shaderClasses` of the policy header. | Source: `.rdata` read at `table + 8·slot`; bytes read from `.text`. Same values as the clip package and its review. |

### 3. Device state the mode touches

| Fact | Evidence |
| --- | --- |
| Vertex constant-buffer slots the engine binds: 0, 1, 2, 7, 8, 9, 10, 11, 12, always one buffer per call. Slot 13 is never bound. | Source: all 64 `VSSetConstantBuffers` (`[vtbl+0x38]`) sites reached through the context global; start slot and count read at each (two sites compute them in registers: 154A880 start 2 count 1, 155F73E start 7 count 1). Limit: the scanner follows the global through one vtable load; a call through a copy of the context pointer held elsewhere would not be seen. |
| At frame begin the engine calls `ID3D11DeviceContext::ClearState` (`[vtbl+0x370]`, 1009E72) and then binds the per-frame buffer to vertex and pixel slot 12 (1009E9E, 1009EBB). | Source: 1009E5C..1009EBB. So nothing the mode binds can survive a frame, whatever else happens. |
| No geometry, hull or domain shader is ever bound through the context global (no `[vtbl+0xB8]`, `+0x1E0`, `+0x200` call). | Source: same scan. The vertex shader is the last stage before the rasteriser, which is where a clip distance must be written. |
| `ID3D11Device::CreateBuffer` is `[vtbl+0x18]` with `(device, &desc, nullptr, &out)`; the engine's dynamic constant buffers use `desc = {ByteWidth, Usage 2, BindFlags 4, CPUAccessFlags 0x10000, 0, 0}`. | Source: 1012841..1012884 and the thirteen further sites (1012A4A, 1012A6C …). The adapter creates its buffer with the same description. |
| `Map` is `[vtbl+0x70]` with `(context, resource, 0, 4, 0, &mapped)`, `Unmap` `[vtbl+0x78]` with `(context, resource, 0)`; `mapped.pData` is the first field. | Source: 1011161..10111C0. |

Every COM method the adapter calls is called by vtable byte offset, and each offset is one the engine itself uses for that method on the same object: device `+0x18` CreateBuffer, `+0x60` CreateVertexShader; context `+0x38` VSSetConstantBuffers, `+0x58` VSSetShader, `+0x70` Map, `+0x78` Unmap. No CommonLib or Windows SDK declaration of these interfaces is relied on.

### 4. World position and the view

| Fact | Evidence |
| --- | --- |
| World matrix builder RVA 15170C0 `(out, NiTransform*)`: rows 0..2 are the scaled rotation columns, row 3 is `translate − posAdjust` with `posAdjust` read from 20CFF5C/60/64 (state `+0x35C`), `[3][3] = 1`. | Source: 15170C0..151717D. |
| Lighting `SetupGeometry` calls it at 154A3DF and 154A414; Utility `SetupGeometry` at 1567617, 156763C, 15679EF. | Source: `E8` scan for 15170C0. So the World matrix these shaders receive in constant buffer 2 is relative to the position adjustment that is in the state during that same `SetupGeometry` call. The adapter reads `posAdjust` immediately after the original `SetupGeometry` returns. |
| `posAdjust` (state `+0x35C`) is the NiCamera's world translation, and state `+0x3A0` (camera data `+0x20`) is column 0 of its world rotation, the camera's view direction. | Source: ID 77520 writes `(rot[0][0], rot[1][0], rot[2][0])` to entry `+0x30` (101E571..101E592) and copies camera `+0xA0..0xA8` to entry `+0x260` (101E794..101E7C2); ID 77521 (101E7F0) copies entry `+X+0x10` to camera data `+X` (101E8B0..101E9EF) and entry `+0x260` to 20CFF5C (101EAB8..101EAE0). |
| `TESObjectREFR::FindReferenceFor3D` ID 19750, RVA 2EECE0: for each node up the parents (`+0x30`), `AsFadeNode` (`[vtbl+0x28]`, 2EED25); if it is a fade node and its `+0xF8` is non-null that is the reference; if that reference's base form (`ref+0x40`) has type byte (`+0x1A`) 0x2A (ammunition), the search continues from the parent and prefers what it finds. | Source: 2EECE0..2EED74. `ownerOf` in the adapter does the same walk with the same offsets (it does not call the engine function, which also asks the player for its first-person 3D). |
| `NiAVObject +0xE4` world bound centre, `+0xF0` radius. | Lead from the clip audit; in game the wave 1 hide used the same field through CommonLib. Not re-derived here. |

### 5. What cannot be verified offline

- That rendering runs on `Main::threadID`. **Hypothesis**; checked on every hook call, a call elsewhere touches nothing (FCS-71).
- That the published camera (`PlayerCamera::cameraRoot` world transform, column 1 as forward) and the render state's eye and view direction agree in the main view. Evidence for the eye: wave 1's room hook claimed with a 96-unit tolerance between the two (ledger). For the direction: cameraRoot column 1 gave the expected headings and pitch in wave 1's rig trace; NiCamera column 0 is what the engine stores as view direction (Source above). That the two are the same direction in game is **Hypothesis**; if not, nothing is cut and FCS-76 reports both vectors.
- Whether the main view runs a Utility depth pass before the lighting pass. **Hypothesis**; both classes are hooked.
- Everything about the GPU side: see "Validation of the patched bytecode".

## Shader-archive audit (`shadersfx\shaders011.fxp`)

All counts are from this work's own parse. Blobs were found by their `DXBC` magic and total-size field; classes by load order (vertex run, then pixel run, per class).

| Fact | Evidence |
| --- | --- |
| 16,044 DXBC blobs: 3,390 vertex, 12,635 pixel, 19 compute; all Shader Model 5.0; all with exactly the chunks ISGN, OSGN, SHEX. Runs: BloodSplatter 2/2, DistantTree 4/4, Grass 18/18, Particle 6/6, Sky 9/9, Effect 606/3,216, **Lighting 127/6,924, Utility 339/177**, Water 2,172/2,172, then image-space pairs. | Source. Agrees with the memo's counts. |
| Every vertex-shader blob is preceded by the loader's entry header: magic at −44, and at −40 a size equal to the blob's own total-size field. | Source: asserted for all 3,390 in the archive test. So object `+0x10` is the exact DXBC length. |
| The DXBC checksum (MD5 block function over everything after the checksum field, with DXBC's own final block) reproduces the stored checksum of **all 16,044** blobs, in a Python reference and in the C++ policy. | Source: scratch reference; archive test. |
| Lighting vertex shaders: 78 rigid, 49 skinned (BLENDWEIGHT/BLENDINDICES inputs). Utility: 178 rigid, 161 skinned. | Source: input signatures. |
| Constant-buffer slots declared by vertex shaders, all classes: 0, 1, 2, 7, 8, 9, 10, 11, 12. None declares 13. | Source: every `dcl_constantbuffer`. |
| Highest output register of a vertex shader: Lighting 15, Utility 3. Highest input register of a pixel shader: Lighting 15, Utility 3. No vertex output or pixel input anywhere is a clip or cull distance. | Source: OSGN of all vertex shaders, ISGN of all pixel shaders, nine world classes. So register 16 is above every pixel-shader input of both classes, and there is no existing clip distance to collide with. |
| Every Lighting and Utility vertex shader has exactly one `ret`, no `retc`, no `call`/`label`. | Source. |
| Rigid Lighting and Utility shaders that use the per-frame view-projection build each row of World-View-Projection as `mul rA, cb12[8+r].xxxx, cb2[k]` / `.yyyy, cb2[k+1]` / `add` / `.zzzz, cb2[k+2]` / `add` / `.wwww, …` / `add rW`, then `dp4 …, rW, rP` with the model position `rP = (v0.xyz, 1)`. k = 0 in Lighting (World is 3×4, the w row is the literal 0,0,0,1), k = 1 in Utility (4×4 in cb2[1..4]). | Source: operand-level match; examples blobs at file offsets 8136A4 (Lighting) and 3172C54 (Utility). |
| The row multiplied by the `.zzzz` column is World's z row, so `dp4 cb2[k+2], rP` is the vertex's world height relative to the position adjustment. In **all 70** patched Lighting shaders the shader itself computes exactly that product, with the same row and the same register, for its `POSITION1` output (world position for the pixel shader): 50 write `dp4 oP.z, cb2[2], rP` directly, 20 through a temp. | Source: archive test (`world-z product confirmed in 50+20 lighting shaders`). Utility shaders have no such output; for them the statement rests on the algebra alone. |
| Patcher outcome over the two classes: **Lighting 70 patched, 8 rejected for shape, 49 skinned; Utility 125 patched, 53 rejected for shape, 161 skinned.** 195 entries, 84 distinct bytecodes. | Source: archive test. |
| The 8 Lighting and 20 of the Utility shape rejects are variants that change the position before projecting (a height offset near the camera, as LOD shaders do; wind animation); the other 33 Utility rejects do not use the per-frame view-projection at all (screen-space). They stay native. | Source: disassembly of blobs 729, 853, 1058 (vertex-shader index in file order). Their purpose (LOD, tree animation) is **Hypothesis** from the arithmetic. |
| 332 of 606 Effect vertex shaders have the same shape and would patch. Not hooked in this prototype. | Source: same run. |

### The patch

For an audited bytecode the patched copy differs only by (disassembly by vkd3d-shader of the first Lighting shader, original versus patched):

```
 dcl_constantBuffer cb2[6], immediateIndexed
+dcl_constantBuffer cb13[1], immediateIndexed
 …
 dcl_output o12.xyzw
-dcl_temps 5
+dcl_output_siv o16.x, clip_distance
+dcl_temps 6
 …
 mov r1.w, l(1.0)
+dp4 r5.x, cb2[2].xyzw, r1.xyzw
+mad o16.x, r5.x, cb13[0].x, cb13[0].y
 dp4 r0.x, r0.xyzw, r1.xyzw
```

plus one more element in the output signature (`SV_ClipDistance`, index 0, system value 2, float, register 16, mask x), the chunk and container sizes, and the checksum. 27 tokens and 40 signature bytes per shader. The two instructions go immediately before the first World-View-Projection product, where the position register is live by construction. The constant is `(−1, planeZ − posAdjust.z)`, so the distance is `planeZ − worldZ`; the rasteriser keeps what is ≥ 0. An all-zero constant gives distance 0 everywhere: never cut.

The patcher (`patchVertexShader`) refuses, before building anything: a container that is not exactly ISGN, OSGN, SHEX, contiguous, with matching sizes; a checksum that does not match; anything but `vs_5_0`; skinning inputs; an existing clip or cull distance; an output register ≥ 16; a declaration of buffer 13; more than one `dcl_temps`; a declaration after code; an unknown opcode; a World row not covered by the cb2 declaration; a position register that is not a declared temp; any instruction other than `mul`, `mov`, `add` between the first `.zzzz` product and the `dp4`; a `dp4` whose operands are not the running sum and one other temp; different World rows in different view-projection rows; the product inside control flow or after a `ret`; `call`/`label`; unbalanced control flow.

### Validation of the patched bytecode

| Check | Result |
| --- | --- |
| vkd3d-shader 2.1 (`libvkd3d-shader.so.1.19.0` of the Steam Linux Runtime on this host, loaded read-only through ctypes; not Proton, not the game): all 195 originals and all 195 patched blobs compile to its D3D assembly and to SPIR-V with no message. | **PASS** (offline). vkd3d-shader checks the container checksum (a patched blob with one flipped byte is refused with `E0003: Invalid DXBC checksum`), so the recomputed checksums are accepted by an independent implementation. |
| The SPIR-V of every patched blob gains a `BuiltIn ClipDistance` decoration that the original does not have. | **PASS** (195 of 195). |
| DXVK's own translator. Read: `src/dxbc/dxbc_compiler.cpp` at tag v2.7.1 (https://github.com/doitsujin/dxvk), through a summarising fetch, not line by line. The clip-distance array length comes from `m_analysis->clipCullOut.numClipPlanes`; `emitClipCullStore` iterates the output signature for entries whose system value is clip distance and reads `m_oRegs.at(e->registerId)` per set bit of `componentMask`; `emitVsFinalize` calls it. That is why the patch writes both the signature element and the `dcl_output_siv`. | **Hypothesis** for the game: the DXVK build inside the Proton the game runs with was not identified, and nothing was compiled by DXVK. |
| Windows D3D11 runtime and a Windows GPU driver. | **NOT_RUN**. Outside the pinned host. |
| That a vertex shader may write an output register no pixel shader reads, and that `SV_ClipDistance` clips without the pixel shader declaring it. | **Hypothesis** (D3D11 linkage rules as documented; standard practice). Not exercised. |

## Design chosen, and the alternatives rejected

**(c) Vertex-stage clip distance, per draw, by binding a patched copy of the bound vertex shader.** Reasons:

1. It cuts in every pass with no pixel shader work. The memo's main obstacle, depth-only passes that bind no pixel shader, does not exist for it.
2. The shader side is two orders of magnitude smaller: 195 vertex shaders against 6,924 Lighting pixel shaders, and the vertex shaders already hold what is needed (World's z row and the position); no reconstruction from `SV_Position`, no inverse matrix, no dependence on the per-frame buffer's layout.
3. The engine keeps vertex-shader bytecode in the object, so nothing is read from disk at run time and the bytecode patched is the bytecode the engine created its shader from.
4. Nothing persistent and nothing global: the engine's shader objects are untouched; unselected draws use the engine's own device shaders bit for bit; no load-time hook, no code patch, no call-site thunk (so no collision with the site Community Shaders patches, though its presence still refuses the install).
5. Pixel-shader input signatures tolerate it: the new output is in a register above every pixel input of both classes.
6. A plane is exactly what a clip distance expresses; the owner asked for a plane.

Rejected:

| Alternative | Why not |
| --- | --- |
| (a) Pixel-shader bytecode patch with a discard | 6,924 Lighting pixel shaders (Source: count; the memo adds that 2,068 of them have no discard today and may lose early depth, not re-derived); world position must be rebuilt per pixel from `SV_Position` and matrices we would have to upload (the review's operand-level parse, not re-derived here, has Lighting pixel shaders reading rows 12-19 and 42-44 of the per-frame buffer only, so no inverse matrix); depth passes with no pixel shader need a new pixel shader forced in at `BeginTechnique`+0xEB (157920B, Source), the call site Community Shaders owns; and if the engine does not keep pixel-shader bytecode (memo: Hypothesis), the archive would have to be read at run time. Its only advantages are non-planar shapes and a dithered edge. |
| (b) Own replacement shaders | Would have to reproduce every material permutation. A renderer project. |
| (c') Swap `VertexShader::shader` in the engine's objects once, cut driven only by the constant | Persistent change to engine objects; every Lighting draw would run with a clip distance output for the whole session; restore is a second global pass. The per-draw bind keeps native rendering native. |
| (c'') Clip plane in clip space from the inverse view-projection | Needs the matrices the draw was really uploaded with (an entry detour on the per-frame upload, as the clip package has), and the Utility shaders alter clip z. World's z row is in the shader already. |
| Hook `LoadVertexShader` or the device's `CreateVertexShader` | Load-time, global, and the device hook is Community Shaders' own. Not needed: shaders are made lazily from the live objects. |
| Per-draw near-plane clip (`iSeeThroughMode=2`) | The owner chose the horizontal cut over it. |

Decisions inside the design (engineering, not owner decisions):

1. **Lazy creation.** A patched shader is created the first time a selected pass has that vertex shader object current, on the render thread, and kept for the session (never released). Only shaders that are used are made.
2. **Audited list.** Only bytecode whose FNV-1a-64 is on the generated list is patched, even though the patcher would also refuse anything it does not understand. Anything else (a replaced shader, the LOD and wind variants) is drawn natively and named once (FCS-75).
3. **Whole view by default** (`fShaderCutRadius=0`): every selected piece in view is cut at the plane, which is what "everything above a horizontal plane" says. A radius is available for comparison.
4. **Main view only.** A pass is cut only if the render state's eye and view direction are the published ones (96 units, cos ≥ 0.99). Shadow maps, reflections, the local map and menu scenes are another view.
5. **Interior cells only**, as the brief scopes it. No key.
6. **Selection**: never skinned geometry, never geometry without an owning reference, never actors (or what they carry), never the kinds wave 1 excluded (lights among them); architecture and placed objects are cut. A bound that does not reach above the plane is not touched at all. No floor rule is needed: a plane cannot remove what is under it.
7. **The plane's height** uses wave 1's `CutPlane` (band 24, settle 0.6 s, snap 96), so a jump does not move it; it starts again after any frame without a plane.
8. **Mode 3 does not also run the wave 1 hide.**
9. **Refusal codes 66..79 with their own 32-bit once-mask**; `fixed::refuse` shifts a 64-bit word by the code and cannot hold them.

### One cut pass

1. `SetupGeometry` hook: call the original. If a plane is published: main thread; snapshot fresh; render state's eye and direction are the published view; geometry not skinned; bound reaches above the plane (and inside the radius, if one is set); owner rules; the current vertex shader object has a patched copy (made now if this is its first use); per-frame cap.
2. Apply: if the constant differs from the last one uploaded, `Map`(WRITE_DISCARD)/`Unmap` the 16-byte buffer (normally once per frame); `VSSetConstantBuffers(13, 1, buffer)`; `VSSetShader(patched)`. The two calls that can fail (creation, map) come before anything is bound.
3. The engine draws the pass.
4. `RestoreGeometry` hook: call the original; `VSSetShader(the engine object's own device shader)` if the engine still has the same object current; `VSSetConstantBuffers(13, 1, null)`.

Nothing is left on any reference, node, material, shader object or state field. There is therefore nothing to restore on F6, F8, menus, loads or death: the next frame is drawn without cut passes, and the engine clears the device state at every frame begin anyway.

### Threads

| Thread | What it does | What it may touch |
| --- | --- | --- |
| Main, camera frame service (`beginFrame`/`endFrame`) | installs once; publishes the snapshot (plain copy, main thread only); writes trace rows; logs refusals | everything |
| Main, render hooks | selection, creation, bind and restore | render state (read), scene graph (read), device and context |
| Any other thread in a hook | calls the original; increments one atomic counter (reported once as FCS-71) | nothing else |
| Any thread, lifecycle events | clears `active` | one atomic |

### Install and guards

`shader::install` (kDataLoaded, possibly off the main thread) only sets a flag. Guards and writes run in the first main-thread camera frame (`frameStart` → `commit`).

| Code | Site | Meaning, and what stays in force |
| --- | --- | --- |
| FCS-66 | `configure` | a shader-cut value was out of range; safe value applied |
| FCS-67 | `commit` | `CommunityShaders.dll` or `d3dcompiler_46e.dll` (ENB; module name is Hypothesis) is loaded, or `d3d11.dll` was not loaded from the system directory (a wrapper such as ENB in the game folder); not installed |
| FCS-68 `check=N` | `commit` | an ID does not resolve to the audited RVA: 1 state, 2 device, 3 context, 10 Lighting vtable, 11 Utility vtable; not installed |
| FCS-69 `check=N` | `commit` | vtable slot or first 16 bytes differ, `N = 10·(class+1) + {1 setup slot, 2 restore slot, 3 setup bytes, 4 restore bytes}`; not installed |
| FCS-70 | `commit` | no engine device or context, or the device refused the plane buffer (flags and HRESULT in the line); not installed |
| FCS-71 | `frameStart` | a hook ran off the main thread; those calls touched nothing |
| FCS-72 `cause=N` | `fault` | a cut pass was still open at 1 the next SetupGeometry, 2 the next camera frame, or 3 its RestoreGeometry found another vertex shader current; it was closed and the mode is off for the session |
| FCS-73 | `prepare` | the device refused a patched shader (hash, HRESULT); that shader stays native |
| FCS-74 | `prepare` | audited bytecode was not patched (reason); stays native. Not expected. |
| FCS-75 | `prepare` | a selected pass used bytecode that is not on the audited list (hash, size); stays native. Expected once if a LOD or wind variant is in view; informative. |
| FCS-76 | `frameStart` | passes were drawn for 120 frames with a plane published and none matched the published view; the line carries the published eye and the last render-state eye and direction. Nothing is cut. |
| FCS-77 | `apply` | the plane buffer could not be mapped; mode off for the session |
| FCS-78 | `prepare` | a current vertex shader object has no device shader or an impossible bytecode size; stays native |
| FCS-79 | `frameStart` | the engine's context pointer changed; mode off for the session |
| FCS-80..88 | | second pass: see "Second pass", Refusal codes |

ENB and Community Shaders are absent from the game directory and from the staged spike profiles under `session-p55` (Source: directory listings, 2026-11-09: no `d3d11.dll`, `dxgi.dll`, `enb*` or `CommunityShaders*`). Under Proton the game's `d3d11.dll` is DXVK's in the prefix's `system32` (Source: `compatdata/489830/pfx/drive_c/windows/system32/d3d11.dll` exists); that the module path reported in the game equals the system directory is **Hypothesis**. If it does not, FCS-67 names it and nothing is installed.

### Settings (`[FixedCameraSpike]`, read only when `bEnabled=1`)

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `iSeeThroughMode` | 0 | 0, 1, 3 here | 3 = this mode |
| `fShaderCutHeight` | 160 | 64-1024 | plane above the feet |
| `fShaderCutRadius` | 0 | 0 or 384-8192 | 0 = every selected piece in view; otherwise only pieces whose bound reaches within this ground distance of the feet (the camera stands about 336 away, so less than 384 is not accepted) |
| `iShaderCutMaxPerFrame` | 4096 | 1-16384 | cap on cut draws per frame |
| `iShaderCutShaders` | 3 | 1-3 | 1 Lighting, 2 Utility |
| `iShaderCutTrace` | 0 | 0/1 | 6,000 `FixedShader` rows at about 10 Hz |

Second-pass keys `bShaderCutEffect`, `bVoidColor`, `sVoidColor`: see "Second pass".

### Trace legend (`FixedShader`, one row per 0.1 s window, in `SkyCRPG_Camera_FixedSpike.log`)

`installed`, `active`, `latched`; `eye`, `feet`, `planeZ`, `radius`: the published snapshot (nan while inactive); `relPlane`: the last constant uploaded, plane height relative to the position adjustment (nan if nothing was cut in the window); `passes`: passes of hooked classes seen while active; `cut`; `byClass`: cut passes by class, Lighting then Utility (a non-zero Utility count answers whether the main view has a depth pass through these shaders); `peak`, `cap`; `uploads`: constant uploads; `shaders=p,r,u`: patched shaders made, rejected (FCS-73/74/78), unaudited (FCS-75), cumulative; `created`: made in this window; `open`: FCS-72 events; `offMain`; `micros`: time in the SetupGeometry hook after the engine function (selection, creation, bind), measured only while tracing; `otherView`: passes whose render state was not the published view, with `otherEye`/`otherDir` of the last one; `stale`; `skip*`: Skin, Bound, Below (does not reach above the plane), Out (radius), Owner (none), Actor, Kind, Shader (no patched copy), Cap.

## Expected artefacts

| What | Why |
| --- | --- |
| Hard edge at the plane; open cross-sections | Clipping does not cap. Single-sided meshes show nothing where their back would be; behind them is whatever was drawn, or the cleared target. |
| Shadows of removed geometry remain; the room is as dark as it was | Shadow maps are not cut, on purpose. Lighting is the light package's subject. |
| Actors, the player's followers and creatures above the plane are drawn whole, standing on nothing | Actors are never cut. |
| Tall objects (shelves, doors, pillars) end at the plane | They are placed objects or architecture and are cut like walls. |
| Effect-shader meshes above the plane stay: fog cards, light shafts, some cobwebs and glows | Effect class not hooked. 332 of its vertex shaders would patch. |
| Particles, water surfaces, grass, sky stay | Their classes are not hooked; water positions vertices otherwise. |
| Wind-animated statics (hanging moss, some plants) and anything drawn with the 28 rejected variants stay whole | Shape not accepted; FCS-75 names the first. |
| Skinned non-actors (banners, some animated statics) stay whole | Skinned geometry is never selected. |
| Lights' own references stay | Excluded kind. |
| Run-time decals on cut surfaces can remain above the plane | Their geometry has no owning reference. Decals that are part of a mesh are cut with it. |
| On stairs and ramps the plane follows the feet in steps (after 0.6 s, or at once beyond 96 units) | `CutPlane` hysteresis; a visible pop per step of height. |
| Ground that rises above feet + 160 in the distance is cut, leaving a void | Whole-view horizontal plane. `fShaderCutRadius` limits it per piece, not per pixel. |
| A void where the engine culled what is behind a removed piece | Room and portal culling are unchanged. Combine with `iRoomSeedMode`. |
| Hitches in the first seconds | Each patched shader is created at first use, and DXVK compiles new pipelines for it. **Hypothesis.** |
| LOD | None in interiors. |

### What a fade would take

A dithered or alpha-to-coverage band needs a per-pixel decision, which is pixel-shader work. With this design in place it would be: move the clip plane to the top of the band (this patch), pass the distance to the pixel shader as an extra interpolant (one more output here), and patch the pixel shaders that draw selected geometry to discard on a screen-space dither pattern inside the band: the 6,924 Lighting pixel shaders, by bytecode, each needing `SV_Position` declared and a discard. Depth passes without a pixel shader would still write full depth inside the band, so they would need a discard-only pixel shader forced in, at the call site Community Shaders owns. That is route (a) with a smaller job per shader; it is not cheap, and it was not built.

## Cost estimate (not measured)

- Per pass of a hooked class while active: one thread check, two atomic loads, 24 bytes of render state, one bound test. Tens of nanoseconds.
- Per cut pass: an owner walk (a virtual call per node), one table lookup, two `VSSetShader`, two `VSSetConstantBuffers`. One `Map` per frame.
- Per vertex of a cut draw: one `dp4` and one `mad`.
- Once per patched shader: about 4 kB parsed and rebuilt, one `CreateVertexShader`; at most 195.
- GPU: fewer fragments shaded above the plane, more behind it.
- Under the 60 fps cap none of this would show in p95 (wave 1 lesson); `micros` is the direct number for the CPU part.

## Tests

Commands are those of the `camera-portable` CI job (`.github/workflows/build.yml`), run from the worktree with `RUNNER_TEMP` in scratch, g++ 15.2.0, Python 3.14.4:

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
| 15 portable C++ tests (14 existing, `fixed_spike_shader_policy_test` new) | **PASS**, exit 0, every test printed `… PASS`. Without the archive the new test prints `archive rows skipped: SKYCRPG_SHADER_ARCHIVE is not set`, as it will in CI. |
| The new test with `SKYCRPG_SHADER_ARCHIVE` set to the shipped archive | **PASS**: `archive: 16044 blobs, 16044 checksums reproduced, 3390 vertex shaders; lighting ok=70 pattern=8 skinned=49; utility ok=125 pattern=53 skinned=161; 195 patchable entries, 84 distinct bytecodes; world-z product confirmed in 50+20 lighting shaders` |
| Wiring audit, six sections (five existing, `shader cut wiring` new) | **PASS**, exit 0 |
| `git diff --check` | **PASS** |
| g++ `-fsyntax-only -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` of `fixed_spike_shader.h` against scratch stand-ins for the Windows, REL and RE declarations it uses | 0 warnings. A parse and type check of the adapter only; **not** an MSVC or CommonLib build. |
| vkd3d-shader over 195 original and 195 patched blobs (assembly and SPIR-V) | **PASS**, as above |
| Ten hand mutations in a scratch copy (audited check dropped, engine shader not put back, no main-thread check, commit never called, install at kDataLoaded, view test dropped, slot constant changed, runtime allocated in every mode, setup slot written first, skin check dropped) | each failed the wiring audit |
| Windows `/W4 /WX` build, DLL hash | **NOT_RUN** |

`fixed_spike_shader_policy_test` covers: codes 66..79 distinct and inside their own mask; the audited constants; settings validation; the snapshot and the view test (tolerances, mirrored, tilted, non-finite); the plane constant (kept under, removed above, zero never cuts); selection (skinned, bad bound, floor, table, chest-high wall, ceiling, tall wall, one-piece mesh, other room's rock, whole view and radius, owner rules, cap); the checksum against seven independent reference values covering both final-block cases; the patcher on synthetic shaders of both class shapes (exact expected token stream, sizes, signature element, name offsets, checksum, a patched shader is not patched again); the patcher's refusals (container, checksum, chunk order, version, skinned, existing clip or cull distance, register 16 used, broken signatures, buffer 13 declared, double `dcl_temps`, late declaration, unknown opcode, undeclared World row, undeclared position temp, malformed lengths, differing World rows, wrong operands, missing sum, foreign instruction, early `ret`, no view-projection, inside `if`, `call`, unbalanced flow, every single-bit corruption of the header and chunk table); the trace row, including that the longest row fits one 768-byte trace row; and, with the archive, every number in the shader-archive audit above.

## Game rows: all NOT_RUN

| Row | Result |
| --- | --- |
| Hooks install under Proton, no FCS-67..70 | NOT_RUN |
| A patched vertex shader is accepted by the device (no FCS-73) and by DXVK's translator | NOT_RUN |
| The cut engages in the main view (`cut > 0`, no FCS-76) | NOT_RUN |
| Embershard Mine, coc arrival, four headings: player and floor visible, rock above the plane gone | NOT_RUN |
| Bleak Falls Barrow corridor (2048,-9216,-2556), heading 225, seed mode 1 | NOT_RUN |
| Bannered Mare hearth: cross beams removed, floor whole | NOT_RUN |
| Depth passes consistent: no grey or black hole where the player should be | NOT_RUN |
| Lifecycle: F6, F8, menu, load door, death; frames native while ineligible; no FCS-72 | NOT_RUN |
| Frame cost in Embershard | NOT_RUN |
| Shadows, fog cards, water, decals, actors above the plane | NOT_RUN |

## Route plan (for the routes package; nothing here was generated or run)

Build name `shader`. Overrides common to every route: `iSeeThroughMode=3`, `iShaderCutTrace=1`, `iRigTrace=1`; other shader-cut keys at their defaults unless stated. The wave 1 routes named below write `iSeeThroughMode` themselves, so the override must win or the routes need a `shader` variant. Every route should fail on any `code=FCS-66` .. `code=FCS-79` line except FCS-75 (informative: record it, do not fail), on `latched=1`, `open>0` or `offMain>0` in any `FixedShader` row, and should require `installed=1`.

| # | Route (base) | Overrides | Looks and assertions |
| --- | --- | --- | --- |
| 1 | `d-cave-dwemer` cave part: EmbershardMine01 coc arrival, headings 45/135/225/315. **The one-screenshot proof.** | `iRoomSeedMode=0` | Screenshot per heading against `batch-p55-d-cave-dwemer-A2-00/d-cave-h0.png` (dark rock filling the frame, player not visible). At each pose: `active=1`, `cut>0`, `planeZ` = feet z + 160 within the `CutPlane` band (24), `relPlane` finite, `otherView` < `passes`. Owner looks at: player visible, whole floor, walls up to head height, the cut edge, what is left hanging (fog, moss), shadows. `byClass` second number tells whether depth passes were cut. |
| 1b | same, with a radius | `fShaderCutRadius=768` | Same assertions; `skipOut>0` expected. Compares the whole-view cut with the limited one. |
| 2 | `c-bfb-seed1`: BleakFallsBarrow01 corridor pose, four headings | `iRoomSeedMode=1` | Screenshot `c-bfb-h2` (heading 225) against `batch-p55-c-both-seed1-hide-A2-00/c-bfb-h2.png` (a rock pillar and roots between camera and player). `cut>0` at heading 225; room trace unchanged in kind from wave 1 (claims present). |
| 3 | `b-mare` hearth part: WhiterunBanneredMare (-117.110,-640,68), four headings | none | Screenshot `b-hearth-h0` against wave 1 hide off (`batch-p55-b-mare-A-00/b-hearth-h0.png`) and hide on (`batch-p55-d-mare-A2-00/d-bm-ground-h0.png`): are the large cross beams gone, is the floor whole (`skipBelow>0` for floor-level pieces), are patrons whole (`skipActor>0`), what does the loft look like cut at 160. |
| 4 | `e-lifecycle` and `e-door` | none | At each exit (F6 first person, F8, menu, load door, death and reload): the `FixedShader` rows written while ineligible have `active=0 cut=0`; after return `active=1` and `cut>0` again at the same pose; no FCS-72 in the whole log; screenshots while ineligible show the uncut scene. |
| 5 | new `f-cave-shader` and `f-cave-base`: Embershard arrival, traces off, the f capture procedure | shader route as 1 with `iShaderCutTrace=0`; base with `iSeeThroughMode=0` | p95 frame time against the base at the same pose and heading, after a warm-up of at least 10 s so that shader creation is over; one extra short run with `iShaderCutTrace=1` to read `cut`, `peak`, `created` and `micros` per window. |
| 6 | optional: a staircase walk (Bleak Falls Barrow or the Bannered Mare loft stairs) | none | `planeZ` steps with the feet; no flicker while standing; screenshots at the bottom, half way and the top. |

## What can go wrong in the first game run, and how it shows

| What | How it shows |
| --- | --- |
| A guard fails at install | one FCS-67..70 line; the installed line says `shaderCut=pending` and no later `shader cut installed` line appears; rendering native |
| `d3d11.dll` is reported outside the system directory under Proton | FCS-67 `d3d11.dll outside the system directory`; nothing installed. The check is then wrong for this host and needs relaxing. |
| The device or DXVK refuses a patched shader | FCS-73 with the hash and HRESULT; `shaders=0,N,…`; `cut=0`, `skipShader` high. DXVK may instead accept the shader and fail at pipeline creation: then selected pieces vanish entirely or the log of DXVK carries an error. A selected piece that disappears completely, floor included, is this case. |
| The published view never matches the render state | FCS-76 after 120 frames with both eyes and the direction; `cut=0`, `otherView` ≈ `passes` |
| Rendering is not on the main thread | FCS-71; `offMain` grows; `cut=0` |
| Owner lookup finds nothing for statics | `skipOwner` high, `cut=0` |
| The main view's depth pass uses a vertex shader that is not patchable | colour is cut, depth is not: the hole shows the cleared target instead of the floor. `byClass` Utility at 0 with `skipShader>0`, and FCS-75 names a hash. |
| The patched shader reads a World row that is not what the audit says for some permutation | the cut surface is tilted or at the wrong height for those pieces only; visible in the screenshot. Lighting is cross-checked against the shaders' own world position; Utility is not. |
| A draw path opens a pass and does not close it | FCS-72, mode off for the session, scene native afterwards |
| A crash in a hook | The offsets dereferenced are in "Binary audit" 1, 2 and 4: state `+0x348`, object `+0x08/+0x10/+0x68`, pass `+0x10`, geometry `+0xE4/+0xF0/+0x130`, node vtable `+0x28`, `+0x30`, `+0xF8`. A wrong one is a bad pointer read on the first candidate pass. The independent review should re-check 100FCAB/100FD8A, 100FEF7, 1560377 and 2EED25. |
| Wrong vtable offset for a device method | a crash or a corrupted device state at the first cut pass or at install (`CreateBuffer`). Each offset is one the engine uses (Binary audit 3). |
| Cost | `micros` per window; `created` shows when shader creation is over; cap `iShaderCutMaxPerFrame` |
| Visual | everything under "Expected artefacts" |

## Open questions for the owner

1. Whole view (default) or a radius around the player (`fShaderCutRadius`)? Route 1/1b shows both.
2. Actors standing above the plane are drawn whole and float. Leave them, or hide actors whose feet are above the plane?
3. Should fog cards, light shafts and other effect meshes be cut too (332 Effect vertex shaders fit the same patch), and wind-animated statics (a wider patcher)?
4. Is the hard edge acceptable, or is the fade wanted at the cost described under "What a fade would take"?
5. Shadows of removed ceilings stay, so the lighting does not change. Wanted, or should shadow maps be cut as well (lights above the plane would then light the floor)?
6. Cut height 160 and the stepwise follow on stairs: acceptable, or should the plane track the feet continuously?

## Proposed shared-file changes (not made; outside this package's ownership)

- `docs/evidence/p5.5/fixed-camera-spike.md` (records package): add see-through mechanism "shader cut (vertex-stage clip distance)" pointing to this file; the refusal table should list FCS-66..79.
- `docs/phases/PHASE_5_5.md`, P5.5-02 see-through mechanisms: the shader route is no longer research only; its smallest sound form is a vertex-stage clip distance (195 vertex shaders), not a pixel discard. Game result NOT_RUN.
- `session-p55/research/shader-discard.md`: its estimate ("patching about 6,900 Lighting pixel shaders … an extra pixel shader in depth passes") holds for a pixel discard; candidate (c) of the wave 2 brief avoids both. Its statement that pixel-shader bytecode is released after creation was not re-checked here and is not needed.
- `briefs/wave2-context.md`: codes above 63 do not fit the once-mask in `fixed::refuse` (`1 << code`); this package keeps its own.
- `session-p55/routes`: a `shader` build with the overrides above; checker rows for the `FixedShader` assertions in the route plan; `FixedShader` lines are a new trace kind.
- When the native packages are combined: `fixed_spike_policy.h` must accept modes 2 and 3 (`> 3` instead of this branch's `> 1 && != 3`); `fixed::configure` gains one settings parameter per package; the clip package and this one both write slots 6 and 7 of the Lighting and Utility vtables, which is safe because only the selected mode has a runtime and each guards the slot value before writing; `shader::publish` needs the same `viewer`/`position`/`parent` that `endFrame` already has.

## Second pass: void colour and Effect-class cut (2026-11-10)

Base `4a3b722`, same worktree and branch. Input: batch W2S on `4a3b722` (seven routes, all clean; the cut works in Embershard Mine, the Bannered Mare and Bleak Falls Barrow; frame cost +0.13% p95; orchestrator records under `session-p55`), the owner's look at `session-p55/results/wave2-full/shader-cave.png`, and the owner decision of 2026-11-10 ("I'll go with your recommendations to start and see how they look"): (1) void colour pure black to start; (2) fog cards and light shafts above the cut removed; (3) with ENB or Community Shaders present everything keeps being refused exactly as the cut is now. Specification: `session-p55/research/out-of-bounds-look.md`, options 1 (variant 1a only) and 6 (Effect class). Option 1b (Sky fog colour), the post pass and everything else in that memo are **not** built; what this work saw that bears on them is listed at the end of this section.

### What was built

| File | Change |
| --- | --- |
| `modules/camera/src/fixed_spike_shader_void.h` (new) | The void colour: `hookClearColor` (the relay's target), `relayPage` (memory for the relay within reach of the call), `installVoid` (guards, relay, the one 4-byte code write). Included at the end of `fixed_spike_shader.h`. |
| `modules/camera/include/fixed_spike_shader_effect_audit.h` (new, generated) | 114 FNV-1a-64 hashes: the Effect vertex-shader bytecodes the patcher may touch. Hashes only. |
| `modules/camera/include/fixed_spike_shader_policy.h` | Effect as a third shader class with its own guard values; groups (0 Lighting+Utility, 1 Effect); codes FCS-80..88 and the per-group preparation codes; `bShaderCutEffect`, `bVoidColor`, `sVoidColor` and the colour parser; `publishes` (the cut's activation rule, now one function); the void call site's audited bytes, `callTarget`, `callDisplacement`, `relayReach`, `relay`, `voidSiteCheck`; `Look` and `lookDecision`; the `FixedShaderLook` row. The `FixedShader` row's format string is unchanged. |
| `modules/camera/src/fixed_spike_shader.h` | Per-group counters, budgets, off-thread counters and audited lists; `slotGuard`/`effectGuard`; `installEffect`; commit calls `installEffect` and `installVoid` after the cut is installed and logged; FCS-83/88 in `frameStart`; the look row in `trace`; 2048 shader-cache entries (1,072 vertex shader objects in the three classes). |
| `modules/camera/src/fixed_spike.h` | One more line budget (`shaderLookLines`), the second emitter, writer rows ×2 for the shader trace. |
| `modules/camera/src/plugin.cpp` | Three keys read inside the existing gate. |
| `modules/camera/config/SkyCRPG_Camera.ini` | The three keys, documented. |
| `modules/camera/tests/fixed_spike_shader_policy_test.cpp`, `tests/lock_refresh_wiring_test.py` | New rows below; a seventh wiring section. |

**Off equals `4a3b722`.** Outside mode 3 there is no shader runtime, so nothing of the second pass exists (the keys are read and ignored). In mode 3 with `bShaderCutEffect=0` and `bVoidColor=0`: no Effect slot is written, the call site is not touched, the `FixedShader` row has the same format and the same counts as on `4a3b722` (Lighting and Utility only), and the only additions are one log line (`shader look installed void=off … effect=off`) and the `FixedShaderLook` rows when tracing. One behavioural difference exists even then: the Lighting/Utility shader cache has 2048 entries instead of 1024 (fewer `skipShader` from a crowded table; none were seen in W2S).

### Re-verified in the binary (SkyrimSE.exe 1.7.104, same hashes and Address Library as above)

Method: `objdump -d -M intel` over the whole image (scratch, deleted after use); IDs resolved forward and reverse through the Address Library file; function extents from the image's own `.pdata`; vtables, RTTI and bytes read with a small PE reader. The memo's "verified" list was used as leads; each fact below was re-derived.

**Void colour**

| Fact | Evidence |
| --- | --- |
| ID 36558 = RVA 656D10, ID 77249 = 100A3F0 (`SetClearColor`), 77250 = 100A460 (`RestoreClearColor`), 77251 = 100A4C0, 77247 = 100A050, 36559 = 656E60. | Source: Address Library. |
| Bytes 656D8B..656DAF: `f30f101de5d0d702 f30f1015d9d0d702 f30f100dcdd0d702 488d0d96b1cd02 e841369b00` = `movss xmm3,[3D3E78]`, `movss xmm2,[3D3E74]`, `movss xmm1,[3D3E70]`, `lea rcx,[3331F40]` (renderer, ID 411393), `call 100A3F0`. Alpha: 656D7D..656D85 stores 1.0 (RVA 1B5BE28) to `[rsp+0x20]`. | Source. These 36 bytes are `voidSiteBytes`; the call's displacement `0x009B3641` is checked by test. |
| `SetClearColor` (100A3F0..100A459): saves the old colour from `rcx+0x27D8..0x27E4` to 3331F20..2C, then stores `xmm1`, `xmm2`, `xmm3` (via `xmm5`) and `[rsp+0x28]` to `rcx+0x27D8..0x27E4`. So it is `void(Renderer*, float r, float g, float b, float a)` in the Windows x64 convention (fourth float in `xmm3`, fifth argument on the stack at `[rsp+0x28]` on entry), which is exactly how `hookClearColor` is declared and how it calls the original. First 16 bytes `f30f1081d8270000 0f28ebf30f11051d` (`clearColorPrologue`). | Source. |
| The fog-colour global 3D3E70..78: writers 0FFB9E/0FFBA6 (initialisation), 41227F/41228B (ID 26229), 4126C5/4126D1 and 4127DE/4127EE (ID 26230); the **only** readers are 656D8B/656D93/656D9B. No other instruction in `.text` references 3D3E70..3D3E7B (the three floats). *Corrected in the third pass after review (MINOR-1): an earlier version of this row said 3D3E60..3D3E8F; other addresses in that wider window are referenced, so only 3D3E70..3D3E7B is claimed.* | Source: every RIP-relative operand in the disassembly. Nothing here writes the global. |
| ID 36558 has three callers: 656834 and 656A78 in ID 36555 (the frame function), 656CC8 in ID 36557. No absolute pointer to it exists in the image (only its `.pdata` entry). ID 36555's callers: ID 36564 (659319), IDs 19121/19122 (2C65DA, 2C6890) and 1A2B13 (function 1A28F0, no ID found). ID 36559 has one caller, 656E2D in ID 36558. | Source. |
| **Where the colour goes.** Inside ID 36559, which 36558 calls right after the call at 656DAA: 65752C sets (0.5, 0.5, 0, 0), clears the target returned by 153C900 (ID 77251) and restores (657563) the colour back; 65757E binds render target 1 to slot 0 and **65758A clears it with the colour set at 656DAA** (77251 reads `renderer+0x27D8`); **6575E5 sets (0, 0, 0, 0)** (`xmm6` is zeroed at 657057 and not written again before) and nothing restores it in that call. No jump from before 657511 lands after it, and there is no `ret` in 656E60..657750. | Source. So the substituted colour is used by one explicit clear, of the main colour target, and does not outlive the call to 36559; that it is not also used by a pending-clear flush (ID 77247 reads `renderer+0x27D8` at 100A127 for slots set to clear) between 656DAA and 65752C is **Hypothesis**: the functions 36559 calls before 657511 were not all followed, nor were indirect calls. The binary review (2026-11-10, MINOR-2) found one unpaired pending-clear flush path, **6571B3 -> 1541755**, between the two: if it flushes a render target that was marked "clear" with the renderer's colour, that target clears with the void colour (black) instead of the fog colour while the cut is active. Worst case a shadow map or the water-reflection target clears black; compare shadows and water reflections with the cut on and off in game. **Hypothesis**, not seen in W3. |
| Explicit render-target clears with the renderer's colour (ID 77251) are called only at 657557, 65758A, 657634 (ID 36559) and 153E7AA..153E826 (ID 107199, which sets and restores its own colour). | Source: call scan. |
| `SetClearColor` has 32 call sites in 28 functions. 22 functions pair it with `RestoreClearColor` (they set their own colour and put the previous one back). Unpaired: ID 16335, ID 36555 (65697F, black, after the world render), ID 36558 (656DAA, ours), ID 36559 (6575E5, zero), ID 39039, ID 77484, ID 105674, and 1546853 (a chunk whose restore is in another chunk, 1546C0F). Each passes its own colour; none reads 3D3E70. | Source. |
| Other views. The water reflections are rendered from 36559's first part through ID 107140 (65721A), which sets and restores its own colour. The local map, the inventory and magic-menu 3D scenes and loading screens are not rendered through ID 36558 (its callers above); while any menu is open the rig is ineligible, so the cut's `active` is false and the hook passes the engine's colour anyway. A save-game screenshot taken while the cut is active, if the engine renders it through 36555 (IDs 19121/19122 look like such a path), would show the black void the player sees. | Calls: Source. Which engine function draws each named view, and the screenshot path: **Hypothesis**. |
| Thread. The call at 656DAA runs in ID 36558, inside the frame function ID 36555, from which the main view's Lighting/Utility passes are also drawn; in W2S every `FixedShader` row of the five traced routes (cave, cave-radius, mare, bfb-seed1, lifecycle; 3,933 rows) has `offMain=0`, and the clip build's Effect hooks ran with `offMain=0` (W2C). | W2S/W2C: Source (route logs). That the call runs on the same thread: **Hypothesis** with that support; checked on every call (FCS-83). |

**Effect class**

| Fact | Evidence |
| --- | --- |
| Effect vtable ID 255194 = RVA 1B48AB0; its complete-object locator (1C810C8, signature 1, offset 0) names `.?AVBSEffectShader@@`; slot 6 = 1557090 (ID 107527), slot 7 = 1557F50 (ID 107528); first 16 bytes `488bc44889501048894808555356488d` and `48895c2408488974241048897c24180f`. Lighting and Utility re-read the same way: values unchanged. | Source: `.rdata` and `.text`. |
| Effect `SetupGeometry` builds the World matrix with 15170C0 (`translate − posAdjust`, first-pass audit 4) at 15572EF from the geometry's world transform (`+0x7C`), or at 155729D from the same transform with an origin offset (geometry `+0x110..0x118`) when shader flag bit 12 is set, and uploads it at 1557381 through 15A9C78; the previous-frame World at 155734F with the previous `posAdjust` swapped in. Flag bit 3 skips both (with skinned geometry: Hypothesis). | Source: 1557090..15573B6. So the World the Effect vertex shader positions with is relative to the same `posAdjust` the plane constant uses, whichever branch built it. |
| Effect `RestoreGeometry` (1557F50..155814D) only updates render-state fields and dirty bits; it binds no shader and calls nothing. | Source. |
| Archive: **606 Effect vertex shaders, 332 patch, 274 skinned, 0 rejected for shape; 114 distinct bytecodes**, none also on the Lighting/Utility list. World is 3×4 in `cb2[0..2]` in all 332 (World z row `cb2[2]`). In **196** of them the shader itself already computes `dp4 …, cb2[2], rP` with the same position register (a world-position output), the product the patch adds; the other 136 rest on the algebra, as Utility does. | Source: archive test. |
| Effect pixel shaders: 3,216; highest input register 10; no clip or cull distance input anywhere. Register 16 (the patch's output) is above all of them. | Source: input signatures of every blob. |
| vkd3d-shader (same library as the first pass): all 332 original and 332 patched Effect blobs compile to D3D assembly and SPIR-V without a message; `BuiltIn ClipDistance` is decorated in 332 of 332 patched and 0 originals; one flipped byte is refused with `E0003: Invalid DXBC checksum`. | **PASS** offline. Windows driver and DXVK: NOT_RUN, as in the first pass. |
| In game the Effect slots 6/7 bracket held in the main view: the clip build `79297e1` with `iClipShaders=7` (route `p55-clip-cave-effect`, batch W2C) clipped 7 Effect passes per window at the Embershard arrival with no refusal and `offMain=0`. | Source: that run's log. |

### Hook sites, with bytes

| Site | Before | After | Guard |
| --- | --- | --- | --- |
| Call at RVA **656DAA** (ID 36558 +0x9A) | `e8 41 36 9b 00` (call 100A3F0) | `e8 <rel32>`: only the 4-byte displacement changes, to a 14-byte relay `ff 25 00 00 00 00 <hookClearColor>` in our own page (allocated free within ±2 GiB of the call, sealed `PAGE_EXECUTE_READ` before the write) | `voidSiteCheck`: ID 36558 → 656D10 (check 1), ID 77249 → 100A3F0 (2), 36 bytes 656D8B..656DAF exact (3), the call's target is `SetClearColor` (4), `SetClearColor`'s first 16 bytes (5); then FCS-82 if no relay page (cause 1) or the relay cannot be reached or sealed (cause 2). Any failure: FCS-81/82, the call site untouched, the cut unchanged. |
| Effect vtable 1B48AB0, slot 6 | 1557090 | `hookSetup<2>` | `effectGuard`: ID 255194 → 1B48AB0 (check 1), slot values (2, 3), first 16 bytes (4, 5). Failure: FCS-80, Effect native, the cut unchanged. |
| Effect vtable 1B48AB0, slot 7 | 1557F50 | `hookRestore<2>` | same |

The relay and the call site are written once, in the main-thread `commit`, after every first-pass guard. They are never undone: when the cut is not active `hookClearColor` calls `SetClearColor` with the engine's own registers and stack argument, so the engine's behaviour is the same apart from one extra call frame.

### How actor-attached effects and effects at or below the plane are kept

1. **At or below the plane.** A clip distance removes only what lies above the plane: every vertex at or under it is kept by construction. A piece whose bound does not reach above the plane is not even touched (`fxSkipBelow`).
2. **Owners.** Effect passes go through the same `consider` as Lighting passes: `ownerOf` walks up as `FindReferenceFor3D` does (first-pass audit 4). No owning reference (temporary hit and impact effects, detached particle systems, anything not under a reference's fade node) → never cut (`fxSkipOwner`). The owner is an actor, the player included (`ActorCharacter`) → never cut (`fxSkipActor`): spell art and shader effects attached to an actor, equipped weapons and their enchantment glows, a held torch, nocked or quivered ammunition (the walk looks past ammunition to the carrier). Only plain placed references (`FormType::Reference`) with the wave 1 kinds are cut; projectiles, explosions, hazards and lights (a wall torch, a candle or a brazier whose base is a Light, with whatever glow its model carries) are excluded kinds (`fxSkipKind`).
3. **Skinned** geometry is never selected, and the 274 skinned Effect vertex shaders are refused by the patcher anyway.
4. **Only the main view** (the render state's eye and direction equal the published camera): the inventory and magic-menu previews, the local map, reflections and shadow maps are other views (`fxOtherView`).
5. **Only audited bytecode**: an Effect pass is patched only from the Effect list; anything else is drawn natively and counted (`fxShaders` third number, FCS-86 once).

That spell art and enchantment glows are attached under the actor's 3D, and that a dropped torch is a Light reference, are engine behaviour known from modding practice, **Hypothesis** here. A spell effect on a placed object (for example a lit brazier activator, or a shrine's glow) is cut above the plane like that object's own meshes, so object and glow end at the same height.

### Gating

Both features follow the cut's `active` flag, which `publish` sets only through `publishes(installed, latched, interiorCell, feetZ)` on an eligible camera frame (third person with the rig on: `rigEligible`), and which every ineligible frame and every lifecycle exit clears (`deactivate`, unchanged). So outdoors, first person, the rig off (F8), every ineligible state (menus included), loads, cell changes, death and after any refusal latch (FCS-72, 77, 79) both are native. The void decision per call is `lookDecision(onMain, active, latched, freshView)`: off-main → engine colour (FCS-83); latched → engine colour; not active → engine colour; snapshot older than 0.25 s (the camera service stopped renewing it) → engine colour; otherwise the configured colour. Effect passes use exactly the Lighting gate (latched, thread, nested pass, stale, view, bound, owner, shader, cap). ENB and Community Shaders: `commit` refuses with FCS-67 before anything is installed, so neither feature exists (owner decision 3).

### Settings (`[FixedCameraSpike]`, read only when `bEnabled=1`, used only in `iSeeThroughMode=3`)

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `bShaderCutEffect` | 1 | 0/1 | also cut Effect-class passes (fog cards, light shafts, effect-shader particles), same rules |
| `bVoidColor` | 1 | 0/1 | while the cut is active the main view clears to `sVoidColor` instead of the cell's fog colour |
| `sVoidColor` | `0,0,0` | three numbers 0..1, comma separated, spaces allowed | linear colour before image space (tonemap and eye adaptation apply to anything but black). A value outside 0..1 is clamped; anything unreadable (two or four numbers, `#000000`, `nan`, words) is black; either is reported once as FCS-66 |

`iShaderCutShaders` (1-3) still selects Lighting and Utility only; `iShaderCutMaxPerFrame` applies to each group separately.

### Log line (once, after `shader cut installed`)

```
FixedCameraSpike shader look installed void=%s color=%.3f,%.3f,%.3f effect=%s effectAudited=%u
```

`void` and `effect`: `off` (key 0), `installed`, `refused` (FCS-80, or FCS-81/82). If the cut itself never installs (FCS-67..70) this line is not written and the look rows say `pending`. The `shader cut installed` line is byte-identical to `4a3b722`.

### Trace: `FixedShader` unchanged, `FixedShaderLook` new

`FixedShader` keeps the format string of `4a3b722` byte for byte (`byClass=L,U`, 35 fields in the same order) and now counts **Lighting and Utility only**, as on `4a3b722`: Effect passes are not in `passes`, `cut`, `byClass`, `peak`, `shaders`, `created`, `micros`, `offMain`, `stale`, `otherView` or any `skip*`. One nuance: `uploads` counts constant uploads made for Lighting/Utility passes, so after an Effect pass has uploaded the frame's constant a Lighting pass may not need to and the number can be lower than on `4a3b722`.

New kind, written right after each `FixedShader` row (same `t` and `frame`, same 0.1 s window, its own budget of 6,000 lines):

```
FixedShaderLook t=%.3f frame=%llu void=%s voidApplied=%llu voidInactive=%llu voidLatched=%llu voidStale=%llu voidOffMain=%llu voidColor=%.3f,%.3f,%.3f nativeColor=%.3f,%.3f,%.3f effect=%s fxPasses=%llu fxCut=%llu fxPeak=%u fxShaders=%u,%u,%u fxCreated=%llu fxMicros=%.1f fxOffMain=%llu fxOtherView=%llu fxStale=%llu fxSkipSkin=%llu fxSkipBound=%llu fxSkipBelow=%llu fxSkipOut=%llu fxSkipOwner=%llu fxSkipActor=%llu fxSkipKind=%llu fxSkipShader=%llu fxSkipCap=%llu sample=window
```

Example (test row): `FixedShaderLook t=1234.500 frame=99 void=installed voidApplied=6 voidInactive=0 voidLatched=0 voidStale=0 voidOffMain=0 voidColor=0.000,0.000,0.000 nativeColor=0.377,0.439,0.400 effect=installed fxPasses=40 fxCut=7 fxPeak=7 fxShaders=2,0,1 fxCreated=2 fxMicros=31.0 fxOffMain=0 fxOtherView=3 fxStale=0 fxSkipSkin=0 fxSkipBound=0 fxSkipBelow=12 fxSkipOut=0 fxSkipOwner=0 fxSkipActor=4 fxSkipKind=0 fxSkipShader=0 fxSkipCap=0 sample=window`. Longest possible row 694 of 768 bytes (test).

Legend: `void`, `effect`: `off`, `pending`, `installed`, `refused`. `voidApplied`: main-view clear-colour calls in the window that got the configured colour; `voidInactive`, `voidLatched`, `voidStale`: calls that kept the engine's colour and why; `voidOffMain`: calls off the main thread (cumulative). `voidColor`: the colour applied (nan if nothing was applied in the window). `nativeColor`: the engine's own colour at the last call in the window, whatever was decided (nan if no call): the cell's fog colour × 0.8333. `fx*`: the Effect group, each field with the meaning of the `FixedShader` field of the same name (`fxShaders` = patched, rejected, unaudited Effect shaders, cumulative; `fxPeak` = most Effect passes cut in one frame of the window). Every token is `name=value`; the colours are three comma-separated numbers or `nan,nan,nan`.

For the checker (`session-p55/routes/check_spike_log.py`, not edited here): register `FixedShaderLook` in `OPTIONAL_KINDS` (the generic parser keeps the colour triples and the `fxShaders` triple as strings) and, if wanted, add `'FixedShaderLook': 6000` to `CAPS`. The existing `FixedShader` parser needs no change.

### Refusal codes (second pass)

| Code | Site | Meaning, and what stays in force |
| --- | --- | --- |
| FCS-80 `check=N` | `installEffect` | Effect vtable ID (1), slot 6/7 value (2, 3) or first bytes (4, 5) differ; Effect native, the cut unchanged |
| FCS-81 `check=N` | `installVoid` | void call site: ID 36558 (1), ID 77249 (2), the 36 bytes (3), call target (4), `SetClearColor` prologue (5); call site untouched, engine colour, the cut unchanged |
| FCS-82 `cause=N` | `installVoid` | no relay memory within reach (1), or the relay out of reach / not sealed (2); call site untouched |
| FCS-83 | `frameStart` | the void hook ran off the main thread; those calls kept the engine's colour |
| FCS-84 | `prepare` (Effect) | the device refused a patched Effect vertex shader; that shader native |
| FCS-85 | `prepare` (Effect) | audited Effect bytecode not patched; native. Not expected |
| FCS-86 | `prepare` (Effect) | an Effect pass used bytecode not on the Effect list; native. **Informative**, like FCS-75 |
| FCS-87 | `prepare` (Effect) | an Effect vertex shader object with no device shader or an impossible size; native |
| FCS-88 | `frameStart` | an Effect hook ran off the main thread; those calls touched nothing |

FCS-72 (an open pass), FCS-76 (no view match) and the install guards FCS-66..70 are shared: an Effect pass left open latches the whole mode like a Lighting one. FCS-76 is computed from Lighting/Utility passes only, as before. All 23 codes 66..88 fit the shader runtime's own 32-bit once-mask.

### Tests

Commands as in "Tests" above, plus the archive run. Results on this host (g++ 15.2.0, Python 3.14.4):

| Check | Result |
| --- | --- |
| 15 portable C++ tests | **PASS**, every test printed `… PASS` (`fixed_spike_shader_policy_test` prints `longest FixedShaderLook row: 694 of 768 bytes` and, without the archive, `archive rows skipped`) |
| `fixed_spike_shader_policy_test` with `SKYCRPG_SHADER_ARCHIVE` | **PASS**: `archive: effect ok=332 pattern=0 skinned=274; 332 patchable entries, 114 distinct bytecodes; world-z product of its own in 196`, and the first-pass line unchanged (`lighting ok=70 … 84 distinct bytecodes; world-z product confirmed in 50+20 lighting shaders`) |
| Wiring audit, seven sections | **PASS**, exit 0 (`shader cut wiring` now checks one site per FCS-66..88; new `shader second-pass wiring`) |
| `git diff --check` and the new files | **PASS** (no whitespace errors) |
| g++ `-fsyntax-only -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` of `fixed_spike_shader.h` (with the void header) against scratch stand-ins for the Windows, REL and RE declarations it uses | 0 warnings. Not an MSVC or CommonLib build. |
| vkd3d-shader over 332 original and 332 patched Effect blobs | **PASS**, above |
| Twelve hand mutations in a scratch copy (void colour applied unconditionally; snapshot read off the main thread; engine arguments changed on the native path; Effect/void install order; `bShaderCutEffect` ignored; Effect feeding the FCS-76 check; Effect patched from the Lighting list; latch ignored in `publishes`; decision order; code written before the original is published; relay sealed read-write-execute; Effect off-thread reported as FCS-71) | each failed the wiring audit |
| Windows `/W4 /WX` build | **NOT_RUN** |

New portable rows: codes 66..88 distinct and contiguous, per-group preparation codes disjoint; the Effect constants; the Effect list sorted, unique, 114, disjoint from the Lighting/Utility list, each group patched only from its own list; defaults (both on, black) and `classEnabled`; `sVoidColor` parsing (exact, spaces, exponent, clamped high and low, 13 unreadable forms all black) and its FCS-66 report; **the gating table**: every camera state × interior/exterior × installed × latched × thread × fresh snapshot, with the void applied exactly when the state is eligible (normal or target lock), interior, installed, not latched, on the main thread and fresh; outdoors, non-finite feet, F8 (disabled), first person and menus never publish; **the call-site check** at three image bases (exact bytes pass; each of the 36 bytes flipped, a redirected call, a moved ID 36558 or 77249, a detoured `SetClearColor` each refused with its check number; the displacement is the binary's `0x009B3641`); rel32 reach both ways and the relay window; the relay bytes; the `FixedShader` row still `byClass=L,U` with an Effect count present; the `FixedShaderLook` row's fields, nan forms and longest length; with the archive, every Effect number above.

### First-run protocol (for the orchestrator; nothing here was run)

Build `shader` at the commit of this pass. Route: the existing `p55-shader-cave` (EmbershardMine01 coc arrival, headings 45/135/225/315, the four screenshots `s-cave-h0..h3`) with `iSeeThroughMode=3`, `iShaderCutTrace=1`, `iRigTrace=1`, `iRoomSeedMode=0`, and the second-pass keys at their defaults (absent, or `bShaderCutEffect=1`, `bVoidColor=1`, `sVoidColor=0,0,0`). Compare each screenshot with `session-p55/results/wave2-full/shader-cave.png` (W2S, same pose).

Expected log lines, in this order, once each:

```
FixedCameraSpike installed rig=1 … seeThrough=3 shaderCut=pending …
FixedCameraSpike shader cut installed shaders=3 height=160.0 radius=0.0 cap=4096 trace=1 audited=84
FixedCameraSpike shader look installed void=installed color=0.000,0.000,0.000 effect=installed effectAudited=114
```

No `code=FCS-66..88` line except FCS-75 or FCS-86 (informative: a LOD, wind or other variant in view). In the trace at each heading, after settling:

- `FixedShader`: as in W2S at the same pose (`installed=1 active=1 latched=0`, `cut` about 308 and `byClass` about 189,119 at heading 45 in W2S, `open=0 offMain=0`). These rows must not change in kind; a large change means the Effect separation is wrong.
- `FixedShaderLook`: `void=installed effect=installed`; `voidApplied` equal to the frames in the window (about 6 at the 60 fps cap) and `voidInactive=voidLatched=voidStale=voidOffMain=0`; `voidColor=0.000,0.000,0.000`; `nativeColor` the cell's grey-green fog colour (not black), the same at every heading; `fxCut>0` at least at heading 45 (the halo there; the clip build cut 7 Effect passes per window at this pose); `fxShaders` first number > 0 and second 0; `fxOffMain=0`; `fxSkipActor`/`fxSkipOwner` may be non-zero.
- While ineligible (the lifecycle route, or any step that opens a menu): `voidApplied=0` and `voidInactive>0` in those windows, and the screenshot taken while ineligible shows the grey-green void (engine colour).

What each screenshot should show: the floor, the player and the lit wooden structures on **black** where the grey-green void was; the cut edges of dark rock fading into the black so the paper-thin shells and dark slivers mostly stop showing; the **blue halo at heading 45 (h0) gone**; lit beams and posts **still ending sharply at the plane** (hard cut, no cap; that is expected); the translucent brown card at h2/h3 gone if it is an Effect mesh above the plane. Then, if the owner wants more evidence, the same keys on `p55-shader-mare`, `p55-shader-bfb-seed1` and `p55-shader-lifecycle`.

### Possible side effects to look for

| What | Why | How it shows |
| --- | --- | --- |
| A grey-green tint on distant geometry | Material fog: shaders still fade distant surfaces toward the cell's fog colour (the Effect vertex shader above computes its fog factor from clip-space distance and blends between two colours of its own constant buffer, `cb0[1]` and `cb0[2]`). Only the clear colour changed. | Far walls and floors greyish against black. Fix would be option 1b (not built). |
| A brighter or darker exposure | Eye adaptation (`ISHDRDownSample*LightAdapt`) averages the frame; half the frame turning black lowers the average, so the scene is probably brightened | The cave looks brighter than in W2S at the same pose; possibly blown highlights on lit beams. Hypothesis. |
| Bloom halo around lights on black | Bloom is image space, unchanged | Soft glows around torches remain, more visible on black. |
| Effect meshes of placed objects cut at the plane | Same rule as their Lighting meshes | A glow or flame of a placed object above the plane ends at the plane with it. |
| A black void in the save-game screenshot | If the save screenshot renders through ID 36555 while the cut is active (IDs 19121/19122 call 36555) | Cosmetic but **persisted in the save file's thumbnail** (review MINOR-3). Hypothesis; check a quicksave thumbnail taken with the cut active. The third pass's sky hide applies to that frame too. |
| First-use hitches | Up to 114 more patched shaders created on first use | `fxCreated` > 0 in the first windows. |

### What this work saw that bears on 1b and the post pass (not built)

- **1b.** The Effect vertex shaders take their fog colours from their own constant buffer 0 (`cb0[1]`, `cb0[2]`, blended by a factor from `cb0[0]` and the clip-space distance; Source: disassembly of the first patched Effect blob), not from the global 3D3E70, so changing the clear colour cannot reach material fog; 1b would have to change what fills those constants (the Sky's fog fields, per the memo; not re-checked). The global 3D3E70 is derived each frame by IDs 26229/26230 and read only for the clear colour, so overriding it would never have reached material fog either.
- **Post pass.** The main colour target is cleared inside ID 36559 at 65758A and every later clear in that call uses (0, 0, 0, 0); a pass inserted after ID 36558 returns (656839 or 656A7D in ID 36555) runs after the whole world render, before image space (ID 105676 calls at 6568D9/6569F2), as the memo proposes. ID 36555 itself sets the clear colour to black at 65697F before an image-space pass in one branch. 65752C clears the target from 153C900 to (0.5, 0.5, 0, 0) before the main pass, which looks like a normal buffer's "no normal" value (Hypothesis): a post pass that wants surface orientation could read it.
- With the void black, a "void as cap" in a post pass would draw over black, so the order of the two does not matter for the look.

### What the reviewer must check

1. `fixed_spike_shader_void.h`: the relay encoding (`ff 25 00000000` + 8-byte address), that only the 4 displacement bytes at 656DAB are written, that `r.clearColor` is published before the write, that the relay is sealed `PAGE_EXECUTE_READ` before the write, and `hookClearColor`'s signature against the call site (rcx, xmm1..3, `[rsp+0x28]`) and against `SetClearColor` at 100A3F0.
2. `voidSiteBytes` against the binary at 656D8B..656DAF and `clearColorPrologue` at 100A3F0.
3. That nothing else consumes the colour between 656DAA and 6575E5 (the Hypothesis row above): the calls in ID 36559 before 657511 (657C90, 657930, 658700, 1537FD0, 1539A70, 153A630, …).
4. The Effect vtable values and bytes (1B48AB0, 1557090, 1557F50) and that Effect `SetupGeometry` uploads the World built by 15170C0 (15572EF, 155729D) to the slot the patched shader reads as `cb2[0..2]`.
5. That `FixedShader` is unchanged: `format` in the policy is untouched, and only group-0 counters feed it.
6. That `relayPage` cannot loop forever and never allocates outside the reach (`relayReach`, then `callDisplacement` checked again before the write).
7. MSVC `/W4 /WX`: `std::array<std::atomic<unsigned long long>,2>` value-initialised, `std::from_chars` for `float`, no shadowing (`onMainThread`, `bytes`, `value`).

### Unverified (NOT_RUN or Hypothesis)

- Every game row: the hooks under Proton, DXVK accepting the patched Effect shaders, the look, the side effects, frame cost. **NOT_RUN.**
- That the void pixels are exactly the clear colour (memo: strongly supported by the per-cell colours).
- That nothing but the main colour target clear uses the substituted colour (see the table).
- That the call at 656DAA runs on the main thread (checked at run time, FCS-83).
- That `SKSE`'s other plugins on the pinned stack do not touch 656D8B..656DAF (the guard refuses if they do).
- Which engine functions draw the local map, the 3D menus and loading screens.
- That spell art and enchantment glows hang under the actor's 3D.
- Windows `/W4 /WX` build and DLL hash.

### Proposed shared-file changes (not made)

- `session-p55/routes/check_spike_log.py`: `OPTIONAL_KINDS = frozenset({'FixedShaderLook'})`; optionally `CAPS['FixedShaderLook'] = 6000`.
- `session-p55/routes/package_checks.py`: `shader_faults` should take FCS-71..88 except the informative FCS-75 and FCS-86; S1-like install checks should require exactly one `^FixedCameraSpike shader look installed void=installed .* effect=installed ` line and fail on FCS-80..82; new look rows: `voidApplied>0` and `voidInactive=0` in active windows, `voidApplied=0` in ineligible windows, `fxCut>0` at the cave heading 45, `fxShaders` second number 0.
- `session-p55/routes`: route variants with `bShaderCutEffect=0`/`bVoidColor=0` reproduce `4a3b722` (A/B for the owner).

## Third pass: what paints the void, and the wedge (2026-11-10)

Base `6397406`, same worktree and branch. Input: batch W3 on `6397406` (all clean; runs `session-p55/routes/runs/p55-shader-*-W3`, screenshots `session-p4/autotest/runs/batch-p55-shader-*-W3-00/screenshots/`, sheets `session-p55/results/wave3/`). The override installed and applied (`voidApplied=7` per window, `voidColor=0.000,0.000,0.000`), yet void pixels were not black. Method as in the earlier passes: `objdump` of the functions named below (scratch, deleted), the Address Library, a PE reader for bytes, RTTI and `.pdata`; the shader archive disassembled with vkd3d-shader (`libvkd3d-shader.so.1.19.0` of the Steam Linux Runtime, through ctypes, target D3D assembly); CELL, LGTM and IMGS records read from the five masters the way `p55-instrument/tools/coverage/cell_census.py` reads them.

### Measured (screenshots, sRGB 0..255; my own pixel count over every fourth pixel)

| Cell (W3) | Void, features off | Void, black clear | Uniformity | `nativeColor` (W3 trace) = fog far × 0.8333 |
| --- | --- | --- | --- | --- |
| EmbershardMine01 | (77,91,85), 43% of the frame at h0 | (19,39,47), 52% at h0, every void pixel identical | exact | 0.399,0.369,0.275 (template MineTemplateMedium fog far (122,113,84)) |
| WhiterunBanneredMare | not run | (32..34, 47..49, 54..55) | ±1 | 0.288,0.271,0.232 (template WhiteRunIntLightingTemplate fog far (88,83,71)) |
| BleakFallsBarrow01 | not run | (47..52, 53..57, 58..63), a vertical gradient (s-bfb-h3: 47 at the top, 49 at the bottom of column 5) | gradient | 0.245,0.324,0.317 and 0.314,0.451,0.441 (cell fog far (75,99,97); the second while the camera sees the other lighting) |

The brief's "nativeColor 0.245,0.324,0.317 in the Mare" is Bleak Falls Barrow's; the Mare's is 0.288,0.271,0.232 (Source: the three W3 logs).

Two facts constrain the cause. (1) The residual is **cool** (blue > green > red) in all three cells, while every fog colour these cells use is warm or neutral; with the features off the warm clear colour plus the residual shows as grey-green. (2) In Bleak Falls Barrow 3-4% of all pixels are exactly (0,0,0), and they lie on geometry (s-bfb-h0: x 38..1922, y 76..1108, inside rock): nothing adds a constant to every pixel after the scene, so the residual is added to the **void pixels only**, before or at image space.

### Candidates excluded (Source: shader archive and binary)

| Pass | Why it is not the residual |
| --- | --- |
| Cell fog in image space, `ISSAOCompositeFog` / `ISSAOCompositeSAOFog` (fxp entries 16002/16004; 16000, the third SAO composite, has no fog) | `fog = min(pow(saturate(dist·cb2[0].y − cb2[0].x), cb2[0].z), cb2[0].w)`, colour `lerp(cb2[1], cb2[2], fog)`, `lerp(scene, colour, fog)·cb2[1].w`, applied only where `depth < 0.999999` (16002: `lt r4.x, r3.z, l(9.99998987e-01)` then `movc`; 16004 the same at its line 75). Void depth is 1.0: never fogged. Its other additive term (`cb2[9].yzw`, noise-modulated) is weighted by `1 − smoothstep(distance from cb2[5].xyz / cb2[8].y)`, 0 beyond `cb2[8].y`; that this covers the far plane is Hypothesis (the constants were not read). |
| Tonemap `ISHDRTonemapBlendCinematic(Fade)` (entries 15846/15848) | Hue-preserving tonemap, then saturation, tint (`lum·tint`, 0 for black), `avg + contrast·(brightness·c − avg)` (a grey lift only when contrast < 1) and the global fade colour. Nothing per pixel by depth, and BFB's pure-black geometry pixels prove no global lift. |
| Depth of field "Fogged" variants (15828/15830) | They blur; no colour is added. |
| The sky in interior mode 1 | Sky::Update for mode 1 (412DF0, ID 26232) copies the fog-far colour into sky upper, lower, horizon and sun (413285..4132E1): warm, not cool; and SetMode hides the root in mode 1 anyway (below). |

### The pass that paints the void: the sky dome of a Show Sky interior (Source, except where marked)

1. **Mode.** When an interior cell attaches, the mode chooser at 1A3F40 (called from 1A2F55; exteriors: 1A36AE..1A371E, same rule) reads `cell+0x40` (CELL DATA flags): no 0x80 (Show Sky) → `Sky::SetMode(1)`; 0x80 without 0x100 (Use Sky Lighting) → mode 2 ("sky dome only"); both → mode 3. **EmbershardMine01 (0x83), BleakFallsBarrow01 (0x83) and WhiterunBanneredMare (0xA1) all have 0x80 and not 0x100: mode 2.** 271 of 759 interior cells in the masters carry 0x80 (one of them also 0x100). *(Corrected after the binary review of 8f1248f: an earlier text named 36E660 (ID 22824) here; that is the ToggleSky console command's handler, which applies the same flag rule. The call 1A2F55 → 1A3F40 was re-read for this correction; the rest of the chooser is the reviewer's reading.)*
2. **The dome is shown.** `Sky::SetMode` (ID 26222, 411660): `r14b = (mode − 2) ≤ 1`; at 411794..4117C7 the root (`Sky+0x08`) gets `flags+0xF4` bit 0 (`kHidden`) cleared when `r14b && !(Sky+0x1DC & 0x80)` (HideSky), set otherwise. So mode 1 interiors draw no sky; modes 2 and 3 do.
3. **Its colours are the exterior weather's.** Sky::Update in mode 2 (413384..413491) fills all 17 sky colours from the current and last weather (`Sky+0x48/+0x50`) through 414CF0, then (413778..41387D) replaces only sunlight (+0xD8), ambient (+0xCC), fog near (+0xB4) and fog far (+0x138) from the cell's lighting (or the lighting override). Sky upper, lower and horizon stay the weather's: blue at day, which explains the cool residual and why it does not follow any cell record. The clear colour is still fog far × 0.8333 (412681..4126D1), which is why `nativeColor` matches the cell.
4. **It covers exactly the void.** Every BSSkyShader vertex shader (fxp entries 60..68) writes `o0 = r1.xyzz` (z = w, depth 1.0 after the divide); the dome technique (VS 68, PS 77) outputs the blend of three colour rows by the vertex weights (`cb2[13..15]`) times `cb2[12].w`, plus `cb2[0].y` and a dither, alpha `v.w·cb2[13].w`. Geometry is nearer than 1.0 everywhere it exists, so only void pixels receive it.
5. **Formula (Hypothesis for the blend):** HDR void = α·dome + (1 − α)·clear, then the cell's image space. The clear colour's visible contribution (features off vs on) shows α < 1 over the lower dome that a camera pitched down 55° sees; the BFB gradient is the dome's vertex-colour gradient. With black clear the void is α·dome, the weather's lower-sky colour, scaled by each cell's eye adaptation and imagespace (BFB `BleakFallsBarrowImagespace01`: saturation 0.95, contrast 1.2, tint 0.25 toward (0.67,0.76,0.98), which is why BFB's residual is greyer and brighter; Mare `WhiteRunIntIS`: contrast 1.22; Embershard `DefaultImageSpaceDungeon`: all 1, no tint).

What would refute it: a mode 1 interior (no Show Sky, e.g. `WhiterunJorrvaskrBasement` 0x21 or `WhiterunDragonsreachBasement` 0x21) whose void is not exactly the clear colour on the build `6397406`. That is the cheapest in-game check of this section (NOT_RUN).

### What was built

| File | Change |
| --- | --- |
| `include/fixed_spike_shader_sky_policy.h` (new) | The audited Sky facts (singleton pointer ID 401652 = 31E0D80, `SetMode` ID 26222 = 411660, its 47-byte head and the 57-byte show/hide rule at 411794), field offsets (`root` +0x08, `mode` +0x1BC, `flags` +0x1DC, HideSky 0x80, NiAVObject flags +0xF4, hidden bit 1), `skySiteCheck`, `engineShowsSky`, the per-frame `skyStep` table, `wholeConstant`, `wholeEffect`, and the `FixedShaderVoid` row. Included at the end of `fixed_spike_shader_policy.h`. |
| `src/fixed_spike_shader_sky.h` (new) | `installSky` (guards, FCS-89) and `settleSky` (the one engine write: the root's hidden bit). Included at the end of `fixed_spike_shader.h`. |
| `include/fixed_spike_shader_policy.h` | FCS-89, FCS-90; `bVoidHideSky` and `bShaderCutEffectWhole` in `Config`. Format strings of `FixedShader` and `FixedShaderLook` untouched. |
| `src/fixed_spike_shader.h` | `ownerSkip` also reports the owner's form ID; `consider` keeps the bound and, for an Effect pass that `wholeEffect` selects, uploads `wholeConstant`; `commit` calls `installSky` after the second-pass line and logs one more line; `settleSky` from `publish`, the main-thread branch of `deactivate`, and `frameStart` after a latch; the `FixedShaderVoid` row in `trace`. |
| `src/fixed_spike.h` | One more line budget (`shaderVoidLines`), the third emitter, writer rows ×3 for the shader trace. |
| `src/plugin.cpp`, `config/SkyCRPG_Camera.ini` | Two keys inside the existing gate, documented. |
| `tests/fixed_spike_shader_policy_test.cpp`, `tests/lock_refresh_wiring_test.py` | New rows below; an eighth wiring section. |

**Off equals `6397406`.** Outside mode 3 nothing of this exists. In mode 3 with `bVoidHideSky=0` and `bShaderCutEffectWhole=0`: no engine byte is written beyond the second pass's, the `FixedShader` and `FixedShaderLook` rows are byte-compatible and count the same, and the additions are one log line (`shader void installed sky=off effectWhole=off`) and the `FixedShaderVoid` rows when tracing.

### Sky hide: hook sites, bytes, rules

No code is patched. Guard (`installSky`, main thread, in `commit` after the void colour): ID 401652 → 31E0D80 (check 1), ID 26222 → 411660 (check 2), `SetMode`'s first 47 bytes `40 57 48 83 ec 30 81 89 dc 01 00 00 00 01 00 00 8d 42 fe 83 f8 01 48 89 6c 24 50 8b 81 bc 01 00 00 8b ea 4c 89 74 24 28 48 8b f9 41 0f 96 c6` (check 3: the HideSky/mode fields and the mode-2/3 rule), and 411794..4117CC `45 84 f6 74 11 8b 87 dc 01 00 00 c1 e8 07 a8 01 75 04 b2 01 eb 02 32 d2 48 8b 4f 08 48 85 c9 74 18 8b 81 f4 00 00 00 84 d2 75 05 83 c8 01 eb 03 83 e0 fe 89 81 f4 00 00 00` (check 4: root at +8, its flags at +F4, bit 0). Any difference: FCS-89 `check=N`, the sky is left to the engine, the cut and the void colour unchanged.

Per main-thread frame (`settleSky(want)`, `want` = the cut published this frame): read `Sky*` from 31E0D80 (never calling the accessor, which would create it), its root, mode and flags, and the root's flags. Then:

| Wanted, root present, engine shows the sky (mode 2/3, no HideSky) | Step |
| --- | --- |
| we hold another root | `leave` (the old one is not touched again; the next frame hides this one) |
| hidden bit clear | `hide`: set bit 0, remember the root |
| bit set and we hold it | `keep` |
| bit set, not ours | `none` (someone else hid it; never restored by us) |
| **Otherwise** (not wanted, no root, or the engine itself hides the sky) | |
| we hold nothing | `none` |
| same root, the engine's rule says shown, bit still set | `restore`: clear bit 0 (only when not wanted) |
| anything else | `leave`: forget the hold, bit as the engine left it (FCS-90 once, informative) |

So the bit is set only on frames that published the cut, and put back on the first main-thread frame that did not (any ineligible frame, a menu, F8, first person, outdoors, a latch, a lifecycle exit seen on the main thread), and only when SetMode's own rule still says "shown". A cell change runs SetMode, which rewrites the bit for the new cell; the hold is then dropped without touching it. The hidden state is one the engine produces itself (mode 1 interiors, or HideSky).

Side effects (Hypothesis): while the cut is active, every view that culls the sky root in a Show Sky interior loses the sky too (a water reflection of the sky in such a cell shows the clear colour); a save-game thumbnail taken in that frame shows the black void.

### The wedge at heading 135 (s-cave-h1)

Identified from the W3 trace and the cell's records (Hypothesis for the exact reference; the new trace names it):

- Shader class **Effect** (it went away with the Effect cut where the whole glow was above the plane: the round blue halo at h0; at h1 in the features-off shot the same blue glow sits over the corridor).
- It **was cut**, not skipped: every active cave window has `fxPasses=84 fxCut=14 fxSkipBelow=56 fxSkipOwner=14 fxShaders=2,0,0 fxSkipShader=0`, i.e. per frame 2 Effect passes cut, 2 ownerless, 8 below the plane, 0 unaudited. What remains is the part **under** the plane, by design: a camera-facing card cut by a horizontal plane ends in a straight edge, bright against the dark void.
- References: the only Effect-model statics near the arrival pose (player 2487,1485,9241; plane 9420.25) are two **FXGlowFillRoundBrt** (`Effects\Ambient\FXGlowFillRndLitBrt.nif`): **0008F94E** at (2449,1197,9392), 291 units from the player, 28 units under the plane, and **0008F94D** at (2196,859,9423), on the plane; two cut passes per frame match them. The other candidates are a Light (`DefaultSunlightBounceAmb01NS` 0008F94C, never cut) and `FXMistLow01` (0008FB47, MSTT at 9153, far under the plane; its particles are a likely source of the 2 ownerless passes, Hypothesis).

Decision: **cut whole**. A selected Effect pass (one the cut would clip, with all its rules: plane reach, owner, kind, audited bytecode, budget) whose bound centre lies above the midpoint between the feet and the plane (`wholeEffect`: centre z > (planeZ + feet z) / 2, 9330.6 here) is drawn with the constant (0, −1), so its clip distance is −1 for every vertex and nothing of it is rasterised. Both glows (centres 9392 and 9423) go whole; floor mist and a fire in a floor brazier (centre lower) keep the plane cut, so their lower part stays. `bShaderCutEffectWhole=0` returns to the plane cut. The patched Effect shaders and the constant buffer are the second pass's; nothing new is bound.

### Settings (`[FixedCameraSpike]`, read only when `bEnabled=1`, used only in `iSeeThroughMode=3`)

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `bVoidHideSky` | 1 | 0/1 | while the cut is active, the sky root carries the hidden bit (no dome over the void in Show Sky interiors) |
| `bShaderCutEffectWhole` | 1 | 0/1 | a selected Effect pass centred above the midpoint of feet and plane is not drawn at all instead of being cut at the plane |

### Log line (once, after `shader look installed …`, which is unchanged)

```
FixedCameraSpike shader void installed sky=%s effectWhole=%s
```

`sky`: `off` (key 0), `installed`, `refused` (FCS-89); `pending` cannot appear here. `effectWhole`: `on` when the key is 1 and the Effect hooks installed, otherwise `off`. If the cut itself never installs (FCS-67..70) this line is not written.

### Trace: `FixedShaderVoid` (new kind; `FixedShader` and `FixedShaderLook` unchanged byte for byte)

Written right after each `FixedShaderLook` row (same `t` and `frame`, same 0.1 s window, its own budget of 6,000 lines):

```
FixedShaderVoid t=%.3f frame=%llu sky=%s skyMode=%d skyHeld=%u skyHides=%llu skyKeeps=%llu skyRestores=%llu skyLeft=%llu skyEngineHidden=%llu skyNone=%llu fxWhole=%s fxWholeCut=%llu fxCutRefs=%s sample=window
```

Example (test row): `FixedShaderVoid t=1234.500 frame=99 sky=installed skyMode=2 skyHeld=1 skyHides=1 skyKeeps=5 skyRestores=0 skyLeft=0 skyEngineHidden=0 skyNone=0 fxWhole=on fxWholeCut=14 fxCutRefs=0008F94E,0008F94D sample=window`. Longest possible row 391 of 768 bytes (test).

Legend: `sky` as in the log line (`pending` before the cut installs). `skyMode`: the Sky's mode at the last main-thread frame (−1 no Sky; 1 interior, 2 dome only, 3 full). `skyHeld`: 1 if the root carries our hidden bit at the end of the window. `skyHides`, `skyKeeps`, `skyRestores`, `skyLeft`: main-thread frames in the window by step (above); `skyEngineHidden`: active frames in which the engine itself hid the sky (mode 1 or HideSky: nothing to do); `skyNone`: active frames with no Sky or no root. `fxWhole`: as `effectWhole` in the log. `fxWholeCut`: Effect passes clipped whole in the window (they are also in `FixedShaderLook fxCut`). `fxCutRefs`: the first four distinct owners (form IDs, 8 hex digits) of cut Effect passes in the window, comma separated, or `none`.

For the checker: register `FixedShaderVoid` in `OPTIONAL_KINDS` (the generic parser keeps `fxCutRefs` as a string), optionally `CAPS['FixedShaderVoid'] = 6000`; add FCS-89 to the shader faults and treat FCS-90 as informative like FCS-75/86 (a cell change while held logs it once).

### Refusal codes (third pass)

| Code | Site | Meaning, and what stays in force |
| --- | --- | --- |
| FCS-89 `check=N` | `installSky` | Sky singleton ID (1), SetMode ID (2), SetMode head bytes (3), show/hide rule bytes (4) differ; the sky is left to the engine; cut and void colour unchanged |
| FCS-90 | `settleSky` | informative: the sky's root, mode or HideSky changed under our hold (a cell change, or the engine hid it); the hold is dropped and the bit left as the engine set it |

All 25 codes 66..90 fit the shader runtime's 32-bit once-mask.

### Tests (g++ 15.2.0, Python 3.14.4, this host)

| Check | Result |
| --- | --- |
| 15 portable C++ tests (the CI loop: `-std=c++20 -Wall -Wextra -Werror -pedantic`) | **PASS**, exit 0 each; `fixed_spike_shader_policy_test` prints `longest FixedShaderVoid row: 391 of 768 bytes` and `sky and whole-Effect rows PASS` |
| `fixed_spike_shader_policy_test` with `SKYCRPG_SHADER_ARCHIVE` | **PASS**, archive lines unchanged (`effect ok=332 …`, `16044 blobs, 16044 checksums reproduced …`) |
| Wiring audit, eight sections | **PASS**, exit 0 (new `shader third-pass wiring`; `shader cut wiring` now one site per FCS-66..90) |
| g++ `-fsyntax-only -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` of `fixed_spike_shader.h` (with the void and sky headers) against scratch stand-ins for the Windows, REL and RE declarations | 0 warnings. Not an MSVC or CommonLib build. |
| `git diff --check` | **PASS** |
| Windows `/W4 /WX` build | **NOT_RUN** |

New portable rows: codes 89/90 contiguous after 88; defaults (both on); the sky facts and that the guarded bytes contain the very loads and stores the offsets name (`mov eax,[rcx+1BC]`, `mov eax,[rdi+1DC]`/`shr 7`, `mov rcx,[rdi+8]`, `[rcx+F4]` `or 1`/`and FFFFFFFE`); `skySiteCheck` at three image bases with every guarded byte flipped (47 + 57), a moved ID, a jump written over SetMode; `engineShowsSky` for modes 0..5 × flag patterns; **every one of the 64 input combinations of `skyStep`** against the table, with the invariants (hide only when wanted and shown by the engine; restore only when not wanted, held, same root, shown, bit set; nothing when neither held nor wanted); a frame sequence (hide, keep, menu restore to the exact flags, a door into a mode 1 interior, back, HideSky under the hold, a new root, no Sky, a root hidden by someone else); `wholeConstant` gives −1 at every height and is not a plane constant; `wholeEffect` with the Embershard numbers (both glows whole, the floor mist not, the boundary at 9330.6, NaN never); the `FixedShaderVoid` row exactly, its `none`/`off` forms, its four-reference limit and its longest length.

### Expected void pixel after the fix (NOT_RUN)

With the dome gone the void is the clear colour (black) through image space: tonemap(0) = 0; saturation and tint of 0 are 0; `avg + contrast·(0 − avg)` = avg·(1 − contrast) ≤ 0 for contrast ≥ 1, clamped to 0 (Embershard contrast 1.0, BFB 1.2, the Mare 1.22, reading CNAM as saturation, brightness, contrast: Hypothesis for the component order); fade alpha 0 unless an image-space modifier is running.

| Cell | Before (W3) | Expected after |
| --- | --- | --- |
| EmbershardMine01 | (19,39,47) | **(0,0,0)** |
| BleakFallsBarrow01 | about (48,54,60), gradient | **(0,0,0)** |
| WhiterunBanneredMare | (33,49,55) | **(0,0,0)** (if CNAM's third float is not contrast, at most a grey of a few levels) |

Exceptions to look for: bloom spill near bright lights (image space, unchanged); anything that draws at depth 1.0 besides the sky (none found); volumetric lighting, which is composited additively (`ISCompositeVolumetricLighting`, entry 15960: `vol·cb2[0].rgb`) and whose use in interiors was not traced (Hypothesis that it is off or negligible there; a uniform cool tint that survives the fix would point at it).

### First-run protocol (for the orchestrator; nothing here was run)

0. **First, same cell (added after the binary review of 8f1248f):** in `EmbershardMine01` with the cut active, console `ts` (ToggleSky, the handler at 36E660) hides the sky root: the void should become exactly (0,0,0) at once; `ts` again restores it. This tests the root cause in the very cell without a second interior or the new build.
1. Optional, cheapest, on the **current** stage `6397406`: one interior **without** Show Sky (`coc WhiterunJorrvaskrBasement` or `WhiterunDragonsreachBasement`, both 0x21) with `iSeeThroughMode=3`, four headings. Expected: void pixels exactly (0,0,0) already. This confirms or refutes the root cause without the new build.
2. On the build of this pass: `p55-shader-cave`, `p55-shader-mare`, `p55-shader-bfb-seed1`, `p55-shader-lifecycle`, keys at their defaults, `iShaderCutTrace=1`. Expected lines once each, in order: `shader cut installed …`, `shader look installed void=installed … effect=installed effectAudited=114`, `shader void installed sky=installed effectWhole=on`. No `code=FCS-` line except FCS-75/86 and possibly one FCS-90 after a door or load.
3. Trace while active in the three cells: `FixedShaderVoid … sky=installed skyMode=2 skyHeld=1`, `skyHides=1` in the first active window and then `skyKeeps` equal to the frames; `skyEngineHidden=0`. In the cave: `fxWholeCut≈14` per window and `fxCutRefs` naming 0008F94E and/or 0008F94D (if other IDs appear, the wedge attribution above is wrong; report them). `FixedShader` and `FixedShaderLook` as in W3.
4. Screenshots: void pixels (0,0,0) (the checker's void-pixel share at ±6 of black near the share of void in W3: about 0.52 at cave h0, 0.39 at h1); no light wedge at cave h1; floor, player, lit structures as in W3; geometry pixels unchanged compared with W3 at the same pose (diff only where the void was).
5. Lifecycle: in every ineligible window `skyHeld=0` and a `skyRestores=1` (or `skyLeft=1` across a door/load) in the window where the cut went inactive; screenshots taken while ineligible show the sky as before (the W3 lifecycle look).
6. A/B: `bVoidHideSky=0` reproduces W3's void; `bShaderCutEffectWhole=0` brings the wedge back.

### What the reviewer must check

1. In the binary: 31E0D80 is the pointer `Sky::GetSingleton` (1C7D00, ID 13878) returns and ID 401652 resolves to it; SetMode (411660, ID 26222) head and 411794..4117CC bytes; the mode choice (now attributed to 1A3F40, see the correction above); that SetMode is the only writer of the sky root's hidden bit (I did not scan for other writers of `[root+0xF4]`; the rule only puts the bit back where SetMode's own rule says "shown").
2. Mode 2 colours: 413384..413491 (weather), 413778..41387D (cell lighting replaces only sunlight, ambient, fog near/far).
3. `settleSky` runs only on the main thread (`publish`, `deactivate` under `thin::onMain()`, `frameStart`), before the frame's culling; the two writes are a read-modify-write of one dword at `root+0xF4`.
4. `consider`: `wholeEffect` uses the same world bound as `reach`; Lighting/Utility never take `wholeConstant`; the budget, owner and audit rules are unchanged for Effect.
5. `FixedShader`/`FixedShaderLook` format strings untouched (they are not in the diff).
6. MSVC `/W4 /WX`: `std::array<std::uint32_t,4>` in `VoidWindow`, the two literal `snprintf` formats in `formatVoid`, `bool` from `size_t && bool`.

### Unverified (NOT_RUN or Hypothesis)

- Every game row; the Windows build.
- That the dome is alpha-blended over the clear colour in its lower part (the clear's visible contribution says so; the blend state was not read).
- That hiding the root before the camera update removes the dome from the same frame's render (the engine's mode 1 uses the same bit; the frame order is the one the cut already relies on).
- That nothing else draws at depth 1.0 in these interiors (volumetric lighting in interiors not traced).
- The wedge's reference (the trace's `fxCutRefs` will say).
- The CNAM component order (affects only the Mare's prediction by a few levels).
- Water reflections and save thumbnails in Show Sky interiors while the cut is active.

## Fourth pass: Stage 1 of the seamless cutaway (2026-11-10)

Owner decision (verbatim, ledger 2026-11-10T22:35): "go I agree with your recommendations", the recommendations being `session-p55/research/seamless-cutaway.md` decisions 1-5. Built as CPU-side per-draw policy on the existing cut hook: **no new shader patch, no new hook, no new engine write, no new binary site.** Every in-game claim below is **NOT_RUN**.

### Rules as built (`iCutStyle=1`, `include/fixed_spike_shader_style_policy.h`)

Each Lighting/Utility pass of the published view finds its owning reference (the flat cut's `ownerOf`), and the reference gets one decision per frame from its world box (base OBND × the 3D root's world rotation, scale and translation, as wave 1's `extentOf`, here with all three rows), its kind (`kindOf`) and the first folder of its base model path:

| Order | Condition | Treatment | Counter |
| --- | --- | --- | --- |
| 0 | The cell is not a kit building (census below) | flat cut at feet + `fShaderCutHeight`, exactly the third pass | `flat` |
| 1 | Piece the policy cannot place: architecture form (Static, MovableStatic) whose model is not under `architecture\`, `clutter\` or `furniture\` (dungeon, landscape, effects, none), or with no OBND | flat cut | `fallback` |
| 2 | The storey above: box bottom ≥ storey line − 8, or a member whose bottom is ≥ held feet + 128 | removed whole; with `bCutKeepFarGallery=1` kept whole when on the back side | `removedWhole` / `removedObjects` / `keptGallery` |
| 3 | Object (object form types, or a static under `clutter\` / `furniture\`) or member (pillar, post, beam: middle span ≤ 128 and longest ≥ 2× it) whose box contains the feet in the ground plane | removed whole when overhead (bottom ≥ feet + 128); kept whole when its top is ≤ feet + 120; otherwise row 4 (fix pass, MINOR-2) | `keptObjects` / `keptMembers` / `removed*` |
| 4 | Other object or member | removed whole when not on the back side **and** in the lane; otherwise kept whole | as above |
| — | Over rows 2-4 (fix pass) | never removed: the floor under the player (MINOR-4) gets the flat cut; an interactable (MINOR-5) gets the flat cut at the plane | `fallback` / `interactCut` |
| 5 | Kit piece on the back side | kept whole (full height, never cut) | `keptFar` |
| 6 | Kit piece on the front (camera) side | sliced at the plane, as now (rule 3) | `slicedNear` (+`slicedLow`) |
| 7 | Kit piece across the player | sliced at the storey line; at the plane where it is in the lane | `slicedSpan` (+`slicedLow`) |

Never touched, as before: actors, skinned geometry, lights and excluded kinds, ownerless geometry, the Effect class (it keeps the third pass's rule, with the floor fix below).

**Near / far ("front" / "back").** The test is a separating plane through the player: a box wholly beyond it, on the side away from the camera, cannot meet any sightline from the camera to the player at any height, so keeping it whole can never hide the player. For a wall (one horizontal span ≤ 64 and the other ≥ 2.5× it, ≥ 128 tall) the plane is perpendicular to its thin axis: with the locked headings 45/135/225/315 (camera offset (±237.3, ±237.3, 683.8), wave 1) each world axis has a definite camera side, so an axis-aligned wall is front or back exactly, with no diagonal split and no drift. A wall in line with the player (thin range within 24 units of the feet, the wall itself to one side), every corner (an L-shaped corner has a square box), every diagonal, curved or blocky piece uses the plane perpendicular to the horizontal camera direction: front when the box's projection starts ≥ 24 units toward the camera, back when it ends ≥ 24 units away, across otherwise. A camera level with the player on an axis (mid-turn) makes both sides of that axis back (the sightline stays in the plane). Pieces that are both floor and wall, and pieces spanning both sides, come out "across", whose treatment is the old slice (the safe default: it is what showed the player in every W2S/W3 Mare screenshot); a piece with no box or an unknown model is flat-cut.

**No popping while walking.** A piece keeps its front/back class while the player stays outside its extent on that axis (clearance 0) and takes a new class only with 24 units of clearance; a heading snap (the camera's quadrant changes) starts without memory. The lane (two segments at feet + 20 and feet + 120, from 24 units toward the camera to the eye; sideways ground-plane margin 24 to enter, 48 to stay; fix pass) has the same hysteresis. The storey line has its own (below).

**Object versus architecture.** Form type first (`kindOf`, unchanged: Furniture, Activator, TalkingActivator, Container, Door, Tree, Flora, Misc, Weapon, Armor, Book, Ingredient, AlchemyItem, Scroll, Ammo, KeyMaster, SoulGem are objects; Static and MovableStatic architecture); then for architecture the model folder: `clutter\` and `furniture\` statics are objects (in the Mare 76 + 12 of 319 statics are: bar sets, chandeliers, pelts, wall baskets, the hearth fire `Campfire01Burning`, noble furniture statics); `architecture\` statics are kit pieces or members by shape. Pillars are kit pieces (`WRIntFreeWallColum01`, 62 × 73 × 292) and are recognised by shape, as are beams (`WRIntBeam512` 22 × 512 × 116) and the inner-corner posts (`WRIntWallCorIn01` 47 × 47 × 288). Source: Skyrim.esm placements read offline (below).

**The storey line ("ceiling").** From bounds, no ray: the lowest bottom of a kit piece (not a member) at least 128 wide in both horizontal spans, starting 128..768 above the held feet height (`CutPlane`, unchanged) and reaching within 512 units of the feet in the ground plane, collected while the previous frame was drawn. Mare hearth: the loft floor `WRIntLoftMid01`, bottom 310 (feet 68, plane 228); Mare loft pose: the ceiling `WRIntCeilMid01` at 555 (feet 321.5, plane 481.5). Unknown (no candidate): the line is the plane, so everything behaves as the flat cut except the back-side and object rules. Hysteresis: a new storey base (stairs, teleport: the `CutPlane` base moved, which itself has the 24/96/0.6 s hysteresis) or the first storey seen takes its value at once; within one storey a lower line found takes effect at once (more is removed); a higher line or losing every candidate takes effect after 0.6 s. After an ineligible gap the held line stays. **Stairs:** the line follows the `CutPlane` base: while the base is the ground floor, the loft floor is above the line and removed; once the base settles on the loft, the next storey up is searched. **Gallery:** with `bCutKeepFarGallery=1` the back-side part of what is above the line (gallery floor, railings, upper walls, overhead beams over the back half) is kept whole. **Never under an uncut ceiling:** every kit slab over the player within 512 is at or above the line and is removed; anything else over the player that is not wholly above the line is "across" and cut at the line, or at the plane when it meets the lane.

**Cells (caves and one-piece meshes).** Once per cell (first eligible frame, main thread, `TESObjectCELL::ForEachReference` as wave 1's scan) the placed statics are counted by model folder: a kit cell has ≥ 16 `architecture\` statics and more than `dungeons\` ones. Bannered Mare 212 vs 0 (kit); Embershard Mine 9 vs 622 and Bleak Falls Barrow 0 vs 1,444 (flat). In a flat cell every pass takes the third pass's path exactly (below-plane passes are not even looked up), so caves, ruins and their one-piece room meshes are untouched; a kit cell's pieces with a `dungeons\` model are flat-cut too (`fallback`). Retried for 120 frames when a cell has no statics yet, then flat.

**Whole-draw removal.** Confirmed: `usable()` gates only the plane constant built from `planeZ` before selection; `apply()` uploads whatever it is given, which is why the third pass's `wholeConstant` (0, −1) works without passing `usable`. Stage 1 uses `removeConstant` (−1, −1e9), which `usable` accepts: clip = −relZ − 1e9 < 0 for every height within ±1e8 (portable test). It lives in its own dynamic buffer created with that content (`D3D11_SUBRESOURCE_DATA`, never Mapped), so removals add no Map; the storey line has a second dynamic buffer Mapped only when its value changes; plane slices use the flat cut's own buffer and `apply()`. A removed reference loses every pass of every geometry, including geometries under the plane (which the flat cut never looks at), so it never shows a lower half.

**Shadows (rule 5).** Source: shadow-map rendering is `BSUtilityShader` work (technique bits 0x4000 `RenderShadowmap`, `shader-discard.md` Q1, RVA in that file), and the Utility class's vtable slots 6/7 are hooked, so shadow passes reach `consider`; they carry another render-state eye and direction (W3 Mare trace: `otherView≈10,300` per window at `otherEye=-900.8,-518.8,115.8 otherDir=1.00,0.00,0.00`), i.e. they are the "other view" passes. Built: with `bCutStyleShadows=1`, in a kit cell, a **Utility** pass of another view whose geometry pointer the main view removed whole in this frame or the previous one is drawn with the same patched shader and `removeConstant` (own per-frame budget, not counted on `FixedShader`). Sliced pieces keep their shadows. Using both frames makes it independent of whether shadow maps are drawn before or after the main view; **that order was not established** (Hypothesis either way), nor whether the shadow passes draw the same `BSGeometry` objects as the main view (if they do not, nothing happens: `shadowPasses=0`), nor whether every shadow technique's vertex shader is on the audited list (if not: one FCS-75 line and `shadowSkips`, drawn natively), nor whether interior shadow maps are re-rendered every frame. No new hook was needed. Reflections and other Utility other-view passes of a removed geometry are also clipped (consistent with "removed"); menus, the local map and inventory scenes are inactive frames and never reach this. If the run shows anything odd, `bCutStyleShadows=0` turns it off alone. Rule 5 is therefore **built, NOT_RUN**.

**Effect floor rule (binary review of 8f1248f, MINOR-3).** `wholeEffect` now also requires the bound's lowest point above the feet (`centre.z − radius > feet.z`); a tall Effect mesh centred high that reaches the player's level (a light shaft to the floor, a waterfall or spray card, a placed magic barrier) keeps the plane cut, so its floor-level part stays. This applies in both styles (it is a correction of the third pass, not part of the style). The Embershard glows (centres 9392 and 9423, feet 9241) still go whole while their radius is below 151 and 182; their radii were not measured: `FixedShaderStyle fxWholeRadius` reports the largest radius clipped whole per window and `fxFloorKept` the Effect passes the new rule kept at the plane (`FixedShaderVoid` is byte-identical). If the wedge at cave h1 returns, `fxFloorKept > 0` there says the glows reach the feet.

### Settings (`[FixedCameraSpike]`, read only with `bEnabled=1`, used only in `iSeeThroughMode=3`)

| Key | Default | Meaning |
| --- | --- | --- |
| `iCutStyle` | 1 | 0: the flat cut exactly as 8f1248f (no style runtime is allocated, no census, no line); 1: Stage 1. Other values: 1, reported as FCS-66 |
| `bCutKeepFarGallery` | 0 | 1: the back half of the storey above stays whole (A/B) |
| `bCutStyleShadows` | 1 | 1: geometry removed whole in the main view is clipped in Utility passes of other views too |

`bShaderCutEffectWhole` keeps its key; only its rule gained the floor condition.

### Log line (once, after `shader void installed …`)

`FixedCameraSpike shader style installed style=%u state=%s gallery=%u shadows=%u` — state `installed`, `refused` (FCS-91) or `off` (`iCutStyle=0`).

### Trace: `FixedShaderStyle` (new optional kind; only with `iCutStyle=1`; `FixedShader`, `FixedShaderLook` and `FixedShaderVoid` keep their formats byte for byte)

One row per 0.1 s window, after `FixedShaderVoid`, same `t` and `frame`:

```
FixedShaderStyle t=%.3f frame=%llu style=%u state=%s cell=%s kit=%u dungeon=%u gallery=%u shadows=%u frames=%llu storeyZ=%.2f relStorey=%.2f lineZ=%.2f keptFar=%llu keptObjects=%llu keptMembers=%llu keptGallery=%llu underLine=%llu slicedNear=%llu slicedSpan=%llu slicedLow=%llu slicedFar=%llu slicedObjects=%llu removedWhole=%llu removedObjects=%llu flat=%llu fallback=%llu refs=%u tableFull=%llu storeyMoves=%llu storeyMaps=%llu shadowPasses=%llu shadowRemoved=%llu shadowSkips=%llu fxFloorKept=%llu fxWholeRadius=%.1f interactCut=%llu sample=window
```

(`interactCut` was appended by the fix pass below; every earlier field keeps its order and format.)

Pass counts per window unless marked. `state` off/pending/installed/refused; `cell` unknown/kit/flat; `kit`/`dungeon` the census; `frames` published frames; `storeyZ` the held storey line (nan when none or inactive), `relStorey` = storeyZ − held feet height, `lineZ` the line in use (the plane when no storey; nan inactive). `kept*` drawn whole by rule; `underLine` straddling passes the line does not reach (drawn whole, nothing to cut); `slicedNear` / `slicedSpan` passes cut; `slicedLow` of those, cut at the plane; **`slicedFar`** passes given a slicing constant whose reference is on the back side (only flat and fallback paths can do this); **`slicedObjects`** passes given a slicing constant whose reference is an object; `removedWhole` (architecture) / `removedObjects` passes clipped whole; **`flat`** passes cut flat because the cell is not a kit building; **`fallback`** passes cut flat in a kit cell (unplaceable piece or a full reference table); `refs` references classified in the last frame; `tableFull` passes whose reference found no table slot (4,096); `storeyMoves` cumulative line changes; `storeyMaps` Maps of the storey buffer; `shadowPasses` other-view Utility passes of removed geometry, `shadowRemoved` of those clipped, `shadowSkips` not (unaudited shader or budget); `fxFloorKept`, `fxWholeRadius` as above. Longest row 760 of 768 bytes (portable test; ten-digit window counters, five-digit census, `refs` ≤ 4,096). On `FixedShader`, `cut` counts slices and removals of the main view; `skipBelow` also counts under-plane passes of references that are not removed; kept passes are on no `FixedShader` field. Checker rows proposed: in the Mare at every heading `cell=kit`, `slicedFar=0`, `slicedObjects=0`, `fallback=0`, `keptFar>0`, `removedWhole>0`, `storeyZ` finite with `relStorey≈242` at the hearth; in Embershard and Bleak Falls Barrow `cell=flat` and `FixedShader` counts equal to W3's.

### Refusal codes (fourth pass)

| Code | Site | Effect |
| --- | --- | --- |
| FCS-91 | `installStyle`: the device refused the storey or the remove buffer | style refused; the flat cut is drawn (iCutStyle=0 behaviour) for the session |
| FCS-92 | `uploadStorey`: Map of the storey buffer failed | latched: native rendering for the session (as FCS-77) |

### Offline game-data check (Source: Skyrim.esm, read-only, a scratch Python reader of CELL/REFR/base records; not committed)

The Mare's 690 placed references (319 statics) were placed with their OBND and run through a Python model of these rules at the hearth (−117.1, −640, 68), the doorway and the loft poses at the four headings (the model uses the z rotation only; x/y rotations, present on a few clutter references, are ignored there, not in the build). Hearth, every heading: storey line 310 (23 candidates); ~119 architecture and ~89 object references removed whole (loft floor, railings, roof, ceilings, beams, everything on the loft); 8-24 back-side kit pieces kept whole, 7-24 front-side walls cut at the plane, 1 across; 11 members (pillars, inner-corner posts) kept whole; ~50 objects kept whole, among them the hearth fire, which contains the pose (rule 3 keeps it; without that rule the fire would have been removed whenever the player stands at the hearth). No object or back-side piece sliced. Embershard 9 vs 622 and Bleak Falls Barrow 0 vs 1,444 statics by folder: flat. These are counts of placed references, not of what the engine draws (culling, LOD and multi-geometry references are not modelled).

### Tests (g++ 15.2.0, Python 3.14.4, this host)

| Check | Result |
| --- | --- |
| 15 portable C++ tests, the CI loop from the repository root (`-std=c++20 -Wall -Wextra -Werror -pedantic -pthread`) | **PASS**, exit 0 each; `fixed_spike_shader_policy_test` prints `longest FixedShaderStyle row: 760 of 768 bytes` (after the fix pass) and `style rules PASS` |
| `fixed_spike_shader_policy_test` with `SKYCRPG_SHADER_ARCHIVE` | **PASS**, archive lines unchanged |
| Wiring audit, nine sections | **PASS** (new `shader fourth-pass wiring`; `shader cut wiring` one site per FCS-66..92) |
| g++ `-fsyntax-only -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` of `fixed_spike_shader.h` (with the void, sky and style headers) against scratch stand-ins for the Windows, REL and RE declarations | 0 warnings. Not an MSVC or CommonLib build. |
| `git diff --check` | **PASS** |
| Windows `/W4 /WX` build | **NOT_RUN** |

New portable rows: codes 91/92; defaults and validation of the three keys; `removeConstant` negative at every height and `usable`; model folders (case, `meshes\`, `/`, near misses); `worldBox` with identity, a 90° turn with scale and an off-centre OBND, invalid inputs; wall/member shapes on Mare OBNDs; `pieceOf` for every kind and folder; **sides at all four headings** for axis-aligned walls north, south, west and east of the hearth (and that a back wall never meets the lane), a corner, a 45° diagonal piece, a spanning beam, a wall in line with the player, a camera level with the player; quadrants; **hysteresis** (kept while outside the extent, lost when crossed, reset by a snap, a stale class never kept); the lane and the segment-box test (numbers as revised by the fix pass); **every decision input combination** with the invariants (caves always flat; objects and members never sliced; nothing on the back side sliced) and the named cases (object on each side and in/out of the lane, a pillar in the way and behind, beams overhead, the a low piece around the player, a lamp over the player, upper floor, **gallery switch** on and off); slice heights; **the storey line** (candidates and exclusions, first value at once, within-tolerance, a higher one after 0.6 s, a lower one at once, losing candidates held 0.6 s, a new base at once, reset); the census on the three cells and its boundary; **style 0 cuts at the plane with the plane constant**; the `FixedShaderStyle` row, its nan forms and longest length; `FixedShaderVoid` unchanged by the new window fields; `wholeEffect` floor rule (shaft reaching the floor keeps the plane; glow boundaries 151/182).

### First-run protocol for the Bannered Mare (for the orchestrator; nothing here was run)

0. Same-cell sky check from the third pass first (above): `ts` in Embershard.
1. Route `p55-shader-mare` (hearth pose (−117.1, −640, 68), headings 45/135/225/315, W3 settle and screenshot steps) on this build, keys at their defaults (`iCutStyle=1`), `iShaderCutTrace=1`. Log: `shader style installed style=1 state=installed gallery=0 shadows=1` once after `shader void installed …`; no FCS-91/92.
2. Trace while posed: `FixedShaderStyle … cell=kit kit=212 dungeon=0`, `storeyZ≈310 relStorey≈242 lineZ≈310`, `slicedFar=0 slicedObjects=0 fallback=0`, `keptFar>0`, `removedWhole>0`, `removedObjects>0` at every heading; `storeyMoves` constant while standing; `shadowPasses`/`shadowRemoved` > 0 if the shadow passes draw the same geometry (0 means rule 5 did nothing: report it).
3. Screenshot per heading against `session-p55/results/wave3/mare-black-void.png` (same pose and headings). Expected differences: **back walls and back corners stand to full height** (≈ 252 units above the floor, top at 320) with their windows, shields, baskets and shelves whole, instead of ending flat at 228; **pillars stand whole with their modelled tops** where they are behind or beside the player, and vanish entirely where they stand in the line of sight; **no beam, rafter, loft floor, railing, roof or ceiling** anywhere (W3 already removed most of it by the plane; what changes is that nothing above 228 is half-drawn and the long beam shadows on the floor should be gone); doors and every object either whole or absent, never cut; **front walls look as in W3** (cut at the plane; most are not drawn from outside); the hearth fire, tables, benches, patrons and the player as in W3. The void black as in W3/8f1248f.
4. What I expect to remain visibly cut: kit pieces straddling the player that reach above the line (at the hearth only one corner piece, cut at 310, i.e. its top 10 units); front-side walls that are drawn from outside (double-sided partitions such as the free-standing walls `WRIntFreeWallStr01`), cut at 228 exactly as in W3; anything in a `dungeons\` folder. Likely new artefacts: objects hanging on an invisible front wall (wall baskets, mounted elk, banners) now stand whole in the void where W3 cut them at 228 (rule 2 keeps them unless they hide the player); a pillar or object popping out when the player walks into its lane (hysteresis 24/48); at the loft the back roof slopes stand whole as an A-frame back wall.
5. A/B rows: `bCutKeepFarGallery=1` at the hearth: the back half of the loft (floor edge, railing, loft walls, the beams over the back half) appears whole along the back walls; `bCutStyleShadows=0`: beam shadows return on the floor if rule 5 worked; `iCutStyle=0`: `FixedShader`, `FixedShaderLook`, `FixedShaderVoid` comparable to W3/8f1248f and no `FixedShaderStyle` line (the Effect floor rule is the one intended difference).
6. Then the loft (`loft` pose, line ≈ 555) and the doorway pose, and `p55-shader-cave`, `p55-shader-bfb-seed1` (expected `cell=flat`, `flat>0`, all else 0, look as W3) and `p55-shader-lifecycle` (no style decision while inactive; census redone after each door).

### What the reviewer must check

1. `consider`: with `iCutStyle=0` (`r.style` null, `styled()` false) the path is byte-for-byte the third pass's; the two new lines are the only entry points and both are `!group && styled()`.
2. `styledPass`: the flat cut's own selection (`reach`, owner rules, audited bytecode, budget) before any bind; under-plane passes are looked up only in kit cells and drawn natively unless removed; `apply()` for plane slices (same buffer, same Map rule); the storey buffer Mapped only on change; the remove buffer never Mapped.
3. `installStyle`: two `CreateBuffer` calls with the engine's dynamic description, the second with `D3D11_SUBRESOURCE_DATA` (16 bytes, `pSysMem` first) holding `removeConstant`; a refusal leaves the flat cut.
4. `shadowPass`: only Utility passes of other views, only for geometry pointers the main view removed in this or the last frame, only in kit cells; the bracket is closed by the existing `finish`; not counted on `FixedShader`.
5. `noteCell`: `ForEachReference` once per cell on the main thread, reading base form type and `TESModel::GetModel()` only (no `BSFixedString` construction); table and sets cleared on a cell change.
6. `refresh`: `Get3D()`, `world.rotate.entry` rows, `world.scale`, `world.translate`, `worldBound`, `boundData` (all as wave 1's `extentOf`); nothing is retained across frames except the reference pointer as a key, which is compared, never dereferenced, unless `ownerOf` returned it this frame.
7. MSVC `/W4 /WX`: no `near`/`far`/`min`/`max` identifiers (sides are `front`/`back`/`across`), `(std::min)`, `(std::max)`, no shadowing (checked with g++ `-Wshadow`).
8. The Effect floor rule changes the third pass's behaviour in both styles (intended, review MINOR-3).

### Unverified (NOT_RUN or Hypothesis)

- Every look, count and cost in game; the Windows build.
- That front-side Mare walls are not drawn from outside (Hypothesis from wave 1 screenshots; it is why rule 3 keeps them at the plane).
- Shadow-pass order, geometry identity in shadow passes, audit coverage of shadow vertex shaders, whether shadow maps are re-rendered each frame.
- Frame cost: in kit cells every Lighting/Utility pass now finds its owner (under-plane passes included) and each reference is classified once per frame; at the Mare about 18,500 passes per 0.1 s window (W3). Not measured; the 60 fps cap bounds what can be seen.
- The storey line from bounds versus what a ray would find; galleries other than the Mare's; ramps and sloped floors; cells mixing kit and dungeon pieces (Markarth, Dwemer-built homes); MovableStatic pieces that move (the box is re-read each frame).
- That 4,096 reference slots and 8,192 removed-geometry slots suffice (`tableFull`, overflowing removals simply keep their shadow).

### Fix pass after the binary review of 30503e0 (CHANGES 0 / 1 MAJOR / 7 MINOR)

| Item | Change |
| --- | --- |
| MAJOR-1, the lane | Both segments (feet + 20 and feet + 120 to the eye) now start where the sightline leaves the body, 24 units from the feet toward the camera in the ground plane. A box is in the lane when the part of a segment whose height lies within the box's height range comes within `grow` of the box's footprint **in the ground plane** (exact point/segment/rectangle distance; no growth in z, no axis-aligned corner inflation). Enter at 24, stay at 48. Portable rows: a 30 × 30 × 45 stool and a 60 × 40 × 200 cabinet with their nearest point 30 and 60 units from the feet on both diagonals perpendicular to the view never enter the lane at any heading and are kept whole (at 30 they would only stay in it if already in it; at 60 not even that); the stool 40 and the cabinet 100 units toward the camera are in it and removed; the stool 100 units toward the camera is under the sightline and not in it. |
| MINOR-1 | `pieceOf`: an object (or clutter/furniture static) with no usable box is `flat` (flat cut), never a whole decision. |
| MINOR-2 | Around the player: removed when overhead (bottom ≥ feet + 128); kept only when its top is ≤ feet + 120; otherwise the lane rule. |
| MINOR-3 | A back-side keep (`keptFar`, `keptGallery`) also needs the pass's own bound sphere on the back side (same thin axis, zero clearance); otherwise the pass gets the flat cut, counted as `fallback` (and as `slicedFar` when it reaches the plane). |
| MINOR-4 | A piece containing the feet in the ground plane whose top is within 32 of the feet (the floor under the player, e.g. the loft floor right after arriving, while the old base still puts it above the line) is never removed: flat cut (it does not reach the plane, so it is drawn whole; counted `fallback` only if it does). |
| MINOR-5 (orchestrator decision for tonight; owner to confirm) | Interactable references are never removed whole: every object form type of `kindOf` except Tree (Door, Container, Activator, TalkingActivator, Furniture, Flora, Misc, Weapon, Armor, Book, Ingredient, AlchemyItem, Scroll, Ammo, KeyMaster, SoulGem). Where any rule would remove one (lane, storey above, overhead), it gets the flat cut at the plane instead and is counted on the new `interactCut` (not on `slicedObjects`); kept stays kept. Clutter and furniture **statics** are not interactable and are still removed whole. |
| MINOR-6 | No build change. `slicedFar` and `slicedObjects` can only rise on the flat paths (non-kit cell, `fallback`, a full table, MINOR-3's sphere check); a kit piece that is "across" (for example a corner touching the player's diagonal) is sliced and counted on `slicedSpan`, never on `slicedFar`, even if most of it lies behind. Across pieces: `slicedSpan` counts every across slice; `slicedLow` counts the slices at the plane, front-side walls (all at the plane) **and** across pieces in the lane together; across pieces cut at the storey line are therefore `slicedSpan` minus the across part of `slicedLow`, which the row does not separate (no room left in the 768-byte row for another field). |
| MINOR-7 | `shadowPass` checks the current vertex shader's bytecode hash against the audited list itself (cached per shader object, 64 each way) **before** `shaderFor`, so an unaudited shadow shader is only counted (`shadowSkips`) and never prepared: no FCS-75 line can come from a shadow pass. Patch or create failures of audited shaders still log FCS-74/73 (real failures). The remove buffer is now `D3D11_USAGE_IMMUTABLE` (usage 1, no CPU access) with its initial data. |

Offline Mare model with these rules (Skyrim.esm placements, hearth pose, pieces reaching above the plane; same basis as above): removed whole **118 architecture + 26 objects** at every heading (before: 119 + 89). The 63 fewer object removals are 65 interactables now flat-cut instead (all of them on the loft, wholly above the plane, so they look exactly as before: absent; they keep their shadows and stay clickable as in W3) less 2 statics. Removed through the lane below the storey line, at all four headings: **only the hearth fire `Campfire01Burning`** (a clutter static whose OBND, 580 × 601 × 502 including its flame and smoke volume, contains the pose and is taller than head height, so MINOR-2 sends it to the lane rule, which it meets). (Superseded by the follow-up below: the fire now gets the flat cut.) The scene trigger activator around the pose is `interactCut` (not drawn anyway). Members removed are the beams and the two wall shelves `WRWallShelf01` (150 × 38 × 32, member-shaped, bottom above feet + 128: "overhead"). Members kept: 12 (pillars, inner-corner posts).

**Follow-up (orchestrator decision, same night): tall objects around the player.** An object or member whose box contains the feet in the ground plane, whose top is above feet + 120 and whose bottom is below feet + 128 (not overhead) is never removed by the lane rule: it gets the flat cut at the plane, exactly what W3 drew for it (a hearth fire's OBND is mostly flame and smoke volume around the player, so a lane test on it says nothing, and the flat cut cannot hide the player more than W3 did). Overhead around-objects are still removed; low ones (top ≤ feet + 120) still kept. Counted internally as `aroundCut` and **reported on the row inside `fallback`** (no room for another field: the row is 760 of 768 bytes); never on `slicedObjects`. Portable row: `Campfire01Burning`'s box at the hearth pose at all four headings is in the lane, not low, not overhead, and gets the flat cut. Offline model, hearth, all headings: removed whole 118 architecture + 25 objects; **no lane removal below the storey line**; the hearth fire and the scene-trigger activator around the pose are `aroundCut` (2 references); 64 interactables `interactCut`. Expect `fallback > 0` at the hearth on the row for this reason (the fire's passes reaching the plane); a checker row `fallback=0` must allow it.

Tests: 15 portable C++ tests PASS (CI loop from the repository root); archive rows unchanged; wiring audit nine sections PASS (fourth-pass section extended: `shadowAudited` before `shaderFor` with no refusal, the immutable description, the sphere check, `decide`'s floor and interact overrides, the row format); g++ `-fsyntax-only` with `-Wshadow -Wconversion …` against the stand-ins: 0 warnings. New portable rows as listed in the table, plus: no removal of an interactable or of the floor under any of 512 × 4 fact combinations; around/low/tall cases; the row with `interactCut`.

### Second fix pass after the delta review of 30503e0..2f5d9f6 (CHANGES 0 / 1 / 3)

- **MAJOR-1, the back-side geometry check.** The MINOR-3 check of the first fix pass asked for the pass's bound sphere to be on the back side; a wall's own sphere (radius about its half-diagonal, ~194 for `WRIntWallStr01`, 256 × 39 × 288) always reaches across the plane through the feet when the wall stands within ~190 units behind the player, so exactly the nearest back walls fell to the flat cut. Now a back-side keep (`keptFar`, `keptGallery`) is distrusted only when the drawn geometry exceeds the reference's world box: its sphere centre lies outside the box grown by 16, or its radius exceeds the box's half-diagonal by more than 16 (`geometryExceeds`, policy header); then the flat cut (`fallback`). Portable rows: that wall 60, 100, 150 and 200 units behind the player at all four headings is back and kept whole; a sphere centred 60 units in front of the box, one 40 larger than the half-diagonal, and one 20 beyond a face are flat-cut (15 beyond is not).
- **MINOR-1.** The shadow shader caches hold 256 objects each way; once both are full an unknown shader is skipped without hashing and counted on `shadowSkips`.
- **MINOR-2 (text).** `fallback` and `interactCut` are informative: the checker reports them and does not require them to be 0 (at the hearth `fallback` includes the hearth fire's `aroundCut` passes). `slicedFar=0` and `slicedObjects=0` remain the Mare requirements.
- **MINOR-3 (open owner question).** Interactable Furniture, Doors and Containers standing between the camera and the player's legs are never removed: they get the flat cut, so their part below the plane stays drawn and can hide the legs, as in W3. Should such pieces instead be removed whole (invisible but still solid and clickable), or faded later (Stage 3)?

Offline Mare model with the adapter check (the ESM carries no NIF geometry bounds, so each reference is modelled as one geometry whose sphere is its OBND's own: centre at the box centre, radius the half-diagonal; real sub-geometry spheres are NOT_RUN). Back-side kit pieces kept whole (`keptFar` references): hearth (−117.1, −640, 68) h0..h3 = 17, 8, 15, 24, none flat-cut by the new check (the first fix pass's sphere test would have flat-cut 0, 1, 1, 0 of them); a pose 100 units in front of the north wall `WRIntWallStr01WinA` (−128, 493, 68): 4, 19, 25, 9, none flat-cut (the earlier test: 1, 2, 3, 3), and that wall itself is `keptFar` at 45 and 315 (camera south) and front, cut at the plane, at 135 and 225. Everything else as in the follow-up above.

Tests: 15 portable C++ tests PASS (CI loop from the repository root; longest `FixedShaderStyle` row 760 of 768, format unchanged since 2f5d9f6); wiring audit nine sections PASS (sphere check replaced by `geometryExceeds`, the cache sizes and the full-cache skip asserted); g++ `-fsyntax-only` with `-Wshadow -Wconversion …` against the stand-ins: 0 warnings.

### Third fix pass: the first in-game results (W5 on 2f5d9f6, 2026-11-11)

W5 (orchestrator's reading): at the hearth the Stage 1 look works (storeyZ 310, relStorey 242, back walls whole with their decorations, pillars whole, nothing sliced mid-object, player visible; `shadowPasses=shadowRemoved=252`, `shadowSkips=0`); cave, lifecycle and style 0 PASS. Fixed here:

1. **Loft (top storey): roofs kept whole and filling the frame.** The Mare's roof pieces (`WRIntRoofOpen01`, `WRIntRoofCor*`, `WRIntRoofStr01`, `WRIntRoofAlcoveCap01`, `…BackCap`) have their box bottom at 320, i.e. on the loft floor (feet 321.5), so neither the storey line (555, the ceiling) nor any head-height rule saw them, and the back half was kept whole. Now: **a roof or ceiling kit piece (model file name contains "roof" or "ceil" and not "wall") is removed whole on every side, gallery switch or not** (`roofModel`, Note `roof`). Walls under the roof (`WRIntLoftWallRoof01`, the gable end `WRIntRoofWall02`) are walls. **Back pieces are kept whole only when they stand on the player's storey** (bottom ≤ held feet + 64) **and do not rise more than 64 above the cap**, the cap being the storey line when one is known and held feet + 350 on a top storey without one (a 600-unit gable there would be cut at 350 above the floor); otherwise they are cut at the cap (`cappedFar`, on FixedShaderStyle2, **not** counted as `slicedFar`). Offline model at the loft pose (−655.5, −262, 321.5), headings 45/135/225/315: 53 roof/ceiling references removed at every heading (none kept); back pieces kept whole 6/5/5/4 (the loft's own walls `WRIntLoftWallRoof01`, `WRIntLoftWallCor01`, `WRIntLoftWallStr01Door`, `WRIntLoftWallRoofDoor01`, the gable `WRIntRoofWall02` at 320..576, under the cap 555 + 64), 4 members (the loft posts `WRIntFreeWallColumExt01`) and 16 objects (wall baskets, doors, jugs, the mounted elk) kept, front loft walls cut at the plane; nothing capped.
2. **Under the loft edge (beside route, s-beside-h0-d at (172.9, −708, 68)).** The storey line there was 320 (roof bottoms within 512; the loft floor slabs are farther than 512), so the loft floor slabs at 310 were below "line − 8" and were kept as back pieces. Now **every architecture piece (kit or member) whose bottom is ≥ held feet + 128 is removed whole, on every side and lane state** (Note `overhead`), independent of the storey line, and at once: the held feet height is set on the first published frame in a cell. Objects keep the line rule (hanging game, baskets and shields on back walls above head height stay). Model at the requested pose (hearth moved 180 units to the player's right at heading 45: (10.2, −767.3, 68), line 320): no architecture piece whose footprint covers the player and whose top is above head height is drawn (only the invisible scene-trigger activator, flat-cut); 53 roof and 66 other architecture references removed whole at every heading. On the "unknown for seconds" report: the census was immediate (`cell=kit kit=212` in the first active window); `storeyZ=nan lineZ=488.55` for ~26 s was the route's coc arrival pose (−1376.7, −490.5, 328.5), a top-storey spot with no storey candidate within 512 (no ceiling slab over the west loft), so the line was the plane by design; the roof and overhead rules above no longer depend on it there.
3. **Gallery A/B: objects sliced.** With `bCutKeepFarGallery=1`, a `keptGallery` pass whose geometry failed the back-side geometry check fell to the flat-cut fallback, which slices objects (`slicedObjects`). Now such a pass is removed like the rest of the storey above (an interactable gets `interactCut`, the floor under the player stays), counted on `galleryRemoved`. The `removedObjects` pops of 2-5 per window were not traced to a cause offline (lane hysteresis while standing is the Hypothesis); NOT_RUN. The switch stays off by default.
4. **Near pillars (no change).** Ground-plane distance from the lane to the pillars at the hearth (ESM boxes): the west `WRIntFreeWallColum01` (−287..−225, −677..−604) is 50 units from the lane at heading 45 and 51 at 135 (125 at 225/315); the east one (−31..31, −677..−604) is 35 at 225 and 35 at 315 (103 at 45/135). Enter is 24 and stay 48, so both are kept whole by rule 2 (the east one would stay removed at 35 only if it had entered the lane first).
5. **Strafe S3 (no change).** `planeZ` follows the held `CutPlane` base, not the feet: inside ±24 of the base it does not move, between 24 and 96 it moves after 0.6 s, beyond 96 at once (a jump or one stair step does not move the plane). So `planeZ − (feet + 160)` can be up to 24 indefinitely and up to 96 for 0.6 s while walking over uneven floor; the 32 seen in the strafe is that smoothing, by design and harmless (the checker should allow ±96 for 0.6 s, or judge only settled windows).

Hearth model with all of this (−117.1, −640, 68), per heading: back pieces kept whole 17/8/14/23, 53 roof/ceiling + 66 other architecture removed whole, 25 objects removed, 64 interactables `interactCut`, 2 `aroundCut` (the hearth fire, the trigger), 12 members and 48 objects kept, front walls cut at the plane 14/23/17/8, 1 across; nothing capped.

**New line `FixedShaderStyle2`** (optional, only with `iCutStyle=1`, its own 6,000-row budget, written right after `FixedShaderStyle` with the same `t` and `frame`; `FixedShaderStyle` is byte for byte as at d219484):

```
FixedShaderStyle2 t=%.3f frame=%llu capZ=%.2f cappedFar=%llu roofRemoved=%llu overheadRemoved=%llu galleryRemoved=%llu sample=window
```

`capZ` the back-side cap in use (nan inactive); `cappedFar` back-side passes cut at the cap; `roofRemoved` / `overheadRemoved` passes removed whole by those two rules (they are also in `removedWhole`); `galleryRemoved` gallery keeps turned into removals by the geometry check. Proposed checker rows: Mare loft `roofRemoved > 0` and `keptFar` far below W5's; beside route `overheadRemoved > 0` at the under-loft step.

Tests: 15 portable C++ tests PASS (CI loop from the repository root); wiring audit nine sections PASS (Style2 emission order and format, the roof/overhead/standing/cap rules, `cappedFar` excluded from `slicedFar`, the gallery removal); syntax check 0 warnings. New portable rows: roof/ceiling names (and walls under the roof that are not); a roof on every side with and without the gallery switch is removed; the loft floor slab at the under-loft pose is not "above" but is overhead and removed on every side and lane state, kept only as a gallery back piece; objects above head height under the line are kept; back pieces kept / capped (not standing, over the cap); `capHeight` and `sliceHeight` for the cap; Mare heights against the slacks; the exact `FixedShaderStyle2` row, its nan form and its longest length.

Still unverified: everything in game; that no Mare piece outside the kit (`clutter\`, `furniture\`) needs the roof rule; other kits' roof and ceiling names (Solitude, Windhelm, Riften, Markarth, farmhouses: not checked); the gallery pops; MSVC.

### Fourth fix pass: delta review of 2f5d9f6..0b738ab (CHANGES 0 / 1 / 4)

- **MAJOR-1, walkable roof-named pieces.** The name rule now (a) excludes names containing "stair" (`rifrmsmbaseceilingstairs01`), and (b) never applies to a piece whose box top is at most feet + 32 (`walkable`), wherever it is: a ceiling-named upper floor (`rifrmsmbaseceiling01`, Hypothesis) or a flat roof the player walks on (`wrintcastleroofflatstr01`) is drawn whenever the player could stand on it or above it. The Mare roof slopes (top 576) are still removed from the loft. **Stairs up to a roof/ceiling-named upper floor** (portable row: floor named "ceiling", bottom 300, top 316 over a storey at 68, feet climbing in steps of 4 with the held base lagging the most it can, 96): removed while the feet are below 284, drawn from 284 on, i.e. once its top is within 32 of the feet; the overhead rule has already let go by then because the held base cannot lag the feet by more than 96 (bottom 300 < base + 128 once the feet pass 268). The held base therefore does not delay it.
- **MINOR-1, roof-named supports and columns.** Both measures: the name rule applies to kit pieces only (a member by shape, e.g. `slggcolceil01`, follows the member rules, so a floor-standing post is kept or removed by the lane like any post, and an overhead one by the overhead rule), and names containing "column", "support" or "beam" are not roofs (`wrintcastleroofsupportbeam*`, `sovintcolumnroof01`), so even a kit-shaped support is placed by the side rules. Why both: shape alone misses kit-shaped supports, names alone miss unnamed columns.
- **MINOR-2.** `cappedFar` passes get the same geometry check as `keptFar`: a pass whose geometry the box does not account for gets the flat cut (`fallback`).
- **MINOR-3 (note).** The total slack around the back-side decision is about 32 (side clearance 24 plus the 16-unit geometry slack applied to sphere centres, less what the OBND already covers) against the 24-unit side clearance alone; a geometry that pokes 16 units past its OBND toward the player is still kept whole. Hypothesis that this never shows; NOT_RUN.
- **MINOR-4 (owner question).** With the overhead rule on the held base, a staircase, landing or mezzanine ahead of the player whose bottom is 128 or more above the held base is invisible until the player is within about 96 of its height (the held base follows the feet beyond 96 at once, within 96 after 0.6 s); and a long jump that lifts the feet more than 96 moves the base at once, so pieces 128-200 above the floor can be drawn for about 0.6 s and vanish again on landing. Is that acceptable, or should landings ahead be kept (for example kit pieces reachable by a stair the player is on)?

**Wider check: the rules over other kit interiors** (Skyrim.esm placements read offline; the pose is the cell's `COCMarkerHeading` or, where there is none, the inner load door moved 96 units toward the statics' centre (Hypothesis for the in-game arrival); counts are placed references, per heading 45/135/225/315; reference-level model of this build's rules, not of what the engine draws):

| Cell | Pose | Census | Storey line | Kept back | Removed above line / overhead / roof-named | Capped | Interactables flat-cut | Walkable removed |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WhiterunBanneredMare (hearth) | given (−117, −640, 68) | kit 212 / 0 | 310 | 17 / 8 / 14 / 23 | 68 / 23 / 53 | 0 | 64 | none |
| WhiterunBanneredMare (loft) | given (−656, −262, 322) | kit | 555 | 6 / 5 / 5 / 4 | 0 / 0 / 53 (16 of them roof slopes starting at the loft floor next to the player) | 0 | 0 | none |
| WhiterunDragonsreach | COC (−448, −128, −320) | kit 707 / 0 | 192 | 23 / 8 / 11 / 27 | 302 / 219 / 109 | 1 / 0 / 0 / 1 | 83 | none |
| RiftenBeeandBarb | COC (−183, −638, 64) | kit 148 / 0 | 314 | 16 / 8 / 4 / 11 | 92 / 0 / 0 | 1 / 0 / 0 / 0 | 43 | none |
| SolitudeWinkingSkeever | load door + 96 (−199, −1468, 128) | kit 224 / 0 | 532 (1 candidate) | 14 / 3 / 0 / 9 | 4 / 65 / 0 | 0 | 0 | none |
| WindhelmCandlehearthHall | load door + 96 (786, −3565, 158) | kit 345 / 0 | 306 | 10 / 19 / 36 / 28 | 197 / 0 / 38 (`whintwoodroofmidtop02`) | 0 | 52 | none |
| RiverwoodSleepingGiantInn | COC (−265, −341, 0) | kit 16 / 0 (at the threshold) | none (top-storey cap 350) | 1 / 0 / 0 / 2 | 22 / 0 / 0 | 0 / 0 / 1 / 2 | 1-2 | none |
| WhiterunBreezehome | COC (−384, −656, 63) | kit 76 / 0 | 310 | 8 / 3 / 4 / 9 | 50 / 10 / 12 | 0 | 54-58 | none |
| MarkarthSilverBloodInn | COC (−5597, −2179, −8) | **flat** 3 / 219 (Dwemer `dungeons\` pieces) | — | flat cut, unchanged | — | — | — | — |

Reading (Hypothesis until run): Breezehome, Candlehearth Hall and Bee and Barb look like the Mare (a storey line from an upper floor, roofs removed by name in Breezehome and Candlehearth, many back pieces kept); Dragonsreach removes most of the hall's upper structure (the COC is in the lower level, line 192); the Sleeping Giant sits exactly at the census threshold (16 architecture statics; much of it is probably other folders) and has no storey line at the COC, so it runs on the top-storey cap; Solitude's line comes from a single candidate. Markarth's Silver-Blood Inn is a flat cell (unchanged). No cell has a removed architecture piece whose top is at or below feet + 32. Roof-named statics per cell: Dragonsreach `wrintcastleroof*` and `wrdunintceilingmudstr01` (pieces of the castle roof and a mud ceiling), Candlehearth `whintwoodroofmidtop02` ×38, Breezehome `wrintroof*` ×10; none in Bee and Barb or the Winking Skeever.

Tests: 15 portable C++ tests PASS; wiring audit nine sections PASS (the roof rule's kit-only and walkable guards, the walkable fact, the cap's geometry check); syntax check 0 warnings. New portable rows: stair/column/support/beam names are not roofs and the reviewer's three walkable names; a roof-named member keeps the member rules; a walkable roof-named kit piece falls to the side rules; the staircase climb (removed below 284, drawn from 284).

### Fifth pass on 82056db (W6 findings, review of 82056db MINOR-1/2)

- `slicedFar` no longer counts back-side interactables given the flat cut (`interactCut`); it now counts only real back-side slices and the geometry-check fallback (W6: `slicedFar` equalled `interactCut` in every failing window).
- **Walkable is never removed** (replaces "the floor containing the feet"): any piece whose top is at most feet + 32 keeps the flat cut whatever rule would remove it. Portable row: arriving on an upper floor of several slabs (310..319, feet 321.5), with the storey line at 310 or 555 and the held base still at 68 or already at 321.5, no slab is removed, whether or not it contains the feet.
- `slggcolceil01`: not excluded by name ("col" alone would also catch unrelated words); it is a column by shape (a member) and the roof rule applies to kit pieces only. The comment that listed it among the name exclusions is corrected.
- **W6 loft, headings 135 and 225 (s-loft-h1, s-loft-h2): the player is not drawn, but no wall covers them.** The test probe's view samples put the player's head and chest at screen (0.498, 0.576) and (0.502, 0.594) at heading 135, the same as at heading 45 where the player is drawn. In the h1 and h2 images the floor and rug are visible at those points: nothing is drawn in front of the player there, and the player model itself is missing. The panel with the hexagonal ornament and round shields is the loft's east wall, two `WRIntLoftWallRoof01` pieces at x −520..−504, y −618..−150, z 320..569, with the wall decorations (`WallBasket*`, `WallMountedElkMale`) at x ≈ −520. The player stands at (−640, −256, 321.5), 120 units west of it. With the camera at x −877 at headings 45 and 135 the wall is beyond the player on its thin axis, so it is a back piece, kept whole. That is correct by the rules: a box wholly beyond the plane x = −616 cannot meet a sightline from x ≤ −640. My projection also puts the wall's bottom edge above the player's head on screen at heading 135. At heading 225 the camera is at x −403, beyond the wall, which is then a front wall cut at the plane. The west wall (x −776..−760) is a front wall cut at 481.5 at 45 and 135 and does not reach the sightline (the line is 727 high there). A lane check on kept back walls would change nothing, because a back piece cannot meet the lane by construction, so it was not added. Why the player is not drawn at 135 and 225 is **open**. It is not the cut's per-piece decision: actors and skinned geometry are never touched, and the rug shows through where the player should be. Hypotheses: an engine or SmoothCam player fade (the probe reports the camera-to-head ray blocked by collision reference 0005D655, base 0005D1C0, at 512 and 609 units in both h0 and h1), or culling of the player's 3D. Suggested check: the same pose and headings with `iCutStyle=0` and with `iSeeThroughMode=0`.

Tests: 15 portable C++ tests PASS; wiring audit nine sections PASS; syntax check 0 warnings. Trace formats unchanged.

### Sixth pass on 78a6a67: the engine's player fade (base rig), thick columns, Breezehome

**1. Player fade: built in the base rig (`src/fixed_spike.h`, `include/fixed_spike_fade_policy.h`), key `bHoldPlayerFade=1`.** Cause established by `session-p55/research/missing-player-loft.md` and W10: with `setini "fCharControllerCheckHeightOffset:Camera" -5000` the player was drawn at all four loft headings.

- **Choice: (b), hold the Setting. No code patch.** I re-verified the following in the binary:
  - The Setting object is at RVA **20BA460** = Address Library **ID 382599**. Its vtable is 17EE3F8; its value at +8 is 124.0f in the file (0x42F80000); its name pointer at +0x10 is 196CFF8, which holds `"fCharControllerCheckHeightOffset:Camera"`. Object, value and name are initialised statically.
  - A scan of `.text` for rip-relative references found every reference to the object or value:
    - **8FAAB1** (`movss xmm6,[20BA468]` at 8FAAAD, inside `PlayerCamera::CheckCameraCollision`, ID 50832 = 8FA5D0) is the only reader of the value;
    - B2BB7, 16B9CD0 and 17A3CE4..17A3D01 register and unregister it with the INI collection (the collection is returned by 19D780 = ID 13254).
  - `CheckCharacterCollision` (557AD0 = ID 33008) has exactly one direct caller, 8FAAE9 (no other `E8`/`E9` targets it). Its return value is used only at 8FAAEE..8FAB02: when true and the cast named no actor, the player singleton (ID 403521, 3230778) becomes the actor to fade. Nothing else depends on it.
  - NPC fades near the camera come from `fActorFadeOutLimit` inside the cast (5574F0, ID 33007; read at 5577A4, per the research) and are untouched.
  - `CheckCameraCollision` has 8 direct callers (8F3B08, 8F4210, 8F4688, 8F4F0B, 8F6BA2, 8FE58E, 8FED28, 8FFC92), i.e. other camera states. While the rig holds, only the third-person one runs.
  - **Fade-in:** at 8FAB29..8FAB55, when the handle at PlayerCamera+130 changes and the old actor's fade state (6FE000) is 2 (out), 691A10 (ID 37574) fades it in.
    - So once the test stops returning true, the engine itself brings back a player that was faded before the rig engaged, or while it held.
    - Remaining gap (Hypothesis): a player faded while the handle was cleared by another path would not be brought back by this; not observed.
  - **Saves:** INI Settings are not written to save games. Hypothesis, from the format: a save carries form and global data, not Setting values. The Skyrim.ini collection is written back to disk only by the console `saveini`. The hold is therefore process memory only, and a quit while held loses it.
- **Gating.**
  - **Hold:** at the end of every frame the rig held the view (`endFrame`, eligible path, the same eligibility as the caster detour), the value is set to −5000. The engine's value is saved first. The hold is in place for the next native `ThirdPersonState::Update`, which SmoothCam calls before its own placement.
  - **Release:** on the first ineligible frame (load, cell change, menu, death, F8, first person, any exit cause) and on every main-thread lifecycle call (`lifecycle()`), the saved value is written back if the Setting still holds −5000. If someone else wrote it meanwhile (console `setini`), it is left as they set it ("leave"). While held, a foreign write is adopted as the new saved value and the hold is re-applied ("rehold").
  - **First frame:** the frame on which the rig first holds still runs with the engine's value (eligibility is known only after that frame's native update). A fade that starts on that frame is undone by the engine on the next.
- **Refusal FCS-15** (free in the base series: base uses 1-3, 10-14, 20-23, 30-31, 40-41, 50, 60-61; packages use 40-49, 50-59, 60-69, 66-92). Raised when ID 382599 does not resolve to 20BA460, or the object's name pointer is missing or names another Setting. The engine's fade is then left alone. The name is read only after the address matched.
- **Log line** (once, after `installed rig=1 …`): `FixedCameraSpike player fade hold=%s setting=382599 holdValue=-5000.0 engineValue=%.1f` (state `installed`, `refused` or `off`).
- **Trace: new optional kind `FixedFade`.** One row per 0.1 s window, written while any spike trace is on, with its own 6,000-row budget. Existing lines are unchanged.
  ```
  FixedFade t=%.3f frame=%llu state=%s held=%u holdValue=%.1f current=%.1f saved=%.1f frames=%llu holds=%llu keeps=%llu reholds=%llu releases=%llu leaves=%llu playerFaded=%llu heldPlayerFaded=%llu sample=window
  ```
  - `playerFaded` counts camera frames in which the engine's fade handle (PlayerCamera+130) named the player; `heldPlayerFaded` counts those that happened while held.
  - Expected at the loft: `heldPlayerFaded` 0 after the first held frame, `playerFaded` > 0 only on frames the rig did not hold.
  - The player's handle value is cached once (`GetHandle()`).
- **Tests:** new `tests/fixed_spike_fade_policy_test.cpp`:
  - the facts;
  - `fadeSettingCheck` at three bases (address moved, no name, another name, prefix or longer name);
  - every one of the 16 inputs of `fadeStep` against the table, with invariants (never written when not installed, never released while the rig holds, written back only when still ours);
  - a frame sequence: hold, a setini under the hold, release, a menu, someone's write before release;
  - the `FixedFade` row exactly, its nan form and its longest length (423 of 768).
- **Wiring audit:** new section `player fade hold`. It checks: install order and guards; the name read only at the audited address; the two `endFrame` call sites and their order; the lifecycle restore; the key in the gate and the INI; no code write. The base code count is now 20 with FCS-15.

**2. Thick columns in Dragonsreach (no rule change).** Skyrim.esm placements at the route pose (−448, −128, −318):

- The tall pieces crossing the plane within 520 units are the castle pillars `WRIntCastleFreePillar01`: OBND 69 × 72 × 545, z −351..194.
- They are members by the current shape rule (middle span ≤ 128), as are every other column-like piece in the cell: `…FreePillar01Mid` 62 × 54 × 514, `…MidEndNubs` 73 × 72 × 593, `…Top` 54 × 46 × 514, `WRIntCastleStoneColumn01` 56 × 56 × 491, `WRTempIntPromCol01` 42 × 42 × 499.
- Each hall column is a **stack of three or four references** (base, mid, top). The base piece stands on the floor and is kept whole (member rules). The pieces above start at 194, at the storey line (192): they are "above" and removed whole.
- What looks like a sliced top in s-kit-dragonsreach-h0 is therefore the **open upper end of the base piece**, which in vanilla is hidden by the next piece. It is not the plane: the column is drawn to 194, 512 above the feet, where the plane is at feet + 160.
- They look thick because they stand next to the camera.
- A shape rule for "thick" columns (vertical ≥ 2 × both horizontal spans, spans similar) would also make the castle corners `WRIntCastleWallCor01` (256 × 256 × 544) and `…DoubleHighExCor01` (247 × 247 × 512) members, so it was not added.
- Options for the owner:
  - (a) keep a stacked column whole up its whole stack (to the roof, ~1,600 units) whenever its base piece is kept;
  - (b) remove the whole stack when any piece of it is removed;
  - (c) leave as is (column ends at the storey line with an open end).
- I recommend (a) for back-side columns only. It is not built.

**3. Breezehome (no rule change).** At the COC pose (−384.5, −656.2, 64; plane 224, storey line 310):

- The walls in frame are `WRIntWallStr01Low`, `WRIntWallStr01WinBLow`, `WRIntWallSTStrWinA`, `WRIntWallSTCor*` and `WRIntWallCorIn01`, all with boxes 32..320.
- At each heading about half of them are front walls, cut at the plane exactly as in style 0 (rule 3), and half are back walls kept whole to 320.
- A pixel diff of the W7 screenshots (style 1 against style 0, threshold 40, every second pixel) changes only 3,031 / 9,984 / 27 / 121 samples at h0..h3. At h0 the difference is the back walls' upper parts at the top edge of the frame: whole in style 1, cut flat in style 0, mostly outside the frame (crop compared by eye).
- From the doorway the camera looks at the room with the back walls at the edge of the picture. The 96 units each back wall gains (224 → 320) project above the top of the frame, and what style 1 removes above the line (roof, ceilings, the upper floor) was already invisible under the flat cut. Both pictures therefore look alike, and the rules do what they should.
- The W7 `slicedFar` (126-140 per window at 45/315) was the `interactCut` miscount fixed in 78a6a67 plus the geometry-check fallback.
- A pose in the middle of the main room would show the difference.

### Proposed shared-file changes (not made)

None needed beyond this package's files.

## Stage 2: the frame pass (2026-11-11, base `3528e92`)

Owner go 2026-11-11 ("Start Stage 2"). Specification: `session-p55/briefs/w4-stage2.md`; design leads `session-p55/research/out-of-bounds-look.md` (candidates 3a, 4a, 5) and `research/seamless-cutaway.md` (C2b, C7, "Stage 2"). In the code this is the cut's **fifth pass** (first: the cut, second: void colour and Effect, third: sky, fourth: Stage 1). Every in-game row is **NOT_RUN**; the Windows `/W4 /WX` build is **NOT_RUN**. Disassembly: `objdump -d -M intel` of the ranges listed under "Reproduce", a PE reader and a byte scanner for RIP-relative references, `call` targets and COM calls through the device and context globals (scratch, deleted after use).

### What it does

While the cut is active (third person, rig on, interior, cut installed and not latched: the same `active` flag as the cut, the void colour and the sky), one full-screen triangle of our own is drawn into the main colour target **after the main world render (ID 36558) returns in the frame function (ID 36555) and before image space**. Its pixel shader reads the main depth, rebuilds the world position and moves the pixel towards `sVoidColor` by

- **band** (3a, soft cut edge): `k = smoothstep(bandFloor, planeZ, z)` for surfaces at or under the plane, `bandFloor = max(planeZ − fFrameBand, heldFeet + fFrameBandHeadroom)`, faded out within 48..96 ground units of the player's feet; nothing above the plane is touched;
- **falloff** (4a): `f = smoothstep(R1, R2, ground distance to the feet)`;
- combined `1 − (1 − k)(1 − f)`, then the **vignette** the same way (`v = fFrameVignette · smoothstep(0.35, 1, |ndc|/√2)`);
- **void** pixels (depth ≥ 0.999999, the engine's own "no geometry" test; depth is cleared to 1.0) get exactly 1.

Blending is `src·α + dst·(1 − α)` with the void colour as `src` and the amount as `α`, RGB only: a lerp in the scene's HDR colour space before tonemap, never an addition; void pixels become exactly the void colour (`void·1 + scene·0`).

**Kit cells.** With Stage 1 (`iCutStyle=1`) in a cell the census calls a kit building, the back walls stand above the plane on purpose and are not cut, so a band at plane height would paint a dark stripe across them. **The band is off there** (`band=kit` in the trace); falloff and vignette stay. Simplest rule that cannot darken a wall that was never cut. Caves and ruins (`cell=flat`) and Stage 1 off (`iCutStyle=0`) get the band.

**Actors.** The brief assumed the plane sits above actors' heads. With `fShaderCutHeight=160` and a 96-unit band it does not: the band would start at feet + 64 and darken every head on the player's floor by up to about 75 %. So the band never reaches below the **held feet + `fFrameBandHeadroom` (136)**: with the default plane the band in force is **24 units** (feet + 136 .. feet + 160), the study's own "last 24-48 units"; `fFrameBandHeadroom=0` gives the full `fFrameBand`. Above the plane the band is 0, so an actor standing tall through the plane is never painted out. The player is also protected by the 48..96 guard around the feet. **Stairs:** the plane holds the feet height within 24 units and follows after 0.6 s (or at once beyond 96, `CutPlane`); while climbing, the player's head can be up to 32 units into the band before the plane follows, and the guard keeps it untouched; another actor on a ledge between feet + 8 and feet + 32 above the player's floor has its upper body in the band (a dark belt under the plane; **Hypothesis** that this is rare enough).

### Binary facts established (SkyrimSE.exe 1.7.104; same hashes and Address Library as above)

| Fact | Evidence |
| --- | --- |
| **Insertion point.** ID 36555 (6565B0) calls ID 36558 (656D10) twice. 6567C7 calls ID 82075 on the UI singleton (ID 400327); 82075 walks the menu stack (`UI+0x110`, count `+0x120`) testing menu flag `+0x1C & 0x20`. If any menu has it, the path at 6567D4 renders the world **once** behind the flag 32303B2 with argument 0xE (656834, a frozen menu background). Otherwise 656A0E clears that flag and reaches **656A78** (`xor edx,edx; mov rcx,rbp; call 656D10`), then jumps to 65689F, where image space starts (ID 105676 at 6568D9 / 6569F2, per-frame upload ID 77258 at 656915). So **656A78 is normal play** and its return is after the whole world render and before image space. Only 656A78 is redirected; 656834 and 656CC8 (ID 36557) are untouched. | **Source**: 6565B0..656C0E, 1169590..1169684. That flag 0x20 is CommonLib's `kFreezeFrameBackground`: Hypothesis (name only). |
| **Call-site guard.** 656A60..656A7C = `48 8b 0d d9 9c bd 02 48 8b 89 28 01 00 00 e8 3d f5 2d 00 33 d2 48 8b cd e8 93 02 00 00`; ID 36558's first 16 bytes `40 57 41 56 41 57 48 83 ec 40 48 c7 44 24 30 fe`. The void relay's site (656DAA) is inside 36558 and is neither read nor written by this pass. | **Source**. |
| **What 36558 leaves.** ID 36559 (656E60..65791E) calls ID 107131 (1537F60) at 6574C5 before the main pass: `SetRenderTarget(slot 0, target 1)` (ID 77453 at 10155B0) and `SetDepthStencilTarget(ID 77457() = 0, mode 3 or 0, view 0)` (ID 77454 at 1015670; ID 77457 at 1015790 is `xor eax,eax; ret`). It then clears target 1 with the void colour (65757E/65758A), adds slots 1 = 7 and 2 = the normals target, draws, and resets slots 1 and 2 to −1 (657853, 657871). Targets are bound lazily: the state flush ID 77386 applies them at the next draw. | **Source**. That target 1 is the HDR scene image space reads next: Hypothesis (the memo's; consistent with the clear and the call order). |
| **Render-target table.** ID 77253 (100A590, reached from 1015450 with `rcx = 3331F40`, the renderer, ID 411393) creates target *i*'s texture at `renderer + 0xA58 + 0x30·i` (CreateTexture2D, device +0x28, 100A640), its render-target view at `+0xA68` (device +0x48), SRV `+0xA70`, copy `+0xA60/+0xA78`, UAV `+0xA80`. The flush reads views at `[3330188] + 0xA58 + 0x30·i` (1010EB0) where global 3330188 (ID 411347) is set to `renderer + 0x10` (0E97CC, 1007B3B). ID 77251 clears `renderer + 0xA68 + 0x30·i`. So **main colour texture `renderer + 0xA88`, its view `renderer + 0xA98`**. | **Source**. |
| **Depth table.** The depth creator at 100A980 (reached from 1015495 with the renderer) creates depth *i*'s texture at `renderer + 0x2018 + 0x98·i` with **BindFlags 0x48** (depth-stencil + shader resource), one sample, `ArraySize` from its descriptor (100AA21: `mov qword [rbp+0x1f],0x48`; 100AA16: samples 1, quality 0), views `+0x2020 + 8·v`, read-only views `+0x2060 + 8·v`, and the **shader-resource view of the depth at `+0x20A0`** (CreateShaderResourceView, 100AB55; formats: typeless R24G8 → SRV R24_UNORM_X8, R32 → R32_FLOAT, R16 → R16_UNORM), stencil view `+0x20A8`. ID 77343 (100F8B0) binds `[renderer + 0x20A0 + 0x98·i]` as a pixel-shader texture; the flush picks `views` or `readOnlyViews` of the bound depth (1010F3A..1010F56). So **main depth texture `renderer + 0x2018`, its SRV `renderer + 0x20A0`**. | **Source**. **CommonLib:** the study quoted "0x2018 from `Renderer +0x10`"; the binary has the table at **`renderer + 0x2018` = `[3330188] + 0x2008`**: a 16-byte difference of exactly the kind the evidence warns about. Nothing here uses CommonLib's renderer declarations. |
| **Depth convention.** Depth is cleared to 1.0 (flush 1010F8C and ID 77252 at 100A534 load 1B5BE28 = 1.0); the sky's vertex shaders write z = w (third pass); the engine's image-space fog treats depth ≥ 0.999999 as no geometry (shader entries 16002/16004). Standard (not reversed) Z. | **Source**. |
| **View-projection.** ID 77258 (100AE80) builds the per-frame buffer on the stack (`rsp+0x40`, `rbp = rsp+0x100`) and copies 0x2D0 bytes into it after `Map` (100B440..100B4F5). Rows 8..11 (`rbp−0x40..−0x10`) are the **transpose of the 4×4 at state + 0x430** (100AF47..100B003, shuffles traced), the rows every patched world vertex shader multiplies `cb2` World with (first-pass shader audit). So `clip = p · M` with `p` the world position **relative to the position adjustment** (state +0x35C; World row 3 is `translate − posAdjust`, first-pass audit) and `M` = state +0x430. It is the matrix the depth was rasterised with, jitter included. Rows 32..35 hold the engine's own inverse of it (100B1D8 → `rbp+0x140`); the pass inverts `M` itself in double instead. | **Source** for the layout. CommonLib's `ViewData::viewProjMat` sits at the same place (camera data at state +0x380, +0xB0): naming only. |
| **Viewport and depth range.** The flush sets the viewport from state + 0x70 (10110E4, RSSetViewports count 1) and its min/max depth from state + 0x5C0 / + 0x5C4 (101106C..101107F), reduced at MaxDepth by a depth-bias table when state + 0xA0 is non-zero. | **Source**. |
| **COM offsets.** D3D11 ABI order (CommonLibSSE-NG `REX/W32/D3D11.h`): engine call sites exist for CreatePixelShader 0x78 (100FFC1), PSSetShaderResources 0x40 (10113F8), PSSetShader 0x48 (10102B1, tail jump), Draw 0x68 (100963F), PSSetConstantBuffers 0x80 (1009EBB), IASetInputLayout 0x88 (100CB7C), IASetPrimitiveTopology 0xC0 (10112DC), OMSetRenderTargets 0x108 (1010FC0), OMSetBlendState 0x118 (101114C), RSSetState 0x158 (101105A), RSSetViewports 0x160 (10110F3). The Get* methods, CreateBlendState 0xA0, CreateRasterizerState 0xB0 and Texture2D::GetDesc 0x50 rest on the ABI order alone. | Call sites: **Source**. ABI-only offsets: **Source** for the header order; no engine call to compare. |
| **No PS UAVs.** No `OMSetRenderTargetsAndUnorderedAccessViews` (context +0x110) call goes through the context global, so OMSetRenderTargets cannot disturb an engine UAV. | **Source** (same scan limit as the first pass: calls through a copy of the context pointer are not seen). |

### Why the first choice and not ID 105676

At 656A7D the HDR scene and its depth are complete and nothing of image space has run, so the paint is tonemapped and eye-adapted like the scene, and a dark void lowers the adaptation input the same way the black clear colour already does; the call site is one guarded `E8` reached only in normal play. Around ID 105676 the pass would have to pick the tonemap's effect index (and ENB replaces that stage), and after the tonemap the depth may be reused. The void relay (656DAA, inside 36558) is untouched.

### Targets, depth read, hazards

At the return the pass binds **only** `renderer + 0xA98` (main colour view) as render target 0 and **no depth-stencil view**, then the engine's own SRV `renderer + 0x20A0` in pixel slot 0. It needs no depth test: depth is read in the pixel shader, so the depth texture is never bound for writing and reading at once. The textures' descriptions are checked whenever either pointer changes (one 2D single-sample depth with the shader-resource bind, a single-sample colour target of the same size, the main viewport inside them; FCS-97 cause 1..5 otherwise, and those frames are skipped). The depth-stencil view is unbound before the SRV is bound; on the way back the SRV leaves slot 0 before the saved targets (the engine's depth-stencil view among them) return, and the saved pixel resources come back after them, so the runtime never has to unbind anything itself (it would otherwise null an SRV of the main target bound elsewhere when we bind that target).

**Fallback not needed.** The depth's SRV is established with Source confidence (created with the shader-resource bind and stored at `+0x20A0`, bound from there by the engine itself), so the void-only form (z = 1, depth test EQUAL) was not built. If the in-game trace shows `skipTargets` or the whole frame comes out the void colour, that is the signal to build it.

### World position and the camera-relative offset

The matrix, the position adjustment, the viewport and the depth range are **captured from the render state during the first Lighting/Utility pass of the published view in the same render** (`consider`, right after the existing view test), not read at the insertion point, so whatever another view leaves in the state after the main pass cannot reach the reconstruction. The pass draws only if that capture exists for this render and was made with the snapshot (`View::frame`) still published, i.e. the plane, feet and active flag are the very values the geometry was cut with. The inverse is computed on the CPU in double from `M`; the shader computes `p_j = dot((ndc.x, ndc.y, (d − min)/(max − min), 1), column_j(M⁻¹))`, divides by `w` and works relative to the position adjustment; feet and plane are uploaded relative to it. A test shifts the whole scene by (60000, −45000, −3000) and gets the same amounts. `moved` in the trace counts frames whose state matrix at the insertion point differs from the captured one (informative).

### Shaders

`modules/camera/shaders/fixed_spike_frame_vs.hlsl` (vertex: one triangle from `SV_VertexID`, no buffers) and `fixed_spike_frame_ps.hlsl` (pixel: the maths above; `cbuffer FramePass` = `FrameConstants`, 160 bytes, 10 rows), written for this project from scratch. Built offline by `modules/camera/tools/frame_shaders.py` with vkd3d-shader 2.1's HLSL compiler (`libvkd3d-shader.so.1.19.0` of the Steam Linux Runtime, through ctypes, read-only; profiles vs_5_0 / ps_5_0; any compiler message fails the build). vkd3d writes RDEF, ISGN, OSGN, SHDR, STAT; the tool rebuilds the container to the **shipped game shaders' form (ISGN, OSGN, SHEX)**, copying signatures and token stream unchanged, and recomputes the checksum. The bytes and the FNV-1a-64 of each source file are in `include/fixed_spike_shader_frame_shaders.h` (vertex 548 bytes, pixel 2,956 bytes). They are created once at install through the engine's device (CreateVertexShader +0x60, CreatePixelShader +0x78) with a dynamic constant buffer (the engine's own description), a blend state and a rasterizer state (cull none, no scissor, depth clip on).

| Check | Result |
| --- | --- |
| `frame_shaders.py --check`: a fresh build equals the committed header byte for byte | **PASS** (`frame shaders MATCH`) |
| `frame_shaders.py --validate`: each blob read back by vkd3d-shader (it verifies the checksum, E0003 otherwise), disassembled and translated to SPIR-V | **PASS**, no message; the disassembly is the HLSL statement for statement |
| Portable test: checksum of both blobs (the policy's own MD5-based checksum), chunks exactly ISGN/OSGN/SHEX, version tokens vs_5_0 / ps_5_0, signatures (`SV_VertexID` in, `SV_Position` out; `SV_Position` in, `SV_Target` out), `cb0` declared with 10 rows = `sizeof(FrameConstants)/16`, one `Texture2D` at t0, the vertex shader with no resource; **the HLSL sources hash to the values recorded in the header** (a source edit without a rebuild fails CI) | **PASS** |
| DXVK and a Windows D3D11 runtime creating them | **NOT_RUN** |

### State hygiene

Read before the draw and set back after, every reference released: render targets 0..7 and the depth-stencil view; blend state, blend factor, sample mask; rasterizer state; all viewports (count first); input layout; topology; vertex shader; pixel shader; pixel constant buffer 0; pixel shader resources 0..15 (the engine's 16 pixel texture slots, state +0x240). Not touched, so not saved: depth-stencil state (no depth-stencil view is bound, so no depth or stencil test runs), vertex buffers and index buffer (no input layout; the vertex shader reads only `SV_VertexID`), vertex constant buffers and resources, samplers (the shader uses `Load`), scissor rectangles (scissor off in our state), geometry/hull/domain shaders (the engine never binds them, first pass). After the restore one read-back compares render target 0, the depth-stencil view and the pixel shader with the saved ones (`mismatch`, cumulative). Device objects are made once at install; per frame there is one Map/Unmap of 160 bytes and no allocation. The engine's `ClearState` at every frame begin (1009E72) would also clear anything left behind, but nothing is relied on that.

### Gating

`frameGate(latched, active, freshView, captured)`: latched (any refusal latch of the cut, or a failed Map) → skip; not active (outdoors, first person, menus, the rig off, any ineligible frame, the frame of any lifecycle exit: `deactivate` clears `active` on every thread at once) → skip; snapshot older than 0.25 s → skip; no capture of this render with this snapshot → skip; then targets and constants. Menus with a frozen background never reach the hook (656834). ENB and Community Shaders: `commit` refuses before anything (FCS-67), so the pass never exists. Thread: the hook calls ID 36558 on whatever thread, and draws only on the main thread (FCS-96 otherwise).

### Settings (`[FixedCameraSpike]`, read only when `bEnabled=1`, used only in `iSeeThroughMode=3`)

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `bFramePass` | 1 | 0/1 | the whole pass. 0: no runtime, no hook, no log line, no trace kind, nothing validated: behaviour of `3528e92` |
| `fFrameBand` | 96 | 0, or 8..512 | soft cut edge under the plane; 0 = off |
| `fFrameBandHeadroom` | 136 | 0..512 | the band never reaches below the held feet + this (actors' heads); 0 = the full band |
| `fFrameRadiusInner` | 900 | 0..16384 | full brightness within this ground distance of the feet |
| `fFrameRadiusOuter` | 1400 | 0..16384 | the void colour beyond it; ≤ inner = falloff off |
| `fFrameVignette` | 0.25 | 0..1 | darkening towards the corners; 0 = off |

Out-of-range values are clamped (NaN takes the default) and reported once as FCS-66. The void colour is `sVoidColor` (second pass), whatever `bVoidColor` says.

### Log line (once, after `shader style installed …`, only with `bFramePass=1`)

```
FixedCameraSpike shader frame installed pass=%s band=%.1f headroom=%.1f radii=%.1f,%.1f vignette=%.3f vs=%u ps=%u
```

`pass`: `installed` or `refused` (FCS-93..95). Not written if the cut itself never installs.

### Trace: `FixedShaderFrame` (new optional kind, only with `bFramePass=1`; `FixedShader`, `FixedShaderLook`, `FixedShaderVoid`, `FixedShaderStyle`, `FixedShaderStyle2` and `FixedFade` unchanged byte for byte)

Written after the style rows of each 0.1 s window (same `t` and `frame`), its own budget of 6,000 lines:

```
FixedShaderFrame t=%.3f frame=%llu pass=%s calls=%llu drawn=%llu skipLatched=%llu skipInactive=%llu skipStale=%llu skipCapture=%llu skipTargets=%llu skipMatrix=%llu offMain=%llu band=%s bandN=%.1f headroom=%.1f bandFrom=%.2f planeZ=%.2f falloff=%s radii=%.1f,%.1f vignette=%.3f depth=%s target=%ux%u formats=%u,%u moved=%llu mismatch=%llu micros=%.1f sample=window
```

Example (test row): `FixedShaderFrame t=1234.500 frame=99 pass=installed calls=6 drawn=5 skipLatched=0 skipInactive=0 skipStale=0 skipCapture=1 skipTargets=0 skipMatrix=0 offMain=0 band=on bandN=96.0 headroom=136.0 bandFrom=9396.25 planeZ=9420.25 falloff=on radii=900.0,1400.0 vignette=0.250 depth=read target=1920x1080 formats=10,44 moved=0 mismatch=0 micros=31.0 sample=window`. Longest possible row 645 of 768 bytes (test).

Legend: `pass` as in the log line (`pending` before the cut installs). `calls`: main-world-render returns in normal play in the window; `drawn`: passes drawn; `skip*`: frames not drawn, by first reason (latched, inactive, stale, capture, targets, matrix). `offMain` cumulative. `band`: at the last drawn frame `on`, `kit` (off in a Stage 1 kit cell) or `off`; `none` if nothing was drawn. `bandN`, `headroom`, `radii`, `vignette`, `falloff`: the settings in force. `bandFrom` / `planeZ`: world heights of the band floor and the plane at the last drawn frame (nan if none, `bandFrom` nan unless `band=on`). `depth`: `read` if a pass was drawn. `target`: the depth texture's size; `formats`: DXGI formats of the main colour and depth textures (0 before the first check). `moved`: drawn frames whose render state at the insertion point held another view-projection than the captured one. `mismatch`: restores whose read-back differed (cumulative; must stay 0). `micros`: time in the pass while tracing.

For the checker (`session-p55/routes/check_spike_log.py`, not edited here): register `FixedShaderFrame` in `OPTIONAL_KINDS` (the generic parser keeps `radii`, `target` and `formats` as strings) and `CAPS['FixedShaderFrame'] = 6000`; FCS-93..96 are faults, FCS-97 cause 1..5 informative (frames skipped), cause 10 a fault.

### Refusal codes (FCS-93..97; all 32 codes 66..97 now fill the shader runtime's once-mask)

| Code | Site | Meaning, and what stays in force |
| --- | --- | --- |
| FCS-93 `check=N` | `installFrame` | 1 ID 36555, 2 ID 36558, 3 the renderer ID 411393, 4 the 29 bytes at 656A60, 5 the call not to ID 36558, 6 ID 36558's prologue, 10..17 an evidence window (ID 107131, ID 77457, the flush's target views, the target creator, the depth-SRV creator, ID 77343, the view-projection rows of ID 77258, the flush's viewport), 18 the viewport depth range; no frame pass, everything else unchanged |
| FCS-94 `cause=N` | `installFrame` | no relay memory within reach (1), or the relay out of reach / not sealed (2); call site untouched |
| FCS-95 `object=N` | `installFrame` | the device refused 1 the vertex shader, 2 the pixel shader, 3 the constant buffer, 4 the blend state, 5 the rasterizer state; what was made is released; call site untouched |
| FCS-96 | `frameStart` | the main world render returned off the main thread; those frames were not touched |
| FCS-97 `cause=N` | `drawFrame` | 1..5 the targets as above (frames skipped while it holds, checked again when a texture changes); 10 the constant buffer could not be mapped: pass off for the session |

### Files

| File | Change |
| --- | --- |
| `include/fixed_spike_shader_frame_policy.h` (new) | audited facts with their bytes, offsets, COM offsets, D3D11 structures, blend/rasterizer descriptions, `FrameCapture`, `FrameConstants`, `invert`, `bandState`, `frameConstants`, a C++ copy of the pixel shader (`framePixel`) for the tests, `frameGate`, `targetCheck`, `frameSiteCheck`, the trace row |
| `include/fixed_spike_shader_frame_shaders.h` (new, generated) | the two DXBC blobs and their source hashes |
| `shaders/fixed_spike_frame_vs.hlsl`, `shaders/fixed_spike_frame_ps.hlsl` (new) | sources |
| `tools/frame_shaders.py` (new) | build, `--check`, `--validate` |
| `src/fixed_spike_shader_frame.h` (new) | runtime, capture, target check, save/restore, draw, hook, install, trace |
| `include/fixed_spike_shader_policy.h` | codes 93..97; `FrameConfig` and `validateFrame` (skipped with `bFramePass=0`); `Config::frame`; the include |
| `src/fixed_spike_shader.h` | `framePass` and `frameEmitter` in the runtime; `captureFrame` after the view test in `consider`; `installFrame` and its log line at the end of `commit`; `frameStartFrame`; `traceFrame`; `traceKinds` +1; one more emitter in `configure`; `configureFrame`; the include |
| `src/fixed_spike.h` | one line budget and the sixth emitter |
| `src/plugin.cpp`, `config/SkyCRPG_Camera.ini` | six keys inside the existing gate, documented |
| `tests/fixed_spike_shader_frame_policy_test.cpp` (new), `tests/fixed_spike_shader_policy_test.cpp`, `tests/lock_refresh_wiring_test.py` | below |

### Tests (g++ 15.2.0, Python 3.14.4, this host)

| Check | Result |
| --- | --- |
| The 17 portable C++ tests (CI loop, `-std=c++20 -Wall -Wextra -Werror -pedantic -pthread`, from the repo root) | **PASS**, every test printed `… PASS`; the new one prints `longest FixedShaderFrame row: 645 of 768 bytes` |
| `fixed_spike_shader_policy_test` with `SKYCRPG_SHADER_ARCHIVE` | **PASS**, archive lines unchanged |
| Wiring audit, eleven sections (new `shader fifth-pass wiring`; `shader cut wiring` now one site per FCS-66..97) | **PASS** |
| Eight hand mutations in a scratch copy (draw before the engine render, no restore, state not published before the write, pixel resources not saved, no thread check, capture from Effect passes, saved references not released, relay sealed RWX) | each failed the audit |
| g++ `-fsyntax-only -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` of `fixed_spike_shader.h` (with every pass, frame included) against scratch stand-ins for the Windows, REL, RE and thin declarations | 0 warnings. Not an MSVC or CommonLib build |
| `git diff --check` | **PASS** |
| Windows `/W4 /WX` build | **NOT_RUN** |

New portable rows: codes 93..97 contiguous, the mask full; defaults; clamping, NaN, negative band, every part off, `bFramePass=0` leaving values unvalidated and unreported; **world position from depth** against a view-projection built like the engine's (row vectors, eye-relative, the rig's 55° pitch and 683.8 height at the Embershard pose, with and without TAA jitter, through a [0.1, 0.9] depth range too) within 0.75 units at five points; the same amounts after a 75,000-unit world shift; void pixels exactly the void colour at centre and corners; the band at a known height, nothing above the plane, the player's head on a lagging stair, another actor's head on the player's floor, headroom 0, kit cells; the falloff; the combination; each part off; never brighter over a screen grid; vignette 0 at the centre and its strength in the corner; unusable captures refused; the gate over every camera state × interior × installed × latched × fresh × captured; `targetCheck` causes 1..5; the blend and rasterizer descriptions; **the evidence windows decoded to the very offsets used** (0xA58, 0xA68, 0x2018, 0x20A0, 0x98, state +0x430/+0x70/+0x5C0, target 1, depth 0, the three call targets of ID 107131); `frameSiteCheck` at three image bases with every guarded byte flipped; the shader blobs and sources (above); the trace row exactly, its `none`/`nan`/`kit` forms and its longest length.

### First-run protocol (for the orchestrator; nothing here was run)

Build `shader` at the commit of this pass. Expected log lines once each, after the existing four: `FixedCameraSpike shader frame installed pass=installed band=96.0 headroom=136.0 radii=900.0,1400.0 vignette=0.250 vs=548 ps=2956`. No FCS-93..97 line.

1. **Embershard Mine** (W12 pose, routes `p55-comb-cave-*` or `p55-shader-cave`, four headings, `iShaderCutTrace=1`): `FixedShaderFrame pass=installed`, `drawn` equal to `calls` (about 6 per window at the 60 fps cap) in settled windows, `band=on`, `bandFrom` = planeZ − 24, `depth=read`, `mismatch=0`, `moved=0`, `skipCapture` at most 1 in the first window. Screenshot against `results/wave12/cave-nofill-vs-fill.png`: the void unchanged (already black); the last ~24 units under the cut edges of rock and the lit wooden frames shade into black instead of ending in a lit razor line; the far passage and the stray shards beyond ~900 units fade out by ~1400; a light darkening in the corners; the player, the floor around the player and the fill light's pool unchanged.
2. **Bleak Falls Barrow** (W12 pose, `p55-comb-bfb-*`): the same rows; the chamber walls' cut tops soften; the neighbouring rooms beyond 1400 sink into black.
3. **Bannered Mare hearth** (Stage 1, `p55-shader-mare` / `p55-comb-mare`): `band=kit`, falloff and vignette on. Against `results/wave11/mare-hearth.png`: the back walls keep their full height with no dark stripe at the plane height; the far ends of the hall (beyond ~900 units of the player) darken towards black (at h0/h2 the far tables and the bar), the corners darken lightly; patrons near the player unchanged.
4. **Lifecycle** (`p55-shader-lifecycle`): in every ineligible window `drawn=0` and `skipInactive>0`; screenshots taken while ineligible show no vignette or falloff.
5. **A/B**: `bFramePass=0` must reproduce the 3528e92 screenshots and traces exactly (no `FixedShaderFrame` row, no new log line); `fFrameBandHeadroom=0` shows the full 96-unit band (expect darkened heads of NPCs on the player's floor: that is why the default is 136); `fFrameVignette=0`, `fFrameRadiusOuter=0` isolate the parts.

### What could look wrong

| What | Why | How it shows |
| --- | --- | --- |
| The whole frame the void colour | the depth read is not the scene's (another depth bound at 656A7D, or depth 0 cleared after the main pass) | black screenshot with `drawn>0`; set `bFramePass=0`. Hypothesis that depth 0 is intact at the return (the engine's own image-space fog reads the depth after this point) |
| The falloff circle off-centre or sliding with the camera | a reconstruction error: wrong matrix or offset | the lit pool not centred on the feet; compare the pool with the player's position at four headings; `moved>0` would point at the capture |
| The scene brighter than in W12 | eye adaptation reacts to the darker frame (more black, lower average luminance) | brighter player and floor; bloom more visible on black. Hypothesis |
| Banding in the gradients | an 8-bit-like gradient after the tonemap of a smooth HDR ramp to black | visible steps in the falloff ring on dark rock; `fFrameRadiusOuter` wider softens it |
| TAA ghosting at the band and the vignette | the band moves with the world (fine for reprojection), the vignette is fixed to the screen and the history was painted at another screen position | a faint trailing edge of the vignette while turning; dark smears behind the cut edge on fast moves. Hypothesis |
| A dark belt on actors | an actor on a ledge 8..32 units above the player's floor stands in the band under the plane; or `fFrameBandHeadroom` lowered | a dark band across the upper body |
| Water reflections, save thumbnails | the save thumbnail (IDs 19121/19122 → ID 36555) is rendered through the same call | the thumbnail shows the vignette and falloff, as it already shows the black void |
| A hitch at install | DXVK compiles the two shaders and the pipeline on first use | one slow frame on the first active frame |

### What the reviewer must check

1. The binary facts above: 656A60..656A7C and the two calls of ID 36558; ID 82075's menu-flag test and that 656A78 is the normal-play branch; ID 107131 and ID 77457 (target 1, depth 0); the RT creator at 100A590 (texture `+0xA58`, view `+0xA68`) and that it is called with the renderer (1015450); the depth creator at 100A980 (bind 0x48, SRV at `+0x20A0`, stride 0x98) and that it is called with the renderer (101548E); ID 77343 at 100F8B0; the per-frame upload's rows 8..11 = transpose of state +0x430; viewport state +0x70 and depth range +0x5C0/+0x5C4.
2. `frameEvidence` bytes against the binary (each window's RVA and length).
3. That nothing between the main pass and 656A7D clears or replaces depth 0 in third person (36559 after 6576F6: 6E4A90, 15436B0, ID 107148, 153ABB0, ID 107143), and that the r14b branch (657729, first-person depth 7 via ID 107129) does not run in third person.
4. `fixed_spike_shader_frame.h`: the relay encoding and the 4-byte displacement write at 656A79 after every check and every device object; `hookMainView` calls ID 36558 once with `rcx, rdx, r8, r9` passed on (36558 takes `rcx, edx`; its home-space spills are the callee's own); save/restore covers everything the draw sets, in the hazard-free order, and releases every reference; `restoredAsSaved` releases its three.
5. The COM offsets that rest on the ABI only (all Get* methods, CreateBlendState 0xA0, CreateRasterizerState 0xB0, Texture2D::GetDesc 0x50) and the D3D11 structure layouts (`TextureDescription` 44, `BlendDescription` 264, `RasterizerDescription` 40, `Viewport` 24).
6. `framePixel` against `shaders/fixed_spike_frame_ps.hlsl` statement for statement (the tests run the C++ copy, not the shader), and `frame_shaders.py --check`.
7. MSVC `/W4 /WX`: `std::array<float,16> !=` comparison, the brace-initialised `TargetBlend` with an `std::uint8_t` array member, `std::initializer_list<void*>` loops, `(std::max)`.

### Unverified (NOT_RUN or Hypothesis)

- Every game row; the Windows build; DXVK and Windows D3D11 accepting the vkd3d-built shaders in the shipped container form.
- That depth 0 still holds the scene's depth at the return of ID 36558 in third person, and that target 1 is what image space reads next.
- That the cb12 the main pass draws with was uploaded from the state values captured at its first pass (strongly supported: the cut's plane constant uses the same state's position adjustment and cuts correctly in game).
- That the main view's viewport depth range is the base range at state +0x5C0 (geometry drawn with a depth-bias mode uses a slightly lower MaxDepth; the error is below a unit).
- That no engine pixel-shader texture outside slots 0..15, and no vertex-shader texture, is a view of the main colour target at 656A7D (if one were, binding the target would make the runtime unbind it and it would not come back).
- Eye adaptation, banding and TAA behaviour; frame cost (one full-screen triangle, one depth load and about 60 ALU per pixel, ~40 COM calls on the CPU; invisible under the 60 fps cap).
- The name `kFreezeFrameBackground` for menu flag 0x20.

## Reproduce the offline checks

```sh
E='/home/dq/.local/share/Steam/steamapps/common/Skyrim Special Edition/SkyrimSE.exe'
objdump -d -M intel --start-address=0x14100fc10 --stop-address=0x14100ff10 "$E"   # vertex shader loader, CreateVertexShader, SetVertexShader
objdump -d -M intel --start-address=0x141579120 --stop-address=0x141579230 "$E"   # BeginTechnique: binds at +0xD5 / +0xEB
objdump -d -M intel --start-address=0x141560340 --stop-address=0x1415606a1 "$E"   # per-pass renderer: slot 6, draw, slot 7
objdump -d -M intel --start-address=0x141562270 --stop-address=0x141562314 "$E"   # helper that tail-jumps to slot 6
objdump -d -M intel --start-address=0x141009e40 --stop-address=0x141009ec1 "$E"   # frame begin: ClearState, slot 12
objdump -d -M intel --start-address=0x141012840 --stop-address=0x141012a70 "$E"   # CreateBuffer and the dynamic buffer description
objdump -d -M intel --start-address=0x141011100 --stop-address=0x141011260 "$E"   # Map / Unmap arguments; input layout on dirty bit 0x400
objdump -d -M intel --start-address=0x1415170c0 --stop-address=0x14151717e "$E"   # world matrix: minus posAdjust
objdump -d -M intel --start-address=0x14101e560 --stop-address=0x14101eaf0 "$E"   # view direction and posAdjust into the render state
objdump -d -M intel --start-address=0x1402eece0 --stop-address=0x1402eed80 "$E"   # FindReferenceFor3D: AsFadeNode, +0xF8, ammunition
SKYCRPG_SHADER_ARCHIVE='/home/dq/.local/share/Steam/steamapps/common/Skyrim Special Edition/Data/Skyrim - Shaders.bsa' \
  "$RUNNER_TEMP/fixed_spike_shader_policy_test"                                    # every shader-archive number above
# second pass
objdump -d -M intel --start-address=0x140656d10 --stop-address=0x140656e57 "$E"   # ID 36558: fog global -> SetClearColor at 656DAA
objdump -d -M intel --start-address=0x14100a3f0 --stop-address=0x14100a521 "$E"   # SetClearColor, RestoreClearColor, the RT clear 77251
objdump -d -M intel --start-address=0x140657500 --stop-address=0x140657650 "$E"   # ID 36559: main target clear at 65758A, (0,0,0,0) at 6575E5
objdump -d -M intel --start-address=0x1406565b0 --stop-address=0x140656c0e "$E"   # ID 36555: the calls to 36558, black at 65697F
objdump -d -M intel --start-address=0x141557090 --stop-address=0x14155814d "$E"   # Effect SetupGeometry / RestoreGeometry
objdump -d -M intel --no-show-raw-insn "$E" | grep -E '# 0x1433d3e[6-8]'          # every reference to the fog-colour global
objdump -d -M intel --no-show-raw-insn "$E" | grep -E 'call +0x14100a(3f0|460|4c0)' # SetClearColor / Restore / RT-clear callers
# third pass
objdump -d -M intel --start-address=0x14036e660 --stop-address=0x14036e700 "$E"   # ToggleSky console handler (ID 22824): same flag rule
objdump -d -M intel --start-address=0x1401a3f40 --stop-address=0x1401a4000 "$E"   # interior sky-mode chooser (called from 1A2F55): cell flags 0x80/0x100 -> Sky::SetMode(1/2/3)
objdump -d -M intel --start-address=0x140411660 --stop-address=0x1404118c0 "$E"   # Sky::SetMode (ID 26222): root +F4 bit 0 at 411794..4117C7
objdump -d -M intel --start-address=0x1401c7d00 --stop-address=0x1401c7d20 "$E"   # Sky::GetSingleton (ID 13878): pointer at 31E0D80 (ID 401652)
objdump -d -M intel --start-address=0x140412df0 --stop-address=0x140413a30 "$E"   # Sky colours: mode 1 (413285), mode 2 weather loop (413384) and cell lighting (413828)
# fourth pass: no new binary site. The Mare / Embershard / Bleak Falls Barrow placements were read from Data/Skyrim.esm
# (CELL EDID -> its persistent and temporary REFR children: NAME, DATA, XSCL; base STAT/MSTT/... records: EDID, OBND, MODL).
# image-space and sky shaders: shaders011.fxp entries counted from 0 in file order (each entry starts with 44 33 22 11);
# entries 60..77 BSSkyShader, 15846/15848 HDR tonemap, 15828/15830 DOF fogged, 15944/15946 VL apply, 15960 VL composite,
# 16000/16002/16004 SAO composite variants; disassembled with vkd3d-shader (D3D assembly target)
# Stage 2 (the frame pass)
objdump -d -M intel --start-address=0x1406565b0 --stop-address=0x140656c0e "$E"   # ID 36555: 656834 (frozen menu background) and 656A78 (normal play)
objdump -d -M intel --start-address=0x141169590 --stop-address=0x141169690 "$E"   # ID 82075: menu stack, flag +0x1C & 0x20
objdump -d -M intel --start-address=0x140656e60 --stop-address=0x14065791f "$E"   # ID 36559: 107131 at 6574C5, the main pass, slots reset at 657853/657871
objdump -d -M intel --start-address=0x141537f60 --stop-address=0x141537fbb "$E"   # ID 107131: target 1, depth ID 77457() = 0
objdump -d -M intel --start-address=0x1410155b0 --stop-address=0x1410157b6 "$E"   # IDs 77453/77454 (set target / depth), 77457 (xor eax,eax)
objdump -d -M intel --start-address=0x141010de0 --stop-address=0x141011160 "$E"   # flush: target views +0xA58, depth views, OMSetRenderTargets, viewport
objdump -d -M intel --start-address=0x14100a590 --stop-address=0x14100a7f0 "$E"   # render-target creator: texture +0xA58, view +0xA68
objdump -d -M intel --start-address=0x14100a980 --stop-address=0x14100aba5 "$E"   # depth creator: bind 0x48, views, SRV +0x20A0
objdump -d -M intel --start-address=0x141015430 --stop-address=0x141015496 "$E"   # both creators reached with the renderer 3331F40
objdump -d -M intel --start-address=0x14100f8b0 --stop-address=0x14100f91a "$E"   # ID 77343: binds +0x20A0 as a pixel texture
objdump -d -M intel --start-address=0x14100ae80 --stop-address=0x14100b521 "$E"   # ID 77258: rows 8..11 = transpose(state+0x430), Map and copy
objdump -d -M intel --start-address=0x1400e97b0 --stop-address=0x1400e97d3 "$E"   # global 3330188 = renderer + 0x10
python3 -B -I modules/camera/tools/frame_shaders.py --check                         # the shader header equals a fresh vkd3d-shader build
python3 -B -I modules/camera/tools/frame_shaders.py --validate "$TMPDIR/frame"      # disassembly and SPIR-V of both blobs
```

Sources read: the binary and Address Library above; `Data/Skyrim - Shaders.bsa` (read-only); `session-p55/research/shader-discard.md` and `p55-spike-clip` (`docs/evidence/p5.5/spike-wave2-clip.md`, `modules/camera/src/fixed_spike_clip.h`, `include/fixed_spike_clip_policy.h`, the wiring test) as leads, each fact used here re-derived except where marked; the coordinator's summary of the clip package's independent review; DXVK `src/dxbc/dxbc_compiler.cpp` at v2.7.1 (zlib/libpng licence) through a summarising fetch, for facts only; `libvkd3d-shader.so.1.19.0` (LGPL) of the Steam Linux Runtime as an offline validator, not linked or copied. No GPL source was opened and no code was copied from any third party. The MD5 block function is written from RFC 1321; the DXBC final-block rule is checked against the archive's own 16,044 stored checksums.
