# P5.5-02 wave 1 (DYL-217) — fixed rig, room seeding and rule-based hide spike

2026-11-09. Worktree `p55-spike`, branch `phase5.5/p55-02-fixed-camera-spike`, base `da16672`. **Disposable: this branch is never merged and nothing here enters the shipped Camera DLL.** Experimental scope only (D045): Skyrim.esm, Dawnguard, HearthFires, Dragonborn on Steam Skyrim SE 1.7.104, SKSE 2.3.1, SmoothCam 1.8.1, TDM 2.3.1.

**Status.** Source, portable tests and offline binary audit are complete. Wave 1 was committed unchanged as `1de4b28` and compiled under MSVC `/W4 /WX` on the first attempt (CI run 42720286058, reported by the orchestrator). This document now also covers **wave 1b**, the fixes from the independent binary review of `1de4b28` (section "Review fixes" below); **the wave 1b changes have not been compiled on Windows (NOT_RUN)**. **Every in-game row is NOT_RUN.** No commit, push, branch change, game or harness launch, download or external post was made. Compilation and portable tests prove no in-game behaviour.

Each claim is marked **Source** (file:line, URL and revision, binary RVA, or a recorded game log) or **Hypothesis**. RVAs are relative to image base `0x140000000` of `SkyrimSE.exe` SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Provider sources read: SmoothCam `e0afd09aff26e5849dac461252e5bffbd5b69d62` (https://github.com/mwilsnd/SkyrimSE-SmoothCam), True Directional Movement `57b913a` "Version 2.3.1" (https://github.com/ersh1/TrueDirectionalMovement), both from the local audit checkouts under `session-20261026/targeting-audit/`; facts only, no code copied. CommonLibSSE-NG `7a60f4de794095d7b0f8928d1b930a52e9a7da83` (the pin in `cmake/CommonLib.cmake`).

## What was built

All of it sits behind `[FixedCameraSpike] bEnabled` (default 0).

| File | Content |
| --- | --- |
| `modules/camera/include/fixed_spike_policy.h` (new) | Portable policy, no engine headers: settings validation and key reservation, the four headings, eased snap turn, yaw-hold input, eligibility, zoom hold, cast decision, pull-in pairing and per-frame rig window, room-seeding decision with the viewer gate, hide classification (bounds, cut, ray, floor), dwell hysteresis, caps, cut-plane tracker, ray targets and marching, restore table, trace formatting, line budgets and the asynchronous trace writer. |
| `modules/camera/src/fixed_spike.h` (new) | Native adapter included by `plugin.cpp` after `thin_occluder.h`: runtime object, caster suppression (own detour or chained through the thin detour), `QPointWithin` vtable hook, frame bracket, hide scan / apply / restore, the three traces, guarded installation with one coded refusal per site. |
| `modules/camera/src/plugin.cpp` | INI loading, look-hook yaw hold, update-hook zoom hold and frame marking, frame bracket in `serviceCameraCall`, snap keys in the input sink, lifecycle requests, DYL-157 suppression, install call. |
| `modules/camera/src/thin_occluder.h` | One function pointer in `thin::Runtime` and one line in `thin::hookCaster` so both features share the single detour on ID 33007. |
| `modules/camera/config/SkyCRPG_Camera.ini` | `[FixedCameraSpike]` section, all defaults off. |
| `modules/camera/tests/fixed_spike_policy_test.cpp` (new), `tests/lock_refresh_wiring_test.py` (extended) | Portable tests and the wiring audit. |

No build file changed: both new headers are reached through `plugin.cpp`, the only source of the `SkyCRPG_Camera` target (`modules/camera/CMakeLists.txt:1`), as `thin_occluder.h` and `cursor_mode.h` are.

### Off equals the branch base

With `bEnabled=0` no `fixed::Runtime` is allocated (`fixed_spike.h` `configure`, called only inside the `flag("FixedCameraSpike","bEnabled",0)` block, `plugin.cpp:325-346`). Every call site in `plugin.cpp` is either guarded by `fixed::runtime`, or is one of five helpers that return at once on a null runtime (`engaged`, `requestRestore`, `dropSnaps`, `lifecycle`, `install`). `thin::Runtime::spikeCast` stays null, so `thin::hookCaster` is unchanged. In `look()` the only edited base statements are `if (pauseCorrection(c) && !rig)` and `} else if (recenter) {`; with `rig == false` both reduce to the base. `tests/lock_refresh_wiring_test.py` asserts all of this on the source text and the portable test asserts the policy side (`rigEligible(false,*)`, `heldZoom(false,…)`, `castDecision(false,…)`, `obstructionSampled(false)`, seed mode 0). **In-game regression of the off state: NOT_RUN.**

A spike that is enabled but refused at install (`active == false`) also takes the base paths: `engaged()` is false, `holdFrame()` returns false, the base zoom code runs.

## Review fixes (wave 1b)

The independent binary review of `1de4b28` returned CHANGES, 0 BLOCKER / 1 MAJOR / 4 MINOR. Each finding was confirmed against the code and the binary and fixed; none is disputed.

| # | Finding | Confirmed how | Fix |
| --- | --- | --- | --- |
| MAJOR 1 | Above-cut hide had no horizontal limit and no interior check, so outdoors it applied to the whole 4096-unit cell | `classify` needed only `minZ >= cutZ`; `scanHide` visited the whole parent cell; nothing tested the cell type | Hide is interior-only (`hideScanned`, `IsInteriorCell`; exterior entry traced once, FCS-31). Above-cut is bounded by `fHideRadius` (default 384, from the recorded camera ground distance 187–216 plus margin) with a 64-unit exit margin. Caps are nearest-first: hides applied in order of reach, and a full table gives its farthest entry to a strictly nearer candidate (`evicted`). A piece that leaves the radius is restored by the same exit dwell. Section 3. |
| MINOR 2 | After a fast climb the floor around the player stayed hidden about 1.2 s | `CutPlane` waited 0.6 s outside the band, then the exit dwell added 0.6 s | The cut follows at once beyond 96 units in either direction; a hidden piece that no longer qualifies and is underfoot or at or below the feet is restored in that scan (`restoreAtOnce`, cause `underfoot`). Portable tests for both directions. |
| MINOR 3 | The 96-unit viewer gate also caught attachment (ID 19163) and water (ID 19002) queries near the camera | `FCE0C0` has no direct reference and one pointer occurrence (the slot); both functions call through the slot with their own points; extents read from `.pdata` | Claims are gated on the caller: return address inside ID 13408, 106400 or 106401, guarded at install by address, live unwind extent and call-site bytes (FCS-23). The position gate stays as the second condition. The room trace counts calls by caller class and `gatedNear`. The earlier statement that attachment queries kept the vanilla answer is corrected in section 2. |
| MINOR 4 | `nativeWouldHit` ignored hidden objects | The probe calls the native caster, which skips app-culled owners at `5577E1-5577F4` | Field renamed `visibleWouldHit`; the legend, INI and route plan say which rows it supports (hide off) and which it cannot (hide on). |
| MINOR 5 | The gateway stayed writable | `VirtualAlloc(..., PAGE_EXECUTE_READWRITE)` and no `VirtualProtect` | Allocated read-write, sealed `PAGE_EXECUTE_READ` before `FlushInstructionCache`; the block is freed and FCS-14 logged if sealing fails. |

Risks the review asked to be observed, not fixed: engine uses of the app-culled bit outside rendering (risk 12, rows G15 and G16), and TDM's heading compensation, which the reviewer did not re-read (row G19; `maxHeadingError` is asserted in routes a and b). No cheap guard was found for the first: excluding doors, containers and furniture from the hide would leave them floating where an upper floor was, which defeats the look the row exists to show.

## 1. Fixed rig

### Which quantity is the world heading

**Source, binary.** Native `ThirdPersonState::UpdateRotation` (vtable ID 205236 slot 0xE, RVA `8FE720`): `targetYaw = cameraTarget->vfunc[0x520/8](0)` at `8FE759-8FE769` (the target's heading), and when `GetFreeRotationMode()` (slot 0xC, RVA `55D590`: `[this+E0] || [this+DC] || PlayerControls[+4B]`) is true, `8FE7B1-8FE7CB`: `pitch -= [this+D8]`, `targetYaw = currentYaw = heading + [this+D4]`. `[this+D4]` is `freeRotation.x`. So **world yaw = player heading (`data.angle.z`) + `freeRotation.x`**; `freeRotation.x` alone is relative to the player's facing.

**Source, SmoothCam.** SmoothCam takes its rotation from exactly that function: `Thirdperson::UpdateInternalRotation` forces `freeRotationEnabled`, calls `tps->UpdateRotation()` and copies `tps->rotation` (`SmoothCam/source/thirdperson.cpp:771-795`), with no rotation smoothing of its own.

**Source, TDM.** When directional movement turns the player it compensates in the same step: `playerCharacter->SetHeading(angle.z + angleDelta); thirdPersonState->freeRotation.x = NormalRelativeAngle(freeRotation.x - angleDelta)` (`src/DirectionalMovementHandler.cpp:1269-1271`), and its `Actor::SetRotationZ` hooks subtract every other heading change (`src/Hooks.cpp:1681-1707`). The sum is therefore already invariant under player turns; only look input and float drift move it. `uAdjustCameraYawDuringMovement = 0` is required by the Camera contract (`plugin.cpp` `checkMovementConfig`), and the lock overlay sets both TDM lock adjust speeds to 0.

**Source, binary.** Native `ThirdPersonState::Update` (slot 3, RVA `8FD590`) calls `HandleLookInput` through the vtable every frame with `&PlayerControls.data.lookInputVec` (`8FD5C8-8FD5EB`, skipped only when `[this+E0]`), and `HandleLookInput` (slot 0xF, RVA `8FEB00`) does `freeRotation.x += input.x * fFreeRotationSpeed * dt` (`8FEB4B-8FEB81`; gain at `20BA588`, dt at `327560C` = ID 410200, the same two values `plugin.cpp:1202-1205` reads).

**What the spike does.** In the existing `look` hook (`plugin.cpp:1189`), when the spike is engaged and the policy state is `normal` or `targetLock`, the player's `input.x` is discarded and replaced by `holdYawInput`: `error = wrap(target − (player heading + freeRotation.x))`, request `= error / (gain·dt)`. Native adds `request·gain·dt = error`, so the sum lands on the target in that same update, before SmoothCam samples it. It is an input request, like the existing pitch hold; nothing writes `freeRotation` or a transform (`wiring test: "freeRotation.x =" absent`). The frame order is SmoothCam `mCameraUpdate` → our `update` → native `Update` → SmoothCam look wrapper → our `look` → native look → native `UpdateRotation`; then SmoothCam `UpdateCamera` (`SmoothCam/source/hooks.cpp:34-73`).

**Headings.** `headingRadians(base, i) = wrap(radians(base + 90·i))`; `fHeadingDegrees` is the compass direction the camera looks toward (0 = +Y north, 90 = +X east), the convention of `lockWorldYaw` (`target_lock_policy.h:175`). The camera therefore stands on the opposite side of the player.

**Snap turn.** `SnapTurn` (policy header): a press sets the next index and the signed remaining travel; the held target is `goal − remaining·(1 − smoothstep(t/fSnapSeconds))`, and once the time is up `target()` returns `headingRadians()` itself, so the final value is bit-identical to the heading definition. A press during a turn retargets from the current eased heading, keeping the press direction; a broken clock or duration ends the turn on the heading. Presses are posted as atomic counters by the input sink and consumed in the look hook on the main thread; pending presses are dropped in any ineligible frame and while cursor mode owns look input.

**POV slide.** In a native POV slide the engine subtracts the raw look x again after `HandleLookInput` (`8FD6B9-8FD6D9`, same gain and dt; the vector is `lookInputVec` or, with a gamepad, `moveInputVec`, `8FD608-8FD639`). The spike keeps holding during a slide and passes `raw x + hold`, so with mouse input the two cancel exactly (`rigYawInput`). **Hypothesis:** with a gamepad the left-stick x is subtracted instead and can leave a one-frame yaw error while the POV button is held; keyboard only in this wave. The rig trace records `slide=1`.

**F7.** While the rig holds, F7 (`recenter`) turns the shortest way back to `fHeadingDegrees` (`SnapTurn::home`). This follows the D55-2 starting point so the owner can try it; it is not an owner decision.

### Fixed distance

In the `update` hook, after the native update and before SmoothCam's, `holdFrame()` writes `state->targetZoomOffset = fZoom` and returns before the base zoom code (`plugin.cpp:1252-1258`). **Source:** SmoothCam reads only `targetZoomOffset` (`thirdperson.cpp:864-865`); wheel input changes it between frames and the native slide changes it inside `Update` (`8FD64C-8FD695`), both before this write. `settings.zoom` and the stored `[Camera] fZoom` are untouched. **Hypothesis:** SmoothCam's per-state offsets still change the rendered distance (the preset has `zoomOffset −80` standing/walking/running/sprinting, `−60` sneaking, `+80` with `sideOffset 20` for bow aim, `modules/camera/config/SmoothCamPreset0.json`), and its follow interpolation lets the camera trail a moving player. Neither is collision. The trace separates them (below); whether they are acceptable for "100% fixed" is a D55-1/D55-8 question.

### No pull-in

**Source, SmoothCam.** The normal third-person state makes two casts per frame: hull 30 from the player origin to the side-offset origin (zero length when `sideOffset` is 0), then hull 15 from there to the pre-collision position (`camera_states/thirdperson/thirdperson.cpp:84-101`); the dialogue and vanity states do the same. `Raycast::CastRay` takes start and end by value and reads the end and the contact only when the engine returns true (`raycast.cpp:62-99`); `ComputeRaycast` returns `rayEnd` itself on a miss (`base_third.cpp:63-71`). The interpolators run on the pre-collision position (`GetCameraPosition().SetWorldPosition(preFinalPos)`, `camera_states/thirdperson/thirdperson.cpp:108`), so the cast result feeds nothing but that frame's rendered position. **Conclusion (Source for the code path, NOT_RUN in game):** returning false with every output untouched is exactly a genuine miss to SmoothCam, and no SmoothCam state depends on collision being present.

**What the spike does.** `suppressCast` (`fixed_spike.h:497`) returns "no hit" when all of: the caster path installed, this frame marked eligible by the `update` hook, main thread, return address inside `SmoothCam.dll`. With `iCastProbe=1` (default) the native sweep still runs on private aligned copies with scratch outputs, as it does on the base, and only its answer is counted (`visibleWouldHit`: the native caster ignores app-culled objects, see the rig trace legend); with 0 the sweep is skipped. Native callers (the one direct call, `8FA95E`) always pass through.

Suppression is frame-scoped: `beginFrame` (before the camera update) and `clearFrame` (entry of every `update` hook, covering the two other `PlayerCamera::Update` callers `746A79` and `9969F5`) clear it, only `holdFrame` sets it, `endFrame` clears it again. A cast outside that bracket is native.

**Install conditions.** On the base the detour exists only with `[ThinOccluder] bEnabled=1`. Now: if the thin detour is installed, the spike chains through it (`thin::Runtime::spikeCast`, checked after the SmoothCam-caller test and before the thin eligibility branch, `thin_occluder.h:629`); otherwise the spike installs its own 14-byte absolute detour with the same address and prologue guards. Its gateway is a private `VirtualAlloc` block, written while read-write and sealed `PAGE_EXECUTE_READ` before it is flushed or used (FCS-14 if sealing fails), not the SKSE trampoline: a second `SKSE::AllocTrampoline` would replace the trampoline that holds the camera-call relay (`CommonLib src/SKSE/API.cpp:256-269`, `include/SKSE/Trampoline.h:41-57,188-199`), and when the first allocation came from `create()` it would free it.

`RequestCameraControl` is never called (wiring test). SmoothCam stays the only transform writer.

### Rig trace (`iRigTrace`)

About 10 Hz, 12000 lines, in `SkyCRPG_Camera_FixedSpike.log`. One line (`formatRig`):

`FixedRig t frame eligible state slide cam=x,y,z player=x,y,z offset=x,y,z prevCam=x,y,z prevPre=x,y,z goal=x,y,z pair pullIn heading target headingError index turning pitch zoom heldZoom suppressed visibleWouldHit passedNative passedHit frames pairs pullInFrames maxPullIn maxHeadingError sample=window`

- `cam` is `cameraRoot->world.translate` after the camera update; SmoothCam writes the root and the NiCamera to the same position (`SmoothCam/source/camera.cpp:213-215`). `offset = cam − player`, `player` = feet.
- **Pull-in.** SmoothCam's API `GetLastCameraPosition()` returns `lastPosition` (`modapi.cpp:115-120`, `thirdperson.cpp:733-735`), which is assigned at the *start* of the next update (`thirdperson.cpp:296`). So the value read after frame N's update is frame N−1's pre-collision position. `pullIn` pairs it with frame N−1's rendered position (`prevCam`), only when both frames were eligible and consecutive (`pair=1`); otherwise `pullIn=nan`. Camera travel and follow lag never enter the number. With no collision the two are the same vector.
- **Every frame, not only samples.** `frames`, `pairs`, `pullInFrames` (pairs with `pullIn > 0.5` or nan), `maxPullIn` and `maxHeadingError` cover every eligible frame since the previous line. "Zero pull-in" is `pullInFrames=0` on every line.
- `suppressed` / `visibleWouldHit` are casts suppressed and how many of those the native sweep would have hit **against visible geometry**. The native caster skips any owner whose app-culled bit is set (`5577E1-5577F4`), so with `iSeeThroughMode=1` contacts with references the hide has removed are not counted. The field supports rows with the hide off (a, b, c with see-through 0: "the casts that used to pull the camera in are seen and ignored"). It cannot support any statement about pull-in masked by the hide (d, e, c with hide on): there it under-reports, and only `pullInFrames=0` speaks. `passedNative` are SmoothCam casts in non-eligible frames; `passedHit` is counted only by the spike's own detour (it reads 0 when chained through the thin hook).
- `state` is the policy `State` (8 normal, 9 targetLock; 255 = the update hook did not run). In non-eligible lines the provider is not asked and the pre-collision fields read nan.
- **Known artifact (Source):** on the first frame after a loading screen or an API hand-back SmoothCam overwrites `lastPosition` with the goal (`thirdperson.cpp:300-308,763-764`), so the first pair after a load can show a false pull-in.

## 2. Room visibility

### What the binary does (Source)

- `BSMultiBoundRoom::QPointWithin`: vtable ID 238170 = RVA `1A69AC8`, slot 0x3F = RVA `FCE0C0` (also ID 75890), prologue `48 89 5C 24 08 57 48 83 EC 20 48 8B D9 48 8B FA`. `RCX=this, RDX=const NiPoint3&`, bool in AL: `multiBound([this+128])->vfunc[0x128/8](point)`, then each joined bound in the list at `[this+168]` when `[this+178] != 0`.
- Three culling functions call it through `[vtable+1F8]` with **camera world position + frustum near × camera forward** (`NiCamera +A0..A8`, `+160`, `+7C/+88/+94`): ID 13408 (`1A97B0`; calls `1A997B`, `1A9A08`), ID 106400 (`1512540`; `1512936`, `15129E8`), ID 106401 (`1513290`; `151369B`, `1513755`). The graph is `[[20D68B0]+228]`, rooms at `+40` (data) / `+50` (count), matching `BSPortalGraph::rooms` in CommonLib.
- Every room that answers true becomes a start room: each gets a call to ID 106378 (`15100B0`). In 106400/106401 the cached room is processed and then **all** other containing rooms (`1512990-1512A31`); in 13408 a cached room that still contains the point is processed alone (`1A99B4-1A99D3`). If no room contains the point, 106378 is called with a **null room** (`1A9A52-1A9A68`, `1512A36-1512A60`).
- The same virtual is used for other things, with arbitrary points: reference-to-room attachment in ID 19163 (`2C9B90`: `QPointWithin(reference point)` at `2C9E48`, then the bound test `[vtable+208]` on `ref3D+E4`; four direct call sites, all inside ID 19165 at `2CA230-2CACAB`, which itself has eight direct callers), and ID 19002 (`2BEE50`, call at `2BF073`; seven direct callers; the reviewer identifies it as the cell water-height lookup, and it does keep the largest `[ref+5C]` of the references whose room contains the point).
- `FCE0C0` is reachable only through that slot: there is no direct `call`/`jmp` to it anywhere in `.text`, and its address occurs once in the image, at `1A69CC0` (the slot). Function extents from `.pdata`, each one unchained entry containing its call sites: ID 13408 `1A97B0-1A9DE7`, ID 106400 `1512540-151328C`, ID 106401 `1513290-1513FBE`, ID 19163 `2C9B90-2CA195`, ID 19002 `2BEE50-2BF14F`. All eight call sites are `FF 90 F8 01 00 00`.

**What a null room draws when the graph has rooms is the open question** (NOT_RUN). One recorded data point: in the Bannered Mare the DYL-199 runs logged `room_graph_available=true` with `player_room=0` in all 739 `goal_context` samples (`session-p5/logs/collected/{final-2154-doorway,final2-2204-doorway,m-filter,m-filter-alphaprop,m-filter-material}/SkyCRPG_OcclusionSpike.jsonl`: 3 + 123 + 205 + 202 + 206), while the Nordic corridor and Dragonsreach runs logged a non-zero player room in every sample (`final2-2204-corridor` 118, `final2-2204-hall` 113). The ESM has no room markers in the Bannered Mare (below). **Hypothesis:** that cell renders through the null-room path every frame already, so a camera outside the shell changes nothing about culling there; cells with rooms are the real test.

### What the spike does

The DYL-199 hook is ported with its thread-safety design, and one correction (the claim gate, tightened again in wave 1b).

- `hookRoom` (`fixed_spike.h:410`) runs on whatever thread culls. It reads only: the saved original function, atomics (`roomActive`, counters), an immutable `RoomView` published through `std::atomic<std::shared_ptr<const RoomView>>`, its own arguments, and `room->worldBound` (plain floats). No engine singleton, no log, no trace (wiring test). The view holds floats only: mode, rendered camera position, player position + 64 (**Hypothesis** for eye height, as in DYL-199), radius, tolerance.
- The vanilla answer is always computed first and a vanilla true is never changed.
- **Claim gate: caller and position.** DYL-199 claimed for any queried point. Because attachment (ID 19163) and the water lookup (ID 19002) ask the same function, that would have attached references loading during a run to the claimed rooms. A claim now needs both:
  1. **Caller:** the hook's return address lies inside one of the three culling functions (`roomCaller`, extents above). Attachment, water and any other or unknown caller, including another mod wrapping the slot, always get the vanilla answer.
  2. **Position:** the queried point is within 96 units of the published camera position (`viewerQuery`), so the culling of another camera (reflection, local map) stays vanilla.

  Wave 1 had only the position condition and this document said attachment queries kept the vanilla answer. **That was wrong** for any point within 96 units of the camera: an NPC, projectile or dropped item there could have been attached to a claimed room, and a water query there could have taken a room-bounded height (review finding 3, confirmed against the binary). The trace counts calls by caller class, `viewerCalls` (culling calls at the camera) and `gatedNear` (non-culling calls at the camera that the caller condition kept vanilla), so both a wrong position gate (`viewerCalls=0`) and the cases the caller gate exists for are visible.
- Modes (`seedClaim`): 1 = the room contains the player point; 2 = the room contains one of four samples on the queried-point-to-player segment at t = 1, 0.75, 0.5, 0.25 (player end first, DYL-199 order); 3 = the room's world-bound sphere reaches within `fRoomSeedRadius` of the player point. **Hypothesis:** `worldBound` of a room node is a usable proxy for its extent; mode 3 depends on it.
- Mode 0 installs no hook at all. For modes 1-3 the hook is installed at data load only if the table is at `1A69AC8`, slot 0x3F holds `FCE0C0`, the 16-byte prologue matches, and the three culling functions are where the audit found them: Address Library address, live unwind-table extent (`RtlLookupFunctionEntry`) and the six call-site byte patterns (`cullingCallersGuard`, refusal FCS-23 with the failing check number). The attachment and water extents are verified the same way but only name a trace counter; unverified they count as `other`. The caller table and the original pointer are stored before the slot is written.
- Seeding is active only in eligible frames (`publishView` in `endFrame`); every lifecycle exit and every ineligible frame clears `roomActive`.

### Room trace (`iRoomTrace`)

About 10 Hz, 12000 lines: `FixedRoom t frame mode hook active cell graph rooms capped playerRoom cameraRoom eyeRooms cameraRooms calls cullCalls attachCalls waterCalls otherCalls viewerCalls gatedNear vanillaTrue claims sample=window`.

`graph` = the cell has a portal graph; `rooms` = its room count; `playerRoom` / `cameraRoom` = index of the first room that natively contains the player point / the rendered camera position (−1 none), `eyeRooms` / `cameraRooms` = how many do. These use the saved original (or the plain virtual when no hook is installed), never the seeded answer. The remaining fields are hook counters for the window (0 in mode 0): `calls` = every call, split into `cullCalls` / `attachCalls` / `waterCalls` / `otherCalls` by return address; `viewerCalls` = culling calls at the camera while seeding is active; `gatedNear` = attachment, water or other calls at the camera while seeding is active (always answered vanilla); `vanillaTrue` and `claims` are subsets of `viewerCalls`. The accessors `loadedData->portalGraph->rooms` and `room->QPointWithin` ran in game in the DYL-199 observer (the logs above), so they are proven on this binary; the hook being *called* is not: its only in-game run was in the Bannered Mare, which has no rooms (`room_calls=0` in `final2-2204-culling-seed2`).

"Camera outside every room: was the player's room drawn?" is answered by a line with `cameraRoom=-1`, `playerRoom>=0` and the screenshot at that time; with seeding, `claims>0` confirms the hook acted.

## 3. Rule-based hide (`iSeeThroughMode=1`)

**Interior cells only in this wave.** In an exterior cell nothing is scanned, no hide ray is cast and nothing is hidden (`hideScanned`, tested on `TESObjectCELL::IsInteriorCell()`, the flag bit `[cell+40] & 1` that `GetbhkWorld` at `2BE9B0` also tests). Entering one restores everything (cause `cell`) and writes `FixedHide … event=inactive cause=exterior code=FCS-31 cell=…` once per entry, plus one `FCS-31` line in the main log per process. An exterior cell is 4096 units of uneven ground, so "above the cut" there would take uphill statics, signs and rooftops across the district (review finding 1). Exterior see-through is wave 2. Worldspace "interiors" such as Blackreach are exterior cells to the engine and are therefore not scanned either.

Every 0.1 s in eligible frames in an interior cell, on the main thread (`scanHide`, `fixed_spike.h:316`):

1. **Cut height.** `cutZ = CutPlane(feet z) + fCutHeight`. Within 24 units of the feet the cut reference does not move; between 24 and 96 units it follows after the feet have stayed there 0.6 s, so a jump (apex about 76 units) or one step does not move it; beyond 96 units, up or down, it follows at once (a teleport, a fall, a fast climb).
2. **Rays** (`castRays`): 11 targets around the player (head +120, chest +80, +40, and 8 points on a 48-unit ring at chest height; never the floor) from the rendered camera position, each marched past up to 6 hits; plus 5 downward floor probes (feet and four points 96 units out, from +48 to −160). `bhkWorld::PickObject` with the player controller's collision group and layer `kLOS`, under the world read lock, the idiom the shipped Selection module uses (`modules/selection/src/plugin.cpp:439-479`). Hit references are only compared by pointer.
3. **Scan** of the player's parent cell (`ForEachReference`, at most 8192 visited). Candidates are plain `REFR` references with 3D: *architecture* = Static, MovableStatic; *object* = Furniture, Activator, TalkingActivator, Container, Door, Tree, Flora, Misc, Weapon, Armor, Book, Ingredient, AlchemyItem, Scroll, Ammo, KeyMaster, SoulGem. Everything else is excluded: actors (ACHR), the player, projectiles, hazards, effects, and Light references (**Hypothesis:** app-culling a light's node would switch the light off; lamp and candle meshes are separate statics and are still hidden with the ceiling).
4. **Classify** (`classify`):
   - **Above the cut:** the reference's *whole* base bounds box is at or above `cutZ` (any candidate kind) **and** its 3D bound reaches within `fHideRadius` of the feet in the ground plane (`groundReach` = distance to the world-bound centre minus its radius, height ignored).
   - **On a ray:** architecture only, hit by a sightline ray; refused when it is a floor-probe hit (`floor-contact`), when it has no usable box (`no-bounds`), or when its box reaches within 16 units of the feet (or lower) and both horizontal spans are at least 200 units (`floor-span`).
5. **Dwell and caps:** a reference must qualify for 0.2 s before it is hidden and fail for 0.6 s before it is restored; a hidden above-cut reference stays hidden until it is 24 units below the cut or 64 units outside the radius; at most `iHideMaxPerFrame` hides per scan and `iHideMaxTotal` tracked (hard maximum 256). **Both caps are nearest-first:** the references ready in a scan are hidden in order of ground reach, and when the table is full a candidate strictly nearer than the farthest tracked reference takes its slot (that one is restored at once, cause `evicted`). A root that is already app-culled is never taken.
   - **Underfoot:** a hidden reference that stops qualifying and is now a floor-probe hit, or whose box bottom is at or below the feet, is restored in that same scan (cause `underfoot`), without the exit dwell.
6. **Hide** = `SetAppCulled(true)` on the reference's 3D root (**Source:** CommonLib `NiAVObject.cpp:162-165`, a write of flag bit 0 at `+F4`; the native caster tests the same bit at `5577EB`; the DYL-199 `app_culled` run hid a beam and restored it six times in game). The slot keeps a strong pointer to that root.

### Radius, walking away, climbing

`fHideRadius` defaults to 384. **Source for the geometry:** in recorded candidate-8 runs at pitch 55 and zoom 0.65 the unobstructed camera stood 187–216 units from the player in the ground plane and 472–488 above the feet (`camera_height` events in `session-p5/logs/collected/{final2-2204-budget-filter,final2-2204-boundary}/SkyCRPG_OcclusionSpike.jsonl`). The camera side therefore needs about 216; 384 adds most of one 256-unit kit tile. On the far side, a view ray from that camera to a floor point D units beyond the player crosses the cut height (160) at (2D − 200)/3 units from the player, so 384 keeps the floor clear to roughly 680 units beyond the player under a ceiling at the cut, and farther under higher ceilings. A larger `fZoom` moves the camera out and needs a larger radius. Because the reach is measured to the bound, not the origin, a piece counts as soon as any part of its sphere is inside the radius.

- **Walking away:** a hidden piece stays hidden until its reach exceeds the radius by 64 units, then stops qualifying and is restored by the ordinary 0.6 s exit dwell (`event=restore … cause=expired`). Walking back hides it again after the 0.2 s enter dwell. Portable test `hide()`.
- **Large halls:** far ceiling pieces no longer qualify at all, and nearest-first caps mean the pieces over the player are hidden first even when more qualify than `iHideMaxTotal`.
- **Climbing or teleporting** (review finding 2: `setpos` from feet 68 to the loft at 324 left the loft tiles hidden about 1.2 s): the cut now follows at once (jump of 256 > 96), and the loft tiles (bottom 310) and loft walls (bottom 320) are at or below the feet, so they return in the next scan, at most 0.1 s later. Pieces between the feet and the new cut still take the 0.6 s dwell; they are overhead, not underfoot.

### Bounds versus origin, and one-piece meshes

"Above the cut" uses the base object's OBND box transformed by the 3D root's world transform: `minZ = origin z + scale · Σ min(R[2][i]·lo[i], R[2][i]·hi[i])` (`worldExtent`). The origin is not used: kit pieces put it at floor level even for ceilings (ESM: `WRIntCeilMid01` OBND z 491..513, `WRIntLoftMid01` z 246..255, `WRIntBeam768` z 140..256; references at z 64 in the Bannered Mare). A bounding sphere is not used either: a flat 512-unit ceiling tile has a sphere reaching 360 units below it. Forms with an all-zero OBND fall back to the 3D world-bound sphere, which can only prove "wholly above" and never clears a ray hit.

Consequences, from ESM bounds (Source) and the rule (portable test `hide()`):

- With the player on the Bannered Mare ground floor (feet z 68, cut 228): loft floor tiles (bottom 310), loft walls (origin 320) and the roof tiles (bottom 555) are above the cut; `WRIntBeam768` (bottom 204) is not, and goes only when a ray hits it. `fCutHeight` nearer 130 would take the beams by the cut.
- A one-piece corridor or cave piece carries its floor: `NorHallSm1way01` OBND is x −112..120, y −128..128, z −17..337. Its bottom is below the cut, so it is never "above"; on a ray it is refused as `floor-contact` (the piece under the player) or `floor-span` (232 × 256 footprint reaching the floor). **So in Nordic corridors and caves mechanism 1 hides nothing of the enclosing piece, by design, and the trace says so**: one `event=skip … cause=floor-contact|floor-span` line per reference (32-entry de-duplication) and cumulative `skipFloorContact` / `skipFloorSpan` counters. Ceiling-only pieces do exist there (`NorRmBgMidCeilingOnlyLow01`, 61 references in `BleakFallsBarrow01`) and are taken by the cut.
- **Hypothesis:** a floor-bearing piece smaller than 200 units in one span (a narrow stair or bridge piece that is not under the player) can still be hidden by a ray and leave a hole. The trace lists every hide with its model, so screenshots can be matched.

### Restore

Hidden state is undone at once by `restoreAll(cause)`, and only our own write is undone (`restoreWrite`: the root was visible before, and is still hidden). Causes and where they come from:

| Cause | Trigger |
| --- | --- |
| `disable` | `settings.enabled` false (F6-owned first person, Camera off) seen by `fixedCause` |
| `f8` | SmoothCam `IsCameraEnabled()` false |
| `first-person` | camera state first person |
| `menu` | policy state `menu`, and at once on any focus-taking menu opening (`MenuOpenCloseEvent`, same classification as the lock code) |
| `load` | `loading`, pre-load / post-load / load-game events (`resetLifecycle`) |
| `cell` | player attach / detach events and a change of the player's parent cell |
| `unload` | per scan: the reference handle is dead or its 3D root changed |
| `death` | player dead |
| `special` | any other handed-off policy state (mount, swim, killmove, furniture, bleedout, beast form, scripted camera) |
| `hook-silent` | the `update` hook did not run and none of the above explains it; restored after 3 frames |
| `expired` | the reference no longer qualifies (below the cut margin, outside the radius margin, off the rays) after the exit dwell |
| `underfoot` | a hidden reference that no longer qualifies is under the player or at or below the feet: restored in that scan |
| `evicted` | the table was full and a strictly nearer candidate took the slot |

Events on other threads only post a flag; the main-thread bracket acts on it. Nothing persists: the flag lives on runtime 3D, no reference flag, collision layer, navmesh or actor collision is touched, nothing is deleted.

### Hide trace (`iHideTrace`)

8000 lines: `FixedHide t frame event=hide|restore|skip|inactive|summary ref base reason=above-cut|on-ray cause hidden cutZ feetZ model`, skips with `minZ maxZ spanX spanY`, `inactive` once per exterior cell entry, and a cumulative `summary` each second while scanning: `hidden tracked scans visited rays rayHits hides restores frameCap totalCap visitCap rayCap rayFailed skipNoBounds skipFloorContact skipFloorSpan conflicts foreign cutZ feetZ exit<Cause>…` (13 causes, including `exitUnderfoot` and `exitEvicted`).

## Interaction with the shipped features (spike on)

| Feature | What happens |
| --- | --- |
| `[Obstruction]` (DYL-157) | Suppressed: `sampleObstruction` resets and returns while the spike is engaged, so the policy never reaches `State::obstruction` and never tightens pitch or zoom. |
| `[ThinOccluder]` | Its detour is reused; in eligible frames the spike's "no hit" comes before the thin filter, so the thin cast path is superseded. Its tick still runs: beams on the sightline can still be faded by its own narrow test. `passedHit` reads 0 in this configuration. |
| `[TargetLock]` | The rig holds heading and zoom in `State::targetLock` exactly as in `normal`; the pitch correction is the base one. The lock lease, indicator and TDM contract code are untouched. No lock rows are planned in this wave. |
| `[CursorMode]` | Cursor mode keeps look-input ownership: while its session is active the look hook returns before the rig branch, so no yaw correction is applied and pending snap presses are dropped. Zoom hold and cast suppression continue (they come from the `update` hook). |
| F6 / F8 / F7 | F6 and F8 behave as on the base and are restore causes. F7 returns to `fHeadingDegrees` while the rig holds. |

## Refusal codes

Each is logged once to `SkyCRPG_Camera.log` as `FixedCameraSpike refused code=FCS-NN …` and leaves native behaviour for that sub-feature (wiring test: one site per code).

| Code | Site | Effect |
| --- | --- | --- |
| FCS-01 | a spike INI value out of range or a reserved key | safe value applied |
| FCS-02 | hash / provider / camera-hook prerequisites not ready | whole spike inert |
| FCS-03 | all-state camera service not installed | whole spike inert |
| FCS-10 | `SmoothCam.dll` not loaded | casts native |
| FCS-11 | ID 33007 is not RVA `5574F0` | casts native |
| FCS-12 | caster prologue differs (another patch) | casts native |
| FCS-13 | gateway allocation failed | casts native |
| FCS-14 | gateway could not be made execute-read (`VirtualProtect`) | block freed, casts native |
| FCS-20 | room vtable is not RVA `1A69AC8` | vanilla culling |
| FCS-21 | slot 0x3F is not `FCE0C0` (already hooked) | vanilla culling |
| FCS-22 | `QPointWithin` prologue differs | vanilla culling |
| FCS-23 | a culling caller differs in address, unwind extent or call-site bytes (`check NN` names which) | vanilla culling |
| FCS-30 | `bhkWorld` slot 0x33 is not `104BE80` | no rays: above-cut hides only (those never touch the floor piece) |
| FCS-31 | hide configured and the player is in an exterior cell (by design, not a fault) | nothing scanned, ray-cast or hidden outdoors |
| FCS-40 | a snap key is natively mapped (at the press) | press ignored |
| FCS-41 | a snap key is unbound or reserved | that direction unavailable |
| FCS-50 | trace writer did not start | traces off |
| FCS-60 | a hidden root was already visible at restore | nothing rewritten |
| FCS-61 | frame service ran off the main thread | that frame ignored |

## INI reference (`[FixedCameraSpike]`, restart to apply)

| Key | Default | Range / meaning |
| --- | --- | --- |
| `bEnabled` | 0 | 1 allocates and installs the spike |
| `fHeadingDegrees` | 45 | any finite value, wrapped to [0,360); compass direction the camera looks toward; headings are +0/90/180/270 |
| `fSnapSeconds` | 0.25 | 0.05–2 |
| `fZoom` | 0.65 | 0.05–1.0, native requested zoom scalar |
| `iSnapLeftKey` / `iSnapRightKey` | 51 / 52 | DIK scan code 1–255 or −1; reserved keys become −1 |
| `iCastProbe` | 1 | 1 runs and counts the native sweep (visible geometry only: it ignores app-culled objects), 0 skips it |
| `iRigTrace` | 0 | 0/1 |
| `iRoomSeedMode` | 0 | 0 vanilla (no hook), 1 player's room, 2 sightline, 3 radius |
| `fRoomSeedRadius` | 1024 | 0–8192 |
| `iRoomTrace` | 0 | 0/1 |
| `iSeeThroughMode` | 0 | 0 none, 1 hide; interior cells only |
| `fCutHeight` | 160 | 64–1024 units above the feet |
| `fHideRadius` | 384 | 128–2048; ground-plane reach from the feet for above-cut hides; restored 64 units beyond it |
| `iHideMaxPerFrame` | 16 | 1–64 hides per scan, nearest first |
| `iHideMaxTotal` | 192 | 1–256 tracked references, nearest kept |
| `iHideTrace` | 0 | 0/1 |

**Snap keys.** Comma (DIK 0x33) and period (0x34). **Source:** `interface\controls\pc\controlmap.txt` read from the installed `Skyrim - Interface.bsa` (format 105), member SHA-256 `54886af6a32c18fa5a8b5805ab3070b6af1b9ddc92f777206494099d4e3353ee` (the same member the DYL-199 audit hashed): neither code appears in any context. The numpad alternatives are partly taken (KP1–KP8 are Hotkey1–8; KP9, KP0, KP−, KP+, KP. are free), and `[` `]` (26/27) are the Selection cycle proposals (`modules/selection/config/SkyCRPG_Selection.ini:27-28`). Comma and period exist on every keyboard, read as left / right, and the route runner can emit them (`session-p4/autotest/input/__init__.py` `COMMA`, `DOT`). The validator also reserves 66, 37, 38, 22, 26, 27, the live F6/F7 bindings and the cursor toggle; a natively mapped key is refused again at the press. These are disposable test bindings, not a D55-2 proposal.

## Binary audit

How: `objdump -d -Mintel` of the pinned executable (whole image disassembled once to scratch), Address Library `versionlib-1-7-104-0.bin` SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842` (header 5,1,7,104,0; ID n at file offset 96+4n), vtable slots read from `.rdata`, a reverse RVA→ID map for naming. Scratch scripts are outside the worktree. All rows are file bytes; live memory differs where TDM and SmoothCam patch (camera call site, vtable slots), which the existing guards and the new ones handle.

| Fact relied on | Checked |
| --- | --- |
| ID 33007 → `5574F0`, 14-byte prologue `48 8B C4 4C 89 48 20 55 56 57 41 54 41 55` | resolved; bytes read; the only direct call is `8FA95E` (`E8 8D CB C5 FF`) |
| Caster skips app-culled objects | `5577E1-5577F4`: layer 0x11 bypass, else `test byte [obj+F4],1` |
| ID 50784+1A6 = `8F8826` calls `558E70` (ID 33025) | `E8 45 06 C6 FF`; other callers `746A79` (call), `9969F5` (jmp) |
| ThirdPersonState vtable ID 205236 → `18ECC08`; slots 3 / C / E / F → `8FD590` (ID 50896) / `55D590` / `8FE720` / `8FEB00` (ID 50914) | slots read; four functions disassembled as described in section 1 |
| `freeRotation` at `+D4/+D8`, `freeRotationEnabled` `+DC`, `targetZoomOffset` `+74` | operands in `8FEB00`, `8FE720`, `8FD590`; matches the existing `static_assert`s |
| ID 410200 → `327560C` (frame delta), gain `20BA588` | operands in `8FEB65`, `8FD6C5` |
| BSMultiBoundRoom vtable ID 238170 → `1A69AC8`, slot 0x3F → `FCE0C0` (= ID 75890), prologue | slot and bytes read; function and six culling call sites plus two non-culling sites disassembled (section 2) |
| `FCE0C0` is reachable only through the slot; extents of its five callers | no rel32 reference in `.text`; one pointer occurrence, at `1A69CC0`; `.pdata` entries read for IDs 13408, 106400, 106401, 19163, 19002 (values in section 2), each containing its call sites, all `FF 90 F8 01 00 00` |
| `TESObjectCELL` interior flag | `[cell+40] & 1` in `GetbhkWorld` (`2BE9B0`); CommonLib `cellFlags` at `0x40`, `kIsInteriorCell = 1 << 0` |
| `BSPortalGraph::rooms` at `+40` | `[graph+40]` / `[graph+50]` in ID 13408 and 106400 |
| bhkWorld vtable ID 238918 → `1A7CDA8`; slot 0x27 → `104B470`; slot 0x33 → `104BE80` | slots read; `104BE80` writes `bhkPickData` `+40/+44/+50/+70/+80` and `pickFailed` `+C0`, reads the ray at `+90` and the collectors at `+A8..+B8`: the CommonLib layout |
| `PickObject` takes the world read lock itself | `[hkpWorld+430]+C598` → ID 68233 (`CE3250`), released by ID 68239 (`CE3510`); CommonLib `worldLock` is at `C598` |
| Nested read lock on one thread cannot self-deadlock | `CE3250`: the owner thread increments; others CAS `(count & 0x0FFFFFFF)+1`, no writer-waiting bit |
| Pick time budget | `104BEAE-104BF0A` fails a pick when `[3336520]` exceeds 0.001 (or 0.03); the only writes to `3336520` are zero (`104ACB5`, `104DF2D`). **Hypothesis:** the budget is inert on this build; `rayFailed` in the hide summary shows if it is not |
| Controller filter getter | `690BE0` (ID 37560) ends in `controller->vfunc[0x40/8]`, slot 8 as CommonLib declares |
| World scale ID 188105 → `17FDE5C` = 0.0142875; inverse ID 187407 → `17F423C` = 69.99 | values read |
| Horse / Bleedout / Dragon camera vtables (IDs 214837 → `196CE48`, 214873 → `196DB70`, 205240 → `18ECCD0`) | slots 3 / E / F: Horse `8FD590` / `8F7360` / `8F71E0`; Bleedout `8F3D40` / `8FE720` / `8FEB00`; Dragon `8FD590` / `55CDF0` / `55CC60` |

Not re-verified here and taken from use on this binary in accepted game runs: `TESObjectCELL::ForEachReference`, `Get3D`, `GetBaseObject`, `boundData`, handles, `FindCollidableRef` (thin occluder, candidate 8); `GetbhkWorld`, `PickObject`, `GetCollisionFilterInfo`, `TESModel::GetModel`, `loadedData->portalGraph->rooms`, `QPointWithin` (DYL-199 game logs, Selection). CommonLib declarations were read for every other accessor; **the wave 1b native changes have not been compiled on Windows** (wave 1 was, in CI).

## Offline content facts (Source: Skyrim.esm SHA-256 `e198c3b8…df73`, scratch TES4 scan)

| Cell | Form | Refs | RoomMarker refs | PortalMarker refs | Arrival |
| --- | --- | --- | --- | --- | --- |
| WhiterunBanneredMare | 0001605E | 690 | 0 | 0 | door 0001A6F4 XTEL → (−125.115, −1101.734, 68.0) |
| RiverwoodAlvorsHouse | 000133C8 | 227 | 0 | 0 | door 00013420 XTEL → (−512.821, −205.791, 240.0), yaw −0.033 |
| WhiterunDragonsreach | 000165A3 | 1431 | 9 | 6 | COCMarkerHeading 00020961 (−448, −128, −320) |
| BleakFallsBarrow01 | 000371DE | 2465 | 34 | 21 | COCMarkerHeading 00109E67 (−7411, 1082, 579) |
| EmbershardMine01 | 000B6BE6 | 1120 | 6 | 3 | COCMarkerHeading 000CEED8 (2487.292, 1485.453, 9250.252) |
| Mzulft01 | 0001523F | 2441 | 22 | 14 | COCMarkerHeading 0010F12C (4111.007, 8742.621, 40.224) |

Every small house scanned (five Riverwood houses, the Sleeping Giant, Breezehome, four Whiterun shops) has no room marker. In Skyrim.esm 246 of 590 interior CELL records have at least one RoomMarker (Dawnguard 16/114, HearthFires 1/46, Dragonborn 30/88; DLC counts include overrides). This is a scratch scan, not the pinned P5.5-01 census.

## 4. Retained-state feasibility (D55-5) — code and binary reading only, no code written

"Hooks run" means the Camera `look`/`update` hooks, which patch only the ThirdPersonState table (`18ECC08`). A derived state has its own table; an identical function address there does not go through our hook.

| State | Camera state and view owner today | Camera hooks run? | Needed to hold heading, pitch and distance without a second writer | Mark |
| --- | --- | --- | --- | --- |
| Surface swimming | `kThirdPerson`; SmoothCam writes the transform; policy hands off through `Hazards.swimming` (`plugin.cpp` `readCameraWorld`, `lifecycle_policy.h:12`) | Yes | Eligibility change only for the camera. But TDM derives swim pitch from camera pitch: `SetDesiredSwimmingPitchOffset((currentPitch − freeRotation.y)·cos|freeRotation.x|)` (`TDM src/Hooks.cpp:673-735`). With pitch held at 55° the player would dive when moving away from the camera. Needs a way to decouple swim pitch, or an in-water test that the surface clamp is enough. | Camera part **Source**; playability **Hypothesis**, likely the blocker |
| Mount | `kMount`, `HorseCameraState` (table `196CE48`): Update is the same native function `8FD590`, look `8F71E0` and rotation `8F7360` are its own. SmoothCam hooks its slots 3 and 0xF (`hooks.cpp:431-449`) and runs its third-person camera (`thirdperson.cpp:289-290`), casts included. Policy hands off: `stateSpecial` and `Hazards.mount`. | No | Two more vtable hooks (HorseCameraState slots 3 and 0xF) reusing the same policy with the horse as heading source: `8F7360` keeps `freeRotation.x` compensated against the horse's heading (`[this+E8]` handle, `[this+EC]` last direction, `8F73D0-8F7474`), and TDM adjusts it too (`TDM src/Hooks.cpp:787-834`). Cast suppression already works for any SmoothCam cast once the frame is marked. | Structure **Source**; hold **Hypothesis** |
| Werewolf, Vampire Lord | `kThirdPerson`; SmoothCam writes; policy hands off through `Hazards.beast` (`PlayerControls.data.povBeastMode`) | Yes | Eligibility change only. Unknown: SmoothCam focus height and offsets for the beast skeletons, the hovering Vampire Lord, `stateNotActive` during transformation. | **Hypothesis**, most likely feasible |
| Bleedout | `kBleedout`, `BleedoutCameraState` (table `196DB70`): own Update `8F3D40`; rotation and look are the ThirdPersonState functions but in its own table. SmoothCam hooks the slots yet its third-person camera does not run there (`thirdperson.cpp:281-293` lists ThirdPerson, Combat, Dialogue, Vanity, Horseback), so the native state positions the camera. | No | Not possible without a second transform writer, or forcing the camera back to `kThirdPerson`, both outside D012. | **Source**: not feasible under D55-1 |
| Dialogue | `kThirdPerson` with the Dialogue Menu open (**Hypothesis** for vanilla; no camera state change found); SmoothCam switches to its ThirdPersonDialogue state (`game_state.cpp:125-126`) and still casts (`camera_states/thirdperson/dialogue/skyrim.cpp:84-98`). Policy hands off through `focused()` → `State::menu`. | Yes | Eligibility for the dialogue menu case only (menu input precedence unchanged), with SmoothCam's dialogue mode left on "Skyrim". The look call can be swallowed by SmoothCam's input lock (`hooks.cpp:76-82`); then the heading is not corrected but nothing moves it either. | **Hypothesis**, likely feasible |

## What this wave learned that bears on wave 2

- **Fade (mechanism 2).** The scan, classification, dwell, caps and restore table are mechanism-independent; only step 6 changes. The material-clone path refuses shared shaders and holds at most 8 references of 16 parts (`thin_occluder_policy.h:10`); a ceiling scan here tracks up to 192. The clone pool, not the rule, is the limit.
- **Per-draw clip (3).** The extents needed to choose a clip plane are already computed per reference (`minZ`, `maxZ`, footprint), and the floor rule identifies exactly the pieces a per-reference hide must skip, which are the ones a clip is for. The projection-helper argument is still unresolved (DYL-199).
- **Shader discard (4).** The culling entry points are now located (IDs 13408, 106400, 106401, 106378); a world-space discard would hook downstream of them. Nothing here reduces the renderer-hook question.
- **Room seeding is a culling answer, not a draw rule.** If a null room draws nothing, seeding mode 1 is the minimum for any mechanism; if it draws everything, seeding is only a cost question.
- **Anything that changes `QPointWithin` must be gated on the caller as well as the point**; five functions share the slot.
- **Rays cannot see through hidden geometry by themselves** (collision is unchanged), hence the marching; a fade or clip mechanism has the same property.

## Tests

Commands (as CI's `camera-portable` job, `.github/workflows/build.yml:272-281`), run from the worktree root with g++ 15.2.0:

```
for test in modules/camera/tests/*_test.cpp; do name=$(basename "$test" .cpp)
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$OUT/$name" && "$OUT/$name"; done
python3 -B -I modules/camera/tests/lock_refresh_wiring_test.py
```

| Check | Result |
| --- | --- |
| 14 portable tests (13 existing + `fixed_spike_policy_test`), rerun after the wave 1b fixes | **PASS**, all exit 0 |
| Wiring audit, rerun after the wave 1b fixes | **PASS**, five lines: the three existing ones, `PASS spike off wiring…`, `PASS spike on wiring…` (now also: sealed gateway, caller-gated room hook with its guard, interior-only radius-bounded nearest-first hide, 19 refusal codes with one site each) |
| Policy header under `-Wshadow -Wconversion -Wsign-conversion -Wfloat-conversion -Wold-style-cast` | **PASS**, 0 warnings |
| `fixed_spike.h` syntax-only against scratch stand-ins for the engine and Windows surface (member names copied from the pinned CommonLib headers), same warning set | **PASS**, 0 warnings. This checks my own syntax, types, shadowing and narrowing; it does **not** prove the real CommonLib or MSVC accept it |
| `tools/tests/test_package*.py` (18 tests), `modules/camera/tools/check_preset.py` | **PASS** (INI section added, preset untouched) |
| Windows `/W4 /WX` build of wave 1 (`1de4b28`) | PASS on the first attempt, CI run 42720286058 (reported by the orchestrator; not observed by me) |
| Windows `/W4 /WX` build of the wave 1b fixes | **NOT_RUN** |

`fixed_spike_policy_test` covers: settings bounds and key reservation; the four headings; snap easing (continuity, monotonic, lands bit-exactly, retarget, reversal, broken clock, home); the yaw request landing the heading sum on target; POV-slide cancellation; eligibility by every state; zoom hold; cast decision truth table; pull-in pairing and the per-frame window; rig and room line formats; seeding modes 0-3, the sample order, the position gate and the caller gate (the six audited return addresses, extent edges, unknown callers, per-class counters); cut-height, ray, floor-contact, floor-span, sphere-fallback and rotated-box classification; height, radius and time hysteresis; the interior-only rule; ground reach; at-once restore of what is underfoot; per-frame and total caps and their nearest-first order; the cut-plane tracker (band, settle, and at-once follow in both directions); ray targets, floor probes and marching progress; the restore table and write rule; line budgets and the trace writer; and "disabled equals base".

## Game rows — all NOT_RUN

| # | Row | Decides | Result |
| --- | --- | --- | --- |
| G1 | Spike off: candidate-8 camera routes unchanged | off = base | NOT_RUN |
| G2 | Install: no `FCS-` line; `installed rig=1 caster=own-detour` | guards on the live image | NOT_RUN |
| G3 | Heading held at four headings; snaps land; F7 home | rig yaw | NOT_RUN |
| G4 | Zoom held against wheel and POV slide | rig distance | NOT_RUN |
| G5 | Zero pull-in in the open and in the Bannered Mare with `visibleWouldHit>0` (hide off) | D55-1 | NOT_RUN |
| G6 | SmoothCam stable with no collision: load door, F6, F8, menu | D55-1 | NOT_RUN |
| G7 | Camera outside every room, seed 0: is the player's room drawn? | phase feasibility indoors | NOT_RUN |
| G8 | Seed modes 1, 2, 3: drawn rooms, `claims>0`, `cullCalls>0`; `gatedNear` recorded; no attach artifacts | room seeding | NOT_RUN |
| G9 | Hide look: kit interior ground floor and loft | D043 kit interiors | NOT_RUN |
| G10 | Hide look: cave, Dwemer chamber; skips traced | D043 one-piece meshes | NOT_RUN |
| G11 | Restore on every exit, by trace | presentation only | NOT_RUN |
| G12 | Frame cost per seed mode and with hide | Phase 4 budget | NOT_RUN |
| G13 | Side effects with hidden ceilings: lights, shadows, fog, void | D043 | NOT_RUN |
| G14 | Target lock with the rig | later wave | NOT_RUN (not planned) |
| G15 | NPCs around hidden geometry behave as with the spike off: pathing, sitting, detection across a hidden wall | whether the app-culled bit reaches AI (unverified) | NOT_RUN |
| G16 | A hidden door, container or activator: is it still activatable, and does a hidden animated static still collide? | whether the bit reaches activation and animated collision (unverified) | NOT_RUN |
| G17 | Hide in an exterior cell: nothing hidden, one `inactive` line per entry | interior-only rule | NOT_RUN |
| G18 | Loft island: after `setpos` to the loft the floor under the player is drawn within one scan | finding 2 | NOT_RUN |
| G19 | Heading error while TDM turns the player (reversals, strafes, sprint) | TDM compensation, not re-read by the reviewer | NOT_RUN |

## Route plan for the first game sessions

Common to all: the candidate-8 camera stack with the CI Camera DLL of this branch; `[ThinOccluder]`, `[TargetLock]`, `[CursorMode]`, `[Obstruction]` at 0; controller off; copy of the clean camera-only save, `load` by console as the Phase 5 routes; `tgm`; `bAllowScreenShot=1`. Collect `SkyCRPG_Camera.log` **and `SkyCRPG_Camera_FixedSpike.log`** (new file, same SKSE log directory). Keys: `COMMA` = snap left, `DOT` = snap right, `F7` home. Every route first asserts in `SkyCRPG_Camera.log`: `FixedCameraSpike enabled`, `FixedCameraSpike installed rig=1 caster=own-detour`, and **no** `FixedCameraSpike refused code=FCS-` line other than `FCS-31` (a refusal makes the affected rows BLOCKED, not FAIL; `FCS-31` is the by-design notice that the hide is inactive outdoors and is expected in any route that goes outside with `iSeeThroughMode=1`). "Eligible lines" are `FixedRig` lines with `eligible=1`. Poses are player poses; each is labelled Source (validated in a recorded Phase 5 run, or an ESM arrival marker) or Hypothesis.

Shared rig assertions **R** (every eligible line unless stated): `pullInFrames=0`; `passedNative=0`; `suppressed>=1`; `zoom` equals `heldZoom`; `maxHeadingError<=0.5`; on lines with `turning=0`, `|headingError|<=0.05` and `target` is one of 45, 135, −135, −45; `pitch` within 55±1 after a 2 s settle. The first `pair=1` line after any load is reported separately (known artifact). `maxHeadingError` is also the only check on TDM's heading compensation, which the independent reviewer did not re-read: it must hold on every line while the player character is turning, not only while standing. Thresholds are diagnostic starting points; D55-6 is the owner's.

### a. Rig hold in the open — about 10 min

INI: `bEnabled=1 iRigTrace=1`. `coc WhiterunExterior01`; `player.setpos x 18825.100`, `y -11005.162`, `z -4580.337`, `player.setangle z 5` (**Source:** the `p5-final-exterior` pose, run in `final2-2204-exterior`; it stands under the stable roof). Wait 5 s; screenshot `a-h0`. `DOT` four times, 1.5 s apart, screenshot after each (`a-h1..a-h3`, `a-h0b`) and one 0.12 s after a press (`a-turning`). `COMMA` four times. `DOT`, then `F7`. Walk a square: hold `W`, `D`, `S`, `A` 3 s each (movement is camera-relative). Then hold `W` 6 s to leave the stable (**Hypothesis:** open ground beyond it) and repeat the four snaps there. TDM turn check (G19): hold `W` 1 s then `S` 1 s five times (each reversal turns the character 180 degrees), then `W`+`D` 2 s, `S`+`A` 2 s, and sprint `W` 2 s. Mouse: move 400 counts right during 2 s; mouse wheel 5 notches each way. Last step, because a short press would toggle first person: hold the POV key (F) 1.5 s while moving the mouse, release.

Decides G2-G5: **R**; at least 100 eligible lines; `index` reads 1,2,3,0 after the rights and 3,2,1,0 after the lefts, 0 after F7; the heading does not change during the mouse, wheel and POV steps (R holds on those lines; `slide=1` lines exist during the POV hold, otherwise that sub-step is NOT_RUN); under the roof at least one line has `visibleWouldHit>0`; on every line of the TDM turn check `maxHeadingError<=0.5` (G19: if it is not, TDM's compensation does not keep the heading sum and the hold needs a different input, a finding for D55-1). Record, without a threshold, the spread of `offset` per heading while standing and while walking (follow lag) for D55-1/D55-8.

### b. Rig hold in the Bannered Mare — about 12 min

INI as a (seed 0, see-through 0). `coc WhiterunBanneredMare`. Poses: hearth/beam `setpos -117.110 -640 68` and doorway `setpos -128 294 68` (**Source:** Phase 5 poses, runs `batch-k`, `final2-2204-doorway`); loft `setpos -640 -256 324` (**Hypothesis:** 5 units above the top of loft tile 000602BC, OBND top 255 at origin z 64). At the hearth: four snaps with screenshots (`b-hearth-h0..h3`). Walls: in each heading hold `W` 4 s. Beam: stand at the hearth pose 5 s (beam 000E7A7B overhead). Doorway: from the doorway pose hold `W` 3 s and `S` 3 s across the frame (000616C4). Loft: `setpos` to the loft pose, four snaps with screenshots (`b-loft-h0..h3`), walk `W` 2 s.

Expected: **blocked views** in most headings (roof, loft floor, walls between camera and player) — that is the point of the row. Decides G5: **R** throughout, and `visibleWouldHit>0` on most lines (the casts that pulled the Phase 5 camera in are now reported and ignored; the hide is off here, so the count is complete). Repeat the reversal part of the TDM turn check at the hearth pose (G19). `FixedRoom` is not needed here: the cell has no rooms.

### c. Room culling — two cells, about 15 min each, run per seed mode

INI: `bEnabled=1 iRigTrace=1 iRoomTrace=1 iRoomSeedMode=N`, see-through 0 first, then `iSeeThroughMode=1 iHideTrace=1`. Modes are a restart each; plan four short launches per cell or split across sessions (each launch about 4 min).

- **Small building:** `coc RiverwoodAlvorsHouse`, `setpos -512.821 -205.791 240` (**Source:** XTEL arrival of door 00013420). Screenshots at four headings. Expected from ESM: `graph` 0 or 1, `rooms=0`, `playerRoom=-1`, hook `calls=0` in every mode. This row shows the look with the camera outside the shell where culling cannot be the cause.
- **Low-ceiling Nordic corridor:** `coc BleakFallsBarrow01`, `setpos 2048 -9216 -2556`, `setangle z 0` (**Source:** the `p5-final-corridor` pose; `player_room` was non-zero there in 118 samples). Screenshots at four headings, then walk `W` 3 s and back.

Decides G7 (mode 0): on lines with `cameraRoom=-1` and `playerRoom>=0`, do the screenshots show the corridor floor and the player? If `cameraRoom>=0` on every line the camera never left the rooms at this pose and the row is NOT_RUN for that pose; raise `fZoom` or move and repeat. Decides G8 (modes 1-3): `cullCalls>0` (else the hook is not reached from the audited culling functions: BLOCKED) and `viewerCalls>0` (else the position gate does not match the culling query: BLOCKED), `claims>0` on lines where `cameraRoom=-1`, and the matched screenshots against mode 0. Record `attachCalls`, `waterCalls`, `otherCalls` and `gatedNear` as found: `gatedNear>0` shows queries at the camera that the caller gate kept vanilla, and a large `otherCalls` means a caller the audit did not find. With hide on, `FixedHide` should show `skipFloorSpan`/`skipFloorContact` for the corridor pieces and hides for ceiling-only pieces.

### d. Hide look — about 15 min

INI: `bEnabled=1 iRigTrace=1 iSeeThroughMode=1 iHideTrace=1 iRoomTrace=1`, seed mode = the best from c (0 if c is not yet run).

- Bannered Mare ground floor (hearth pose) and loft (loft pose): four headings each, screenshots `d-bm-ground-h*`, `d-bm-loft-h*`; walk under the loft edge and back.
- Loft island (G18): from the hearth pose with `hidden>0`, `setpos` to the loft pose and take a screenshot 0.5 s later (`d-loft-arrival`). The floor under the player must be drawn; the trace must show `event=restore … cause=underfoot` lines for loft references within 0.2 s of the first `FixedRig` line with the new player z, and `cutZ` in the next summary equal to the new feet z + `fCutHeight`. Then `setpos` back to the hearth pose: the loft is hidden again after the enter dwell.
- Walking away (radius): from the hearth pose hold `W` 4 s and return; `event=restore … cause=expired` lines appear for pieces left behind and `tracked` stays at or below `iHideMaxTotal` with `totalCap` recorded.
- NPCs (G15, unverified engine behaviour): at the hearth pose with the roof and loft hidden, watch the patrons for 60 s: walking, sitting, turning to the player; then stand so a hidden wall or loft wall is between the player and an NPC for 30 s. Compare with the same 90 s with `iSeeThroughMode=0`. Record any NPC that stops, loses its path or reacts differently.
- Activation and animated collision (G16, unverified): restart with `fCutHeight=64` so furniture, containers and doors above 64 units are hidden; pick a hidden container or activator from the `event=hide` model names, walk to it, and record whether it can still be activated (and whether the player still collides with a hidden animated static such as a door leaf). Either answer is a finding; neither is assumed.
- Cave: `coc EmbershardMine01` (**Source:** arrival at COCMarkerHeading 000CEED8), four headings, walk `W` 4 s.
- Dwemer: `coc Mzulft01` (**Source:** COCMarkerHeading 0010F12C; that it is a chamber is a **Hypothesis**), four headings, walk `W` 4 s.

Decides G9/G10 by screenshots (the owner reviews the looks, D043) plus: `summary` lines show `hides>0` on the ground floor; every `event=hide` has a model and reason; no `event=hide` for the reference under the player (it appears as `skip … cause=floor-contact` if a ray hit it); `totalCap` and `frameCap` values recorded; in the cave `skipFloorSpan+skipFloorContact>0`. Also note lights and shadows with ceilings hidden, fog, and what is behind the hidden roof (G13). With the hide on, `visibleWouldHit` under-reports and is not asserted.

### e. Lifecycle — about 12 min

INI as d, in the Bannered Mare at the hearth pose, after `hidden>0` in a summary line. In order, each followed by 3 s and a `summary`:

| Step | Restore assertion (cumulative `summary` counter increases, `hidden=0` right after) |
| --- | --- |
| F6 to first person, F6 back | `exitDisable` or `exitFirstPerson`; after return `hidden>0` again and **R** |
| F8 off, F8 on | `exitF8`; after return **R** |
| Tab menu open 2 s, close | `exitMenu` |
| Console open, close | `exitMenu` |
| Load door: `setpos -125.115 -1101.734 68`, `setangle z 180`, activate (E), wait 10 s outside, then back in. If the door is not offered from the fixed view (record it: a P5.5-06 finding), use console `player.moveto 0001A6F4` then `coc WhiterunBanneredMare` and mark the door sub-row NOT_RUN | `exitLoad` or `exitCell`; heading index unchanged across the load. Outside (G17): exactly one `event=inactive cause=exterior code=FCS-31` line for that entry, **no** `event=hide` line and no `summary` line with `hidden>0` while the cell is exterior, **R** still holds; back inside `hidden>0` again |
| Quicksave, change heading, quickload | `exitLoad`; `index` after the load is whatever it was at the load (the spike keeps the index in memory, it is not saved) |

Every restore must appear as `event=restore` lines whose count equals the `hidden` value of the preceding summary; `conflicts=0`; no `FCS-60`. `kill`-based death is not planned in the first session (the player-death state needs a disposable save; add it once the rest passes).

### f. Frame cost — about 10 min per mode

The Phase 4 capture (`docs/evidence/p4/performance.md`: 60 s standing and 60 s turning, anchor ratio in [0.9,1.1]) in Dragonsreach at the `p5-final-hall` pose `setpos -448 -128 -316` (**Source:** run `final2-2204-hall`; 9 rooms). INI: `bEnabled=1`, **all traces 0**, `iRoomSeedMode` 0, 1, 2, 3 in turn, see-through 0; then one run with the chosen mode and `iSeeThroughMode=1`. Because yaw input is discarded, "turning" is a `DOT` press every 1.5 s for 60 s. Baseline: the same captures with `bEnabled=0`. Report p95 standing and turning per mode and `(mode/baseline − 1)·100`; the Phase 4 budget applies. A separate 60 s run per mode with `iRoomTrace=1` confirms `claims>0` there; cost runs with a trace on are not comparable.

## What could go wrong in the first game run

1. **The wave 1b changes have not been compiled on Windows** (wave 1 compiled first time). New Windows surface: `VirtualProtect`, `VirtualFree`, `RtlLookupFunctionEntry`, `_ReturnAddress` in the room hook, `TESObjectCELL::IsInteriorCell`.
2. **`previousLook` skipped by SmoothCam's input lock** leaves the heading uncorrected for those frames; `maxHeadingError` shows it.
3. **Heading snaps on first engage.** The first eligible frame moves the camera to the configured heading in one frame, wherever it was.
4. **The position gate may not match** if the culling camera is not at the published position (for example a later scene-graph update); then `viewerCalls=0` and seeding does nothing. It fails toward vanilla.
5. **A null room may draw nothing**: a blank or partial interior at the corridor pose in mode 0 is a finding, not a defect.
6. **Hidden references still collide** and may still be activatable; Light references are never hidden, so a room can stay lit by lamps whose fixture mesh is gone, and shadows cast by hidden geometry may or may not remain.
7. **Restore latency in menus.** Restores are applied on the menu event when it arrives on the main thread, otherwise at the next camera frame; if the camera update does not tick while a menu is open, hidden references stay hidden behind that menu until it closes.
8. **A missed `update` hook for up to two frames** keeps hides (grace) but not suppression: the camera can pull in for those frames and the trace will count `passedNative`.
9. **Exteriors:** the hide does nothing outdoors in this wave (by design, `FCS-31`); the rig and room parts still run there. A camera under an exterior overhang is simply blocked.
10. **Trace cost and loss:** traces flush every 100 ms from a worker; a killed process loses the last 100 ms. Traces off for cost rows.
11. **The pick budget** (audit table) could make later engine picks fail in a scan frame if it is live; `rayFailed>0` is the signal.
12. **The app-culled bit outside rendering (unverified, flagged by the independent review).** The reviewer could not rule out engine uses of `[obj+F4] & 1` outside rendering: about 50 functions test it, among them IDs 36643, 36589, 40570, 32174 and 32189 (67 `test byte [reg+F4],1` instructions in the image by my count; I resolved those five IDs to `65E610`, `65A400`, `754DE0`, `5299B0`, `52C760` but did not analyse them). AI line of sight, activation and animated collision of hidden references may therefore change while they are hidden. Nothing is saved and every exit restores, but during a hide an NPC behind a hidden wall, a hidden door or a hidden container may behave differently. Rows G15 and G16 exist to observe this; until they are run it is plainly unknown.
13. **The room-caller guard (FCS-23) relies on the live unwind table** matching the file. If Proton or a patcher presents different extents, room seeding refuses with `check 2x` and culling stays vanilla; the row is then BLOCKED, not FAIL.
14. **Eviction flicker.** With more qualifying pieces than `iHideMaxTotal`, pieces at the far edge of the set can be restored and re-hidden as the player moves; `exitEvicted` and `totalCap` show it.

## Proposed shared-file changes (not edited)

- `docs/STATUS.md`: P5.5-02 wave 1 spike source complete on `phase5.5/p55-02-fixed-camera-spike`; portable tests PASS; Windows build and all game rows NOT_RUN; nothing merged.
- `docs/phases/PHASE_5_5.md`, "Things the request did not list" item 1: add that the room test is also used for reference-to-room attachment (ID 19163), so any seeding must be limited to the main view's query; and that 246 of 590 Skyrim.esm interior cells carry room markers while small houses and the Bannered Mare carry none (scratch scan; the P5.5-01 census is authoritative).
- `docs/phases/PHASE_5_5.md`, P5.5-02 "Retained states": the table above (bleedout not feasible under D55-1; swimming blocked on TDM's camera-pitch-driven swim pitch).
- `docs/CONTROLS.md`: nothing until the owner rules on D55-2; the spike keys are test bindings.
- P5.5-01 probe/routes: collect `SkyCRPG_Camera_FixedSpike.log`; the rig, room and hide line formats above are the assertion surface.

## Open questions for the owner

1. Is SmoothCam's follow lag and its per-state offset change (sneak, bow aim) acceptable inside "pitch, height and distance never change" (D042), or must the rig also pin those (a SmoothCam preset change, or D55-1's fallback)?
2. `fCutHeight`: 160 leaves head-height beams unless a ray hits them; about 130 takes them by the cut. Which look should the screenshots be taken at, or both?
3. F7 as "return to the default heading" is in the spike for trial. Keep it in the proposal?
4. Comma / period are only test bindings. Should the D55-2 keyboard proposal start from them?
5. Bleedout cannot keep the fixed view without a second transform writer. Is handing off there acceptable, so D55-5 need not reopen D012?
