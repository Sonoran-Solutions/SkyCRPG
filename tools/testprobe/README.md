# SkyCRPG TestProbe (internal tests only)

`SkyCRPG_TestProbe.dll` is a read-only SKSE observer for the unattended-test
runner. It is **never shipped** in Camera/UI release ZIPs or manifests. It
installs no hooks, emits no input, changes no game state and has no runtime
dependency on either SkyCRPG module. Inventory reads explicitly use
`a_noInit=true`; no inventory initialization, display-name mutation or item
actions are performed.

The target is Windows x64, Skyrim **1.7.104.0**, SKSE **2.3.1**, and the existing
CommonLibSSE-NG **7.2.0** source pin. The runner must supply Address Library 13
in every disposable profile using this probe (including a baseline profile).
Before CommonLib initialization, the probe checks
`versionlib-1-7-104-0.bin` against SHA-256
`8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`.
Missing or mismatched libraries cause load rejection, with a diagnostic.

The default build switch is OFF. To build just this target in the repository's
pinned Windows toolchain:

```powershell
cmake --preset windows-x64-release -DSKYCRPG_BUILD_CAMERA=OFF -DSKYCRPG_BUILD_UI=OFF -DSKYCRPG_BUILD_TEST_PROBE=ON -DSKYCRPG_SOURCE_COMMIT=<sha> -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build --preset windows-x64-release --target SkyCRPG_TestProbe --parallel 2
```

CI uploads only the DLL and `SHA256SUMS` as `skycrpg-testprobe-<full-sha>`.
It does not run the release packager for this target. The normal packager's
explicit Camera/UI file lists remain unchanged.

The **runner** verifies that checksum and installs the DLL only into the
disposable profile's `Data/SKSE/Plugins/`, then removes it during profile
restoration. It may copy the sample `SkyCRPG_TestProbe.ini` alongside the DLL or
generate the same settings. This package does not deploy anything, edit the
owner's files or launch the game. Keep CommonLib's source/license notices with
internal candidate provenance; see `LICENSES/CommonLibSSE-NG-*` and
`cmake/README.md`. No public distribution is authorized.

```ini
[Probe]
bEnabled=1
iStateIntervalMs=1000
```

Settings are read once at startup; a missing INI uses these defaults.
`bEnabled=0` writes only `probe_loaded` with `enabled:false`, then stays inert.
`iStateIntervalMs=0` disables both the timer and request polling; load/new-game
and menu-close snapshots still run while enabled. Intervals from 1 through
2147483647 ms are accepted; invalid values fall back to 1000 ms.

Output is appended to `<My Games>/Skyrim Special Edition/SKSE/` using
`skycrpg::LogDirectory()`:

- `SkyCRPG_TestProbe.jsonl`: one JSON object and LF per event; writes are
  serialized by a mutex and flushed before returning.
- `SkyCRPG_TestProbe.log`: best-effort diagnostics for unavailable state,
  registration, I/O and caught C++ exceptions (also sent to the debug output).

The runner should record the starting JSONL offset; previous runs remain in the
file. Times restart on a new process. Normal periodic samples start after a
successful saved-game load or new game. A timer queues at most one outstanding
sample; engine stalls delay/coalesce timer ticks. Every menu close and
post-load message queues its own additional sample.

## Checkpoint requests

Write UTF-8 text to `<My Games>/Skyrim Special Edition/SKSE/SkyCRPG_TestProbe.request`
(an empty touched file is valid). Prefer a completed temporary file atomically
renamed into place. The timer reads it at its configured cadence and queues a
task-drain `player_state` with `requested:true` and **the exact file contents**
in `tag`. Tags are raw text, not JSON; whitespace, newlines and NULs are
preserved and JSON-escaped. The maximum tag is 65536 bytes. Oversized/unreadable
requests are logged and left on disk; ordinary periodic samples continue.

After enqueueing, the opened request file is marked for deletion by handle,
preventing a path race with a later checkpoint. A sharing violation is retried
on a later tick. Deletion failure is diagnosed and can yield repeated tags;
deletion acknowledges enqueueing, not completion. Send one request at a time
and wait for the matching `tag` with `available:true` before asserting game
state. An unavailable/world-changed response is explicit and must not count as
a successful checkpoint. No request polling occurs with the timer disabled.

## JSONL schema

Every line has `t` (nonnegative monotonic milliseconds since probe startup),
`wall` (UTC `YYYY-MM-DDTHH:MM:SSZ`) and `ev`. Numbers use locale-independent
formatting; non-finite values become `null`. Names/tags are UTF-8; quotes,
backslashes and control characters are escaped. Form IDs are fixed-width
uppercase strings such as `"0x00000014"`; absent objects are `null`.

| `ev` | Additional fields |
| --- | --- |
| `probe_loaded` | `runtime`, `skse` (four-part version strings), `probe_version`, `snapshot_dispatch` (`"ui_task_drain_v3"`), `build_commit` (`"unknown"` if not supplied), `enabled`, `state_interval_ms` |
| `data_loaded` | None; SKSE `kDataLoaded` |
| `game_loaded` | `success` from SKSE `kPostLoadGame`'s pointer-encoded boolean; `save_name` copied from `kPreLoadGame`, or `null` |
| `new_game` | None; SKSE `kNewGame` |
| `thread_context` | `source` (`data_loaded`, `pre_load_game`, `post_load_game`, `new_game`, `menu`, `skse_task_post_load`), `current_thread_id`, `captured_main_thread_id`, `main_thread_id` (CommonLib's `Main::threadID`, or `null` without a singleton) |
| `menu` | `name`, `opening` |
| `equip` | Player only: `form_id` (base object), `name`, `equipped`. Slot omitted because the event has no reliable slot field. |
| `container` | Player in/out only: `form_id`, `name`, `player_in`, `player_out`, `old_container`, `new_container`, signed `count_delta`, `new_count`, `count_status` |
| `player_state` | `requested`, optional `tag`, `available`; when available: `position:{x,y,z}`, `cell:{form_id,editor_id,name}` or `null`, `health`, `magicka`, `stamina` (each `{current,max}`), `equipped:{right,left,ammo}`, `camera_state`, `paused`, `open_menus`, `item_card_submenu` |

`container.count_delta` is positive into the player and negative out; a transfer
from the player to itself is zero. `new_count` is currently always `null`
with `count_status:"off_thread"`: equip/container callbacks copy only
IDs/scalars into a probe task, where names are read, and a count taken there
would not be the immediate post-event count.
Missing forms produce `name:null`; observations queued across a load are
discarded with a diagnostic.

All snapshots execute through `SKSE::GetTaskInterface()->AddUITask`, drained
after SKSE's native UI event processing. The timer only polls files and queues
work; it never reads player/cell/UI/camera. SKSE drains these tasks on
rotating engine worker threads (observed 424-444 on 1.7.104 under Proton), never
on `Main::threadID` or the `kDataLoaded` thread, so the probe does not gate on a
thread ID. Each probe task sets a thread-local flag while it runs inside the
drain, and engine reads require that flag. The captured `kDataLoaded` ID and
`Main::threadID` are diagnostic only; menu and lifecycle `thread_context` events
and one ordinary `AddTask` after each saved-game load record executor threads.

Snapshots reacquire player/cell/UI/camera and retain no engine pointers. Every
`player_state` includes the three thread fields above, `queued_generation`,
`current_generation`, and `loaded`. A load generation prevents old queued tasks
from reading a new world. An unavailable snapshot has `available:false` and
`reason`, selected in order: `outside_task_drain`, `generation_changed`,
`not_loaded`, then `player_unavailable`; request metadata is preserved and state
fields are omitted. Unavailable snapshots always include the observed current and
engine thread IDs, plus the captured ID. A main
or loading menu also makes player state unavailable. Missing camera/UI/cell
components yield `null`. `paused` is `UI::GameIsPaused()`. `item_card_submenu` is `true` while SkyUI's inventory
ItemCard has a submenu up (quantity slider, list, message or name edit: its
`InputHandler` is set), `false` when the card is idle, and `null` when
InventoryMenu is closed or the card path does not resolve. The quantity slider
raises no menu event, so routes use this field to see it. `open_menus` lists
registered menus whose `IMenu::OnStack()` flag is set; ordering is unspecified.
`camera_state` is the native numeric camera state ID, not a claim about which
mod owns the camera. Resources use live current values and permanent plus
temporary maximums; this is base-form telemetry, not instance identity or
ownership/weight evidence.

## Portable check

```bash
g++ -std=c++20 -Wall -Wextra -Werror -pedantic tools/testprobe/tests/json_line_test.cpp -o /tmp/skycrpg-probe-json-test
/tmp/skycrpg-probe-json-test
g++ -std=c++20 -Wall -Wextra -Werror -pedantic tools/testprobe/tests/snapshot_policy_test.cpp -o /tmp/skycrpg-probe-policy-test
/tmp/skycrpg-probe-policy-test
```

This tests escaping, UTF-8 preservation, numeric formatting (including a decimal
comma locale), envelope formatting, raw request parsing/bounds and interval
parsing. It does not compile the Windows adapter or prove engine behavior.
The policy test covers all eight drain/generation/load combinations and checkpoint tag preservation in failures.
Windows MSVC builds in CI (job `testprobe`). Runtime: the DYL-167 combined
route passed with this probe on `1b4e4c5`, both supervised
(`auto-dyl167-combined-20261028T181647Z`) and unattended
(`auto-dyl167-combined-20261028T181916Z`). Snapshots were available on the
task drain, `item_card_submenu` tracked the quantity slider, and the runner
verified the probe was removed after each run. The process-lifetime sinks/timer do not support hot
unloading. C++ exceptions are caught at plugin/callback/task boundaries; native
engine faults and disk/power-loss atomicity cannot be guaranteed by this probe.
