# vinput under Proton (test tooling, never shipped)

`tools/vinput/` pins [llm-game-automation-mcp](https://github.com/CorruptedAng3l/llm-game-automation-mcp)
("vinput") at the commit in [`UPSTREAM.lock`](UPSTREAM.lock) and builds it on CI as an
**internal test input** for the route harness spike. Nothing here is a SkyCRPG module,
nothing is packaged by `tools/package.py`, and no shipped configuration references it.

vinput is a Windows DLL that lives inside the game process. It fakes window focus, delivers
keyboard, mouse and XInput input on every channel the game reads (window messages, Raw
Input, DirectInput 8, `GetAsyncKeyState`, XInput) and captures frames from the D3D11
swapchain `Present` hook. A CLI (`vinput.exe`) talks to the DLL over the per-process named
pipe `\\.\pipe\vinput_<pid>`, and an MCP server wraps the CLI. Upstream's own notes for
Skyrim SE are in its `docs/AGENT_TIPS.md`.

## What CI builds

Workflow [`vinput.yml`](../../.github/workflows/vinput.yml), job `vinput-x64` on `windows-2022`:

1. Checks out the upstream repository at the pinned commit into `external/vinput` and
   fails if `git rev-parse HEAD` differs from `UPSTREAM.lock`.
2. Runs upstream `build.bat :arch x64` unchanged (x64 only; the x86 half is not needed).
3. Builds `SkyCRPG_VInput.dll` from [`plugin/`](plugin/): a minimal SKSE plugin with no
   CommonLib or Address Library dependency that links `vinput_x64.lib` with
   `/DVINPUT_COMPILE_IN` and calls `VINPUT_INIT()` from `SKSEPlugin_Load`, so no injector
   is needed. It is compiled `/W4 /WX /permissive-` like the modules.
4. Uploads `skycrpg-vinput-<sha>` with `vinput.exe`, `vinput64.dll`, `vinput_x64.lib`,
   `SkyCRPG_VInput.dll`, `SHA256SUMS` and the upstream MIT and MinHook licence texts
   (MinHook's licence requires the notice next to binary redistributions, even internal ones).

No Windows binary is built or committed on the Linux host. The harness verifies the
`SHA256SUMS` before staging anything into a disposable profile.

## The SKSE host plugin

`SkyCRPG_VInput.dll` is opt-in like the TestProbe: it does nothing unless
`SkyCRPG_VInput.ini` beside it says `[VInput] bEnabled=1`. When enabled it writes
`<My Games>\Skyrim Special Edition\SKSE\SkyCRPG_VInput.log` (runtime, SKSE and vinput
versions, the Windows PID, the `VInput_Init` result) and `SkyCRPG_VInput.pid` holding the
Windows PID, which the Linux wrapper reads when `vinput.exe list` cannot enumerate pipes
under Wine. vinput itself logs to `<plugin dir>\logs\vinput_<pid>.log` (inside the MO2
mod folder or `Data\SKSE\Plugins\logs\`).

The plugin accepts only runtime 1.7.104.0; on any other runtime it logs a rejection and
does not start vinput.

## Host side

The Linux wrapper and launch notes live outside the repository in
`session-p5/vinput/` (see the evidence note under `docs/evidence/p5/vinput-spike.md`).
The game must be launched with `STEAM_COMPAT_LAUNCHER_SERVICE=proton` added to the
existing launch environment so that `steam-runtime-launch-client
--bus-name=com.steampowered.App489830` can run `vinput.exe` through Proton's `wine`
inside the game's container, where the Wine named pipe is reachable.

## Known limits

- OCR commands (`ocr`, `click-text`, `wait-text`) use WinRT `Windows.Media.Ocr` and are
  expected to fail under Wine.
- XInput emulation exercises the game's XInput reader; it is not evidence of real
  controller support.
- vinput hooks `Present`, user32 focus functions, Raw Input, DirectInput and XInput in the
  same process as SmoothCam, TDM and our modules; coexistence is a spike question, not a given.
