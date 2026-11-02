# DYL-170 — Camera blocker resolution

**Provenance:** base `c7333c7c13284ad9d228e6f15213bc8465bd6e19`; runtime-tested head `1d95915` (Windows CI [run 40773296811](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40773296811), DLL `73a8ff6874a284037c14c2e6bc21635dc0237bf192cf0c349d56a60be6a08080`, package `SkyCRPG_Camera-0.0.1-1d95915.zip` `fd4d02369e61ca11d2b6bd15658a63f420794d85bfa73db8a5117d6102d66db8`); merged head `1daf2aa` (#27) adds the notice-intent fix (Windows CI [run 41371612285](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41371612285) PASS, DLL `1eae1696cc5597613bfd11a23a0df43c0a63e6b809de05d23633bb88301b620c`, package `SkyCRPG_Camera-0.0.1-1daf2aa.zip` `d59ed7417dc4041d6f43f0c103342f1c3d75a9cea75f390edd68a19a207f8736`). Planned runtime: Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13, CommonLibSSE-NG 7.2.0, SmoothCam 1.8.1, TDM 2.3.1, MCM Helper 1.6.3, SkyUI Community 6.11, Proton host. Record exact host, executable, dependency and profile hashes for every eventual run. No game or Windows build was run by this worker.

## Audit

| Item | Existing coverage and change | Status |
| --- | --- | --- |
| Disable/restart | `select` gates framing on `enabled`; F6 and POV fallback, no control-disabling calls. Prior [controls](../p2/camera-controls.md) and [lifecycle](../p2/lifecycle.md) are bounded; restart not established. Disabled startup skips the TDM lock-disable lease; runtime enable requests it before enabling, while F6 precision retains it. Provider override layers remain. | **NOT_RUN**; full independent recovery open |
| Provider failure | Exact hash gate, TDM API and lease checks, chain and ownership guard exist. Failure logs and requests a single in-game HUD notice when failure persists during eligible gameplay while the user intends Camera on, including after a refused runtime enable. Input observes a replaced update hook; test input and chain failure. UI is not referenced. | **NOT_RUN** |
| Wrong runtime | SKSE version declaration rejects unsupported runtime before plugin logic. No in-game notice or Camera log can be promised; installed SmoothCam/TDM settings continue independently. | **NOT_RUN** |
| INI | Existing validation bounds values and invalid schema disables; now malformed/nonfinite numbers and bad flags default, changed numeric bounds/bindings and schema log. Missing keys silently default. | **NOT_RUN** in game; portable validation tests PASS |
| Active obstruction F8 and zoom | Existing `resetZoomRecovery` tests four transitions including F8/provider and `resetObstructionZoom` preservation, plus closed-loop tests; [obstruction](../p2/obstruction.md) says F8 and `28aae27` game checks remain open. | **PASS** portable; **NOT_RUN** in game |
| Crossbow/concentration | Existing code uses `IsWeaponDrawn` to suppress automatic obstruction, not equipment classification. Added test of that policy gate. Whether engine reports these states as drawn and whether combat works remains unverified. [Combat](../p2/combat-and-activation.md). | **PASS** portable; **NOT_RUN** in game |
| Range/LOS | No Camera activation, crosshair pick, range or LOS mutation found; Skyrim remains authoritative. [Combat](../p2/combat-and-activation.md) does not establish range/LOS. | **NOT_RUN** in game |
| Special states | `specialHazard` has individual mount, kill move, swim, furniture/sit, bleedout, beast and scripted portable assertions; lifecycle suspends custom framing. [Lifecycle](../p2/lifecycle.md) is bounded, not each state. | **PASS** portable; **NOT_RUN** each in game |
| Aim/lock | [Phase 3 gate](../p3/gate.md): overhead ranged aim **FAIL**, target lock **BLOCKED**; F6 native first person demonstrated and accepted for Phase 2. [Phase 5 proposal PR #24](https://github.com/Sonoran-Solutions/SkyCRPG/pull/24) is a draft, not an implementation or owner decision. | **FAIL / BLOCKED — accepted MVP limitation (owner decision 2026-10-28, see DECISIONS.md)** |

## Design and limitations

No new camera owner or cross-module dependency. Engine notification uses `RE::SendHUDMessage::ShowHUDMessage(const char*, const char*, bool)`: pinned CommonLib's `RE/S/SendHUDMessage.h` declares it and `RE/Skyrim.h` includes it; the requested `RE::DebugNotification` is absent from those headers. Notification delivery timing and thread behavior are **NOT_RUN**. A process-wide latch limits notices to one; while the user intends Camera on, observed failures must persist across two eligible observations at least one second apart in a loaded world with no active menu or load (HUD, cursor, fader and mist overlays are passive). Intent starts from `bEnabled` and a runtime enable request sets it before the lease attempt, including when refused; F6 first-person precision does not change it. Menu and load transitions reset the observation. Input observes failures even when startup prevents hook installation; without subsequent update or input the notice can be delayed. F8 is a deliberate provider disable and is not treated as a provider failure. Exact executable/Address Library hash rejection occurs before CommonLib initialization, and SKSE refusal can precede DLL entry: neither can safely issue a HUD notice. If another provider replaces the update hook entirely, input events can still observe the chain; without input, notification may be delayed. A valid `bEnabled=0` launch does not request the TDM lease. Enabling at runtime acquires it first; refusal leaves Camera disabled and does not save preferences. F6 first-person precision retains an existing lease. These paths require runtime verification. The owner accepted overhead ranged aim FAIL and target lock BLOCKED as permanent MVP limitations on 2026-10-28; target lock moves to Phase 5. F6 native first person is the fallback.

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

## DYL-170 runtime result (2026-11-01)

Scoped automated runtime **PASS** on Proton Experimental `experimental-11.0-20261027b-x86_64`, Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, test probe `ac28d3c` (#30), Camera `1d95915`. Restore passed and the probe was absent after every route. Runs are kept in the session autotest tree (`session-p4/autotest/runs/`).

| Route | Run | Result |
| --- | --- | --- |
| cam-main, cam-disabled, cam-no-smoothcam, cam-no-tdm, cam-no-mcmhelper, cam-malformed-ini, cam-schema, cam-obstruction, cam-unsafe-tdm | `batch-dyl170-runtime-p20261027b-10`..`18` | 2/2 sections each: load and camera controls (F6 first-person claim via `precision_trace event=11 camera=9/0`, F7 recenter, or the expected refusal/notice for the variant) |
| combined-main, combined-cam-disabled, combined-ui-disabled | `-19`, `batch-dyl170-last-two-01`, `-21` | 5/5, 4/4, 4/4: Camera and UI independence |

The owner approved all review screenshots on 2026-11-01. `1daf2aa` (senior review P2) makes the failure notice follow the user's enable intent, so a refused runtime enable after a `bEnabled=0` launch warns once; it is covered by portable tests and Windows CI, and its in-game delivery is **NOT_RUN**. Still **NOT_RUN**: controller play, combat and activation, special native states (mount, kill move, swimming, sitting) and the lease-refusal fixture of item 6. Overhead aim FAIL and target lock BLOCKED remain accepted MVP limitations, not passes. The acceptance ledger above records the pre-run audit.

## Runtime route checklist (pre-run plan)

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
