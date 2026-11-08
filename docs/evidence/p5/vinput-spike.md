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

**Recommendation: do not adopt vinput in its current form; one more bounded experiment is
justified before dropping it, if the owner approves it.** With vinput compiled in and
started at plugin load, Skyrim SE 1.7.104 under Proton crashed on the main thread inside its
DirectInput buffered-keyboard read, about 3 s after the main menu opened, in a configuration
(same stage without the vinput DLL) that has run dozens of Phase 5 routes. The crash
analysis below points at the DirectInput `GetDeviceData` interposition or the activation
refresh that triggers the first poll; a single run with vinput's faking switched off until
after the save loads would tell them apart.

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
| Game reaches a usable main menu | TestProbe: `data_loaded`, `input_registration` (t=7329 ms), `Main Menu` opening (t=7388 ms, 20:01:36Z); Proton log 253214.022: `Exception 0xc0000005 ... addr=0000000140E128B2`, `info[1]=00000000A38A1118` (read fault), thread 0160 (main thread); backtrace `SkyrimSE.exe+0xE128B2 → +0xCF9624 → +0x658DEE → +0x651465 → +0x15A843E → kernel32 → ntdll`. vinput started at 253203.024, so 11.0 s after init and about 3 s after the main menu opened; no vinput log line after 4.488 s; watchdog `exit_code 0`, `stop_reason null`, `leftover_pids []`. Fault is inside Skyrim's DirectInput buffered-keyboard read; see the crash analysis | **FAIL** |
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

### Crash analysis (orchestrator disassembly, checked here against the executable and the trace)

Source: the Wine `seh` trace at `steam-489830.log` line 9478 and the on-disk `SkyrimSE.exe`
1.7.104 (`846efccf…`). Items marked *verified* were re-derived in this session with
`objdump` on the executable's bytes and the logged registers; the rest is the orchestrator's
reading of the surrounding disassembly.

- *Verified.* The faulting instruction at `0x140E128B2` is `movzx r8d, byte ptr [r14+rbp]`,
  immediately after `call qword ptr [0x1417C88F8]`, and that import is
  `USER32.dll!MapVirtualKeyA`. Just before it, `r14d` is loaded from `[rbx + rcx*24 + 0x78]`
  and `esi` from `[rbx + rcx*24 + 0x7C]` with `ecx = r15d`: a loop over 24-byte records
  starting at `this+0x78`, reading each record's `dwOfs` and `dwData`.
- *Verified.* Registers at the fault: `rbx` (this) `0x819421A0`, `rbp` `0x81942408` =
  `this+0x268`, `r15` = 1 (record index), `r12` = `0x32` (MapVirtualKeyA result for record
  0, scan code `0x03`), `rax` = `0x51` (result for record 1), `r14` = `0x21F5ED10` = record
  1's `dwOfs`; `rbp + r14` = `0xA38A1118` = the logged fault address. So the buffered read
  returned at least two records and record 1's `dwOfs` was not a scan code.
- *Verified.* The return address `0x140CF9624` sits in a loop that calls `vtable+0x10` on six
  objects (`mov edi,6`, `call [rax+0x10]`, `add rbx,8`), consistent with the per-device
  update inside `Main::Update`; the rest of the backtrace (`0x140658DEE`, `0x140651465`)
  is the main-thread loop.
- *Orchestrator reading, not re-verified here.* The faulting function `0x140E127C0..0x140E12AD3`
  is Skyrim's buffered keyboard read: a wrapper at `0x140CFAF60` calls
  `IDirectInputDevice8::Poll` (vtable `+0xC8`) then `GetDeviceData` (vtable `+0x50`) with
  `cbObjectData = 0x18`, `rgdod = this+0x78`, `pdwInOut = 128`, flags 0, then maps each
  `dwOfs` with `MapVirtualKeyA(…, MAPVK_VSC_TO_VK)` and reads a state byte at
  `this+0x268+dwOfs`.
- *Layout hypothesis (one sample, unproven).* `0x21F5ED10` (about 570 million) looks like a
  tick count. If the hook delivered 16-byte `DIDEVICEOBJECTDATA_DX3` records where Skyrim
  reads 24-byte `DIDEVICEOBJECTDATA`, Skyrim's record 1 `dwOfs` lands on native record 1's
  `dwTimeStamp`, which matches record 0 reading correctly and record 1 being garbage.
- *Attribution.* vinput does not hook `MapVirtualKeyA`, but it interposes exactly this
  `GetDeviceData` call (upstream `vi_di_format_hooks.inl` `DiFormattedBuffer` and
  `vinput_core.cpp` `DIGetDeviceDataImpl`), plus `Acquire` and `SetCooperativeLevel` on the
  claimed device, and its "virtual activation adoption refresh" at 4.5 s
  (`WM_ACTIVATEAPP`/`WM_ACTIVATE`/`WM_SETFOCUS` through `CallWindowProc`) is what makes
  Skyrim re-acquire and poll the keyboard. Hundreds of vinput-free routes never crashed here.

Caveats that hold regardless: OCR commands (`ocr`, `click-text`, `wait-text`) use WinRT
`Windows.Media.Ocr` and are expected to fail under Wine; XInput emulation is not controller
proof; the DLLs are test-only and never ship.

## Recommendation

**One more bounded experiment is justified before dropping vinput, if the owner approves
it; otherwise drop it and keep the uinput harness.** Do not adopt it for routes or for
interactive steering until the experiment has run. Proposed run (one process, 20 minutes,
same stage, same launch; companion script `session-p5/vinput/spike3_driver.sh`, which never
launches anything):

1. As soon as the pipe answers (0.7 s after `VInput_Init`; the main menu came up about 8 s
   after it in spike-2), the wrapper sends `config fakefocus=false isolate=false`, so vinput
   neither fakes activation nor interposes physical input before Skyrim's first keyboard poll.
2. Capture `status` JSON at the main menu (which input channels Skyrim reads).
3. Load the save; once the TestProbe reports `player_state` available, send
   `config fakefocus=true isolate=true`, then one `key w`.

| Outcome | Reading |
| --- | --- |
| Survives with faking off, crashes after re-enabling | the activation / cooperative-level path |
| Crashes anyway | the DirectInput buffered-read hook under Wine's `dinput` |
| Survives both | the first crash was timing-dependent; proceed to covered `capture`, `hold w`, `move`, `pad` |

If it still crashes, drop vinput and keep the uinput, focus and `xwd` harness.

## Open risks (updated)

- **Startup crash with hooks installed at plugin load** (observed). Located in Skyrim's
  DirectInput buffered-keyboard read with a garbage second record; attributed to vinput's
  `GetDeviceData` interposition or its activation refresh (see the crash analysis), not yet
  separated. This blocks everything else.
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
