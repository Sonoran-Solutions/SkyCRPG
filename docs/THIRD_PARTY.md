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

Each DLL is a combined work containing statically linked CommonLibSSE-NG, conveyed under GPL-3.0-or-later terms subject to its exceptions. Original SkyCRPG source remains MIT. For a candidate, provide the exact SkyCRPG source at `<candidate source URL and full SHA>` together with [CommonLibSSE-NG at its pinned commit](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), build instructions and the pinned vcpkg baseline in [build inputs](../cmake/README.md), plus any other required corresponding source. Make this material available with the candidate and verify completeness before distribution. This draft is not a legal determination or public distribution approval. CommonLib notices are in [LICENSES](../LICENSES/); The rc1 manifests verify the static dependency notices; see [packaging](evidence/p4/packaging.md).

Public redistribution and third-party rights clearance are deferred until at least Phase 5 completion by [D023](DECISIONS.md). Phase 4 candidates remain internal.

## Test tooling (never shipped)

The following components are built on CI as **internal test inputs** for the route harness
(see [tools/vinput/README.md](../tools/vinput/README.md)). They are installed only into
disposable test profiles, are removed by profile restoration, and must never appear in a
Camera/UI package, manifest or shipped configuration. Their notices are recorded here
because the CI artifact redistributes their binaries internally.

### vinput (LLM Game Automation MCP) — MIT

Source: <https://github.com/CorruptedAng3l/llm-game-automation-mcp>, pinned at commit
`ae28e65336b59f73d03e6ecd2f3c434c8fab3ee2` (2026-11-08) in
[tools/vinput/UPSTREAM.lock](../tools/vinput/UPSTREAM.lock). Copyright (c) 2026 CorruptedAngel.
Licensed under the MIT License (upstream `LICENSE`); the notice below applies to
`vinput.exe`, `vinput64.dll`, `vinput_x64.lib` and to `SkyCRPG_VInput.dll`, which links
`vinput_x64.lib`. Upstream's `THIRD_PARTY_NOTICES.md` also lists Vulkan-Headers
(Apache-2.0 OR MIT, header-only, no runtime material in our artifact).

```
MIT License

Copyright (c) 2026 CorruptedAngel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### MinHook and Hacker Disassembler Engine — BSD-2-Clause

vinput vendors MinHook (commit `8af6b4acae5a9388fd742b56fa79ece89d96f823`, symbols renamed
`VIMH_*`) and compiles it into `vinput64.dll` and `vinput_x64.lib`, so `SkyCRPG_VInput.dll`
contains it too. Clause 2 requires this notice alongside any binary redistribution; the CI
artifact therefore carries upstream `third_party/minhook/LICENSE.txt` as `LICENSE-MinHook.txt`.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C / Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
