# P5.5-02 wave 2, package "shader" (DYL-217): world-space horizontal cut in the shader stage, `iSeeThroughMode=3`

2026-11-09. Worktree `p55-spike-shader`, branch `phase5.5/p55-02-spike-shader`, base `744609f9fb459aa1e50688b3ab590bb6cbbac05a`. **Disposable: never merged, nothing here enters the shipped Camera DLL.** Experimental scope of PHASE_5_5 (four official masters, Steam Skyrim SE 1.7.104, SKSE 2.3.1, SmoothCam 1.8.1, TDM 2.3.1).

**Second pass (2026-11-10, base `4a3b722`).** Owner decision after batch W2S: void colour pure black to start, fog cards and light shafts above the cut removed, ENB or Community Shaders still refuse everything. Built: the void colour override and the Effect-class cut, both behind new keys that default on in `iSeeThroughMode=3` only. Everything about it is in **"Second pass: void colour and Effect-class cut"** below; the sections before it describe the first pass and stay valid except where that section says otherwise. Its game rows are **NOT_RUN**.

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
| The fog-colour global 3D3E70..78: writers 0FFB9E/0FFBA6 (initialisation), 41227F/41228B (ID 26229), 4126C5/4126D1 and 4127DE/4127EE (ID 26230); the **only** readers are 656D8B/656D93/656D9B. No other instruction in `.text` references 3D3E60..3D3E8F. | Source: every RIP-relative operand in the disassembly. Nothing here writes the global. |
| ID 36558 has three callers: 656834 and 656A78 in ID 36555 (the frame function), 656CC8 in ID 36557. No absolute pointer to it exists in the image (only its `.pdata` entry). ID 36555's callers: ID 36564 (659319), IDs 19121/19122 (2C65DA, 2C6890) and 1A2B13 (function 1A28F0, no ID found). ID 36559 has one caller, 656E2D in ID 36558. | Source. |
| **Where the colour goes.** Inside ID 36559, which 36558 calls right after the call at 656DAA: 65752C sets (0.5, 0.5, 0, 0), clears the target returned by 153C900 (ID 77251) and restores (657563) the colour back; 65757E binds render target 1 to slot 0 and **65758A clears it with the colour set at 656DAA** (77251 reads `renderer+0x27D8`); **6575E5 sets (0, 0, 0, 0)** (`xmm6` is zeroed at 657057 and not written again before) and nothing restores it in that call. No jump from before 657511 lands after it, and there is no `ret` in 656E60..657750. | Source. So the substituted colour is used by one explicit clear, of the main colour target, and does not outlive the call to 36559; that it is not also used by a pending-clear flush (ID 77247 reads `renderer+0x27D8` at 100A127 for slots set to clear) between 656DAA and 65752C is **Hypothesis**: the functions 36559 calls before 657511 were not all followed. |
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
| A black void in the save-game screenshot | If the save screenshot renders through ID 36555 while the cut is active | Only cosmetic. Hypothesis. |
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
```

Sources read: the binary and Address Library above; `Data/Skyrim - Shaders.bsa` (read-only); `session-p55/research/shader-discard.md` and `p55-spike-clip` (`docs/evidence/p5.5/spike-wave2-clip.md`, `modules/camera/src/fixed_spike_clip.h`, `include/fixed_spike_clip_policy.h`, the wiring test) as leads, each fact used here re-derived except where marked; the coordinator's summary of the clip package's independent review; DXVK `src/dxbc/dxbc_compiler.cpp` at v2.7.1 (zlib/libpng licence) through a summarising fetch, for facts only; `libvkd3d-shader.so.1.19.0` (LGPL) of the Steam Linux Runtime as an offline validator, not linked or copied. No GPL source was opened and no code was copied from any third party. The MD5 block function is written from RFC 1321; the DXBC final-block rule is checked against the archive's own 16,044 stored checksums.
