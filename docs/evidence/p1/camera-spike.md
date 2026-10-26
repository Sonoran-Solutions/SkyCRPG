# DYL-152 / P1-03 camera feasibility spike

Updated 2026-10-26. **Recommendation: existing SmoothCam provider plus a small original adapter for later pitch persistence and exclusive aim-mode handoff. The adapter is unimplemented; the proven spike uses original configuration and native first-person precision fallback, with no second transform writer.** The recovered owner route demonstrates usable pulled-back movement/melee and first-person precision fallback. **FAIL** for top-down third-person bow/spell aiming: projectiles aim at the player's feet. Provider configuration alone does not satisfy the ranged-combat requirement; an existing targeting alternative is under investigation.

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
| Open road: movement and framing | **PASS (owner-observed, qualified)** | Run20261026T073401: owner considers quick MVP usable. Stable held pitch/recenter not separately established. |
| Tight Nordic room, corridor, doorway: collision and close fallback | **NOT_RUN** | Provider collision path is retained; actual wall/floor recovery is not proven by source inspection. |
| Melee readability and camera control | **PASS (owner-observed, qualified)** | Owner reports melee is just about fine; no quantitative target readability measure. |
| Top-down third-person bow and aimed spells | **FAIL** | Owner reports arrows/spells aim at player feet. This reproduces a provider/configuration limitation for current framing. |
| First-person precision fallback / world activation | **PASS (owner-observed)** | Owner explicitly confirms first person works and object activation remained usable. |
| Ten F8 toggle/handoff pairs | **PASS (owner-observed)** | Owner confirms all ten requested pairs restored normal third person and returned reliably. |
| Interior/exterior transition usability | **PASS (owner-observed)** | Owner confirms cave/outdoor transition remains usable. |
| Pitch persistence through loading screens | **FAIL — owner-deferred, nonblocking for these PRs** | Combined run075556: zoom persists but top-down pitch resets to normal third person. DYL-159 tracks later restoration. |

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

See [session evidence](session-2026-10-26.md) for timestamps and logs. The first interactive run ended after systemd-oomd killed GNOME Shell under memory pressure; the root memory contributor is unresolved. A post-reboot launch failed because Steam was not running. Steam was restored before changed-condition run20261026T073401, which reached plugin/data-load markers under a memory watchdog. Owner gameplay report is recorded above. Recovery run exited cleanly07:42:59 with no leftovers; minimum17.13GiB available RAM and zero full pressure avg10. Controller is available; complete input coverage remains broader than these owner checks.

The next bounded alternative is TDM's existing lock-on/projectile aiming, contingent on verifying camera ownership and input conflicts in source. This is investigation, not acceptance or a new large custom controller. First-person aiming remains the proven precision fallback. No camera-only screenshot/video was supplied for run073401; evidence is explicit owner report plus timestamped loader/provider/run logs. The later combined run075556 supplies a timestamped overhead/minimap screenshot with exact artifact hash.


## Bounded alternative: existing TDM targeting

**BLOCKED as a settings-only coexistence option.** Pinned TDM2.3.1 source `57b913ae17fed857c9e281799174898cd66a3e82`, `src/DirectionalMovementHandler.cpp:2827-2829,2856-2876`, writes camera yaw and pitch during lock-on. Its SmoothCam calls request crosshair ownership only (`:674,896`); no camera-control request/release is used. Setting interpolation speeds to zero is insufficient because player pitch and camera pitch offsets still get assigned. SmoothCam1.8.1 exposes exclusive camera handoff in its permitted API, but current TDM does not use it.

TDM's arrow/missile aim enums are0=FreeAim,1=Predict,2=Homing; assist requires an active lock. Beam targeting exists, but not every concentration spell is proven; target-location spells are excluded. No targeting mode was enabled in the game, so lock-on combat remains **NOT_RUN**. Source audit is the bounded alternative investigation, not another gameplay PASS. The observed aiming-at-feet behavior is real; an elevated camera/native aim-vector mismatch is only a causal hypothesis.

Recommendation: **existing provider plus a small original adapter**, retaining the proven SmoothCam configuration and native POV precision fallback. Any future targeting mode must explicitly suspend/release one camera owner before enabling another, and must recover on unlock, menus, unload and provider absence. Such an adapter still needs implementation and evidence; the present configuration remains the playable spike. No exclusive custom controller or broad provider fork is justified. Phase2 should reuse provider obstruction/framing, TDM directional movement, original config staging, F8 emergency control and the copied fixture route. Stable held pitch/recenter and polished target selection remain open.


## Combined result and owner-deferred pitch defect

Combined Camera+UI run20261026T075556 **PASS** for all8 DLL loads and owner checkpoint, clean exit/no leftovers. [Overhead camera/minimap capture](captures/combined-20261026-topdown-minimap.png) and [capture hashes/times](captures/combined-captures.json) directly demonstrate the combined view. [Installed-file manifest](tested-artifacts.json) pins combined artifact`8a31a81a18b196cf51431f570d609f8b0cf0375f967299ac75a22488432ef307`; native skeleton source/archive pins are in native-package-inputs.json, provider archives in dependency-lock.md.

Owner reports top-down camera angle resets after Helgen Keep loading screens in either direction while zoom persists. This qualifies the earlier transition result: controls recover, but pitch preference does not. Owner explicitly says this should not block these PRs; DYL-159 remains Backlog with reproduction and acceptance follow-up. No repair or Phase2 work started. The final environment is undeployed with original INIs/plugin list restored and146 original/copied files unchanged.
