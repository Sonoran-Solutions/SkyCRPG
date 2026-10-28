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

Settings are read once at startup. The probe is **opt-in**: only an explicit
`bEnabled=1` enables it. A missing INI, a missing key, `bEnabled=0` or any
other value leaves the DLL fully inert. It registers no sinks, starts no timer
and writes no JSONL or log, so an accidental install without the runner's INI
does nothing. The runner always installs this INI and refuses to start unless
it says `bEnabled=1`.
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
in `tag`. Checkpoint tags are raw text, not JSON; whitespace, newlines and NULs are
preserved and JSON-escaped, except the reserved `@perf ` and `@state ` command namespaces below. The maximum tag is 65536 bytes. Oversized/unreadable
requests are logged and left on disk; ordinary periodic samples continue.

After enqueueing, the opened request file is marked for deletion by handle,
preventing a path race with a later checkpoint. A sharing violation is retried
on a later tick. Deletion failure is diagnosed and can yield repeated tags;
deletion acknowledges enqueueing, not completion. Send one request at a time
and wait for the matching `tag` with `available:true` before asserting game
state. An unavailable/world-changed response is explicit and must not count as
a successful checkpoint. No request polling occurs with the timer disabled.

Performance requests use the same atomic file writer and deletion acknowledgement:
`@perf start <label>` and `@perf stop <label>`, with **no trailing newline**.
Labels match `[A-Za-z0-9_.-]{1,40}`. Only one capture may run at a time;
stop must name the active label. Wait for `perf_capture` with the exact `tag`,
`phase`, and `available:true` before proceeding. Invalid, overlapping,
not-loaded or mismatched requests produce `available:false`; they do not start
another chain. The stop emits `frame_summary` before its acknowledgement.
A stop/restart while a task is queued reuses that single chain. Loading or a new
game cancels an active capture; the later stop then fails explicitly. Ordinary
checkpoint requests retain their existing `player_state` contract.

The runner never overwrites a still-pending request. Labels `standing`, `turn`,
`inventory` and `resource` are used by the performance routes. `resource`
start/stop also emit resource-window markers in probe time.

## JSONL schema

Every line has `t` (nonnegative monotonic milliseconds since probe startup),
`wall` (UTC `YYYY-MM-DDTHH:MM:SSZ`) and `ev`. Numbers use locale-independent
formatting; non-finite values become `null`. Names/tags are UTF-8; quotes,
backslashes and control characters are escaped. Form IDs are fixed-width
uppercase strings such as `"0x00000014"`; absent objects are `null`.

| `ev` | Additional fields |
| --- | --- |
| `probe_loaded` | `runtime`, `skse` (four-part version strings), `probe_version`, `snapshot_dispatch` (`"ui_task_drain_v3"`), `perf_protocol` (`1`), `observation_protocol` (`2`), `build_commit` (`"unknown"` if not supplied), `enabled` (always `true`; a disabled probe writes nothing), `state_interval_ms` |
| `data_loaded` | None; SKSE `kDataLoaded` |
| `game_loaded` | `success` from SKSE `kPostLoadGame`'s pointer-encoded boolean; `save_name` copied from `kPreLoadGame`, or `null` |
| `new_game` | None; SKSE `kNewGame` |
| `thread_context` | `source` (`data_loaded`, `pre_load_game`, `post_load_game`, `new_game`, `menu`, `skse_task_post_load`), `current_thread_id`, `captured_main_thread_id`, `main_thread_id` (CommonLib's `Main::threadID`, or `null` without a singleton) |
| `menu` | `name`, `opening` |
| `equip` | Player only: `form_id` (base object), `name`, `equipped`. Slot omitted because the event has no reliable slot field. |
| `container` | Player in/out only: `form_id`, `name`, `player_in`, `player_out`, `old_container`, `new_container`, signed `count_delta`, `new_count`, `count_status` |
| `perf_capture` | `tag` (exact command for requests; absent on abort), `label`, `phase` (`start`/`stop`/`abort`), `available`, `reason` on abort (`runaway`, `enqueue_failed`, `ui_task_failed`, `main_task_failed`) |
| `frame_summary` | `label`, `count` (retained periods), `total_count`, `median`, `p95`, `p99`, `max` (ms), `over33` (>33.3 ms), `over50` (>50 ms), `duration_ms`, `engine_count`, `engine_total_count`, `engine_median`, `engine_p95` (ms from BSTimer realTimeDelta), `anchor_ratio` (probe median / engine median, null without data) |
| `inventory_open` | `label`, `cold` (first inventory open in this process), `correlated`, `key_qpc_ms` (or null), `open_qpc_ms`, `pre_open_median_ms`, `t_event_ms`, `t_settled_ms` (each null if unavailable), `periods_ms` (up to the first 30 post-open UI-drain periods) |
| `resource_check` | `phase` (`start`/`stop`); `t` defines the memory analysis window |
| `process_memory` | `private_bytes`, `working_set_bytes`; sampled by the existing timer while loaded, at `iStateIntervalMs`, coalesced if a tick is busy. Failed queries omit the event and log once. |
| `player_state` | `requested`, optional `tag`, `available`; when available: `position:{x,y,z}`, `cell:{form_id,editor_id,name}` or `null`, `health`, `magicka`, `stamina` (each `{current,max}`), `equipped:{right,left,ammo,two_handed,head,body,ring,amulet}`, `camera_state`, `paused`, `open_menus`, `item_card_submenu`; protocol 2 adds carry/count/weight/voice/HUD/inventory fields documented below |

`container.count_delta` is positive into the player and negative out; a transfer
from the player to itself is zero. `new_count` is currently always `null`
with `count_status:"off_thread"`: equip/container callbacks copy only
IDs/scalars into a probe task, where names are read, and a count taken there
would not be the immediate post-event count.
Missing forms produce `name:null`; observations queued across a load are
discarded with a diagnostic.

Player-state snapshots execute through `SKSE::GetTaskInterface()->AddUITask`, drained
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
temporary maximums; resources are base-form telemetry, not instance identity or ownership evidence.
Protocol 2 adds numeric carried-weight samples and aggregate requested counts below.

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
The config test covers missing, explicitly enabled, explicitly disabled and
malformed `bEnabled` values. CI builds and runs all three portable tests on
Linux (job `testprobe-portable`).
Windows MSVC builds in CI (job `testprobe`). Runtime: the DYL-167 combined
route passed with this probe on `1b4e4c5`, both supervised
(`auto-dyl167-combined-20261028T181647Z`) and unattended
(`auto-dyl167-combined-20261028T181916Z`). Snapshots were available on the
task drain, `item_card_submenu` tracked the quantity slider, and the runner
verified the probe was removed after each run. The process-lifetime sinks/timer do not support hot
unloading. C++ exceptions are caught at plugin/callback/task boundaries; native
engine faults and disk/power-loss atomicity cannot be guaranteed by this probe.

## Performance sampling

`src/perf_policy.h` supplies two fixed 16384-double rings, nearest-rank p95/p99,
median/max/tail counts, request parsing, six-frame settling, and the portable
`FrameChain` state machine. During capture, a process-lifetime
`SKSE::detail::UIDelegate_v1` samples QPC and `RE::BSTimer::realTimeDelta` (seconds,
converted to ms) at the UI drain. It queues a persistent
`SKSE::detail::TaskDelegate` on the main queue; that helper only queues the next
UI delegate. Both `Dispose()` methods are no-ops. The pointer overloads in
CommonLib 7.2.0 `SKSE/Interfaces.h:193,195` forward unchanged in
`src/SKSE/Interfaces.cpp:184–196`; `Impl/Stubs.h:98–110` declares both Run/Dispose
interfaces without a deleting destructor. No per-frame lambda/delegate allocation
is needed. Only scalar samples cross callbacks; no engine pointer is retained.
Sorting/JSON formatting occurs at input, stop or inventory-window completion.

The earlier same-queue self-requeue design was unsafe: SKSE 2.3.1 drains until
empty and would immediately run the requeued task forever. Its main drain has
two hook sites, so it also cannot serve as a once-per-frame anchor. The UI drain
has one hook site, but also drains to empty. Each hop now queues only the other
queue. A single reservation spans both hops, including stop/restart and the gap
before enqueue, so restarting reuses a pending hop. A stopped pending hop drains
without sampling or continuing. Enqueueing happens outside the capture mutex to
avoid inversion with SKSE's UI queue lock. The normal state/memory timer remains.
If a ninth UI run occurs within any 1 ms QPC window, capture stops, logs once,
and emits `perf_capture` with `phase:"abort"`, `available:false`, `reason:"runaway"`.
The guard is a safety net, not the scheduling mechanism.

This measures **UI-drain CPU period**, not GPU present or proof of a painted grid.
One sample per frame depends on a main drain between consecutive UI drains;
validate this hypothesis on the pinned host, including paused inventory. The
second preallocated ring pairs each QPC period with the engine's real-time delta
(the first UI run establishes the QPC baseline). `frame_summary` reports both
medians/p95 and `anchor_ratio = median / engine_median`. `perf_compare.py` requires
an anchor in **[0.9, 1.1] inclusive** for each standing, turn, inventory and resource
capture. Outside that range, missing/invalid deltas, count mismatches or an abort
make that capture's budget **NOT_RUN**, never a performance PASS/FAIL. The ratio
is independently recomputed from medians. A missing anchor in either comparison
capture leaves the combined frame budget NOT_RUN. Raw anchor values remain in
the analysis table for diagnosis; passing the ratio alone does not prove cadence.
BSTimer may clamp/smooth or retain its delta when paused; actual behavior remains
NOT_RUN and is a reason to reject evidence, never to relax the anchor rule.

Relative comparison uses the identical probe/settings/host in every profile.
`total_count > count` exposes ring wrap; truncated standing/turn captures are
refused. Resource ring wrap does not truncate the separately streamed memory data,
but its retained anchor must still be valid before the resource budget is judged.

The input sink resolves `Inventory` and `Tween Menu` through
`RE::ControlMap::GetMappedKey(event, device)` and timestamps button-down on the
same QPC clock as `MenuOpenCloseEvent` and the sampler. It consumes no input.
The InventoryMenu endpoint is captured before diagnostic I/O. A key older than
10 seconds, no observed key, or no pre-open periods yields unknown latency or
settling. The pre-open median is the active capture's retained periods at
key-down. The probe watches the 3 seconds after open, recording the first 30
periods. Settling is the end of the **first** period ≤2× pre-open median that is
followed by five more consecutive qualifying periods; the result is reported
only after all six are observed. `t_event_ms` is key → menu-open event;
`t_settled_ms` is key → that first qualifying frame end. Early close/stop flushes
partial evidence; unavailable settling stays null. Cold is tracked across
captures and loads for the process lifetime.

The runner's `inventory_cycle` observes 3 seconds for settling, scrolls at 10
wheel ticks/second for `scroll_s`, closes with TAB, then waits `wait_s`. It starts
an inventory capture itself when used outside a `perf_capture`. The supplied
keyboard/mouse routes require Inventory=I and Back=TAB; physical controller off.
Controller/Tween behavior requires a separate manual runtime pass.

Run **all five** portable contracts using the same glob as CI:

```bash
for source in tools/testprobe/tests/*_test.cpp; do
  binary="/tmp/$(basename "$source" .cpp)"
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic "$source" -o "$binary" && "$binary" || exit
done
```

The performance test covers ring wrap/clear, invalid values, percentiles/tail
counts, six-frame threshold/reset, bounded raw periods, missing correlation and
reserved requests, two-queue single-chain start/stop/restart, idle hops, enqueue failure,
sliding runaway window and recovery. Windows `/W4 /WX /permissive-` build and the new performance
adapter's runtime behavior are **NOT_RUN**. Earlier runtime passes above apply
only to the earlier player-state observer, not these new measurements.

## DYL-170 observations (protocol 2; runtime NOT_RUN)

`probe_loaded.observation_protocol` and every `player_state.observation_protocol`
are **2**. The frame timing/performance protocol remains **1**; its request
namespace, queue chain and portable tests are preserved. Rebuild the Windows
probe before running DYL-170; an earlier DLL cannot provide these observations.

The existing raw checkpoint request is unchanged. A second reserved namespace
requests counts for at most 16 distinct nonzero base FormIDs:

```text
@state drop-before 0x000139BD,0x00012EB7
```

No trailing newline. The tag matches `[A-Za-z0-9_.-]{1,40}`. Forms use `0x` plus
eight hex digits; duplicates, zero, malformed IDs, an empty list and more than
16 IDs are rejected. The response `tag` is `drop-before`, not the entire wire
command. Invalid structured requests return `available:false` with
`reason:invalid_state_request`. Ordinary raw tags still preserve their exact
bytes. `@perf ` keeps its existing meaning. All requests share the existing
atomic file, enqueue acknowledgement and generation/task-drain gates.

Additional available `player_state` fields:

- `carry_weight`: live `ActorValue::kInventoryWeight`, the current carried
  inventory weight, not the maximum carry capacity.
- `item_counts` and `item_weights`: objects keyed by canonical uppercase
  FormIDs. A valid known base object absent from inventory has count zero;
  an unresolved/non-bound form has null count/weight. An uninitialized native
  inventory has null counts rather than fabricated zeroes. Weight is the native
  base-form weight, not an invented quantity or per-instance/perk weight.
  Counts aggregate same-base items and do not prove ownership/instance identity.
  The filtered `GetInventoryCounts(filter, true)` scan runs only for an explicit
  request with IDs, once per request, never per frame or periodic snapshot.
- `equipped`: existing `right`, `left`, `ammo`, plus `two_handed` (right-hand
  weapon type: two-handed sword/axe, bow or crossbow), and `head`, `body`, `ring`,
  `amulet` (worn armor FormID or null). Armor reads use `GetWornArmor(slot,true)`
  only on explicit requested snapshots; those four fields are null on periodic
  snapshots to avoid adding periodic inventory scans.
  `head` is additive protocol 2 for the audited Iron Helmet hotbar equip/unequip
  check; rebuild the probe DLL in Windows CI before using the revised routes.
- `selected_power`, `voice_recovery` (null without a current high process),
  `weapon_drawn`: native shout/power, remaining global voice recovery seconds
  and native drawn state. These do not implement a cooldown or combat policy.
- `hud.compass`, `hud.minimap`, `hud.root`: each `{present,visible}`, or null
  without a HUD movie. `root` observes the active SkyCRPG status clip;
  `hud.data_ready` separately reports its `_skycrpgDataReady` flag. Visibility
  includes ancestor `_visible` and `_alpha`, bounded at 16 ancestors; malformed
  or unresolved chains produce null visibility. A missing clip in an available
  movie reports present=false and visible=false.
- `hud.hotbar`: eight copied display cells, each `{key,kind,name,count}` from
  their text fields (null when unavailable). Names may be visually abbreviated;
  these are presentation observations, not an authoritative assignment API.
  The runner correlates decision logs with native equip/container events and
  requests authoritative counts separately.
- `inventory`: null when InventoryMenu is closed or its list cannot resolve;
  otherwise `{grid_active,selected_name,selected_count,selected_favorite}`. Missing `skycrpgGrid`
  on a stock list means false; an unexpected property type means null. A missing
  selection has null name/count/favorite. `selected_favorite` is a read-only
  nullable boolean copied from SkyUI 6.11 `selectedEntry.favorite`, the member
  used by `InventoryListEntry.setEntry` for the favorite icon. Missing or wrongly
  typed data remains null, never false. This additive protocol-2 field requires
  a CI rebuild of the probe DLL; the favorite-setup runner refuses old snapshots
  before sending a favorite toggle. No request parsing changes are needed.

The exact read-only movie paths were checked against SkyUI 6.11's source:
`source/actionscript/Vanilla/HUDMenu.as`, `HUDWidgets/WidgetLoader.as` and
`source/swf/hudmenu.xml` establish `_root.HUDMovieBaseInstance`;
`CompassShoutMeterHolder.Compass` is the native compass. Our
`modules/ui/scaleform/compass-policy.as.inc` observes `HUDMovieBaseInstance.Minimap`;
`modules/ui/src/minimap/pins.h` pins InfinityUI's
`Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/Minimap.swf` and the three
provider DLLs. `SkyCRPGStatus` and `SkyCRPGHotbar.slot0` through `slot7` come
from `hud-init.as.inc`. Inventory uses `_root.Menu_mc.inventoryLists.itemList`
with `skycrpgGrid`, `selectedEntry.text` and `selectedEntry.count`, matching
SkyUI's InventoryMenu/InventoryLists and our inventory presentation include.
No GFx function is invoked; no movie, player, inventory entry or form pointer
is retained across tasks.

CommonLib 7.2.0 signatures were verified in the supplied local headers:
`UI::GetMovieView(string_view) -> GPtr<GFxMovieView>`, `GFxValue` getters,
`TESForm::GetWeight() const`, `TESObjectREFR::GetInventoryCounts(filter,bool)`,
`Actor::GetWornArmor(BipedObjectSlot,bool)`, `GetEquippedObject(bool)`,
`GetActorRuntimeData().selectedPower/currentProcess`, `GetVoiceRecoveryTime()`,
`IsWeaponDrawn()` and `ActorValue::kInventoryWeight`. All three biped slot
constants and weapon type numbers are present in that pin. Windows MSVC
compilation and live GFx behavior remain NOT_RUN; portable checks do not prove
these engine adapters.

`observation_policy_test.cpp` covers the new parser (including raw-tag
compatibility, duplicates, invalid digits and exact 16-ID boundary) and weapon
classification. Run the existing `tools/testprobe/tests/*_test.cpp` glob to
include it alongside all four earlier suites. The observer remains opt-in and
never ships in either module package.

DYL-170 also reads `item_card_quantity` from the same task-local ItemCard's
`QuantitySlider_mc.value` while a submenu is open (otherwise null). SkyUI's
ItemCard.as sets this slider's maximum/current value in ShowQuantityMenu and
reads that exact value in HandleQuantityMenuInput before native quantitySelect.
The route adjusts with LEFT and requires a requested numeric value of exactly
one **before** sending confirmation, preventing a bad adjustment from dropping
an unintended quantity. Windows/runtime validation remains NOT_RUN.
