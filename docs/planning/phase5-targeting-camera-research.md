# Phase 5 research — targeting, lock-on and fixed-height camera

Desk research and source audit, **2026-10-28**, against main `abd5735`. This is an engineering assessment of the owner's Phase 3+ requests recorded in the [Phase 2 gate](../evidence/p2/gate.md#phase-3-owner-requests). **Nothing here was built or run in game.** Facts marked *source* were read in the local provider checkouts under `session-20261026/targeting-audit` (SmoothCam 1.8.1, TDM 2.3.1) and `session-p2-20261026/commonlib-7.2.0`; facts marked *web* come from public repositories and are cited at the end; **unverified** means neither.

The plan that consumes this document is [Phase 5](../phases/PHASE_5.md).

## Requests under assessment

| ID | Owner request (2026-10-26) | Short name |
| --- | --- | --- |
| R1 | Activate picks the closest usable thing near the character; a key/button cycles candidates like MMO tab targeting; closest is the default. | Proximity activation |
| R2 | Keyboard/mouse: hover the pointer over an object to select it. Controller: a hotkey enters a cursor mode where the right stick moves a reticle. | Cursor mode |
| R3 | Enemy lock-on to help combat, especially ranged. | Lock-on |
| R4 | Camera keeps its height/distance unless the user changes it; geometry that would cover the player or block the view becomes see-through instead of the camera pulling in. | Fixed height + occluder fade |

## Summary

| Request | Reuse available | Original work left | Feasibility on the pinned stack | Risk |
| --- | --- | --- | --- | --- |
| R1 | **Better Third Person Selection (BTPS) 0.9.0** does proximity scoring, cycling and the selection widget by substituting the engine's crosshair pick target. | Dependency pin, config deltas, elevated-camera tuning, controller cycle binding, validation. | High | GPL-3 external package; JContainers 1.7.104 build unverified; no free controller button proven. |
| R2 | Nothing. No mod provides a free gameplay cursor or stick-driven world picking. | Cursor overlay, screen-to-world pick, target injection into the pick path, input ownership. | Medium | Needs an injection API that BTPS does not have (fork or upstream change); mouse look versus free cursor is a product decision. |
| R3 | **TDM 2.3.1 target lock** with yaw/pitch adjust speeds set to 0 writes no camera rotation and re-aims arrows/missiles at the target at launch. | Release the existing disable lease conditionally, settings fragment, a lock state in the camera policy, reticle provider, validation. | High | Switch-target inputs (mouse/wheel/stick) collide with yaw/zoom; reticle needs TrueHUD (1.7.104 build unverified); MCM settings are user-editable. |
| R4 | **One precedent only:** Skyrim Camera Disocclusion System (SE 1.5.97 offsets, near-plane clipping, culling hooks). Not usable as-is on 1.7.104. | Collision filtering, occluder detection, fade/hide mechanism, room-bounds handling, hysteresis, performance. | Low–Medium; staged | SmoothCam collision has no toggle and no fork licence; static fade mechanism unverified; room-bounds culling when the camera leaves a room. |

## R1 — Proximity activation: reuse BTPS

**What exists.** Better Third Person Selection by Shrimperator (Nexus 64339; source on GitLab). *Web:* 0.9.0 (2026-10-02) is built on CommonLibSSE-NG, handles the 1.7.99+ vtable shift and the author states 1.7.99/1.7.104 tested; 0.8.9 is the 1.5/1.6 build. Licence changed to **GPL-3.0-or-later** on 2026-10-02. Requires Address Library, **JContainers** and MCM Helper.

**How it selects.** *Web (source read by the research agent):* it gathers references within 200 units of the player and scores each by distance, angle from the camera forward vector, type and value, with small bonuses for the native crosshair target and the previous selection. Occlusion is tested with hull raycasts **from the player's body**, not from the camera, so an elevated camera does not hide a bench behind the camera ray. `fMaxAngleDif=2.0` makes the score purely proximity-based. Cycling walks the sorted list (`iCycleUpKey`/`iCycleDownKey` with a modifier, rebindable to gamepad codes, optional wrap); the cycled object sticks until out of range.

**How it injects.** It patches `CrossHairPickData::Pick` (AE ID 26127 at a fixed offset) so the engine's own crosshair target becomes BTPS's choice. Activation, prompts and QuickLoot therefore follow it without further hooks. It also hooks `PlayerCharacter::UpdateCrosshairText`, `ActivateHandler::ProcessButton` and `HUDMenu::PostDisplay` for its widget; it hides the widget during TDM lock-on through the TDM API.

**Fit with SkyCRPG.** SmoothCam does not override the pick (it calls `PickCrosshairReference` and reads `collisionPoint` for its 3D crosshair), so BTPS and SmoothCam coexist as they do for ordinary users. The Camera module never touches the pick. The bench case from the Bannered Mare route is exactly the case BTPS was written for; whether its default angle weighting behaves under a 55° held pitch is **unverified** and must be tuned in game.

**Gaps.** No API to push an external target or to read the candidate list (its V0 API only reports enabled state and hides/shows the widget). Cycling is key-driven only. A second plugin cannot patch the same `Pick` offset without conflicting, so R2 must go through BTPS (fork or upstream) or replace it.

**Alternatives examined.** QuickLoot IE/EE, Simple Activate, SmartHarvest, Simple Sit, Use or Take: none select targets; they consume the crosshair reference or add hotkey actions. TDM target lock is actor-only. SmoothCam's 3D crosshair still follows the camera ray.

## R2 — Cursor mode: original work on top of BTPS

**What exists.** *Web:* nothing provides a free gameplay cursor or a stick-driven world reticle. Precedents for stick-driven gameplay overlays are Wheeler (BSD-3, ImGui over D3D11) and powerofthree's PhotoMode (GPL-3, ImGui). Cursor Unbound only smooths the menu cursor. A Scaleform menu flagged `kUsesCursor` shows the native cursor but no mod uses it for world picking.

**Mechanics available.** *Source:* `RE::CrosshairPickData` exposes `target`, `targetActor`, `collisionPoint`, `targetCollider` and the `pickCollider` phantom; the native pick is a camera-origin hull ray (`fActivatePickRadius`, `fActivatePickLength`) through `bhkWorld::PickObject`. `RE::MenuCursor` carries `cursorPosX/Y`; `NiCamera::WorldPtToScreenPt3` and the inverse ray helpers exist in CommonLib. The Camera module already owns the third-person look hook, so consuming right-stick/mouse deltas while a cursor mode is active is a policy change, not a new hook.

**Design consequence.** On keyboard/mouse the pointer currently drives yaw. A hover cursor therefore needs either a toggle (same as the controller hotkey) or a CRPG-style "hold right mouse to rotate, pointer otherwise free" scheme. This is an owner decision; the plan assumes a toggle on both devices first, with hold-to-rotate as a follow-up option.

**Injection path.** Three options, in order of preference: (1) upstream a small "external target provider" API to BTPS; (2) maintain a GPL-3 fork of BTPS with that API, shipped as a separate external package like the GPL minimap fork; (3) replace BTPS with an original selection plugin (duplicates R1 work, rejected unless 1 and 2 fail). The Camera module stays MIT and talks to the provider through its exported API, mirroring the SmoothCam/TDM pattern.

## R3 — Lock-on: reuse TDM target lock

**Why it is currently blocked.** The Camera module takes `RequestDisableTargetLock` at data load (`plugin.cpp`), and its policy prerequisites require `!GetTargetLockState()`, because TDM's lock writes camera yaw and pitch and would fight the held-pitch filter ([architecture](../ARCHITECTURE.md#adapter-boundary-and-settings-restoration)).

**What the source actually does.** *Source (`DirectionalMovementHandler::LookAtTarget`):* each frame while locked, TDM writes `ThirdPersonState::freeRotation.x` only if `fTargetLockYawAdjustSpeed > 0`; it always sets the **player's** pitch toward the target and adds the same delta to `freeRotation.y`, which keeps the camera's world pitch unchanged; it then smooths `freeRotation.y` toward the target only if `fTargetLockPitchAdjustSpeed > 0`. With both speeds at 0 the lock writes no camera rotation and the held-pitch policy sees an unchanged world pitch. `uTargetLockMode = kRelaxed` additionally leaves mouse look untouched; in `kLocked` mode the look hook swallows mouse movement and uses it to switch targets.

**Why it fixes the ranged FAIL.** *Source (`Hooks::ProjectileHook`):* while locked, player arrows and missiles with `uTargetLockArrowAimType`/`uTargetLockMissileAimType = kPredict` get their launch velocity solved toward the target point once (`kHoming` re-aims every tick). This ignores the camera entirely, which is precisely the Phase 1 overhead-aim failure mode (projectiles follow the camera ray into the ground). SmoothCam's own `useProjectileFixes` aims along the camera pitch and does **not** help from an elevated view; it is arrows-only.

**Target selection and switching.** *Source:* `FindTarget` prefers the actor under the crosshair, else the closest valid actor (alive, hostile if `bTargetLockHostileActorsOnly`, in line of sight, within `fTargetLockDistance`); manual presses use a distance-times-angle score. Switching uses the right thumbstick, mouse delta over a sensitivity threshold (only in `kLocked`), or the scroll wheel (`bTargetLockUseScrollWheel`). The wheel collides with SkyCRPG zoom and the stick with yaw; the fragment must disable the colliding inputs and the plan must validate the remaining ones. `bAutoTargetNextOnDeath` exists.

**API.** *Source (`TrueDirectionalMovementAPI.h`, V5):* `GetTargetLockState`, `GetCurrentTarget`, `RequestDisableTargetLock`/`ReleaseDisableTargetLock`, `RequestYawControl`/`SetPlayerYaw`. There is **no** API to start a lock, choose a target or change settings; those remain MCM/INI settings, which the project already manages as a merged fragment.

**Reticle.** *Source:* TDM's lock reticle is drawn through TrueHUD's API (`TDM_Widgets.swf`); without TrueHUD there is no reticle and `bTargetLockHideCrosshair` hides the crosshair. TrueHUD's 1.7.104 build is **unverified**. Camera-only installs need either TrueHUD as an optional external dependency or an original minimal lock indicator; the UI module must not become a Camera requirement.

**Other lock-on mods.** *Web:* himika's Lock-On (LE), LockonSSE/Simple Lock-On (1.5.x era, unsupported), TDM Lock-on Fixes (reticle positions), Precision (hit detection only), AIM FIX (crosshair offsets). No maintained alternative; nothing to reuse beyond TDM.

## R4 — Fixed height and occluder fade: staged original work

**Why the camera pulls in today.** *Source:* SmoothCam's collision is unconditional: `thirdperson.cpp` casts player→side-offset (hull 30) then `BaseThird::ComputeRaycast` (hull 15) through the engine camera caster (AE ID 33007) and moves the camera to the hit. There is no config toggle, no per-layer filter and no fork licence (the [reuse audit](../REUSE_RESEARCH.md) records only the API-header permission). The opt-in DYL-157 compression policy then lowers pitch/zoom on top of that; it is the "zoom in under beams" the owner dislikes and defaults OFF.

**Ways to stop the pull-in without a second transform writer.**

1. **Detour the engine camera caster** (AE ID 33007) while the SkyCRPG policy is eligible: call the original, then discard hits that the policy classifies as fade-able occluders (for example overhead hits whose normal points down and whose goal position is still in open space), restoring the requested end point. SmoothCam remains the only transform writer; it simply sees "no hit". This keeps F8/native behaviour when the policy is off. **Unverified:** the function's full contract (the DYL-157 audit already noted a write through the camera context before the cast) and that SmoothCam tolerates a reported miss where the goal is inside thin geometry.
2. **Collision layers.** `NiAVObject::SetCollisionLayer` on an occluder's 3D or editing `bhkCollisionFilter::layerBitfields` for the camera layer. Rejected as the primary route: beams are frequently part of the same mesh as walls, changing a layer also changes actor collision, and the camera phantom's actual layer is unverified.
3. **Take camera control** (`RequestCameraControl`) and own collision. This is the "original controller" route explicitly deferred in Phase 1; it also means owning interpolation and every special state. Not recommended for this phase.
4. **Camera Studio** (Nexus 196829, GPL-3 + MPL, Improved Camera NG fork, states SE 1.5.97–1.7.104) adds an isometric third-person mode. *Web:* no statement about collision, occluders or ceilings. Worth one evaluation session as an alternative provider, not a plan dependency.

**Making occluders see-through.** *Web:* the only precedent is **Skyrim Camera Disocclusion System** (machinegod420, Nexus 178575, GPL-3, May 2026): ring raycasts from camera to player each frame, accepts only `kStatic`/`kAnimStatic` hits that are not floor-like, then **near-clips the occluder per draw call** by rewriting the projection near plane inside `BSBatchRenderer::RenderPassImmediately`; it defeats room culling by hooking `BSMultiBoundRoom::QPointWithin`, moving occlusion shapes and neutralising `BSOcclusionPlane`. Its render hooks are **SE-only offsets**, the author states AE is untested, reports artefacts ("doesn't fade very well") and warns about draw-call cost indoors. It is a reference for detection and culling, not a drop-in dependency.

Mechanisms a 1.7.104 implementation can try, in order of cost:

| Mechanism | What it does | Status |
| --- | --- | --- |
| `NiAVObject::SetAppCulled` on the occluder's 3D | Hides the reference outright (CRPG cutaway, no fade) | Known to work for hiding; pops; needs hysteresis and restore on every lifecycle exit. |
| `NiAVObject::UpdateMaterialAlpha` | Sets `materialAlpha` on each geometry's lighting shader material; this is how actor alpha works | **Unverified for statics**: a static without an alpha-blending `NiAlphaProperty` may ignore it, and the engine may rewrite alpha from `BSFadeNode::currentFade`. Spike required. |
| Add/modify `NiAlphaProperty` and clear render passes | Forces alpha blending on the occluder | Unverified; sorting artefacts likely; per-object cost. |
| Near-plane clipping per draw (Disocclusion approach) | Clips the occluder without materials | Needs 1.7.104 offsets for the render hook; artefacts documented by its author. |
| Shader-level dither (Community Shaders feature) | Proper fade | Out of scope: requires a renderer dependency. |

**Interior culling risk.** *Web (CK wiki):* when the camera leaves all room bounds, the engine draws only what portals expose; interiors have no outer shell. A camera held above a low rock ceiling therefore shows nothing. Disocclusion's `QPointWithin` hook is the only known workaround and it forces every room visible. Hence the staging below: keep collision for true enclosure (the preferred camera position lies inside solid geometry or outside the room) and only ignore **thin overhead occluders** whose far side is open space. Full see-through through walls is a later stage that depends on a culling-fix spike.

**The "jarring pop" already observed.** The owner reported low overhead objects disappearing at a threshold. The research found no near-camera fade for statics; the likely cause is the near clip plane (`fNearDistance`, default 15) as the collided camera passes through the beam. Keeping the camera away from the beam (R4 stage 1) removes the trigger rather than the symptom.

## Input budget

Three new bindings are needed on controller (lock toggle, cycle/tab, cursor-mode toggle) and the Camera module refuses natively mapped buttons. TDM's default lock toggle is a short press of the POV button (`bTargetLockUsePOVSwitchGamepad`, `fTargetLockPOVHoldDuration`), which the fragment currently disables; re-enabling it shares R3 with native POV hold. Cycle and cursor mode have no proven free button. This is an owner decision recorded in the plan, not an engineering default.

## Licensing notes

BTPS (GPL-3.0-or-later), TDM (GPL-3 with modding and linking exceptions), Disocclusion (GPL-3) and Camera Studio (GPL-3 + MPL) are all external. Using their exported APIs from the MIT Camera module follows the existing SmoothCam/TDM pattern. A BTPS fork would be a separately distributed GPL package with corresponding source, as already planned for the minimap fork. Copying Disocclusion's or TDM's code into the Camera module is not compatible with D015 and is not proposed.

## Open verification items

- JContainers and TrueHUD builds for the 1.7 line (dependency lock entries, hashes).
- BTPS 0.9.0 behaviour under a 55° held pitch with SmoothCam 1.8.1, including the widget position.
- TDM `kRelaxed` + zero adjust speeds in game: no camera writes, projectile prediction from the elevated view, switching inputs.
- Engine camera caster detour contract on 1.7.104.
- Static alpha behaviour (`UpdateMaterialAlpha` / `NiAlphaProperty`) on 1.7.104.
- Room-bounds culling when the held-height camera leaves a room.

## Sources

- BTPS: <https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection>, <https://www.nexusmods.com/skyrimspecialedition/mods/64339>
- TDM: <https://github.com/ersh1/TrueDirectionalMovement> (`src/DirectionalMovementHandler.cpp`, `src/Hooks.cpp`, `src/Settings.h`, `src/TrueDirectionalMovementAPI.h`, `EXCEPTIONS`), <https://www.nexusmods.com/skyrimspecialedition/mods/51614>
- SmoothCam: <https://github.com/mwilsnd/SkyrimSE-SmoothCam> (`source/raycast.cpp`, `source/camera_states/thirdperson/thirdperson.cpp`, `source/camera_states/base_third.cpp`, `source/arrow_fixes.cpp`, `include/config.h`)
- Skyrim Camera Disocclusion System: <https://github.com/machinegod420/SkyrimCameraDisocclusion>, <https://www.nexusmods.com/skyrimspecialedition/mods/178575>
- Camera Studio: <https://www.nexusmods.com/skyrimspecialedition/mods/196829>
- CommonLibSSE-NG: <https://github.com/CharmedBaryon/CommonLibSSE-NG> (`RE/C/CrosshairPickData.h`, `RE/N/NiAVObject.h`, `RE/B/BSFadeNode.h`, `RE/C/CollisionLayers.h`, `RE/B/bhkCollisionFilter.h`, `RE/P/PlayerCamera.h`, `RE/P/PlayerCharacter.h`)
- Room bounds: <https://ck.uesp.net/wiki/Room_Bounds_and_Portal_Basics>
- INI references: <https://stepmodifications.org/wiki/Guide:Skyrim_INI/Camera>, <https://stepmodifications.org/wiki/Guide:Skyrim_INI/Interface>, <https://stepmodifications.org/wiki/Guide:Skyrim_INI/Display>
- Other mods examined: QuickLoot IE <https://github.com/MissCorruption/QuickLootIE>, QuickLoot EE <https://github.com/WaterFace/QuickLootEE>, Simple Activate <https://github.com/powerof3/SimpleActivateSKSE>, SmartHarvest <https://github.com/SteveTownsend/SmartHarvestSE>, Wheeler <https://github.com/D7ry/wheeler>, Cursor Unbound <https://github.com/kevinggundersen/CursorUnbound>, LockonSSE <https://github.com/aers/LockonSSE>, Precision <https://github.com/ersh1/Precision>, TISS guide <https://www.nexusmods.com/skyrimspecialedition/mods/89358>
