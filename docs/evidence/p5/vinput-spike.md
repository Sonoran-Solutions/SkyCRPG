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
| Host wiring: launcher service on the bus, wrapper, PID discovery | **PASS** (bus name and PID file confirmed with the game up) |
| In-game: plugin load, vinput start, hooks, pipe | **PASS** (SKSE handle 4; `VInput_Init returned 1`; all hooks ok; pipe created) |
| In-game: game survives to the main menu with vinput hooked | **FAIL** (access violation in `SkyrimSE.exe` ~3 s after the main menu opened) |
| In-game: `status`, covered `capture`, `hold w`, `move`, `pad` | **BLOCKED** by the crash |

**Recommendation: do not adopt vinput as the route input/capture layer in its current
form.** With vinput compiled in and started at plugin load, Skyrim SE 1.7.104 under Proton
crashed on the main thread before any gameplay, in a configuration (same stage without the
vinput DLL) that has run dozens of Phase 5 routes. One further bounded experiment is worth
running before dropping it, see below; interactive steering is not an option either until
the crash is understood, because the crash happens before the first command.

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
| Bus name present while the game runs | **PASS** (spike-2: `com.steampowered.App489830` on the session bus with the game up; Proton log: "Starting program with command-launcher service.") |
| `vinput.exe list` enumerating `\\.\pipe\*` under Wine | BLOCKED (the game crashed before the CLI connected); the PID file alternative worked: `SkyCRPG_VInput.pid` = 348 |
| Second wine process connecting to the game's named pipe | BLOCKED (game crashed first; the launch client then reported the bus name gone) |
| WIC PNG encoding under Wine (`capture`) | NOT_RUN |

## Game spike (2026-11-08, run `vinput-spike-2`)

Owner go relayed verbatim through the orchestrator session: "Steam's up, go". One process,
camera-only profile, stage `session-p5/stage-vinput` (= `stage-camera-p5-off` plus
`SkyCRPG_TestProbe.dll/.ini` and `SkyCRPG_VInput.dll/.ini`, manifest
`session-p5/p5-stage-manifest-vinput.json`), dist `dist-camera-p5-off` (Camera rc1
`6b4c0fc`), copied owner quicksave, `runtime_profile.py prepare`, `runtime_watchdog.py 1200`
with `STEAM_COMPAT_LAUNCHER_SERVICE=proton`, restore after. A first attempt
(`vinput-spike-1`) never launched: this session's permission classifier refused the launch
command; the deployment was restored (PASS) and the user approved a retry. Preflight for
the retry: no Skyrim or loader process, 13.1 GiB available, Steam running, no active
deployment. Ledger: `session-p5/ledger.md`; logs: `session-p5/logs/vinput/spike-2/`.

| Step | Evidence | Result |
| --- | --- | --- |
| Plugin loads | `skse64.log`: `plugin SkyCRPG_VInput.dll (00000001 SkyCRPG VInput (test only) 00000001) loaded correctly (handle 4)`; `SkyCRPG_VInput.log`: runtime 1.7.104.0, SKSE 2.3.1.0, `pid=348`, `VInput_Init returned 1 (started by this plugin)` | **PASS** |
| vinput hooks and pipe | `vinput_348.log` 0.704 s: `hooks ok:` every listed user32, raw-input, DirectInput 8 (`dinput8 keyboard/mouse A/W poll coverage: 15/15`), `wglSwapBuffers`, `IDXGISwapChain::Present/GetFullscreenState/SetFullscreenState`; no `hooks FAILED` line; `pipe \\.\pipe\vinput_348`; 4.486 s: `target window 0000000000030080 class=Skyrim Special Edition client=1920x1080` | **PASS** |
| Launcher service and PID discovery | `vinput --host-check`: bus `com.steampowered.App489830` on the session bus; `SkyCRPG_VInput.pid` read as 348 | **PASS** |
| Game reaches a usable main menu | TestProbe: `data_loaded`, `input_registration` (t=7329 ms), `Main Menu` opening (t=7388 ms, 20:01:36Z); Proton log 253214.022: `Exception 0xc0000005 ... addr=0000000140E128B2`, `info[1]=00000000A38A1118` (read fault), thread 0160 (main thread); backtrace `SkyrimSE.exe+0xE128B2 → +0xCF9624 → +0x658DEE → +0x651465 → +0x15A843E → kernel32 → ntdll`. vinput started at 253203.024, so 11.0 s after init and about 3 s after the main menu opened; no vinput log line after 4.488 s; watchdog `exit_code 0`, `stop_reason null`, `leftover_pids []` | **FAIL** |
| `status` (input channels) | launch client: `Unable to connect to any of the specified bus names` (the game had already exited) | BLOCKED |
| `capture` with another window in front | not reached; no capture file produced | BLOCKED |
| `hold w 500` + TestProbe position | not reached; no `player_state` with `available:true` in the run (2 samples, both `not_loaded`) | BLOCKED |
| `move 200 0` (camera yaw) | not reached | BLOCKED |
| `pad` right stick, controller off | not reached; XInput emulation would not have been controller proof in any case | BLOCKED |
| Restore | `restoration.json` result PASS, `mismatch []`, `game_pids []`; the test DLL's `Data/SKSE/Plugins/logs/vinput_348.log` was preserved to the log folder and removed before restore so `Data/SKSE` disappeared cleanly | **PASS** |

Log hashes (SHA-256): `steam-489830.log` `8dfe2343c03f6de57cf97be00ee92bcbbb855033e5ef139e1c6251c50ffefdd7`;
`vinput_348.log` `bd74724cc50d06367c6306dc8548368665c82f6595af638c218d497ef6203f27`;
`SkyCRPG_VInput.log` `d4fc6703526b08db5fd38830d821392636b5f189ed20b1f54f9984be21657055`;
`skse64.log` `67eb8803aeb9d4afd24641b3b521dd4eda310463e5723b298b512c899e3b3aac`.
Binaries as in the CI table above (`SkyCRPG_VInput.dll` `475fcbf0…`, `SkyrimSE.exe`
`846efccf…`, Proton `experimental-11.0-20261027b`).

What the crash does and does not show. The faulting instruction and the whole backtrace
are inside `SkyrimSE.exe`, reached from the main-thread startup path. vinput logged nothing
between adopting the window (4.5 s) and the crash (11 s), so the fault is not in a vinput
call site; it is consistent with the game consuming a value one of vinput's hooks returned
(raw input, DirectInput 8 device state, XInput state, cursor or focus queries) at the moment
input polling begins with the main menu. It is not consistent with a load failure, with the
TestProbe (identical probe in every Phase 5 route), or with the Camera module (same package
and INI as the passing `off` routes). One run is not a reproduction; the control (same stage
without `SkyCRPG_VInput.dll`) exists only historically in the Phase 5 runs.

Caveats that hold regardless: OCR commands (`ocr`, `click-text`, `wait-text`) use WinRT
`Windows.Media.Ocr` and are expected to fail under Wine; XInput emulation is not controller
proof; the DLLs are test-only and never ship.

## Recommendation

**Drop vinput as the route input/capture layer for now; do not adopt it for interactive
steering either.** It crashed the game before the first command. The build and wiring work
is cheap to keep on the branch for one more bounded experiment, which needs its own owner go:

1. Same stage **without** `SkyCRPG_VInput.dll` as the control (one process, expect the main
   menu and a save load as in every Phase 5 route), then
2. plain `vinput64.dll` injected **after** the save has loaded, through the wrapper
   (`vinput inject --pid 348`-style, from the wrapper with `--json`), so the hooks are
   installed after startup input registration. If the game survives, run `status` and the
   covered `capture`; if it crashes again, drop vinput.
3. If step 2 passes, bisect hook families with upstream's `config raw=off msgs=off`
   options before any route integration.

If the owner does not want to spend two more game processes on this, the recommendation is
to drop it and keep the current uinput, focus and `xwd` harness.

## Open risks (updated)

- **Startup crash with hooks installed at plugin load** (observed). Cause unattributed;
  candidates are the raw-input, DirectInput 8, XInput, cursor and focus hooks hit when the
  main menu starts polling input. This blocks everything else.
- **Present hook on DXVK** (not reached). DXVK's presenter did start (`Presenter: Actual
  swapchain properties`) with vinput's `IDXGISwapChain::Present` hook in place; whether a
  capture would be black, wrong-format or absent is unknown.
- **Fake focus under XWayland** (not reached). vinput adopted the window with
  `actual_fg=0` and queued a "virtual activation"; how Wine routes raw input and cursor
  clipping to a window it believes is in the background is untested.
- **WIC PNG encoding in Wine** (not reached).
- **Hook coexistence with SmoothCam, TDM and our Camera module** (not isolated). The
  crash happened with all of them loaded; the control run above would separate this from
  the Proton question.
- **Pipe discovery.** `list` enumerates `\\.\pipe\*`, probably unsupported under Wine; the
  PID file works, but the MCP server's `list`/`launch` flows would need the wrapper.
- **WinRT imports.** Both the DLL (compiled in) and the CLI link `windowsapp.lib`; the DLL
  loaded, so Wine's `combase` satisfied its imports; the CLI was never exercised.
