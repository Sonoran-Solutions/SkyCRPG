# DYL-201 / P5-05 — production Stage 1 candidate

2026-11-02. **Implementation prepared; acceptance remains NOT_RUN.** Worktree `p5-fixed`, base `151a2c62e9b8d77c242f6a8f093cdc44fa6dbcbf` (target-lock precedence present). Uncommitted changes for the orchestrator. No commit, push, PR, Skyrim/Proton launch, external post, downloaded binary/archive/mod, UI edit, shared-document edit or build/packaging change.

The current owner brief and D028 authorize implementation with a curated base-form list and the approved spike appearance; they supersede the historical classifier/start-gate wording in the phase plan and ticket. D026 and D5-2 through D5-6 are preserved. This is not a claim of a new runtime GO, Windows build, owner playtest acceptance or release.

## What is implemented

- Default `[ThinOccluder] bEnabled=0`: no optional runtime allocation, caster/camera hooks, form resolution, scene scans or fade writes. Existing Camera policy and DYL-157 measurement remain unchanged. Restart after editing this section; it is not a live reload interface.
- With opt-in, executable/Address Library hashes are checked before CommonLib initialization, and pinned provider hashes before installation, using the existing Camera checks. The adapter additionally guards caster, sweep, material attach/clone/release, factory, locking/scales and PlayerCamera call site/destructor slot. Unexpected hook owners/prologues refuse with a bounded log and no optional hook installation.
- Only State::normal and State::targetLock third person qualify. The adaptive obstruction state suspends filtering while continuing to observe native collision results for DYL-157. First person, F6/F8, focus/menu, special states, invalid world, disabled SmoothCam, foreign camera ownership and loading retain native collision. SmoothCam remains the sole normal transform writer. No RequestCameraControl, crosshair hook, room-seeding, app-culling, near-clip or collision-layer changes.
- Curated `plugin|localHexID` entries resolve through TESDataHandler on Main, not in kDataLoaded. Static base forms only; missing/non-static/malformed entries refuse. Engine camera casts from callers outside the pinned SmoothCam module pass through unchanged.
- Filtered hits require a prepared, admitted fade. Each continuation advances at least `max(30, 2*hull)` units, bounded to eight steps. Before advancing, the same phantom/hull/filter sweeps the entire gap with separate fixed-capacity cast and start-overlap collectors. All contacts, not just the first beam, are inspected. The phantom's null world is never compared; GetWorld1 supplies the query world.
- A complete gap containing an unlisted static forward hit returns the **nearest** such hit, ordered by the native contact fraction, with the full native root/shape-key/contact record and world-scaled contact position. The first beam is not substituted for this known enclosure. An unlisted start overlap, unknown/actor contact requiring extra native rules, malformed contact, incomplete original-contact rediscovery or budget exhaustion retains the original native result. Continuations reset the phantom through the original gateway. Refusal does **not** latch filtering off: the next eligible call can prove its path again.
- Fades track recently hit/listed references for 400 ms, enter filtering after 100 ms of prepared fade time, and ramp alpha at 4 units/s (bounded dt), including the return to opaque. A 10 Hz check considers listed statics whose world-bound sphere intersects the current or preferred player-to-camera sightline, with a 30-unit margin. Bounds locate already-listed forms; they never add a form to the allowlist or authorize a collision discard. This also prepares neighbouring beams before a boundary contact. Broad spheres can fade a nearby listed beam that is not exactly on the visual ray.
- Alpha writes touch only a private manager-created material clone. The shader must have exactly the geometry's owning reference (or geometry + cache reference on reuse); ambiguous additional owners refuse. A private NiAlphaProperty enables source-alpha/inverse-source-alpha blending and clears alpha-test bit 9. Original `0x12EC` becomes `0x10ED`, threshold 0; opacity is not implemented by hiding geometry. Original material/property identities are restored only while the installed pointers and last written alpha/flags still match. Conflicts preserve foreign state and quarantine that slot.
- A bounded pool retains up to 256 clone/property pairs through process exit; no private allocation is freed at swap-back. Idle, non-conflicting slots may be reused after two later camera updates only for the **same original material allocation, original alpha and blend flags**, without changing texture/layout data. A slot used by a live reference cannot be reused. New material varieties after saturation retain native collision. Two updates are a reuse delay, not a claimed GPU fence: allocations remain alive. Original geometry/shader references use normal intrusive ownership. Renderer lifetime/appearance still require the route below.
- A guarded all-state PlayerCamera call-site wrapper checks Main::threadID and services lifecycle flags/fades before chaining the existing native camera update. It also checks eligibility after the chain. Worker-thread menu/load/cell/input notifications only post restoration requests for this feature. Cell change and reference/3D identity checks restore held objects; the scalar-deleting PlayerCamera destructor has a guarded Main-thread restore path. No scene teardown runs under the DLL loader lock. Forced process termination cannot execute a restore callback; process memory is reclaimed and no fade state is serialized.
- DYL-157 stays default OFF. When this filter is installed, compression sampling requires a kept native collision in the preceding camera frame; a filtered-to-clear beam cannot engage it. Target-lock precedence and suppression of adaptive compression are unchanged. With ThinOccluder OFF, the existing branch-base approximate obstruction measurement is preserved.

**Remaining pops:** entering a beam before its fade can be prepared, reference/part/pool caps, unknown contact/overlap, another material writer or a real inward enclosure boundary can still produce a native inward movement. No cached clear result or grace frame is allowed to put the camera inside enclosure. Temporal admission/linger removes single missing-observation fade churn; it cannot safely mask a genuinely new obstruction or incomplete collision proof. A 30-second no-pop runtime PASS is not claimed.

## Curated forms (offline Source)

Read-only Skyrim.esm SHA-256 `e198c3b85e5e48e0c92a6580d8f66e644256b68d812ee32b61735cf9b753df73`, using the existing `session-p5/routes/scan_vanilla.py` parser. The entries below are STAT records with the listed OBND values. **PASS: record identity and bounds. NOT_RUN: this production candidate's visuals/collision on each form.** Bounds alone do not prove the whole collision mesh safe; D028 supplies the explicit curation rule, and runtime enclosure sweeps remain mandatory.

| Local form / plugin | Editor ID | OBND min → max; extents | Inclusion |
| --- | --- | --- | --- |
| `0602A6` / Skyrim.esm | WRIntBeam768 | (−11,−384,140) → (11,384,256); 22×768×116 | Tested spike beam family; selected reference E7A7B at (−128,−640,64), cell 1605E. |
| `0602AF` / Skyrim.esm | WRIntBeam512 | (−11,−256,140) → (11,256,256); 22×512×116 | Thin sibling; Bannered Mare references include 602B1 at (0,−384,64) and 602B2 at (−256,−384,64). |
| `0616EA` / Skyrim.esm | WRIntBeam256 | (−119,−10,241) → (120,11,255); 239×21×14 | Short horizontal beam; references 602CE at (384,−384,64), 616EB at (−1152,−384,64). |

Excluded despite their names: WRIntBeamSupport01 `602A7` (14×302×236, tall support) and WRIntBeamWall01 `602AD` (17×247×114, wall-connected variant). No ceiling, roof, loft, corridor, chandelier, wall column or floor form is included. No separate lintel form was added without verification. Owners can extend up to 16 entries after inspecting the complete form/mesh and route; mod overrides that change an approved form into enclosure need recuration. Actors/player cannot qualify as STAT references. Production never applies the failed down-facing-normal classifier.

## Boundary investigation: what the existing capture actually says

Source: read-only `session-p5/logs/collected/final2-2204-boundary/SkyCRPG_OcclusionSpike.jsonl`, SHA-256 `108e3e73d2a5dce04070f89b863adf5885f525a972defb52bd747cf27e7f601a`. These are historical spike observations, not test results for this candidate.

The timeline is more specific than “the guard caused a 200-unit fall.” Camera-height samples are taken **before** the native collision call. At 99,467.568 ms the camera is z=551.943; at 99,582.513 ms it is already z=368.270; at 99,697.396 ms it is z=346.649. The guard refusal is later, at **99,697.498 ms**, sequence 10174, frame 5157. It then restores/disarms the spike. The last 475.8 ms contact-count window includes SmoothCam hits on beam E7A7B (6), ceiling 5D68E (14) and wall-column extension 60339 (9); column 5D67D has 9 native non-SmoothCam hits. The log's quiet mode omits per-gap roots, so it does **not** identify the rejecting gap contact or prove a WRIntBeam512 was responsible.

Offline geometry strengthens the enclosure explanation:

| Reference / base | Verified placement and bounds |
| --- | --- |
| 60339 / 60337 WRIntFreeWallColumExt01 | Origin (0,−640,320), no rotation; bounds x ±27, y ±24, world z 324…575. |
| 5D67D / 5D61D WRIntFreeWallColum01 | Origin (0,−640,64), no rotation; bounds x −31…31, y −677…−604, world z 32…324. |
| 5D68E / 5D1AA WRIntCeilMid01 | Origin (−128,−768,64), yaw 4.712389; local x/y ±128, world z 555…577. |

At the drop the player is around x=−41.98 and y=−570…−585. **Hypothesis:** the 15-unit hull reaches the column/extension boundary near x=−27 while the high goal approaches the ceiling. Those objects are deliberately unlisted. Exact triangle contact, step and overlap attribution are unavailable in the quiet log. Thus adding neighbouring beams alone cannot promise to remove this drop. This candidate returns a known nearest gap obstruction, preserves unsafe overlaps, and resumes filtering after clearance instead of permanently disarming. Opt-in production trace adds kept/gap reference IDs to help distinguish these outcomes.

The matched historical `final2-2204-budget-filter-material` JSONL SHA-256 is `ea6619004589fc2b2a45594b0b4de9ccba7bfbe6e42aa1429188a7da131d809d`. Its five parts used private clones, and two original alpha properties had decimal 4844 (`0x12EC`). Existing capped 16.5 ms p95 evidence is not an uncapped budget PASS for this candidate.

## Binary and source audit

**PASS, offline only.** Executable SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; Address Library SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`. The exact read-only paths were those in the brief. Python read PE sections and Address Library v5 offsets; objdump inspected code. No executable was launched.

| Checked binary fact | Address / finding |
| --- | --- |
| Caster ID 33007 | RVA 5574F0; exact 14-byte prologue `48 8b c4 4c 89 48 20 55 56 57 41 54 41 55`, complete instructions without RIP-relative operand. |
| Gap sweep ID 61402 | B59D80, native call at 5576BF. [context] → wrapper+10 → phantom; collidable +20. GetWorld1 virtual slot 27h; no phantom-world comparison. |
| GetWorld1 | bhkWorld vtable ID 238918 at 1A7CDA8, slot 27h → 104B470, `mov rax,[rcx+10h]; ret`. Runtime refuses a different implementation for optional sweeps. |
| Contact copying | Native collector 1096330 copies contact, follows body parents +18h, copies roots and shape keys from incoming bodies at +30h/+38h; full root record size 40h. |
| Nearest ordering / output | Native sort at 7E9810 compares contact +1Ch (separatingNormal.w). Caster 557936…5579A3 copies the chosen root record and scales contact.position into end xyz. Separate overlap and cast collectors avoid treating penetration distance as a forward fraction. |
| Native exclusions | 5577E1…557800 excludes missing/culled objects except layer 17; actor branches exclude player at 55784D. Unknown actor-specific output is conservatively refused. |
| Material clone/attachment | 105544→14E8000, shader material +78h; manager 107719→1563D10 dispatches Create/CopyMembers; 105545→14E8080 attaches exact original and reference-counts through manager. 107720→1563FC0 and singleton 403555→32309D8 guarded. Private clones remain retained. |
| Alpha factory and geometry | 70687→EF2BE0 creates the 38h-byte property with flags +30h/threshold +32h. Native material-alpha setter 151D560 writes only material+80h (same assignment as the pinned CommonLib UpdateMaterialAlpha helper). BSGeometry alpha attach function 26D430 uses geometry+120h and intrusive refs +8; shader is geometry+128h. World-bound updater EFC810 accesses +E4h center/+F0h radius. |
| Cell enumeration | CommonLib's versioned runtime data uses references +88h (container data at +90h) and spin lock +120h. Native destructor 2B7F10 accesses +90h; native lock wrapper 2CBED0 addresses +120h. Enumeration is under the CommonLib spin lock and capped at 1024 references. |
| All-state update / destruction | PlayerCamera vtable ID 214859→196D980; slot 2→558E70 calls current state slot 3, then changes states. Slot 0→8FC670 is the scalar-deleting destructor (RCX object, EDX flags, RAX object). PlayerCamera uses a **direct call**, ID 50784+1A6 at 8F8826, bytes `e8 45 06 c6 ff`, so the wrapper patches that call rather than slot 2. The other direct caller at 746A79 is left alone. Call target, vtable slots and entry bytes are guarded; Main thread check remains mandatory. |
| Locks/scales | 68233→CE3250, 68239→CE3510; world scale 188105→17FDE5C, inverse 187407→17F423C. Matching runtime guards checked against file bytes. |

Source: [CommonLibSSE-NG 7.2.0](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), revision `7a60f4de794095d7b0f8928d1b930a52e9a7da83`, read from the existing checkout. Relevant files: TESObjectCELL.cpp, TESHavokUtilities.cpp, BSShaderProperty.cpp, BSGeometry.h, NiAVObject.h, NiAlphaProperty.h, hkpCdPoint.h, hkpRootCdPoint.h, PlayerCamera.h, TESCamera.h. CommonLib's `LinearCastPhantom` declaration is not used as the caster ABI; the audited bool-returning signature is explicit.

Source: [SmoothCam](https://github.com/mwilsnd/SkyrimSE-SmoothCam/tree/e0afd09aff26e5849dac461252e5bffbd5b69d62), revision `e0afd09aff26e5849dac461252e5bffbd5b69d62`, existing local source. Raycast::CastRay consumes end only on a returned hit; BaseThird::ComputeRaycast applies its own normal/hull offset. This adapter returns collision information and does not write a transform. No provider implementation was copied.

Source: original MIT [SkyCRPG spike](https://github.com/Sonoran-Solutions/SkyCRPG/tree/a2d766c14a778b673dd3758705d025791a2efccf/tools/occlusion-spike), existing read-only `p5-spike` HEAD `a2d766c14a778b673dd3758705d025791a2efccf`. Reused the independently audited gateway/collector/material-manager approach; no diagnostic journal, app-cull/room/near-plane code or GPL implementation was imported. Copyright/SPDX notices retained in new files.

## Bounds and lifecycle

Per camera frame: optional continuations stop once the native-call count would exceed 24 (reserving the original-input reset), at most 16 gap sweeps, eight continuations per outer call, 256 contacts in each sweep/overlap collector, 32 parent links/contact. Mandatory upstream native calls are never suppressed to meet a counter cap. Eight active references × sixteen parts = 128 ramp writes/update plus at most 16 preparation resets, at most one preparation/100 ms with 256 visited nodes/reference, and one sightline scan/100 ms with 1024 visited references. Pool: 256 retained private pairs. Caps refuse; they never imply clear space. No unbounded contact vectors or journals.

`iTrace=1` adds at most 1200 ordinary log lines, approximately 10 Hz, sampled at camera update before the next native frame. Fields: frame, monotonic seconds, camera z, z relative to player, previous-frame cast/gap/fade/scan/discard/kept/refusal counters, pool usage, keptRef, gapRef, reason. Reasons: 0 none, 1 fade not prepared/admitted or ref cap, 2 invalid/near-goal/depth/cast budget/state continuation, 3 gap incomplete/unknown/overlap/cap, 4 invalid returned position. It is opt-in, not a diagnostic journal. Native-call/CPU cost and GPU cost need measurement; finite counts are not a performance PASS.

| Exit | Implemented action | Runtime evidence |
| --- | --- | --- |
| Disable/F6/F8/first person/special/disabled provider | Post stop intent, Main-thread all-state update rechecks eligibility, identity-checked immediate restore | NOT_RUN |
| Menu (including quick open/close) | Event posts stop flag; no event-thread scene writes; next Main update restores before any new admission | NOT_RUN |
| Pre-load/new game/post-load | Atomic world flag plus restore request; held scene objects restored before re-resolution/admission | NOT_RUN |
| Cell transition/reference detach or replacement 3D | Event requests restore; every Main update validates cell and every active handle/root | NOT_RUN |
| Renderer/material/property conflict | Restore only still-owned values; quarantine conflicted slot, keep collision | NOT_RUN |
| Normal quit/camera destruction | Main-thread scalar-deleting destructor wrapper restores then chains; off-main posts only; pool persists to OS teardown | NOT_RUN |
| Fatal/forced process termination | No callback guarantee; memory reclaimed, no serialized mutation | NOT_RUN |

## Actual worker checks and acceptance

Jobs were recorded before execution in `/tmp/dyl201/jobs.md`: purpose, owner (DYL-201 worker), command, log, expected exit 0, foreground tool handle, timeout and cleanup. All returned; no background/game processes started. Scratch scripts, disassembly, binaries and logs stay in `/tmp/dyl201`; game binaries/assets/personal configuration were not copied into the repository.

| Command / scope | Actual result |
| --- | --- |
| `timeout 600 bash /tmp/dyl201/test.sh` — each Camera `*_test.cpp` compiled with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/camera/include` then executed | **PASS**, 9/9 Camera test executables, `tests.log`; repeated after malformed-contact changes, **PASS** 9/9, `tests-final.log`. |
| `timeout 60 bash -c 'g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/camera/include modules/camera/tests/thin_occluder_policy_test.cpp -o /tmp/dyl201/bin/thin_occluder_policy_test && /tmp/dyl201/bin/thin_occluder_policy_test'` | **PASS**, exit 0, `focused-final.log`, after final pool-reuse/alpha-validation cases. |
| `timeout 120 python3 /tmp/dyl201/verify_offline.py`; `more_abi.py`, `accessors.py`, `camera_callers.py`, `final_abi.py` with the same 120 s bound | **PASS**, corrected extraction and final assertions; logs `offline-corrected.log`, `more-abi.log`, `accessors.log`, `camera-callers.log`, `final-abi.log`. Initial extraction **FAIL** because it tried to read a BSS singleton as file bytes; corrected to verify its RVA only. The initial script name also shadowed Python inspect during error handling; renamed. No identical retry. |
| `git diff --check` | **PASS**. |
| Windows MSVC `/W4 /WX`, native link, DLL/package hash | **NOT_RUN**; no MSVC/cmake here, orchestrator owns CI. API signatures/nodiscard/conversions/forward declarations reviewed against the pinned headers; this is not a compilation result. |

Portable tests cover plugin/local-ID parsing and resolved matching, all refusal gates, disabled/base compression policy, state eligibility, hull-sized progress/depth/near-goal/nonfinite refusal, nearest-gap selection including callback order, unknown/overlap/malformed/cap cases and recovery without a latch, native exclusions, budgets, bounded sightline intersection, alpha validation/flag conversion, enter/exit/fade ramp, restoration identity table and safe pool-reuse predicates. They do not emulate Havok or prove render behaviour.

| P5-05 acceptance requirement | Result on this candidate | Required evidence |
| --- | --- | --- |
| Bannered Mare preference height/zoom, visibly translucent beams, no near-plane pop/oscillation for 30 s | **NOT_RUN** | Windows candidate, traced boundary hold/crossings and screenshots; distinguish preserved ceiling/column collision from beam filtering. |
| Corridor, doorway, stairs, low rock ceiling, large hall preserve collision and room visibility | **NOT_RUN** | Five routes; all unlisted geometry retained, no culling changes. |
| Target lock and F6/F8/menu/load/death/unload restore, no stale state | **NOT_RUN** | Interruption sequence in Camera-only, plus final combined integration. |
| Feature OFF equals branch base; independent modules | **NOT_RUN** | Source off-path review passes; visually/behaviourally compare Camera-only and UI-only/combined under orchestrator control. |
| Same-host frame overhead within Phase 4 budget | **NOT_RUN** | Uncapped matched comparison if obtainable; otherwise explicitly capped evidence with no GPU-headroom claim. |
| Owner accepts translucent production appearance (including alpha-test parts) | **NOT_RUN** | This candidate's captures/playtest. Spike approval is not automatically production acceptance. |

## Proposed route definitions (not executed)

Orchestrator owns Windows build, package hashes and serialized game jobs. Use copied saves and isolated Camera-only profile; one Skyrim process per profile. Before each job record purpose, exact command/route/profile, expected capture/log output, owner, PID/CI URL, deadline and cleanup signal. Do not duplicate a quiet process. Each manual/scripted route ≤20 minutes; each performance sample ≤10 minutes. Capture runtime, host, source+diff identity, candidate/package hashes, SKSE/SmoothCam/TDM versions/hashes, load order and trace settings. No candidate hash exists yet.

| Proposed route | Definition / captures / pass criteria |
| --- | --- |
| `p5-fixed-boundary-30s` | Reuse final2-2204 boundary save/pose in Bannered Mare, preference unchanged, bEnabled=1, alpha=.35, trace=1. Settle 2 s. Ten 1.5 s forward/back pairs at the prior copied-save speedmult 35 (30 s movement), then 30 s stationary on the beam boundary; take before/crossing/settled/restored screenshots. Record which keptRef/gapRef explains each height change. Walk back clear after any refusal and verify automatic recovery. No blanket promise of z≈552 through the verified column/ceiling. |
| `p5-fixed-corridor` | Use final corridor-filter fixture, identical path/pose, new feature ON; 25–30 s forward/back/turn. No listed vanilla Whiterun beams here: zero discards, native collision and complete room visuals. |
| `p5-fixed-doorway` | Bannered Mare low doorway, both approaches and a slow turn for 30 s. Stop on the lintel/column boundary. Listed beam may fade; wall/column/door geometry must remain. Capture kept/gap references when constrained. |
| `p5-fixed-stairs` | Interior stair flight: ascend/descend twice, stop beneath landing and turn. Steps/landing/floor stay opaque and collidable; restore after leaving sightline. |
| `p5-fixed-low-rock` | Verified Nordic low rock-ceiling fixture, approach/turn/retreat 30 s. No filtering or fade on rock/ceiling/corridor pieces, no camera outside-room blank view. |
| `p5-fixed-large-hall` | Reuse Dragonsreach large-hall fixture; 30 s slow turn and walk near beam/column intersections. Check list-only fades, complete geometry and stable bounded counters. |
| `p5-fixed-interruptions` | While actively fading: F6 return, F8 pairs, quick inventory/console/map open-close, dialogue, target-lock on/off, target death, save/load, door/cell transition, player death/reload, ordinary quit. Reenter fixture between cases. Check originals restored, new fade ramps, fresh target-lock requirements unchanged. Repeat with configured feature OFF after restart. Include shared-material lookalike beams outside the selected sightline. |
| `p5-fixed-off` | Restart with bEnabled=0 and deliberately invalid thin form/trace/alpha fields; no optional hooks or scans should run. Repeat branch-base camera route; compare height/zoom/input/menus and load logs. Separate absence test: Camera-only without UI; UI-only without Camera handled by integration gate. |
| `p5-fixed-frame-budget` | Record 60 s standing and 60 s slow turn at identical save/pose/settings for OFF and ON. Use an unobstructed pose for matched rendered framing, then a beam pose for end-to-end cost. Beam OFF/ON can render different scenes; label that confound rather than calling it isolated fade overhead. Default trace OFF for timing; a separate bounded traced capture supplies counts/heights. Same host/package/dependencies, CPU/GPU timings, median/p95/p99, memory and cap state; require Phase 4 threshold (≤10% p95 overhead). If FPS remains capped, retain that fact and leave uncapped headroom NOT_RUN. |

## Proposed shared-file changes

No shared file was edited. Proposed CONTROLS text: “Camera's `[ThinOccluder] bEnabled=0` is an independent opt-in Stage 1 beam filter/fade. Restart after edits. It has no new input binding; F6/native POV, F8, menus and special states suspend it. Default alpha .35, validated .1–.8. The curated `plugin|localHexID` list is not an automatic geometry detector. `iTrace=1` is bounded optional evidence logging. Native enclosure collision stays enabled; adaptive compression remains optional/default OFF.”

Proposed ARCHITECTURE text: “SmoothCam alone writes normal third-person transforms. Camera may intercept the pinned engine camera caster solely for curated Stage 1 static forms while normal/targetLock is eligible. Every skipped interval is swept; nearest unlisted forward enclosure is retained, ambiguity fails closed per call. Private per-geometry material/property allocations provide fade and conditional restoration, bounded by fixed caps. Worker notifications post flags; only Main::threadID performs queries and scene/shader work via the guarded all-state Camera update. No UI dependency, crosshair modification or room-culling policy.”

Proposed STATUS/affected ticket text: “DYL-201 production candidate prepared, portable Camera policy suite and exact-binary/source audit PASS; Windows build/link and every production runtime acceptance row NOT_RUN. D028 curation implemented. Boundary reanalysis finds drop preceding spike guard refusal and native ceiling/column contacts; nearest-gap preservation is implemented but no-pop acceptance remains open.”

## Worker handoff

Files changed: `modules/camera/src/plugin.cpp`, new `modules/camera/src/thin_occluder.h`, new `modules/camera/include/thin_occluder_policy.h`, new `modules/camera/tests/thin_occluder_policy_test.cpp`, `modules/camera/config/SkyCRPG_Camera.ini`, this evidence file.

Decisions: three conservative vanilla beam forms; no wall/support/lintel guess; nearest verified gap contact instead of original-beam rollback for a known forward obstruction; immediate native fallback for unsafe/unknown cases; 100 ms admission/400 ms linger; 10 Hz listed-only sightline preparation; private alpha-test-disabled blending; bounded retained/reusable allocations; no permanent collision-refusal latch; all-state Main-thread lifecycle call-site wrapper. These are implementation choices within D028, not new owner product rulings.

Unverified: MSVC compile/link, actual shader reference counts on live render passes, alpha-blending/sorting/depth appearance, returned-gap collision response including SmoothCam's normal offset, complete Havok contact coverage, lifecycle callback timing during pause/load/quit, pool pressure in long sessions, all route acceptance and actual frame budget. Bounds of work and portable PASS do not establish these.

Questions for owner: none required to implement this brief. The orchestrator should build/review the diff, then collect the listed routes; any requirement to suppress an unavoidable enclosure/unknown-case inward movement needs a separate safety/scope decision.
