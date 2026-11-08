# Spike: vinput under Proton (test tooling)

Question: can upstream [vinput](https://github.com/CorruptedAng3l/llm-game-automation-mcp)
(in-process virtual keyboard/mouse/XInput input and swapchain frame capture, MIT, commit
`ae28e65336b59f73d03e6ecd2f3c434c8fab3ee2`) work inside Skyrim SE 1.7.104.0 under Proton on
the test host, so that routes no longer need the host desktop, uinput devices, window focus
and `xwd`? This is a bounded spike. Nothing here is integrated into routes, shipped, or part
of a Camera/UI package. The DLLs are test-only components and must never appear in a
shipped package or in `modules/**` defaults.

PR: [#39 "Spike: vinput under Proton"](https://github.com/Sonoran-Solutions/SkyCRPG/pull/39)
(draft, do not merge). Build inputs: [tools/vinput](../../../tools/vinput/README.md),
pin in [dependency-lock](../../compatibility/dependency-lock.md#test-tooling-pins-never-shipped),
notices in [THIRD_PARTY](../../THIRD_PARTY.md#test-tooling-never-shipped).

## Summary

| Area | Result |
| --- | --- |
| Pinned upstream build on CI (`build.bat :arch x64`, windows-2022) | **PASS** |
| `SkyCRPG_VInput.dll` compiled-in SKSE host (`/W4 /WX`, no CommonLib) | **PASS** (built first iteration; no injector needed) |
| Host wiring (launcher service, wrapper, path translation, PID discovery) | **PASS** for everything checkable without the game; container-side checks **NOT_RUN** |
| In-game: DLL load, `status`, covered `capture`, `hold w`, `move`, `pad` | **NOT_RUN** (awaiting owner go) |

## CI build and artifact

Source `6df6ed6c5bdd7fc8074afd83e2ee7759fbc41fd2` on `spike/vinput-proton`. Workflow
`vinput.yml`, run [42517110475](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/42517110475),
job `vinput-x64` **PASS** in 2m01s. The job checks the upstream checkout against
`tools/vinput/UPSTREAM.lock`, runs upstream `build.bat :arch x64` unchanged, builds the host
plugin with the standalone CMake project in `tools/vinput/plugin/`, and uploads
`skycrpg-vinput-6df6ed6c5bdd7fc8074afd83e2ee7759fbc41fd2`. `SHA256SUMS` as uploaded and as
verified on the host after download (`sha256sum -c` PASS):

| File | SHA-256 |
| --- | --- |
| `SkyCRPG_VInput.dll` | `475fcbf0beb40408b3e994375b9ea7653d3dfb74c1301010785b33162c901a67` |
| `SkyCRPG_VInput.ini` | `4ce6b60aee591ac173b74ae3e0b00461429c612089ec236a93190ca10ee399fd` |
| `vinput.exe` | `98e50cec35fe9a6cf26ad53999ce6136c0b233d2b30645e7db24291b50c718b0` |
| `vinput64.dll` | `4612e434083988697f8e32de37846897aa911020d1fed0b2cd32cf072a572dea` |
| `vinput_x64.lib` | `3db31a2f1aeda3f1ee768971d77a2a3846216ef873725cfa63abf6ed509d9c84` |
| `UPSTREAM.lock` | `b62f66527062d397ce9f1bd1e8123f4bfc2d00e65684383ffb36bd0de51d1d2c` |
| `LICENSE-vinput.txt` | `abfbd671a8b3bad70021f62bbe06bfc0b84034ad8ff9f0755d86bdf793a16efa` |
| `LICENSE-MinHook.txt` | `4f21f857550d7be854da6ea5f2da4e6775ca4e3fbb535e4f3d961c47d0bf3335` |

The compiled-in route cost one CI iteration, so the fallback (plain `vinput64.dll` plus
`vinput.exe inject --pid`) was not needed; both files are in the artifact anyway.
The full `build.yml` matrix also ran on this PR because `tools/ci_scope.py` and its test
changed (`tools/vinput/` is now classified as tooling that selects no native target).

## Host wiring (no game launch)

Files: `session-p5/vinput/vinput` (wrapper, usable as the MCP server's `VINPUT_EXE`),
`session-p5/vinput/launch_spec.py` (the harness MO2 launch spec plus one variable, print only),
`session-p5/vinput/README.md`. The existing launcher defaults are not edited.

Mechanism, verified by inspecting the installed Steam Linux Runtime and Proton:
`pressure-vessel-wrap` reads `STEAM_COMPAT_LAUNCHER_SERVICE`; the installed Proton
Experimental script (`experimental-11.0-20261027b`) wraps its wine invocation in
`steam-runtime-launcher-interface-0 proton` whenever that tool is on PATH; with the variable
set to `proton`, a `steam-runtime-launcher-service` named `com.steampowered.App489830` runs
inside the game's container with Proton's environment. The wrapper then runs
`steam-runtime-launch-client --bus-name=com.steampowered.App489830 --directory=<dist> -- <Proton>/files/bin/wine Z:\...\dist\vinput.exe <args>`,
adding `--pid` from the plugin's PID file when none is given and translating absolute Linux
paths to `Z:\` paths (captures must live under the shared project root; `/tmp` is private to
the container).

| Check (2026-11-08) | Result |
| --- | --- |
| `steam-runtime-launch-client`, `-launcher-service`, `-launcher-interface-0` present | PASS |
| `pressure-vessel-wrap` references `STEAM_COMPAT_LAUNCHER_SERVICE`; Proton uses the launcher interface | PASS (inspection) |
| `launch_spec.py` reproduces the harness MO2 command; the only delta is `STEAM_COMPAT_LAUNCHER_SERVICE=proton` | PASS (dry print) |
| `vinput --host-check`: binaries, `SHA256SUMS`, dist under a container-shared path | PASS |
| `vinput --print-command` for `list`, `status`, `capture <linux path>` | PASS (correct `Z:\` translation and `--pid` insertion) |
| Bus name present while the game runs | NOT_RUN |
| `vinput.exe list` enumerating `\\.\pipe\*` under Wine | NOT_RUN; believed unsupported in Wine, hence the PID file |
| Second wine process connecting to the game's named pipe | NOT_RUN |
| WIC PNG encoding under Wine (`capture`) | NOT_RUN |

## Game spike

NOT_RUN. Awaiting the owner's go in chat. Plan: ledger entry; disposable profile with
`SkyCRPG_VInput.dll` + `.ini` staged beside `SkyCRPG_TestProbe`; one process; 20 minutes;
restore after. Steps and the evidence each must produce:

| Step | Evidence | Result |
| --- | --- | --- |
| Plugin loads; vinput starts | `SkyCRPG_VInput.log` (`VInput_Init returned 1`), `SkyCRPG_VInput.pid`, `logs/vinput_<pid>.log` hooks line | NOT_RUN |
| `status` | which input channels Skyrim reads (`channels`), `really_foreground`, readiness | NOT_RUN |
| `capture` while another window covers the game | PNG under `session-p5/vinput/captures/`, its hash, `capture_method` | NOT_RUN |
| `hold w 500` | TestProbe `player_state.position` before/after differ | NOT_RUN |
| `move 200 0` | camera yaw changed (TestProbe state or capture pair) | NOT_RUN |
| `pad` right stick, controller off | observed camera/menu reaction, with the caveat below | NOT_RUN |

Caveats that hold regardless of outcome: OCR commands (`ocr`, `click-text`, `wait-text`)
use WinRT `Windows.Media.Ocr` and are expected to fail under Wine; XInput emulation is not
controller proof; the DLLs are test-only and never ship.

## Recommendation

Pending the game spike. The build and wiring side is cheap and now exists; the decision
(adopt as the route input/capture layer, adopt only for interactive steering, or drop)
depends on whether `Present` capture and in-process input work under DXVK and Proton.

## Open risks

- **Present hook on DXVK.** vinput hooks `IDXGISwapChain::Present` through a dummy device
  vtable; under Proton the D3D11/DXGI implementation is DXVK (Vulkan). Captures may be black,
  wrong-format, or absent.
- **Fake focus under XWayland.** vinput fakes `GetForegroundWindow`/activation inside the
  process; Wine's own focus model on XWayland may still route raw input and cursor clipping
  differently than Windows.
- **WIC PNG encoding in Wine.** `capture` encodes through `windowscodecs`; Wine's encoder
  support exists but is less exercised than Windows'.
- **Hook coexistence.** vinput, SmoothCam, TDM and our modules all install hooks in one
  process. Camera-profile runs are the real-world case; a baseline profile isolates vinput.
- **Pipe discovery.** `list` enumerates `\\.\pipe\*`, likely unsupported under Wine; the
  PID file covers it but the MCP server's `list`/`launch` flows would need the wrapper.
- **WinRT imports.** The CLI and DLL link `windowsapp.lib` for OCR; a missing import in
  Wine's `combase` would stop them loading at all, which the first `status` would reveal.
