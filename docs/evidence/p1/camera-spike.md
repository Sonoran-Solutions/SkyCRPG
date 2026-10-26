# DYL-152 / P1-03 camera feasibility spike

Updated 2026-10-26. **Recommendation: continue with SmoothCam 1.8.1 as the only third-person camera writer, using an original provider-format framing preset. Do not add a second transform controller at this point.** Provider startup passed on the real host, but gameplay remains **NOT_RUN** and no camera behavior is accepted. The first owner route was interrupted by a desktop memory-pressure failure; the monitored recovery route is pending.

## Candidate route

`modules/camera/config/SmoothCam.json` is a small original active configuration in SmoothCam's `UserConfig` format; SmoothCam reads it from `Data/SKSE/Plugins/SmoothCam.json` at startup. `modules/camera/config/SmoothCamPreset0.json` carries the same values in the exact `{"name": ..., "config": ...}` envelope for later tuning through MCM slot 1. Its `Preset`, `UserConfig` and nested `OffsetGroup` readers use `j.value(key, existing_member)` through `VALUE_FROM_JSON`, so omitted members keep their C++ in-class defaults; a partial nested group is not zero-filled. Candidate states supply only `sideOffset`, `upOffset` and `zoomOffset`, leaving combat sub-offsets and interpolation values at their declared class defaults. Standing, walking, running and sprinting use a centered `upOffset=80` and pulled-back `zoomOffset=-80`; sneaking uses `60/-60`; bow aim returns toward a shoulder framing at `20/80`. Every supplied offset stays within SmoothCam MCM's default slider interval of -100 to 100. These are initial values for in-game tuning, not measured distances or proof of overhead pitch.

SmoothCam's existing camera path retains its obstruction handling and raycast aim implementation. The config does not replace or bypass those systems. SmoothCam has no held-pitch setting or direct pitch setter in the inspected 1.8.1 user-config/API surfaces, so fixed/elevated pitch and recenter semantics remain unresolved. The game spike must verify whether the angle follows the native camera aim acceptably with mouse and controller input. For emergency provider handoff, the active config starts SmoothCam enabled and binds `modToggleKey=66` (DirectInput F8 scan code); source inspection confirms keyboard `idCode` is compared directly with this value and toggles `modDisabled`. SmoothCam MCM also exposes **Disable SmoothCam**, **Toggle SmoothCam Key**, and **Force camera to thirdperson**. The native Skyrim **Toggle POV** action is passed into SmoothCam's POV-state handler and is the precision-aim fallback to first person; handoff behavior is **NOT_RUN**.

Use TDM 2.3.1 for directional movement with `DirectionalMovement/uAdjustCameraYawDuringMovement=0` to prevent TDM from competing with the intended camera heading. The original settings fragment is `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini`; apply these values to the user's existing `Data/MCM/Config/TrueDirectionalMovement/settings.ini`, never as a replacement for the full file. It disables TDM leaning and headtracking, target-lock reticle and POV-switch behaviors, clears the target-lock activation binding (`Keys/uTargetLockKey=-1`), and disables camera yaw auto-adjustment. Keep `uTargetLockMode` unchanged: TDM enum value `0` means **locked**, not disabled. MCM Helper 1.6.3 and SkyUI 6+ are required for TDM's MCM route.

For precise bow/spell aiming, compare SmoothCam's built-in ranged framing and raycast crosshair against native first-person/vanilla third-person aiming via the POV switch. On activation prompts, check the actual world-interaction ray/crosshair; do not infer it from the combat reticle. Melee uses the centered running preset and TDM directional movement. If the provider preset cannot hold usable framing or aiming requires a closer view, prefer its native/vanilla camera fallback. Consider a SkyCRPG adapter only after a concrete failure and only through an ownership API that SmoothCam grants; no transform hook or custom controller is included in this spike.

## Evidence and gate

| Check | Result | Evidence / limit |
| --- | --- | --- |
| SmoothCam schema, nested defaults, offset values | **PASS (static)** | `python3 modules/camera/tools/check_preset.py --smoothcam-source <pinned-checkout>` derives and cross-checks `Preset`/`UserConfig`/`OffsetGroup` read/write JSON keys, confirms `VALUE_FROM_JSON` preserves omitted member defaults, confirms the declared SliderSetting range, and validates both active config and preset. |
| Original config staging and archive reproducibility | **PASS (static)** | `python3 modules/camera/tools/stage_preset.py --out <directory> --commit <sha>` stages `SKSE/Plugins/SmoothCam.json`, `SKSE/Plugins/SmoothCamPreset0.json`, and a hash manifest, with archive/member hashes in `SHA256SUMS`. |
| Emergency provider/native POV recovery controls | **PASS (static)** | SmoothCam 1.8.1 source confirms its keyboard event id is compared to `modToggleKey` and toggles `modDisabled`; the active config selects F8 (`66`) and starts enabled. Its MCM source also exposes Disable SmoothCam and Force camera to thirdperson. POV events are routed to the provider handler. Live handoff remains NOT_RUN. |
| TDM camera auto-adjust disable value | **PASS (static)** | TDM 2.3.1 source enum: `kDisable=0`, `kDuringSprint=1`, `kAlways=2`. |
| TDM target-lock fallback | **PASS (static)** | Source enum: `kLocked=0`, `kRelaxed=1`; no disabled enum value. Clear bindings as listed above. |
| Provider startup | **PASS (load only)** | Run20261026T071539: MCM Helper, SmoothCam, TDM and camera skeleton loaded;30-second settle, no leftovers. See shared session logs. |
| Open road: framing, pitch, recenter, movement | **NOT_RUN** | Owner had not begun when the desktop session failed in run20261026T072045. |
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
| Address Library | 13 | Acquired13 and loaded with the selected providers on the shared host; package hash in dependency lock. |
| SmoothCam | 1.8.1 Beta1.8.1; AE 1.7.99+ package line | External archive `/tmp/skycrpg-dyl151-audit/SmoothCam-Beta1.8.1.zip`; archive SHA-256 `efed5592257aa67d92beb48f9d6f8cacb00c1784491cfb9c748bec9a9ec5d07b`; inspected source checkout commit `e0afd09aff26e5849dac461252e5bffbd5b69d62`. Do not redistribute archive/DLL. |
| TDM | 2.3.1 | Archive SHA-256 `0593379072997689f3d1db2302b0bf060c0ddc116743ab9585feaf328bec0b0f`; installed package defaults inspected from session extraction. Source audit checkout `/tmp/skycrpg-dyl151-audit/TrueDirectionalMovement`. |
| MCM Helper | 1.6.3 | Archive SHA-256 `505af9c27062febd497be09b661d5df82e013ae4a205ffbec99698adf4503f52`. |
| SkyUI Community | 6.11 | Archive SHA-256 `4eb0c9fc5241e5c3dd2fd773ee8a89627c924bf4445cf70373d3a4c7c4393ff1`. |

The candidate JSON and TDM settings fragment are original configuration; no third-party code or assets are copied. `modules/camera/tools/stage_preset.py` stages the active config and matching MCM preset plus a manifest and SHA256SUMS, outside the game install. It does not package TDM's user settings fragment or any provider payload. This config-only archive is ready for the disposable camera-only profile. The normal DLL package remains unchanged until the game route accepts and tunes the preset.

## Bounded game route for the integrator

Use a copied exterior/interior save pair in the isolated `camera-only` profile. Record host, executable, provider versions and hashes, profile manifest, run ID and timestamped captures. Install the staged `SmoothCam.json` as the provider startup configuration; load slot 1 from MCM only if tuning/reloading the matching preset. Confirm F8 returns to the vanilla third-person camera, then check: (1) open road walk/run/sprint and recenter; (2) melee movement/readability; (3) bow, aimed spell and activation prompts with explicit close-view fallback; (4) a narrow corridor and doorway, confirming walls/floors still occlude and the camera returns closer without lockup; (5) ten overhead/native POV handoffs; (6) the paired interior/exterior transition. Stop after a first failure, record it, and only repeat after a named change. Keep crashes/aim behavior distinct from the successful load-only result in DYL-150.

## Runtime interruption and retry

See [session evidence](session-2026-10-26.md) for timestamps and logs. The first interactive run ended after systemd-oomd killed GNOME Shell under memory pressure; the root memory contributor is unresolved. A post-reboot launch failed because Steam was not running. Steam was restored before changed-condition run20261026T073401, which reached plugin/data-load markers under a memory watchdog. Owner gameplay report pending. Controller is available through owner playtesting; camera controller acceptance is not yet established.
