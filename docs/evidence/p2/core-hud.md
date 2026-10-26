# DYL-160 — core CRPG HUD evidence

**Status: Scaleform asset compiled; native build pending; runtime acceptance NOT_RUN.** Recorded 2026-10-26 on branch `phase2/dyl-160-hud`, source commit `8183897570ae6bf179457049227dd400580949dd` (parent's shared dependency/build commit: `e7f141f6b92b1305695d4f744c48ab438e8e0803`). This report distinguishes source/bytecode evidence from gameplay evidence. No Skyrim process was launched or deployed by this worker. Compact, reviewable provenance is in [core-hud provenance](core-hud/README.md).

## Implementation

- HUD data comes from the pinned SkyUI `HUDMenu`'s engine-fed `SetHealthMeterPercent`, `SetMagickaMeterPercent` and `SetStaminaMeterPercent` callbacks. The source patch marks receipt of each callback and waits for all three values before displaying replacement meters. It applies the same `HealthPenaltyPercent`, `MagickaPenaltyPercent` and `StaminaPenaltyPercent` adjustment as the native meters.
- The charcoal/brass player panel anchors at bottom center with health, magicka and stamina percentages. Native resource groups are hidden only after current native data is valid and the current HUD mode allows the replacement. Missing bridge/data and invalid values restore the native meters according to the native HUD mode. The native crosshair and native rollover/activation text stay in control.
- An upper-left reserved party panel explicitly says “Follower data unavailable”; it invents no follower data. The selected map provider remains in its existing position and the native compass remains available. No map renderer, hotbar, follower adapter or Camera dependency was added.
- New clips join SkyUI's existing `HudElements`/`ShowElements` mode contract. Reinitialization removes prior clips and frame callbacks and restores native resource visibility before rebuilding. The panel follows the HUD menu's visibility lifecycle.
- UI settings are versioned in `Data/SKSE/Plugins/SkyCRPG_UI.ini`. `[Settings] iVersion=1`; `[HUD] bEnabled=1`, `iScalePercent=100`, `iOffsetX=0`, `iOffsetY=0`. Scale clamps to 85–115%; X offset to −120..120 px; Y offset to −60..60 px. Position is bounded to the visible viewport. Missing/invalid version resets all values to defaults; malformed fields use safe defaults; out-of-range values clamp. Reset by restoring the defaults and restarting. No in-game hotkey or extra preference store is introduced.
- The UI DLL registers a read-only `skse.plugins.SkyCRPGUI` settings object through the pinned SKSE Scaleform interface. The module remains usable if Scaleform registration is unavailable: the DLL loads, while the SWF leaves native meters and prompt untouched. CommonLib is used only for the pinned Scaleform/GFx wrapper; the shared skeleton ABI was not extended. Plugin metadata no longer claims `NoStructUse`.
- Before any CommonLib initialization, the DLL now checks `Data/SKSE/Plugins/versionlib-1-7-104-0.bin` against the exact Address Library 13 / runtime 1.7.104 SHA-256 in the repository dependency lock. Missing, unreadable, hashing-unavailable or mismatched files make `SKSEPlugin_Load` return false before `SKSE::Init`, leaving the native HUD in place. This is a bounded prerequisite guard, not an Address Library parser or compatibility probe. Focused source and digest-pin tests pass; Windows DLL build remains pending.

## Focused validation

| Check | Result | Evidence and limit |
| --- | --- | --- |
| Versioned config defaults, malformed input, version fallback, clamping | **PASS** | C++ parser test `modules/ui/tests/hud_settings_test.cpp`, compiled with `g++ -std=c++20 -Wall -Wextra -Werror` and executed successfully. This tests the parser, not Windows profile-file I/O. |
| Scaleform/source contracts, exact pinned SkyUI transforms and Address Library guard order | **PASS** | `SKYCRPG_SKYUI_SOURCE=/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11 python3 -m unittest discover -s modules/ui/tests -p test_scaleform_contract.py -v`: 10 tests passed, no skips. It verifies the guard precedes `SKSE::Init` and pins the exact filename and payload digest. The source checkout is SkyUI 6.11 commit `3d537a366f1a45c14a7012ea0aa00be18108f073`; source hashes are in the candidate `build.json`. |
| Address Library digest pin and settings parser | **PASS** | `g++ -std=c++20 -Wall -Wextra -Werror modules/ui/tests/hud_settings_test.cpp -o /srv/storage/Projects/SkyCRPG/session-p2-20261026/validation/hud_settings_test && /srv/storage/Projects/SkyCRPG/session-p2-20261026/validation/hud_settings_test`: compiled and exited 0. The fixture checks the exact 1.7.104 file name/hash and rejects the archive hash and empty digest. This is a portable pin/parser test; Windows BCrypt file hashing is covered by source review and awaits native build. |
| HUD data refresh, native fallback, mode visibility, teardown and layout bounds | **PASS (mocked Scaleform harness)** | `/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node modules/ui/tests/hud_contract.js modules/ui /srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11`. The harness executes the actual HUD initialization snippet and extracts the pinned `CheckAgainstHudMode`, `ShowElements` and three meter setter bodies. It checks callback receipt before replacement, penalty math, data fallback, `All`/`Favor` visibility, handler cleanup, and position/size bounds at 1280×720 and 1920×1080. It does not execute Scaleform in Skyrim.
| Changed HUD asset build | **PASS — COMPILED_NOT_RUN** | FFDec 26.0.0, Java `17.0.21+9`, 512 MiB heap, one HUD target, timeout 600 s; PID 130895; elapsed 0.765 s; exit 0. Exact manifest: `/srv/storage/Projects/SkyCRPG/session-p2-20261026/ui-hud-candidate-20261026c/build.json`; compiler log: `/srv/storage/Projects/SkyCRPG/session-p2-20261026/ui-hud-candidate-20261026c/hudmenu.swf.log`. Only `hudmenu.swf` was built; unchanged inventory SWFs were not rebuilt. |
| Compiled HUD source inspection | **PASS (static bytecode export)** | FFDec export exited 0 in 0.89 s. Exported `/srv/storage/Projects/SkyCRPG/session-p2-20261026/ui-hud-candidate-20261026c/export/hud/scripts/__Packages/HUDMenu.as` contains all three callback receipt flags, settings-based HUD initialization after `Stage.scaleMode="showAll"`, and the injected post-`ShowElements` mode synchronization. Export inspection is not an engine/runtime test. |
| Diff whitespace | **PASS** | `git diff --check` passed before commit. |
| Native Windows/CommonLib plugin build | **NOT_RUN** | Parent owns the UI-only Windows build workflow; commit `8183897` is ready for that build. Use its CI result before promoting native binary build status. |
| 1080p in-game layout and live UI-only/combined gameplay | **NOT_RUN** | The harness verifies bounds only. No game profile was deployed or launched here; owner observation and runtime captures remain required. |

### Scaleform candidate identity

| Artifact | SHA-256 |
| --- | --- |
| Acquired Nexus SkyUI 6.11 base `hudmenu.swf` | `e8277c1fedcda3ac8c159441f4e7c55b62fbe564ec3cc1bd602b70427f0a47cf` |
| Generated `hudmenu.swf` | `bef13e3819082612669ae6bcbc7a2e5cd9d0898bef7c72974f0055666e7715b6` |
| HUD patch `hud-init.as.inc` | `544adbe7bacc7688d0238fc54de85ace41f469cd22bba3dbf9d25019377539bb` |
| FFDec-exported `HUDMenu.as` | `257c0cfc1a914bd6a3971dc34d186022c8f14bbc896044c87a755811a802abff` |

The output is kept outside Git at `/srv/storage/Projects/SkyCRPG/session-p2-20261026/ui-hud-candidate-20261026c/Data/Interface/hudmenu.swf`. The build uses the immutable acquired Nexus BSA interface, pinned SkyUI source checkout and its FFDec JAR. No proprietary SWF, external source dump, image asset or game file is committed. The output is an internal compile candidate and is not a public redistribution artifact.

## Acceptance disposition

Code-level data, fallback, layout bounds and native HUD mode ownership have focused PASS evidence. DYL-160's UI-only and combined real-time gameplay criteria, actual 1080p visual check, Windows native build, prompt appearance, profile packaging and owner acceptance remain **NOT_RUN**. Compilation and mocked source behavior do not close those gates. No runtime or public-release claim is made.
