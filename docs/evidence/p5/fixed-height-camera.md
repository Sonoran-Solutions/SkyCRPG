# DYL-201 / P5-05 — Stage 1, senior review pass

2026-11-02. **Implementation revised; Windows build and all game acceptance remain NOT_RUN.** Base `e14df1a0bf0c948c6963889cefcccbb66a1b1bbb`, worktree `p5-fixed`. No commit, push, PR, Skyrim/Proton launch, external post, UI change, shared-document change or repository build/packaging change. The explicitly requested external route/stage helper changes are under `session-p5/routes`.

D028's curated static-form rule supersedes the old automatic-classifier wording in the phase plan and DYL-201. D026 and D5-2…D5-6 remain intact. This document supersedes the first-pass implementation descriptions; historical spike observations are not production results.

## Review findings and changes

| Finding | Confirmed evidence and implemented response |
| --- | --- |
| 1 — TDM call ownership | **PASS, source audit.** TDM `PlayerCameraHook::Hook` writes the E8 call at ID 50784+1A6 during plugin load; `Update` chains native and calls `UpdatePlayerPitch`. The original file-byte check would refuse that live patch. The new guard accepts an E8 to the exact native target, a target in the pinned TDM image, or a readable FF25 relay into that image. It chains the **returned previous target** from `write_call<5>`, including the TDM relay. A foreign/unreadable/non-E8 target refuses installation. Each address/prologue/owner guard logs its live-memory outcome once; they do not short-circuit away later guard diagnostics. Live installation **NOT_RUN**. |
| Existing Camera update alternative | The existing `ThirdPersonState::Update` hook does not run in first person or other camera states and cannot service all pending lifecycle exits. Therefore the all-state call-site wrapper remains. No PlayerCamera vtable update/destructor patch remains. Camera's existing third-person slot 3/15 native-owner guards remain, with opt-in guard logging. SmoothCam attaches its wrappers later via `DeferredAttach`; `providerChainReady` still checks those live slots belong to SmoothCam. Its collision caster is called, not patched, by the inspected SmoothCam source. The caster's 14-byte entry is compared against **live memory** before patching; a detour by any other plugin refuses. |
| 2 — MSVC warning hazards | Renamed the local increment in `FadeState::step`; explicitly typed both zero alpha-flag alternatives as `std::uint16_t{0}`. Reviewed added conversions, format argument types, declarations and return values. Portable `-Wshadow -Wconversion -Werror` check **PASS**; this is not MSVC compilation. |
| 3 — real column/ceiling collision | See numerical correction below. The column and ceiling remain unlisted, fully collidable and opaque. No transform smoothing, previous-clear-position cache or enclosure grace frame was added. |
| 4 — excessive restoration/recovery | Removed thin restoration from general `suspend()` (which look hitches and `!freeRotationEnabled` use). Removed the unconditional cell-object event restore. Only player attach/detach, actual cell/3D change, loaded-state changes, focused menu events, F6/F8/POV intent or real eligibility loss restore. HUD/Cursor/Fader/Mist events do not request thin restoration. Preparation has no global 100 ms throttle: hit references are processed first, all eight slots can prepare within the bounded tick, then retain 100 ms admission and 400 ms contact/visibility hold. A hitch clamps fade dt; it does not retire a reference. |
| 5 — trace and preparation diagnostics | Every loaded frame contributes height min/max and largest consecutive-frame z change to the next 10 Hz sample. Counters are cumulative, including casts/gaps/fade writes/scans/discards/kept/refusals, separate unknown/overlap/cap counts, restore counts by cause, goal z, player x/y, alpha, prepared references/parts and separately faded references/parts. Timing includes hook-chain/native/adapter/tick totals and max hook/frame interval. Trace starts only after a loaded game, max 12000 lines (about 20 minutes). Prepare refusals log once per reason with trace OFF. First eligible shader logs `GetRefCount()` once. |
| 6 — repeated target-lock work in caster | Eligibility/context runs once per thin tick. `safeNow()` reads the published Main-thread eligibility plus atomic enabled/loaded/pending flags; it never calls `context`, TDM or an effect shader. The ordinary existing Camera context calls are unchanged. One caller-module `VirtualQuery` remains per outer cast to restrict interception to SmoothCam; none is added per continuation. |
| 7 — optional compression latch | Thin filtering stays eligible in `State::obstruction`, subject to all higher-priority state exclusions; target-lock precedence is unchanged. Eligible kept-contact accounting excludes curated beams (even when their admission is pending) and all zero-length probes. Only a real retained non-beam contact can feed DYL-157. DYL-157 remains default OFF. |
| 8 — excessive fades | Whole-model spheres now only prepare opaque private clones. Actual alpha is driven by recent caster contact or a narrow segment/OBND box intersection in the reference's inverse world transform, with a 2-unit local margin and 400 ms hold. No geometry detection changes the allowlist. A nearby beam whose 419-unit sphere overlaps the line does not fade merely for that reason. Multiple genuinely intersecting/recently contacted beams can still fade within the reference cap; no claim of exactly one in all scenes. |
| 9 — native contact classes | Rechecked the dispatch table and actor branch in the exact binary. For native actor-handling layers, player contacts are skipped; Character contacts are skipped except layer 32. Doors/furniture/clutter with resolved native metadata now participate in nearest-forward-contact selection instead of forcing original-beam rollback. Layer 17 follows the native missing-scene-object exception. Unknown roots and unlisted start overlaps still fail closed. Native suffix calls retain their character output; reconstructed gap endpoints represent kept collision geometry, not an actor target. |
| 10 — near-goal rollback / scan origin | An inability to advance after a proven prefix returns the **current** listed collision, rather than the first beam. Unknown or unsafe gap evidence still rolls back. Depth caps are counted. Broad/narrow checks use the observed main camera cast origin (hull 15), with provider goal-minus-local focus before the first observation; the hardcoded `player+80` origin is gone. |
| 11 — retired scene references | Retiring a part releases its geometry and original-property NiPointers immediately. Shader is a raw identity pointer kept alive by its geometry, not an extra owner; no RefCount==2 special case remains. `check()` compares the current geometry shader before dereferencing the old identity, which may have been replaced/destroyed. Retained allocations are the bounded original material/clone/private property pool, not 256 retired scene graphs. Failed partial preparations are also retired. |
| 12 — shutdown destructor | Removed the destructor wrapper and its vtable guard/write. No material-manager call is made from a destructor or loader-lock path. Ordinary menu/load/eligibility exits restore at the next Main update; if process shutdown prevents another update, OS teardown reclaims the process without a late scene call. No serialized fade state exists. |

## Collision drop: expected enclosure response, not a beam-filter defect

**Source, historical spike:** `session-p5/logs/collected/final2-2204-boundary/SkyCRPG_OcclusionSpike.jsonl`, SHA-256 `108e3e73d2a5dce04070f89b863adf5885f525a972defb52bd747cf27e7f601a`. Camera samples precede the collision call: z=551.943 at 99,467.568 ms, z=368.270 at 99,582.513 ms, z=346.649 at 99,697.396 ms. The later gap refusal at 99,697.498 ms disarmed the spike; it did not initiate the fall. Prior contacts include the beam, ceiling and column extension.

**Source, offline Skyrim.esm bounds/placements, rechecked this pass:**

| Reference / base | Geometry |
| --- | --- |
| `60339` / `60337` WRIntFreeWallColumExt01 | Origin (0,−640,320), no rotation; x −27…27, y −664…−616, z 324…575. |
| `5D67D` / `5D61D` WRIntFreeWallColum01 | Origin (0,−640,64), x −31…31, y −677…−604, z 32…324. |
| `5D68E` / `5D1AA` WRIntCeilMid01 | Origin (−128,−768,64), yaw 4.712389; local xy ±128, world z 555…577. |

The review's x≈−42 collision conclusion is consistent with these records: **−42 + 15 = −27**, exactly the column extension's face. The roughly 200-unit fall (552→347) is an expected native collision pop when the camera hull reaches that column. It must not be filtered or smoothed through. The high z≈552 pose is itself a kept `WRIntCeilMid01` ceiling contact, so **“height equals configured preference” cannot literally pass in this room**. The correct implementable claim is that curated beams do not add pull-in beyond real enclosure constraints. Changing that acceptance wording needs the owner; this worker does not change the owner decision.

The original quiet journal does not name every rejecting swept-gap root. Production chooses the nearest known forward obstruction, never substitutes a much nearer beam for that known result. Unsafe overlaps, unknown/malformed contacts, caps and changing eligibility still retain safe native collision. Refusal is per call, not a permanent feature latch. A return from a proven prefix to its current beam near the goal is conservative and no longer rolls back unnecessarily.

**Remaining pops:** real column/ceiling/wall contact; the first ~100 ms of material admission; unlisted overlap/unknown contact; pool/part/reference/cast caps; a foreign material writer; lifecycle exit. The fade hold cannot mask unsafe collision for one frame. No production no-pop PASS is claimed.

## Curated forms and rendering

**Source:** Skyrim.esm SHA-256 `e198c3b85e5e48e0c92a6580d8f66e644256b68d812ee32b61735cf9b753df73`, inspected read-only with `scan_vanilla.py`. List unchanged from reviewed base; resolved by plugin name/local form on Main.

| Plugin/local form | STAT editor ID | OBND min → max; dimensions |
| --- | --- | --- |
| Skyrim.esm / `0602A6` | WRIntBeam768 | (−11,−384,140) → (11,384,256); 22×768×116. Spike reference E7A7B. |
| Skyrim.esm / `0602AF` | WRIntBeam512 | (−11,−256,140) → (11,256,256); 22×512×116. Thin sibling. |
| Skyrim.esm / `0616EA` | WRIntBeam256 | (−119,−10,241) → (120,11,255); 239×21×14. Short horizontal sibling. |

No ceiling, roof, loft, corridor, chandelier, floor, column or wall-connected/support beam was added. Owner extension stays limited to 16 explicit `plugin|localHexID` STAT entries; missing/non-static forms refuse the feature. Mod overrides of a curated base require recuration. Actors/player never qualify.

Each geometry uses a private manager-created material clone. Original alpha-test flags `0x12EC` become `0x10ED`: blend SRC_ALPHA/INV_SRC_ALPHA, disable alpha test, threshold zero. Default target alpha .35, validated .1… .8, ramp 4 alpha units/s with dt capped at .05 s. No shared material alpha write, app-cull, near clip, collision-layer change or transform write.

Shader sharing is not inferred away. **The binary proves the reference-count layout/operations, not a live beam count of 1.** First live count is logged. Additional owners refuse with `shared-or-owned-shader`; a scan of the attached cell cannot prove that other owners elsewhere do not exist. This is a concrete safe fallback, not a verified successful preparation. If the pinned live beams have additional shader owners, the game gate is BLOCKED until that evidence establishes a safe private-shader strategy; count==1 is not fabricated here.

Restore compares geometry shader, installed clone/property and last written alpha/flags before restoring only owned state. Conflicts preserve foreign state and quarantine the slot. Original material/clone/private properties stay in a max-256 pool; compatible non-conflicting slots can be reused after two later updates. No allocation is freed during swap-back; the delay is not a claimed GPU fence. Active scene references are released at retirement. The historical material capture SHA-256 is `ea6619004589fc2b2a45594b0b4de9ccba7bfbe6e42aa1429188a7da131d809d`; its five parts and shared-material counts are spike observations, not current production passes.

## Binary and source audit

**PASS: offline checks only.** Read exact files supplied in the brief. SkyrimSE.exe SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; Address Library SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`. Address Library v5, 1.7.104.0, offsets at `96+4*ID`. No binary/archive downloaded or executed.

| ABI/access | Evidence |
| --- | --- |
| Caster 33007 | RVA 5574F0, whole 14-byte entry `48 8b c4 4c 89 48 20 55 56 57 41 54 41 55`; no RIP-relative operand. Explicit bool ABI retained instead of erroneous CommonLib void declaration. Live comparison before gateway/patch. |
| Update call | ID 50784+1A6 → 8F8826. File E8 targets 558E70; **not** assumed to be live E8 destination. TDM's preexisting relay is checked and chained. |
| Native actor dispatch | 557AA8 table selects layers 5,8,19,20,25,30,32,33,35. 55784D skips player. 5578C7 compares form type 3Eh; 5578D4 preserves layer 32, otherwise 5578D9…557931 records Character and clears hit choice before continuing. |
| Native normal contacts | 5577E1…557800 ignores missing/culled scene objects except layer 17. 557936…5579A3 copies chosen root and scales contact position, regardless of a Static base-form classification. Doors/furniture/clutter therefore need no artificial static-only restriction. |
| Gap query | 61402→B59D80, called at 5576BF; existing context→wrapper+10→phantom, collidable+20. No phantom-world test. GetWorld1 slot 27h→104B470 loads +10h. Cast and start-overlap collectors remain separate; contacts sorted by separatingNormal.w. |
| Material operations (reused audit) | 105544→14E8000 unique manager clone; 105545→14E8080 exact attachment; 107719→1563D10 Create/CopyMembers; 107720→1563FC0 release; singleton 403555→32309D8. Shader material +78, material alpha +80. Live guarded functions retain their exact prologue checks. |
| Property / count | 70687→EF2BE0 factory, flags +30/threshold +32. Native property attachment at 26D430 uses geometry+120 and `lock inc [property+8]`/decrement/delete-at-one. NiRefObject count is plain +8, not an actor-style packed reference count. Shader geometry slot +128. No binary evidence of a different shader-count representation was found. |
| Narrow bounds (new accessor audit) | TESBoundObject OBND loader at 277B3E writes 12 bytes to object+20. BSGeometry bound update at EFC906 passes object+7C as world transform; EFCF20 uses row-major rotation, scale at transform+30 and translation +24/+28/+2C. Added compile-time offset assertions. No new linked CommonLib matrix/accessor function is needed. |
| Locking/scales/cell enumeration (reused audit) | 68233→CE3250; 68239→CE3510; scale 188105→17FDE5C, inverse 187407→17F423C. Cell reference container +88/data+90, spin lock +120. Existing Main-only bounded enumeration unchanged. |

Source: [TDM Hooks.h / Hooks.cpp](https://github.com/ersh1/TrueDirectionalMovement/tree/57b913ae17fed857c9e281799174898cd66a3e82), revision `57b913ae17fed857c9e281799174898cd66a3e82`; inspected existing local source for facts only. No GPL implementation copied.

Source: [SmoothCam](https://github.com/mwilsnd/SkyrimSE-SmoothCam/tree/e0afd09aff26e5849dac461252e5bffbd5b69d62), revision `e0afd09aff26e5849dac461252e5bffbd5b69d62`; `main.cpp`, `hooks.cpp`, `raycast.cpp`, `base_third.cpp`. Deferred camera-state wrappers preserve the existing inner Camera chain; the engine caster is invoked by `CastRay`, and provider applies collision/normal offsets. No provider transform implementation copied.

Source: [CommonLibSSE-NG 7.2.0](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), revision `7a60f4de794095d7b0f8928d1b930a52e9a7da83`; checked trampoline return/FF25 format, raw lightingShaderProp_cast return, NiRefObject, geometry/bound/transform layout, material APIs and nodiscard declarations. Source: original MIT [occlusion spike](https://github.com/Sonoran-Solutions/SkyCRPG/tree/a2d766c14a778b673dd3758705d025791a2efccf/tools/occlusion-spike), revision `a2d766c14a778b673dd3758705d025791a2efccf`; prior independently audited caster/collector/material approach retained without its journal or permanent disarm.

## Bounds, lifecycle and trace interpretation

Default `bEnabled=0` still allocates no optional Runtime, installs no thin hooks, resolves no forms and scans no scenes; `iTrace` is ignored. The existing Camera-only/UI-only independence and target-lock state priority remain unchanged. Runtime hashes include executable/library and pinned SmoothCam/TDM/MCMHelper checks before optional installation.

Per frame: optional cast chain ≤24 gateway invocations (reserve reset; unavoidable upstream calls are never suppressed), ≤16 gap sweeps, eight advances per outer call, 256 cast and 256 overlap contacts/query, 32 parent links/contact. Eight references ×16 parts; at most eight preparations ×256 traversed nodes per tick, 128 preparation alpha writes plus 128 ramp writes. Listed-reference broad scan ≤1024 references at 10 Hz. Pool ≤256. Raising preparation throughput removes the old 0.6–0.7 s global queue; it is **not** a measured frame-budget PASS.

Trace counters are cumulative across the loaded route. `engaged=1` means published policy eligibility; proof of filtering additionally needs positive `discarded`, active parts and alpha below 1. `alpha` is the minimum written material alpha. `refs/parts` count prepared clones (including opaque ones); `fadedRefs/fadedParts` count only alpha below .999, so broad preparation is distinguishable from collateral fading. Refusal `reason` is the last call's compact reason (1 admission, 2 advance/state/budget, 3 unsafe gap, 4 invalid endpoint); cumulative `unknown`, `overlap`, `cap` retain the distinct safety failures between samples. A frame with a return-and-recovery height pop still appears in `minZ/maxZ/maxDeltaZ`. Restore counters count retired references by cause, not menu notifications. If multiple requests coalesce, the first priority cause receives the retirement count.

`hookUs` includes native caster, recasts, reset and gap work after the caller guard; `nativeUs` covers the gateway calls; `adapterUs=hookUs-nativeUs` includes gap/collection/filter work; `tickUs` covers frame policy/preparation/fade/trace. These are CPU wall durations with opt-in instrumentation and logging overhead, not GPU time. `maxHookUs` is the largest outer cast; `framesOverBudget` counts loaded update intervals >16.667 ms, including setup, menus and transitions. Do not label those whole-session totals as the isolated 60-second capture. OFF uses the unchanged testprobe's anchored median/p95/p99 and over33/over50 counts; its 16.667 ms count is unavailable. The external FPS cap remains in place and no uncapped headroom PASS can be inferred.

| Exit | Main-thread action | Game result |
| --- | --- | --- |
| Disable, focused menu, F6/first person, F8/disabled provider, special/invalid state | Pending flags or actual eligibility loss restore owned materials; no look-hitch/freeRotation-triggered restore | NOT_RUN |
| Pre/post load/new game; player cell attach/detach | Atomic load/cause flags; next Main all-state update restores before admission | NOT_RUN |
| Cell ID change; reference unload/3D replacement | Per-tick validation restores and releases retained scene references | NOT_RUN |
| Material/property/shader ownership conflict | Compare-and-restore only; quarantine conflicting slot and keep collision | NOT_RUN |
| Normal quit | Focus/loading/eligibility path if an update remains; no destructor callback | NOT_RUN |
| Forced termination / shutdown without update | No late manager call; OS reclamation, no saved fade state | NOT_RUN |

## Actual commands and results

Job purposes, commands, expected outputs, owner (DYL-201 review worker), timeouts and cleanup were recorded in `/tmp/dyl201-review/jobs.md`. Foreground timeout/tool sessions provided handles; Camera compile/test session `69508` completed with exit 0. No background/game process was created. Raw offline extracts, synthetic binaries and logs remain under `/tmp`; temporary stage fixtures clean themselves up.

| Command | Actual result |
| --- | --- |
| `timeout 120 python3 /tmp/dyl201-review/audit.py` | **PASS**, hashes, caster/update bytes, native actor table, ESM beam/column/ceiling records, historical log hashes. `audit.log`. |
| `timeout 120 python3 /tmp/dyl201-review/accessor_audit.py` | **PASS**, new OBND/world-transform accessor assertions and disassembly. `accessors.log`, assembly files. File inspection is not live-memory validation. |
| `timeout 120 python3 /tmp/dyl201-review/fixtures.py` | **PASS**, targeted vanilla record scan: real load door, stair and low cave-hall geometry. `fixtures.log`; compact provenance in `gen/fixed/fixed-fixtures.provenance`. Poses still require screenshots. |
| `timeout 600 bash /tmp/dyl201-review/tests.sh` | **PASS**, nine Camera portable executables, using `g++ -std=c++20 -Wall -Wextra -Wshadow -Werror -pedantic -Imodules/camera/include`. `tests.log`; final unchanged-policy confirmation `tests-final.log` PASS 9/9. |
| Focused `g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -Werror -pedantic -Imodules/camera/include modules/camera/tests/thin_occluder_policy_test.cpp -o /tmp/dyl201-review/bin/thin-conversion` then execute, 60 s | **PASS**, exit 0. `conversion.log`. |
| `timeout 60 python3 -B session-p5/routes/write_fixed_routes.py` | **PASS**, generated 11 routes. Runtime NOT_RUN. |
| `timeout 600 python3 -B session-p5/routes/test_fixed_tools.py` | Initial **FAIL**, four tests passed/one schema error: key holds were 3 s, runner maximum 2 s. Changed to ten 1.5 s pairs, preserving 30 s movement. Diagnostic rerun **PASS**, 5/5; `fixed-tools-final.log`; final trace-schema confirmation `fixed-tools-verified.log` PASS 5/5. |
| `timeout 600 python3 -B session-p5/routes/test_route_tools.py` | **PASS**, existing 23/23 tests, `route-tools.log` and `route-tools-verified.log`. Synthetic stage/preflight checks report Steam unavailable, as expected; no prepare/launch attempted. |
| `timeout 60 bash -n session-p5/routes/run_fixed.sh`; `git diff --check` | **PASS**. The external routes directory is not a Git repository; its files are external deliverables, not part of the Camera Git diff. |
| Windows MSVC `/W4 /WX`, native linking, candidate DLL/package hashes | **NOT_RUN**, no MSVC/cmake. Orchestrator owns CI after review/commit. No current DLL/hash invented. |

Portable additions cover obstruction eligibility, zero-length/listed contact accounting, native actor-layer decisions, broad opaque preparation versus narrow fade, short-beam box intersections, hitch-safe fade progression, and 552→347→552 within-sample extrema/delta capture. Existing matching, gap nearest/overlap/unknown/cap/refusal, restoration identity, fade hysteresis and disabled/base tests remain. Route tests verify schema/keys, 60-second hold, ≥30-second walking, exact off-step parity, identical budget-pair actions, fixed artifact selection/defaults, refusal of empty/loaded-only/corrupt/reset trace and absent evidence. They do not emulate Havok or renderers.

## P5-05 acceptance

| Requirement | Current production result | Required interpretation/evidence |
| --- | --- | --- |
| Beam translucent; 30-second stable hold without beam-induced pull-in | NOT_RUN | Run 60-second beam hold. Ceiling-limited z≈552 is not configured preference. Literal preference-height criterion requires owner correction. |
| Boundary/corridor/doorway/stairs/rock ceiling/hall retain enclosure and room visibility | NOT_RUN | Native column pop is expected at x≈−42. No blank room/culling workaround is authorized. |
| F6/F8/menu/load/cell/unload/disable restore; target-lock precedence unchanged | NOT_RUN | Interruption captures and per-cause trace; native no-console faded-to-door/cell crossing and lock/compression-both-on coverage still need runtime evidence. |
| Feature OFF equals base; independent modules | NOT_RUN | Source audit and portable disabled/base checks PASS; exact existing route steps retained. Visual equivalence and UI-only/combined remain integration game gates. |
| Same-host performance within Phase 4 budget | NOT_RUN | Capped matched player-pose capture, hook CPU cost, probe intervals/over33/over50, ON loaded-frame >16.667 ms counts. No uncapped/GPU-headroom claim. |
| Owner accepts this production transparency | NOT_RUN | Current candidate screenshots/playtest; spike appearance approval is not production acceptance. |

## Authored routes and orchestrator handoff

External directory: `/srv/storage/Projects/SkyCRPG/session-p5/routes`. New `write_fixed_routes.py` generates only `gen/fixed`; refuses overwrite by default. `make_stages.py --variants fixed,fixed-off` uses **artifacts/fixed/SkyCRPG_Camera.dll** for both, preserving copied RC1 framing while adding production curated defaults, ThinOccluder enabled/disabled, trace=1, adaptive compression OFF. Existing variants remain unchanged. Stage creation with real CI artifacts is NOT_RUN; only temporary synthetic fixtures were assembled.

| Route suffix | Authored actions |
| --- | --- |
| `beam-hold` | Bannered Mare spike beam pose, 60 s standing, before/after screenshots. |
| `boundary` | 30 s W/S at x=−117.110 avoiding column, then 30 s at x=−42 crossing its hull boundary, speedmult 35; captures each segment. |
| `corridor` | First prove beam engagement, then existing Bleak Falls corridor fixture, 30 s movement plus turn. |
| `doorway` | First prove engagement, Bannered Mare interior-door fixture, 30 s movement plus turn. |
| `stairs` | First prove engagement, Bleak Falls `373AD` / NorHallBg1wayStairs256 approach (4992,300,−764), 30 s alternating ascent/descent attempt plus turn. Pose/navigation Hypothesis until captures. |
| `rock-ceiling` | First prove engagement, `389B6` / CaveGHall1Way01, pose (2816,−7744,−2618), ~268-unit cave enclosure, 30 s movement plus turn. Surface/floor clearance Hypothesis until captures. |
| `hall` | First prove engagement, Dragonsreach existing hall fixture, 30 s movement plus turn. |
| `restores` | F6, F8, focused menu while fading; restored/reengaged screenshots; verified load door `16072` (XTEL `1A6F4`), return to beam, exterior cell change, return/engage. Console activation/coc necessarily introduces a menu interruption first: **does not prove uninterrupted faded-to-load/cell crossing**. This is a runner stimulus limitation, not invented runtime evidence. |
| `off` | Exact `p5-cam-off-main` steps, fixed DLL with `bEnabled=0`; no thin trace/hook expected. |
| `budget-off`, `budget-on` | Identical fixture/actions, 60-second anchored probe standing capture, before/after images. Same source DLL, cap retained. Beam OFF/ON may render different framing, so the delta is end-to-end, not isolated fade cost. |

`bash run_fixed.sh <unique-label> [one-suffix]` runs each route in its own invocation; default batch serializes all eleven. It records owner/purpose/command/deadline/log/PID/completion, checks existing processes before each launch, uses bounded INT/kill grace only for its runner process group, and relies on runner cleanup/report. It refuses collection while any Skyrim/loader remains; never kills another task's process. `collect_fixed.py` preserves Camera/probe/SKSE logs with stable-read hashes and mtime. `invocation.json` ties them to runner start/exit/report.

`check_fixed_log.py` requires installation plus actual engaged casts, positive discards and visible alpha/parts for ON routes. A loaded DLL with no engagement fails; OFF instead requires absence of thin hook/trace. `summarize_fixed.py --prefix <label>` additionally verifies log hash/mtime/route, successful runner report and effective setting, includes hardware/package hashes and anchored frame metrics. Its PASS is **only** correlated execution/engagement; missing or failed evidence remains NOT_RUN. Capped comparison is descriptive and leaves budget acceptance NOT_RUN pending package/source/visual review. Logs for these new routes do not exist yet.

## Proposed shared-file changes

No shared document was edited.

**CONTROLS:** “`[ThinOccluder] bEnabled=0` is a restart-only Camera opt-in. It uses a curated plugin/local STAT list, no new binding. F6/native POV, F8, focused menus and special states suspend it. Default alpha .35 (.1–.8). `iTrace=1` logs bounded cumulative collision/fade/restore/timing counters and per-window height extrema at about 10 Hz, up to 12000 loaded-game lines. Adaptive compression remains optional/default OFF.”

**ARCHITECTURE:** “SmoothCam remains sole third-person transform writer. Camera's curated Stage 1 caster filter is eligible in normal/targetLock and the optional obstruction framing substate. Eligibility is published once per Main tick; casts do not run target-lock policy. Every skipped interval gets a bounded hull sweep: nearest known non-beam contact is retained, unlisted overlap/unknown fails closed. Camera chains the guarded live native/TDM all-state update target; it does not patch the PlayerCamera destructor. Broad opaque preparation is separate from narrow/contact-driven per-geometry private-material fading. Scene references release on retirement; retained private allocations are bounded.”

**Acceptance/STATUS proposal for owner review:** “DYL-201 code review revisions and portable/offline tests PASS; Windows/game acceptance NOT_RUN. Bannered Mare's ceiling limits z≈552 and the column causes an expected native 552→347 inward collision at x≈−42. Assess no *beam-induced* pull-in subject to enclosure, not literal preference height/no pop through the column.” This is an engineering recommendation, not an owner ruling.

## Decisions, unverified items and owner question

Engineering decisions: retain a correctly chained all-state update for lifecycle coverage; refuse foreign live patches and genuinely shared shaders; keep broad clones opaque; use real bounds/contact to drive fades; retain nearest non-beam collisions and current near-goal contact; no collision grace frame; no shutdown material-manager call. No list or product-scope expansion.

Unverified: MSVC/link; live guard chain/TDM pitch retention; live shader ownership count; fade sorting/depth/alpha appearance; precise gap endpoint response through SmoothCam; Havok completeness; lifecycle timing; longer-session pool pressure; preparation burst cost; both optional features together; all route outcomes. OFF source equivalence and portable PASS do not establish visual/runtime equivalence.

**Question for owner:** accept the native column pop and ceiling-limited height as expected, and amend the literal configured-height/no-pop criterion to “no additional beam-induced pull-in, subject to enclosure”? No answer was required to implement these safety-preserving fixes.

## In-game runs fx1-2359 and fx2-0002 — 2026-11-02

Eleven routes on `50a3dd2` (Camera DLL `28de1800…`), camera-only, keyboard/mouse, unattended; no crash, every runner exit 0, and restores clean. `check_fixed_log.py` shows engagement only, not acceptance, so the table below comes from traces and screenshots.

| Row | Result | Evidence |
| --- | --- | --- |
| Hooks on the pinned stack | **PASS** | Every guard PASS in live memory, including TDM's patched call. |
| Standing under the beam, 60 s | **PASS** | z=539.639 with zero change at the end of the hold, alpha 0.35, one reference faded (5 parts), cap 0, unknown 0. |
| Transparent beam | **PASS, orchestrator read** | Fire, floor and NPC visible through it. Owner approved the spike look; production look awaits the owner. |
| Listed beams only, enclosure kept | **PASS on these routes** | Corridor, rock ceiling, stairs and hall ended with 0 discards and no fades. The shots show cave floors and rock, so whether the corridor and rock-ceiling poses are the intended fixtures is unconfirmed; the stairs start shot does show steps. |
| Character stays in view | **FAIL at these spots, not caused by this feature at two of them** | Hall: fully visible. Stairs start: visible. Corridor start: top of the head only. Corridor end, rock ceiling start and end: not in frame. Doorway end: top of the head only. Feature-off run `fxb-1735` gives the same framing at corridor start and end and rock ceiling start and end. Stairs end differs: character visible with the feature off, not in frame with it on, but walking ended at different positions, so that one is unresolved. Doorway has no feature-off comparison. In these spots the camera sits about 460-480 units below its goal on a kept native hit. Both runs set `[Obstruction] bEnabled=0`, so framing with compression on is NOT_RUN. |
| F8, POV and menu restores | **PASS on counters** | F8 screenshot shows the beam opaque under the native camera. Restores by cause recorded. |
| Load, cell and unload restores | **NOT_RUN as a distinct test** | Console runs first, so the beam is restored as menu; `restoresUnload` nonzero. |
| Feature off | **PASS, bounded** | `p5-fixed-off` passes with no trace rows; this is the Phase 4 route set, not a byte-level comparison. |
| Frame budget | **PASS at the frame cap** | Off against on at the same pose: p95 +1.04%, no frames over 33 ms; hook about 30 µs and tick about 64 µs per frame. Framing can differ; uncapped NOT_RUN. |
| No pull-in or oscillation walking | **FAIL as written** | z varies from about 288 to 552 walking this room, with about two jumps per pass (up to 236 units) landing on real geometry: columns, ceiling corners and neighbouring pieces. Standing is steady. |
| Target lock with this feature | **NOT_RUN** | |
| Controller | **NOT_RUN** | |

Open for the owner: the literal "height equals preference" can't hold where real geometry is nearer, since z≈540 here is a kept ceiling hit. Whether the remaining walking jumps are acceptable needs a hands-on judgement.

## Menu-close fix, run fx4-1850 — 2026-11-02

`c10e6bc`, Camera DLL `5409d1bd…`, camera-only, keyboard/mouse, unattended: beam-hold, restores and boundary, all runner exit 0, no crash.

Cause: the menu event queued a restore that cleared admission, so the beam hit was kept for the 0.1 s enter dwell. Paused menus run no camera tick (frame numbers stop while the console is open), so that restore only ran on the first frame after close. The event no longer queues a restore. A menu that ticks still restores through eligibility, and beams that were faded are re-admitted on the first eligible frame.

| Row | Result | Evidence |
| --- | --- | --- |
| Closing the console under a beam | **PASS** | No drop in any of these routes; the old build dropped on each close. |
| Closing the Tab menu under a beam | **PASS** | z stays 539.639 with `resumes=1` while alpha returns from 1.0 to 0.35. |
| Beam during the Tab menu | **PASS** | Opaque behind the menu; `restoresMenu=4`. |
| Beam during the console | **Limitation** | It stays faded, because no tick runs while paused. The old build did this too, so `menu` restore doesn't hold for paused menus. Owner decision. |
| Standing hold on this build | **PASS** | z range zero over the last 200 samples, alpha 0.35, one beam faded. |
| Clone pool | **PASS on these routes** | 23-27 at the end of each route, no refusal for the cap. |
| Return from F8 under a beam | **Defect, not fixed** | Still drops 521.737 to 289.418 for one sample. F8 still clears admission on purpose. |
| Walking | **FAIL as written, unchanged** | Three jumps over 150 units in boundary, all kept hits on `00060339` or `0005D67D`, one after the route repositions the player. |
| Closing dialogue | **NOT_RUN** | A special frame after close is covered by the carry, but no route exercises it. |
| Frame budget on this build | **NOT_RUN** | The +1.04% figure is from `50a3dd2`. |

An earlier build of this fix, `2f02dfc`, exited at the main menu once in beam-hold before any save loaded, with no crash log found. It didn't recur on `c10e6bc`, and the cause is unknown.

## Owner hands-on run owner-fixed-1 — 2026-11-03

`c10e6bc`, Camera DLL `5409d1bd…`, camera-only, compression off, target lock off, Bannered Mare. Profile restore PASS. Camera log `7f57bc7d…` in `session-p5/logs/owner-fixed-1/`. Keyboard/mouse, confirmed by the owner after the run.

The owner walked under the beams and columns, closed the console, Tab and dialogue under a beam, pressed F8 twice and F6, then reported: "everything passed and I approve". They skipped the optional low-space check.

| Row | Result | Evidence |
| --- | --- | --- |
| Walking feel under beams and columns | **PASS, owner** | Owner approval. The trace still shows 27 height changes over 150 units while engaged, from kept hits on unlisted geometry (for example `0005D65A`, `0005D67C`, `00060339`), including a 523 to 1004 alternation near the bar before any beam faded. The owner accepted this as felt; the literal "no pull-in" row stays FAIL as written. |
| Console, Tab and dialogue close under a beam | **PASS, owner** | `restoresMenu=11`, `resumes=3`; the camera stayed at height on each resume. Dialogue isn't separable from other menus in the counters. |
| Beam during the paused console | **Accepted limitation, owner** | Seen by the owner and passed. |
| F8 off and on | **PASS, owner** | `restoresF8=5`. The one-sample drop on return is the known defect; the owner passed it. |
| F6 first person | **PASS, owner** | `restoresPOV=5`. |
| Low-space framing | **NOT_RUN** | Optional step skipped. |
| Frame budget | **NOT_RUN as a measurement** | 104 of 6013 ticked frames over budget, worst 2212 ms (a load hitch). Not a controlled comparison. |
| Controller, target lock with this feature | **NOT_RUN** | |

