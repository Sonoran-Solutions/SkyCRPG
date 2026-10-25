# P1 environment record (DYL-150 / P1-01)

Captured: 2026-10-25 by read-only inspection of the owner's workstation (sections Game through Build toolchain). The [load evidence](#p1-01-load-evidence) section below records the P1-01 runtime results; procedures are in [BUILD_AND_TEST.md](../../BUILD_AND_TEST.md) and tested combinations in [runtime-matrix.md](../../compatibility/runtime-matrix.md).

Labels: **OBSERVED** = read directly from files/commands on the host. **REPORTED** = from an upstream page, not verified locally. **ASSUMED** = working assumption awaiting evidence.

Credentials, Steam account identifiers, depot keys, save character names and other personal data are deliberately excluded.

## Game

| Field | Value | Basis |
| --- | --- | --- |
| Product / storefront | The Elder Scrolls V: Skyrim Special Edition, Steam app 489830 | OBSERVED (`appmanifest_489830.acf`) |
| Steam build ID | 24914197 (`StateFlags 4` = fully installed, auto-update behavior 0 = default) | OBSERVED |
| `SkyrimSE.exe` file version | **1.7.104.0** (PE timestamp 2026-08-24 16:24:38 UTC) | OBSERVED (VS_FIXEDFILEINFO) |
| `SkyrimSE.exe` SHA-256 | `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` | OBSERVED |
| `SkyrimSELauncher.exe` SHA-256 | `9204f00ba4e0093f00fd461883f171f1b98f7d7de27969ff5e41ba3608ae271b` | OBSERVED |
| Install path | `~/.local/share/Steam/steamapps/common/Skyrim Special Edition` (15 GB, installed 2026-10-22) | OBSERVED |
| Content | Base game + Dawnguard/Hearthfires/Dragonborn + the four free Creation Club files (Fish, Survival Mode, Curios, Adv. Dwemer Spheres) + `_ResourcePack`. **No Anniversary Upgrade content installed.** | OBSERVED (`Data/`) |
| Launch history on this host | Never launched: `LastPlayed 0`, no `Skyrim.ini`/`plugins.txt` generated in the Proton prefix | OBSERVED |
| SKSE | Not installed at capture. Owner downloaded SKSE64 2.3.1 from Nexus (archive sha256 `7bad616ed360823a027f8828801d91e3a4aa2eacd952023af7f3e31adb2ae250`). It is deployed only while a test profile is active and removed afterwards. | OBSERVED |
| Address Library | **Not installed** | OBSERVED |
| Other mods | None (vanilla `Data/`) | OBSERVED |
| Display settings (`Skyrim/SkyrimPrefs.ini` template) | 1920x1080, windowed (`bFull Screen=0`, `bBorderless=0`) | OBSERVED |

Upstream support for this executable (REPORTED, 2026-10-25): the official [SKSE page](https://skse.silverlock.org/) lists **SKSE64 2.3.1 for runtime 1.7.104** (Nexus download) and GOG 2.2.6 for 1.6.1179. SKSE's `skse_version.h` defines `RUNTIME_VERSION_1_7_104 = 0x01070680` and packs 2.3.1 as `0x02030010`; its `PluginAPI.h` adds `kVersionIndependentEx_AddressLibraryV5`, i.e. a changed Address Library format for the 1.7 line. The [Address Library Nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files) lists a 1.7.104.0 file; third-party sources disagree on its version number and report that some older SKSE plugins broke on 1.7.99/1.7.104. These are claims for DYL-151 to verify, not SkyCRPG evidence.

## Host

| Field | Value | Basis |
| --- | --- | --- |
| OS | Ubuntu 26.04.1 LTS, kernel 7.0.0-38-generic, Wayland session (XWayland `:0` present) | OBSERVED |
| CPU / RAM | 16 logical CPUs (Intel Rocket Lake), 30 GiB RAM (heavily used by other workloads at capture time) | OBSERVED |
| GPU | NVIDIA GeForce RTX 3080 10 GB, driver 595.99.02 (Intel UHD 750 iGPU also present) | OBSERVED |
| Displays | HDMI 2560x1440 @ 143.9 Hz (primary), DP 1920x1080 @ 60 Hz | OBSERVED |
| Baseline test resolution | 1920x1080 windowed (the game's own default); 1280x720 for the later layout matrix | ASSUMED (proposal) |
| Steam client | Native `/usr/bin/steam`, data at `~/.local/share/Steam`, running at capture time | OBSERVED |
| Proton | Only **Proton - Experimental** installed (`experimental-11.0-20261014`); app prefix `compatdata/489830` stamped `11.0-100`. No per-app compat-tool override found, so the default (Experimental) applies. | OBSERVED |
| Input devices | Logitech K400 Plus keyboard (integrated touchpad) and a Logitech USB-receiver mouse. **No game controller detected** (no `/dev/input/js*`, no gamepad USB IDs). | OBSERVED |
| Free disk | `/` 752 GB free; `/srv/storage` 831 GB free | OBSERVED |

## Mod manager and profiles

| Field | Value | Basis |
| --- | --- | --- |
| Mod manager | **None installed** (no Mod Organizer 2, Vortex or Limo found) | OBSERVED |
| Existing owner profiles | None; the install is vanilla and unmodded | OBSERVED |
| Saves | 135 Steam Cloud `.ess` saves dated 2021-06/07 (954 MB) in the Proton prefix `Documents/My Games/Skyrim Special Edition/Saves`. They predate this executable; whether they load on 1.7.104 is unverified. | OBSERVED / unverified |
| Safe isolated profile location | Outside the repo and outside Steam: `/srv/storage/Projects/SkyCRPG/profiles/` (staged files, copied saves, logs). Only reversible, manifest-tracked files are deployed into the game directory. See [BUILD_AND_TEST.md](../../BUILD_AND_TEST.md). | Proposal |

## Build toolchain

| Field | Value | Basis |
| --- | --- | --- |
| Local Windows-capable compiler | **None**: no MSVC, clang-cl/lld-link, MinGW, Wine, xwin or CMake on the host | OBSERVED |
| Available locally | Python 3.14.4, Docker 29.8.2, `zip`, `gh` (authenticated) | OBSERVED |
| Build runner | GitHub Actions on the public repository (Actions enabled, no self-hosted runners). GitHub-hosted Windows Server 2022 x64 runner with MSVC + CMake is the Windows x64 build path. | OBSERVED (repo settings); runner image contents REPORTED until the first CI run |

## Immediate blockers and risks

1. ~~SKSE 2.3.1 not installed~~. Resolved: the owner supplied the 2.3.1 archive, and `tools/profile.py` deploys it per profile. SKSE's files are additive; nothing in the base game is overwritten.
2. **1.7.104 is a new runtime line.** CommonLib forks, Address Library v5-format support and candidate providers (TDM, SmoothCam, SkyUI/minimap stacks) may not yet support it. DYL-151 must check the exact runtime intersection; the SkyCRPG skeletons deliberately avoid CommonLib and Address Library so they do not prejudge that choice.
3. **No local game controller.** Controller acceptance (U6) cannot be run on this host until one is attached.
4. **No mod manager.** Isolated profiles are implemented with a small manifest-tracked deploy script instead of MO2. Adopting MO2 under Proton later is possible but needs its own install/approval.
5. **Saves are old (2021).** Copied saves may fail on 1.7.104; a fresh new-game fixture save may be needed for the route.
6. **Shared workstation.** The GPU/display and RAM are also used by other workloads; game jobs need exclusive access and one Skyrim process at a time.

## P1-01 load evidence

Run 2026-10-25 on this host with owner approval. Each launch was automated with `tools/profile.py launch --until-loaded --timeout 300 --settle 30` and stopped by the tool, and each left no remaining processes. Common to every run:

- Executable: `SkyrimSE.exe` 1.7.104.0, sha256 `846efccf…1402f`.
- SKSE: 64 2.3.1. Loader sha256 `cde6ae26e7340a59c72fc113e7e5534087db5a86b7cb833876d2d1d84a39dd59`; `skse64_1_7_104.dll` sha256 `d5158f8c61a2257138692940ed27048c40d6ff62676e64791bdc83d4549fb94e`.
- Proton: Experimental `experimental-11.0-20261014`, DXVK v3.1.1-97, Steam Linux Runtime 4.
- Display and input: 1920x1080 windowed, keyboard/mouse.
- Packages: from commit `ec34e4d717ed6ad145341e6d543255fbd2266fec` ([CI run 40246039569](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40246039569)). Hashes are in [BUILD_AND_TEST.md](../../BUILD_AND_TEST.md#p1-01-verification-2026-10-25).

| Profile | Start (local, -0700) | Result | `skse64.log` sha256 | Module log sha256 |
| --- | --- | --- | --- | --- |
| baseline | 16:46:40 | PASS: SKSE init, 0 plugins, data loaded (msg 8) | `548c3305…27e5b` | none |
| camera-only | 16:47:28 | PASS: `SkyCRPG_Camera.dll` loaded correctly (handle 1) | `9e54361e…30f25` | Camera `19913f1d…33e65` |
| ui-only | 16:48:19 | PASS: `SkyCRPG_UI.dll` loaded correctly (handle 1) | `9c7c12df…946e7` | UI `0120f18b…f2ef` |
| combined | 16:49:04 | PASS: both loaded correctly (handles 1, 2) | `0b2e2a4c…8c6` | Camera `c4d76580…8d00`, UI `08755f49…c3fa` |

Excerpts, with home path shortened to `~`:

```text
# baseline skse64.log
SKSE64 runtime: initialize (version = 2.3.1 01070680 01DD64DB16681E69, os = 6.2 (9200))
scanning plugin directory Z:~\...\Skyrim Special Edition\Data\SKSE\Plugins\
init complete
dispatch message (8) to plugin listeners

# camera-only skse64.log
checking plugin SkyCRPG_Camera.dll
plugin SkyCRPG_Camera.dll (00000001 SkyCRPG Camera 00000010) loaded correctly (handle 1)

# camera-only SkyCRPG_Camera.log
SkyCRPG Camera 0.0.1 loaded at 2026-10-25T23:47:30Z
source commit: ec34e4d717ed6ad145341e6d543255fbd2266fec
skse: 2.3.1.0 (0x02030010)
runtime: 1.7.104.0 (0x01070680)
settings: Z:~\...\Data\SKSE\Plugins\SkyCRPG_Camera.ini (found, iLogLevel=1)
runtime check: PASS pinned 1.7.104

# ui-only skse64.log (no Camera plugin scanned)
checking plugin SkyCRPG_UI.dll
plugin SkyCRPG_UI.dll (00000001 SkyCRPG UI 00000010) loaded correctly (handle 1)

# combined skse64.log
plugin SkyCRPG_Camera.dll (00000001 SkyCRPG Camera 00000010) loaded correctly (handle 1)
plugin SkyCRPG_UI.dll (00000001 SkyCRPG UI 00000010) loaded correctly (handle 2)
```

The full logs, `run.json` files and Proton logs stay in the profile staging directory `/srv/storage/Projects/SkyCRPG/profiles/<profile>/runs/` and are not committed, because Proton logs contain local paths.

Not established by these runs: whether any save loads; camera, HUD or inventory behavior; performance; controller input; any provider or Address Library compatibility. The baseline route is NOT_RUN and will be played by the owner (see [BUILD_AND_TEST.md](../../BUILD_AND_TEST.md#baseline-route-owner-not_run)).
