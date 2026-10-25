# Runtime and host matrix

Updated 2026-10-25 (P1-01 / DYL-150). Only **load-only skeletons** have been tested. No SkyCRPG camera or UI behavior has been tested on any runtime. The primary baseline stays provisional until DYL-151/ADR-001 confirms a provider stack for it.

| Stack | Baseline | Camera only | UI only | Combined | Disposition |
| --- | --- | --- | --- | --- | --- |
| **Provisional primary:** Steam `SkyrimSE.exe` 1.7.104.0 (build 24914197), SKSE64 2.3.1, Proton Experimental 11.0-20261014, Ubuntu 26.04.1, RTX 3080 | PASS (load only); route NOT_RUN | PASS (load only) | PASS (load only) | PASS (load only) | Only tested combination. Candidate MVP support baseline, pending DYL-151 provider intersection. |
| Same exe, other Proton versions / native Windows | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | No claim. The skeleton also targets native Windows, but that is untested. |
| Other Steam executables (1.6.x, 1.5.97, later 1.7.x) | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | No claim. The skeletons are pinned to 1.7.104 and SKSE will not load them elsewhere. |
| GOG variant (1.6.1179) | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | Deferred unless chosen as primary. |
| VR, consoles, Legendary Edition | Out of scope | Out of scope | Out of scope | Out of scope | Separate product/porting work. |

## Tested row detail (provisional primary)

| Field | Value |
| --- | --- |
| Executable | `SkyrimSE.exe` 1.7.104.0, sha256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; SE content + four free CC files, no AE Upgrade |
| SKSE | 2.3.1; `skse64_1_7_104.dll` sha256 `d5158f8c61a2257138692940ed27048c40d6ff62676e64791bdc83d4549fb94e` |
| Address Library | Not used (skeletons use no addresses) |
| Providers | None |
| Build source | `ec34e4d717ed6ad145341e6d543255fbd2266fec`, CI run 40246039569 (windows-2022, VS 17.14.42) |
| Packages | Camera zip `14d6afad…738b`, UI zip `ad9696f8…9dc6` (full hashes in [BUILD_AND_TEST.md](../BUILD_AND_TEST.md#p1-01-verification-2026-10-25)) |
| Host | Ubuntu 26.04.1, kernel 7.0.0-38, Wayland/XWayland; Proton Experimental `experimental-11.0-20261014`, DXVK v3.1.1-97, Steam Linux Runtime 4 |
| Graphics | RTX 3080 10 GB, NVIDIA 595.99.02; 1920x1080 windowed; game default preset |
| Input | Keyboard/mouse. No controller available (controller cases NOT_RUN). |
| Evidence | [evidence/p1/environment.md](../evidence/p1/environment.md#p1-01-load-evidence) |

Each tested row must include executable version, storefront, SKSE, Address Library, every selected provider version, build source commit, package hashes, host/Proton, graphics configuration, controller and links to evidence.

A load PASS means only that SKSE accepted and loaded the plugin and the game reached data load. Native camera or UI fallbacks for difficult or special states are expected behavior. An unsupported state must not be labeled fully supported just because the game did not crash.
