# Third-party credits and notices

Internal candidates only. See [dependency lock](compatibility/dependency-lock.md) for exact providers and [licensing scope](LICENSING.md) for original grants and exclusions. Provider archives are external; the UI candidate contains modified SkyUI movies and Camera contains configuration overrides. Neither candidate contains full provider archives.

| Component | Credit and use |
| --- | --- |
| SkyUI Community 6.11 | SkyUI Team, including snakster and Mardoxx, and SkyUI 6.x Community contributors including doodlum and zndxcvbn. The three patched SWFs derive from hash-checked SkyUI movies; base assets and embedded fonts/art are not relicensed. Public distribution and HUD asset provenance remain for owner review. |
| SmoothCam 1.8.1 | mwilsnd; external camera provider; candidate includes framing config and preset. |
| True Directional Movement 2.3.1 | Ersh; external movement provider; candidate includes merged MCM settings. |
| MCM Helper 1.6.3 | Parapets; external prerequisite. |
| Address Library 13 | meh321 and contributors; external prerequisite. |
| SKSE64 2.3.1 | Skyrim Script Extender team; external prerequisite. |
| Infinity UI NG 2.1.3, Local Map Upgrade settings fork 1.3.4, Dragon's Eye Minimap settings fork 1.7.5 | Respective authors and contributors; optional external minimap stack, see dependency lock for provenance. |
| CommonLibSSE-NG 7.2.0 | CommonLibSSE-NG contributors and legacy CommonLibSSE author Ryan-rsm-McKenzie. GPL text, exceptions and legacy MIT notice accompany each candidate. |
| spdlog 1.16.0, fmt 12.1.0, rapidcsv 8.90, directxtk 2025-10-27, directxmath 2025-04-03 | Respective authors and contributors; licence names are per the shipped copyright files in `Docs/<module>/third-party/`. These notices are included by native packaging; verification on the next CI build is pending. See [docs review](evidence/p4/docs-review.md). |

## Draft corresponding-source statement — owner review required

Each DLL is a combined work containing statically linked CommonLibSSE-NG, conveyed under GPL-3.0-or-later terms subject to its exceptions. Original SkyCRPG source remains MIT. For a candidate, provide the exact SkyCRPG source at `<candidate source URL and full SHA>` together with [CommonLibSSE-NG at its pinned commit](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), build instructions and the pinned vcpkg baseline in [build inputs](../cmake/README.md), plus any other required corresponding source. Make this material available with the candidate and verify completeness before distribution. This draft is not a legal determination or public distribution approval. CommonLib notices are in [LICENSES](../LICENSES/); The static dependency notice packaging change awaits verification on the next CI build.
