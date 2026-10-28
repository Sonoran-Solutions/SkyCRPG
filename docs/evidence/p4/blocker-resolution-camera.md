# DYL-170 — Camera blocker resolution

**Provenance:** base `c7333c7c13284ad9d228e6f15213bc8465bd6e19`; candidate `<head SHA>`; Windows CI `<CI run>`; Camera DLL `<DLL sha256>`; package hash `<package sha256>`. Planned runtime: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SmoothCam 1.8.1, TDM 2.3.1, MCM Helper 1.6.3, SkyUI Community 6.11, Proton host. Record exact host, executable, dependency and profile hashes for every eventual run. No game or Windows build was run by this worker.

## Audit

| Item | Existing coverage and change | Status |
| --- | --- | --- |
| Disable/restart | `select` gates framing on `enabled`; F6 and POV fallback, no control-disabling calls. Prior [controls](../p2/camera-controls.md) and [lifecycle](../p2/lifecycle.md) are bounded; restart not established. Disabled startup skips the TDM lock-disable lease; runtime enable requests it before enabling, while F6 precision retains it. Provider override layers remain. | **NOT_RUN**; full independent recovery open |
| Provider failure | Exact hash gate, TDM API and lease checks, chain and ownership guard exist. Failure now logs and requests a single in-game HUD notice when a loaded session observes failure. Input observes a replaced update hook; test input and chain failure. UI is not referenced. | **NOT_RUN** |
| Wrong runtime | SKSE version declaration rejects unsupported runtime before plugin logic. No in-game notice or Camera log can be promised; installed SmoothCam/TDM settings continue independently. | **NOT_RUN** |
| INI | Existing validation bounds values and invalid schema disables; now malformed/nonfinite numbers and bad flags default, changed numeric bounds/bindings and schema log. Missing keys silently default. | **NOT_RUN** in game; portable validation tests PASS |
| Active obstruction F8 and zoom | Existing `resetZoomRecovery` tests four transitions including F8/provider and `resetObstructionZoom` preservation, plus closed-loop tests; [obstruction](../p2/obstruction.md) says F8 and `28aae27` game checks remain open. | **PASS** portable; **NOT_RUN** in game |
| Crossbow/concentration | Existing code uses `IsWeaponDrawn` to suppress automatic obstruction, not equipment classification. Added test of that policy gate. Whether engine reports these states as drawn and whether combat works remains unverified. [Combat](../p2/combat-and-activation.md). | **PASS** portable; **NOT_RUN** in game |
| Range/LOS | No Camera activation, crosshair pick, range or LOS mutation found; Skyrim remains authoritative. [Combat](../p2/combat-and-activation.md) does not establish range/LOS. | **NOT_RUN** in game |
| Special states | `specialHazard` has individual mount, kill move, swim, furniture/sit, bleedout, beast and scripted portable assertions; lifecycle suspends custom framing. [Lifecycle](../p2/lifecycle.md) is bounded, not each state. | **PASS** portable; **NOT_RUN** each in game |
| Aim/lock | [Phase 3 gate](../p3/gate.md): overhead ranged aim **FAIL**, target lock **BLOCKED**; F6 native first person demonstrated and accepted for Phase 2. [Phase 5 proposal PR #24](https://github.com/Sonoran-Solutions/SkyCRPG/pull/24) is a draft, not an implementation or owner decision. | **FAIL / BLOCKED — accepted MVP limitation (owner decision 2026-10-28, see DECISIONS.md)** |

## Design and limitations

No new camera owner or cross-module dependency. Engine notification uses `RE::SendHUDMessage::ShowHUDMessage(const char*, const char*, bool)`: pinned CommonLib's `RE/S/SendHUDMessage.h` declares it and `RE/Skyrim.h` includes it; the requested `RE::DebugNotification` is absent from those headers. Notification delivery timing and thread behavior are **NOT_RUN**. A process-wide latch limits notices to one; observed failures must persist across two eligible observations at least one second apart in a loaded world with no active menu or load (HUD, cursor, fader and mist overlays are passive). Menu and load transitions reset the observation. Input observes failures even when startup prevents hook installation; without subsequent update or input the notice can be delayed. F8 is a deliberate provider disable and is not treated as a provider failure. Exact executable/Address Library hash rejection occurs before CommonLib initialization, and SKSE refusal can precede DLL entry: neither can safely issue a HUD notice. If another provider replaces the update hook entirely, input events can still observe the chain; without input, notification may be delayed. A valid `bEnabled=0` launch does not request the TDM lease. Enabling at runtime acquires it first; refusal leaves Camera disabled and does not save preferences. F6 first-person precision retains an existing lease. These paths require runtime verification. The owner accepted overhead ranged aim FAIL and target lock BLOCKED as permanent MVP limitations on 2026-10-28; target lock moves to Phase 5. F6 native first person is the fallback.

## Focused checks

Job plan and complete output: `/srv/storage/Projects/SkyCRPG/session-p4/logs/camera/job-plan-round2.md` and `test-round2.log`. Owner camera worker, timeout 600 seconds per command; completion is exit status, timeout owns cleanup. Linux 7.0.0-38-generic, g++ 15.2.0 portable checks only, binaries outside repo:

```
for src in modules/camera/tests/*_test.cpp; do
  name=$(basename "$src" .cpp)
  timeout 600 g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Imodules/camera/include "$src" -o /srv/storage/Projects/SkyCRPG/session-p4/logs/camera/"$name"
  timeout 600 /srv/storage/Projects/SkyCRPG/session-p4/logs/camera/"$name"
done
timeout 600 python3 modules/camera/tools/check_preset.py --smoothcam-source /srv/storage/Projects/SkyCRPG/session-20261026/targeting-audit/SmoothCam
```

**PASS:** all six suites and preset checker, exit 0. Windows `/W4 /WX /permissive-` compilation **NOT_RUN**; in-game **NOT_RUN**.

## Acceptance ledger

| Criterion | Result |
| --- | --- |
| Independent camera operation and failure/disable fallback | **NOT_RUN**; disabled startup skips TDM lease in code; provider settings persist |
| UI independence / no stranded controls | **NOT_RUN**; source has no UI DLL dependency |
| Camera obstruction, reset, special states and zoom | **NOT_RUN** runtime; bounded earlier results and portable tests above |
| Overhead aim / target lock | **FAIL / BLOCKED — accepted MVP limitation (owner decision 2026-10-28, see DECISIONS.md)**; prior evidence; no new feature |
| Critical crash, capture and gameplay defects | **NOT_RUN** final candidate |

## Runtime route checklist — NOT_RUN

Orchestrator: use isolated camera-only profile and copied save, record provenance and hashes above, bound each launch to 20 minutes, retain process handle/log/deadline and clean up only that process. All rows below **NOT_RUN**. Owner records actual device for each input check; repeat controls separately on keyboard/mouse and controller (controller custom binding only where configured and unmapped).

1. **auto + owner:** Camera-only load copied save; capture startup log, inspect elevated view and native movement; zoom, F7 recenter, F6 first-person and return, native POV and F8 on keyboard/mouse; repeat on controller with mapped native controls and explicitly configured nonconflicting custom inputs. Check no duplicate action and no disabled controls.
2. **auto + owner:** Temporarily opt into obstruction; enter known corridor, confirm active framing, press F8, move/look and use native POV, then restore SmoothCam and zoom. Verify preference is preserved after obstruction and cell/menu transitions; inspect log and visual result. Repeat device-specific F8/zoom checks on both devices where bound.
3. **owner:** Melee, crossbow and concentration spell; use F6 to aim, then compare native interaction range and line of sight at door, loot and small object with and without a wall. Record results per device and whether crossbow/concentration are reported drawn.
4. **owner:** Individually enter/exit mount, kill move, swimming, sitting, bleedout, beast and scripted camera; check native view/control handoff and recovery. Record device and unavailable fixtures.
5. **auto + owner:** Separate launch for each: `bEnabled=0`; SmoothCam absent; SmoothCam wrong version; TDM absent; TDM wrong version; MCM Helper absent; MCM Helper wrong version; unsafe TDM yaw config; unsupported schema; malformed/out-of-range INI. Inspect log and one notice where applicable, move/look/POV and native menus. On the `bEnabled=0` launch verify TDM target lock works natively, then toggle Camera on in game and verify lock is disabled again; distinguish provider settings from Camera policy. Restore baseline after each launch.
6. **auto + owner:** Separate controlled compatible-host fixture for third-person chain not owned by SmoothCam and TDM lease refused or target lock active; inspect one notice, suspension, camera and controls. Use available fixture tooling without replacing pinned providers in the ordinary route.
7. **auto + owner:** Separate unsupported-runtime launch: verify SKSE rejection in its log and observe remaining SmoothCam/TDM behavior; no Camera HUD message expected. Do not treat unavailable host as pass.
8. **auto + owner:** Combined profile: baseline load, move, HUD/compass and menu; Camera `bEnabled=0` and restart on copied save, verify UI remains usable; then disable UI and restart, verify Camera and native compass. Repeat relevant control checks on keyboard/mouse and controller, recording devices. Restore all layers and copied-save profile.

## Owner decisions needed

None for the aim/lock limitation: the owner accepted it on 2026-10-28. This decision is not release or playtest acceptance. Complete runtime routes remain release work.
