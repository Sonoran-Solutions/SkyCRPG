# DYL-173 — rc1 packaged regression

Batch `dyl173-rc1` **22/22 PASS**, unattended keyboard/mouse via `tools/profile.py` deploy, on the exact [rc1 packages](acceptance.md). Host: Ubuntu 26.04.1, RTX 3080, Proton Experimental `experimental-11.0-20261027b-x86_64`, Steam Skyrim SE 1.7.104.0, SKSE 2.3.1; pinned providers in the [runtime matrix](../../compatibility/runtime-matrix.md). Source `6b4c0fc`, CI `41376680950`; executable, package, dependency and settings hashes are in each report. Route generation and variants: `session-p4/logs/autotest/final-dyl173-rc-routes.md` and `session-p4/stage-rc1-overrides.json`. Batch evidence: `session-p4/autotest/batches/dyl173-rc1/summary.{md,json}`. Restores and probe removal verified; runner start/end hashes: `session-p4/logs/autotest/dyl173-rc1-runner-{start,end}.sha256`. Rc `bHeadtrackSpine=0` follows the repository fragment.

| Route | Profile | Coverage (recorded checkpoints) | Result | Evidence path |
| --- | --- | --- | --- | --- |
| dyl173-ui-main | UI-only | protocol; hud; 2.0-hotbar; 4.0-hotbar; 5.0-hotbar; 6.0-hotbar; 7.0-hotbar; 8.6-hotbar; inventory; equip-twohand; equip-body; equip-ring; equip-amulet; dagger-selected; equip-left; drop-stack-empty; drop-before; quantity-one; drop-after; interior; door-exit; exterior; reloaded; hidden; shown | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-00/report.json` |
| dyl173-ui-disabled | UI-only | protocol; variant-hud; inventory; disabled-before; disabled-after | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-01/report.json` |
| dyl173-ui-dll-absent | UI-only | protocol; variant-hud; inventory; disabled-before; disabled-after | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-02/report.json` |
| dyl173-ui-iversion | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-03/report.json` |
| dyl173-ui-resetlayout | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-04/report.json` |
| dyl173-ui-minimap-absent | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-05/report.json` |
| dyl173-ui-no-compass-replace | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-06/report.json` |
| dyl173-ui-aspect-4x3 | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-07/report.json` |
| dyl173-ui-aspect-16x10 | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-08/report.json` |
| dyl173-ui-scale115 | UI-only | protocol; variant-hud; inventory | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-09/report.json` |
| dyl173-cam-main | Camera-only | protocol; 0.2-native_pov; cam-start; first-person; third-person; recenter-state; smoothcam-off; native-pov; recovered | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-10/report.json` |
| dyl173-cam-disabled | Camera-only | protocol; 0.2-native_pov; enabled | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-11/report.json` |
| dyl173-cam-no-smoothcam | Camera-only | protocol; 0.2-native_pov; native-fallback; native-pov | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-12/report.json` |
| dyl173-cam-no-tdm | Camera-only | protocol; 0.2-native_pov; native-fallback; native-pov | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-13/report.json` |
| dyl173-cam-no-mcmhelper | Camera-only | protocol; 0.2-native_pov; native-fallback; native-pov | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-14/report.json` |
| dyl173-cam-malformed-ini | Camera-only | protocol; 0.2-native_pov; cam-start; first-person; third-person; recenter-state; smoothcam-off; native-pov; recovered | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-15/report.json` |
| dyl173-cam-schema | Camera-only | protocol; 0.2-native_pov; cam-start | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-16/report.json` |
| dyl173-cam-obstruction | Camera-only | protocol; 0.2-native_pov; cam-start; first-person; third-person; recenter-state; smoothcam-off; native-pov; recovered | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-17/report.json` |
| dyl173-cam-unsafe-tdm | Camera-only | protocol; 0.2-native_pov; native-yaw | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-18/report.json` |
| dyl173-combined-main | Combined | protocol; 0.3-native_pov; hud; inventory; hidden; shown; 4.0-hotbar; combined-hotbar; combined-camera | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-19/report.json` |
| dyl173-combined-cam-disabled | Combined, Camera disabled | protocol; 0.3-native_pov; combined-ui; inventory; 3.0-hotbar | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-20/report.json` |
| dyl173-combined-ui-disabled | Combined, UI disabled | protocol; 0.2-native_pov; combined-native; inventory; camera-independent | PASS | `session-p4/autotest/runs/batch-dyl173-rc1-21/report.json` |
| dyl173-mo2-removal-combined | Combined, MO2 2.5.2 | Both layers disabled; native HUD/compass, stock inventory, native first/third POV; providers retained, F8 disables SmoothCam | PASS | `session-p4/autotest/runs/batch-dyl171-mo2-rc1c-04/report.json` |

The automated route assertions are bounded and do not imply complete acceptance of every MVP ID, subjective visual review or owner play. Controller automation **NOT_RUN** by host policy; follower lifecycle, marker travel accuracy, broader transitions, manual written-instruction installs, Vortex/other managers, edited-settings migration and separate layer removal remain **NOT_RUN**. Overhead ranged aim remains **FAIL** and target lock **BLOCKED** under the owner limitation in [notes](../../RELEASE_NOTES.md).
