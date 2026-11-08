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
| In-game: game survives to the main menu with vinput hooked (spike-2, spike-3 attempt 2) | **FAIL** twice (access violation in `SkyrimSE.exe` at `+0xE128B2`, ~3 s after the main menu opened, identical backtrace both runs) |
| In-game bisect (spike-3): `config fakefocus=false isolate=false` 0.5 s after the pipe appeared | **PASS** accepted (`ok:true`); the game then survived the main menu for 47 s |
| In-game bisect (spike-3): `status` at the main menu | **PASS** (29 KB JSON; channels below) |
| In-game bisect (spike-3): game survives real window activation with faking off | **FAIL** (same crash, same address and backtrace, ~1 s after `WM_ACTIVATEAPP`/`WM_ACTIVATE`/`WM_SETFOCUS` with `realfg=1`) |
| In-game: save load, re-enable, `key w`, covered `capture`, `move`, `pad` | **NOT_RUN** (crash came first; no key was ever sent by the driver) |

**Recommendation: drop vinput and keep the uinput, focus and `xwd` harness.** Three game
runs, three identical crashes at `SkyrimSE.exe+0xE128B2` inside Skyrim's DirectInput
buffered-keyboard read. The spike-3 bisect showed that vinput's fake activation only decides
*when* the crash happens: with `fakefocus=false isolate=false` the game sat at the main menu
for 47 s (spike-2 died at 11 s) and crashed about one second after the window became the
real foreground, on the first keyboard poll after activation. The DirectInput interposition
itself (installed unconditionally at `VInput_Init`, `dinput8 keyboard/mouse A/W poll coverage:
15/15`) is therefore what Skyrim 1.7.104 under Wine's `dinput` cannot survive, and it has no
runtime switch. Not adopted as a route input/capture layer and not adopted for interactive
steering.

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

## Game spike 3 (2026-11-08, runs `vinput-spike-3`, bisect)

Owner go relayed verbatim by the orchestrator: "Run the bisect"; the user in this session
allowed the harness launch after the permission classifier denied it once (attempt 1,
NOT_RUN, deployment restored). Same stage (`session-p5/stage-vinput`), dist
(`dist-camera-p5-off`), profile `camera-only`; the owner's 2026-10-26 Riverwood quicksave was
copied into the profile's save folder as `SkyCRPG_VInput3` (`.ess` `284c9b8d226fc9ae94d6b811f139028fd44b63bed69bd95226fc82f3800fdb7f`)
after prepare, so restore removed it. Host side: `spike3_driver.sh` (pipe wait, `config`,
`status`) and `spike3_run.py` (console load through the harness's uinput keyboard, re-enable,
one `key w`, `qqq`); both only talk to a game the harness already started. Logs:
`session-p5/logs/vinput/spike-3/`.

**Attempt 2 (no bisect, my sequencing error).** The pipe driver was started after the
launch command returned; the game had already crashed. It reproduced spike-2 exactly:
plugin loaded, `pid=356`, `VInput_Init returned 1`, Main Menu at 22:51:47Z, `Exception
0xc0000005 ... addr=0000000140E128B2` on the main thread at 22:51:52Z, same seven-frame
backtrace, `leftover_pids []`. Proton log `fa8cd83fbce4b0e6edb6d9ce0d2b475072d4e50922b28f2eeeda46115100e7ee`, vinput log
`0d03d1b067b083ded9076e4b54e80318c65b6f8cf32d578c6eb1fa5cd80a7935` (`attempt2-nobisect/`). It counts as the second
unmodified reproduction; the orchestrator's "one unchanged retry" was then used for attempt 3.

**Attempt 3 (bisect).** Driver started before the launch.

| Step | Evidence | Result |
| --- | --- | --- |
| Plugin loads, vinput starts | `SkyCRPG_VInput.log`: `pid=348`, runtime 1.7.104.0, SKSE 2.3.1.0, `VInput_Init returned 1`; `vinput_348.log` 0.581 s: `hooks ok:` (54 entries, `hooks_failed` empty), `dinput8 keyboard/mouse A/W poll coverage: 15/15`, `pipe \\.\pipe\vinput_348` | **PASS** |
| `config fakefocus=false isolate=false` at the pipe | `driver.log` 22:55:49.9 PID file read, 22:55:50.4 reply `{"ok":true,...,"pid":348,...}` (first try; the pipe answered through the launcher service) | **PASS** |
| Main menu with faking off | TestProbe `Main Menu` opening 22:55:56Z; `status` at 22:56:00 (`uptime_ms 10725`): `fakefocus false`, `isolate false`, `physical_input.physical_passthrough true`, `really_foreground false`, `user_took_foreground false`, `blockfocus true`, `frame_api d3d11`, swapchain 1920x1080, `frames 164`, `capture_path none`; `raw_kbd false`, `raw_mouse false`, `raw_registration.count 0`; `directinput_readers`: `factory_hooked true`, `created_device_returns 4`, `state_entries 2`, `buffer_entries 2`, `hook_failures 4`, `formatted_buffer_reads 0`; `directinput_mouse.poll_channel none`; `virtual_pad.active false`, XInput users 0-3 all `entries 0`. Reading: at the main menu Skyrim had created its DirectInput keyboard and mouse devices but had not yet polled them; Raw Input was not registered | **PASS** |
| Game survives real activation | `vinput_348.log` +52.005 s: `trace msg 0x1C w=0x1 ... realfg=1` (WM_ACTIVATEAPP), `msg 0x6 w=0x2 ... realfg=1` (WM_ACTIVATE), `msg 0x7 ... realfg=1` (WM_SETFOCUS); last line of the log. Proton log 263716.048 (init 263664.6, so +51.4 s): `Exception 0xc0000005 ... addr=0000000140E128B2`, thread 0160, backtrace `+0xE128B2 → +0xCF9624 → +0x658DEE → +0x651465 → +0x15A843E → kernel32 → ntdll` (identical to spike-2 and attempt 2). `run.log`: the Skyrim window became the active X11 window at 22:56:41.8 (not by this driver: `spike3_run.py` had only been waiting for focus since 22:56:04, and its first GRAVE tap at 22:56:44.3 was refused because the active window was already gone); `run.json` ended 22:56:43, `leftover_pids []` | **FAIL** |
| Save load via console, `config fakefocus=true isolate=true`, `key w 500`, position change | not reached; no key was sent to the game by either driver (`result.json` steps: `main_menu PASS`, `focus PASS`, then the process was gone) | NOT_RUN |
| Restore | `restoration.json` PASS, `mismatch []`, `game_pids []`; `Data/SKSE/Plugins/logs/vinput_348.log` and `vinput_356.log` preserved to `plugin-logs/` and removed first so `Data/SKSE` disappeared cleanly | **PASS** |

| File (`session-p5/logs/vinput/spike-3/`) | SHA-256 |
| --- | --- |
| `steam-489830.log` | `93289f527a5ba6503eb3f3f29a99944be5216d3a534ef7d969d1d454df0559ad` |
| `plugin-logs/vinput_348.log` | `8f3c40ce8243236a9af3e1661c540d6a43b3eef790109ab287ff9a101cb325ef` |
| `SkyCRPG_VInput.log` | `7b21c29465d8e901d818eada6be4c3886ecfd8921682cb010eb5b731f0e25359` |
| `skse64.log` | `1ab145f229eb65082be1d8f1169d4f7a248488aa4908aabd2e2b32562fb62f0d` |
| `SkyCRPG_TestProbe-run.jsonl` | `d1ce2a932a180480a20959a94d9aaf4f573d73352a3151d9e701c20739e23139` |
| `01-config-off.json` | `549de57fe1e5ad2eb81e82e6253dcde49cb39886f74a9418dbaa5a81e1979ea6` |
| `02-status-mainmenu.clean.json` | `016320aa5af0b6e17d4549c66cdc49a2af86454b5f2b99096d73664a75a5904a` |
| `driver.log` / `run.log` / `result.json` | `d8bba5f590bb9cc98db0602f350b8e822db1a3ed024d9d729f8031e876455089` / `c1a8793362bcc39a6a3b2929b7f7ce5be319660c80a508a25596ce77c05b851f` / `1d20c6fa005ad955a47fa7eadc9e0213485669f6d96808f3a9fed33503fee501` |

**Reading of the bisect.** The outcome table below predicted three cases; the result is a
sharper version of "crashes anyway". Switching off fake focus and isolation (the only
vinput behaviour that has a runtime switch) moved the crash from 11 s after init to the
moment the window was really activated, and the fault, address and backtrace did not
change. So the activation path merely triggers Skyrim's first buffered keyboard poll; the
poll itself faults with vinput's DirectInput reader hooks in place, whether or not vinput
is faking anything. Caveats: real activation at 22:56:41 came from outside the drivers (the
owner or the compositor), which does not weaken the reading because the crash needed exactly
that; `isolate=false` means the hook was in pass-through mode and still corrupted or
mis-sized the buffered records; the status JSON records `hook_failures 4` under
`directinput_readers` with every top-level hook reported ok, which upstream would have to
explain. A further bisect would need a vinput build without the `dinput8` hooks, which is
a code change upstream, outside this spike.

## Recommendation

**Drop vinput; keep the uinput, focus and `xwd` harness.** The bisect the previous version
of this note proposed has run (above). Its outcome table and result:

| Outcome | Reading | Result |
| --- | --- | --- |
| Survives with faking off, crashes after re-enabling | the activation / cooperative-level path | not this |
| Crashes anyway | the DirectInput buffered-read hook under Wine's `dinput` | **this**, refined: survives until real activation, then the same fault |
| Survives both | the first crash was timing-dependent | not this |

Do not adopt vinput as a route input/capture layer, and do not adopt it for interactive
steering: the first real keyboard poll kills the game. The vendored pin, CI job, test-only
plugin, wrapper and drivers stay in this draft PR as the record of what was tried; nothing
is integrated, shipped or configured in `modules/**`. Revisit only with an upstream build
that can leave DirectInput alone.

## Open risks (updated)

- **Crash on the first buffered DirectInput keyboard poll with vinput's reader hooks in place**
  (observed three times, bisected once). Located in Skyrim's DirectInput buffered-keyboard
  read with a garbage second record; fake activation only sets the time of the first poll
  (11 s after init), real activation triggers it too (spike-3, 47 s at the main menu with
  faking off). No runtime switch disables these hooks. This blocks everything else.
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
