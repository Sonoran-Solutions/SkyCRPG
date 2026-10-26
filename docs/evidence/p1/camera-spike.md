# DYL-152 / P1-03 camera feasibility spike

Updated 2026-10-26. **Recommendation: continue with SmoothCam 1.8.1 as the only third-person camera writer, using an original provider-format framing preset. Do not add a second transform controller at this point.** This is a source/config feasibility result; the game route remains **NOT_RUN** and no camera behavior is accepted.

## Candidate route

`modules/camera/config/SmoothCamPreset0.json` is a small original preset in the exact `{"name": ..., "config": ...}` envelope read by SmoothCam 1.8.1. SmoothCam loads preset slot zero from `Data/SKSE/Plugins/SmoothCamPreset0.json`. Its `UserConfig` JSON reader applies defaults to omitted properties, so the candidate supplies only per-state position offsets. Standing, walking, running and sprinting use a centered `upOffset=90` and pulled-back `zoomOffset=-180`; sneaking uses a closer `65/-110`; bow aim returns toward a shoulder framing at `20/80`. These are initial values for in-game tuning, not measured distances or proof of overhead pitch.

SmoothCam's existing camera path retains its obstruction handling and raycast aim implementation. The preset does not replace or bypass those systems. SmoothCam has no held-pitch setting or direct pitch setter in the inspected 1.8.1 user-config/API surfaces, so fixed/elevated pitch, recenter semantics, and an emergency transition to normal third person remain unresolved. The game spike must verify whether the angle follows the native camera aim acceptably with mouse and controller input. The standard Skyrim POV switch is the available immediate native recovery while testing; its handoff behavior is **NOT_RUN**.

Use TDM 2.3.1 for directional movement with `DirectionalMovement/uAdjustCameraYawDuringMovement=0` to prevent TDM from competing with the intended camera heading. The original settings fragment is `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini`; apply it to the user's existing `Data/MCM/Config/TrueDirectionalMovement/settings.ini`, never as a replacement for the full settings file. It also clears TDM's target-lock activation binding (`Keys/uTargetLockKey=-1`) and stops target lock from switching the gamepad POV (`TargetLock/bTargetLockUsePOVSwitchGamepad=0`). Leave `uTargetLockMode` alone: TDM enum value `0` means **locked**, not disabled. MCM Helper 1.6.3 and SkyUI 6+ are required for TDM's MCM route.

For precise bow/spell aiming, compare SmoothCam's built-in ranged framing and raycast crosshair against native first-person/vanilla third-person aiming via the POV switch. On activation prompts, check the actual world-interaction ray/crosshair; do not infer it from the combat reticle. Melee uses the centered running preset and TDM directional movement. If the provider preset cannot hold usable framing or aiming requires a closer view, prefer its native/vanilla camera fallback. Consider a SkyCRPG adapter only after a concrete failure and only through an ownership API that SmoothCam grants; no transform hook or custom controller is included in this spike.

## Evidence and gate

| Check | Result | Evidence / limit |
| --- | --- | --- |
| Exact SmoothCam preset envelope and per-state JSON names | **PASS (static)** | Inspected `Config::Preset`, `Config::UserConfig` and `OffsetGroup` serialization/deserialization in the 1.8.1 source. `python3 modules/camera/tools/check_preset.py` validates the checked-in envelope and candidate offsets. |
| TDM camera auto-adjust disable value | **PASS (static)** | TDM 2.3.1 source enum: `kDisable=0`, `kDuringSprint=1`, `kAlways=2`. |
| TDM target-lock fallback | **PASS (static)** | Source enum: `kLocked=0`, `kRelaxed=1`; no disabled enum value. Clear bindings as listed above. |
| Open road: framing, pitch, recenter, movement | **NOT_RUN** | No game launch/deployment in this worker lane. |
| Tight Nordic room, corridor, doorway: collision and close fallback | **NOT_RUN** | Provider collision path is retained; actual wall/floor recovery is not proven by source inspection. |
| Melee readability and camera control | **NOT_RUN** | No gameplay session. |
| Bow and aimed spell accuracy / activation prompts | **NOT_RUN** | No gameplay session; native POV aiming fallback needs a live check. |
| Ten toggle/handoff cycles | **NOT_RUN** | No gameplay session. |
| Interior/exterior transition | **NOT_RUN** | No gameplay session. |

Source/config PASS does not satisfy P1-03's in-game acceptance. The assigned shared-install owner must install the selected external dependencies in an isolated camera-only profile and capture the bounded route before P1-05 can freeze this stack.

## Pinned inputs

| Component | Version/target | Source and integrity |
| --- | --- | --- |
| Skyrim | Steam 1.7.104.0 | DYL-150 environment record; worker did not launch or alter the game. |
| SKSE | 2.3.1 Steam | DYL-150 load-only skeleton baseline. |
| Address Library | 13 | Candidate prerequisite from DYL-151; not installed or tested by this worker. |
| SmoothCam | 1.8.1 Beta1.8.1; AE 1.7.99+ package line | External archive `/tmp/skycrpg-dyl151-audit/SmoothCam-Beta1.8.1.zip`; archive SHA-256 `efed5592257aa67d92beb48f9d6f8cacb00c1784491cfb9c748bec9a9ec5d07b`; inspected source checkout commit `e0afd09aff26e5849dac461252e5bffbd5b69d62`. Do not redistribute archive/DLL. |
| TDM | 2.3.1 | DYL-151 source audit checkout `/tmp/skycrpg-dyl151-audit/TrueDirectionalMovement`; no provider binary in this spike. |
| MCM Helper | 1.6.3 | DYL-151 selected Steam 1.7.99+ file line; archive not acquired in this lane. |

The candidate JSON is original configuration and contains no third-party code or assets. It is not yet wired into `tools/package.py`; the packaging owner should include it at `SKSE/Plugins/SmoothCamPreset0.json` only after the game route accepts/tunes the preset. Until then, deploy it manually only in a disposable test profile.

## Bounded game route for the integrator

Use a copied exterior/interior save pair in the isolated `camera-only` profile. Record host, executable, provider versions and hashes, profile manifest, run ID and timestamped captures. Load the preset through SmoothCam's slot-0 MCM control, then check: (1) open road walk/run/sprint and recenter; (2) melee movement/readability; (3) bow, aimed spell and activation prompts with explicit close-view fallback; (4) a narrow corridor and doorway, confirming walls/floors still occlude and the camera returns closer without lockup; (5) ten overhead/native POV handoffs; (6) the paired interior/exterior transition. Stop after a first failure, record it, and only repeat after a named change. Keep crashes/aim behavior distinct from the successful load-only result in DYL-150.
