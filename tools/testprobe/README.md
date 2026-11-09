# SkyCRPG TestProbe (internal tests only)

`SkyCRPG_TestProbe.dll` is a read-only SKSE observer for the unattended-test
runner. It is **never shipped** in Camera/UI release ZIPs or manifests. It
installs no hooks, emits no input, changes no game state and has no runtime
dependency on either SkyCRPG module. Inventory reads explicitly use
`a_noInit=true`; no inventory initialization, display-name mutation or item
actions are performed. The optional view samples (below) only read: Havok ray
casts under the world read lock, camera and scene-graph fields. They take
temporary reference counts on scene objects while reading them and briefly
hold the Havok world read lock, which can delay a Havok writer by the length
of one cast.

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
; optional, see "View samples"
bViewSamples=1
iViewIntervalMs=0
iViewRingPoints=8
iViewRingRadius=128
iViewMaxHits=8
bViewMainThreadOnly=0
bViewPeriodicOffMain=0
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
preserved and JSON-escaped, except the reserved `@perf ` and `@state ` command namespaces below (and `@view ` while `bViewSamples=1`). The maximum tag is 65536 bytes. Oversized/unreadable
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
| `probe_loaded` | `runtime`, `skse` (four-part version strings), `probe_version`, `snapshot_dispatch` (`"ui_task_drain_v3"`), `perf_protocol` (`1`), `observation_protocol` (`2`), `build_commit` (`"unknown"` if not supplied), `enabled` (always `true`; a disabled probe writes nothing), `state_interval_ms`; only with `bViewSamples=1`: `view_protocol` (`1`), `view_interval_ms`, `view_ring_points`, `view_ring_radius`, `view_max_hits`, `view_main_thread_only` |
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
| `view_sample` | Only with `bViewSamples=1`. Camera pose, projection and geometric-clearance rays; see "View samples" |

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
malformed `bEnabled` values. CI builds and runs every portable test on
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

Run **every** portable contract (`*_test.cpp`, nine since P5.5-01) using the same glob as CI:

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
  check. It reports the first worn armor in the Head, Hair or Circlet slot,
  because vanilla helmets often skip Head (Iron Helmet BOD2 = slots 31 and 42;
  live run auto-dyl170-ui-main-20261029T004643Z saw the equip event with `head`
  null under a Head-only read).
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
Only the read-only cursor methods described below are invoked; no movie,
player, inventory entry or form pointer is retained across tasks.

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

`inventory.cursor` is an additive protocol-2 observation while InventoryMenu is
open. `x` and `y` are root stage coordinates; `over_list` and `hit` are the
read-only SkyUI list and selected-clip hit tests. `selected_clip` contains
`x_min`, `x_max`, `y_min`, `y_max` from `getBounds(_root)` and `visible` from
`_visible`. Missing, nonnumeric or nonfinite coordinates and bounds are null;
missing or nonboolean hit-test results are null, not false. An unavailable clip
is null. `getClipByIndex`, `getBounds` and `hitTest` are invoked only on local
snapshot values; no GFx object is retained. `cursor` is present whenever
`inventory` is not null. Live behavior is NOT_RUN.

For an active grid, `inventory.cursor.selected_cell` gives the painted 165×68 cell mapped into root coordinates; it is null if the grid, selected clip, mapping or finite coordinates are unavailable. `selected_clip` remains the native `getBounds` evidence and can be inflated by hidden children. The mapping is a snapshot and retains no GFx values.

`magic` is null when MagicMenu is closed. When open, it contains `selected_name`, `selected_filter_flag` (the raw numeric `filterFlag`) and `selected_favorite` (derived from `filterFlag` bit 1, mask `1`); unavailable or invalid values are null. It reads `_root.Menu_mc.inventoryLists.itemList.selectedEntry` without mutation. `selected_filter_flag` is additive under protocol 2. Runtime NOT_RUN.

## Proton performance diagnostics (DYL-172)

The opt-in observer now uses CommonLib 7.2.0's PrependEventSink, with a one-time
input_registration record of the probe and native sink indices. It never stops
or modifies an input event. During each capture it retains at most 32 input_diag
records in fixed storage; the timer emits them and stop flushes the remainder,
outside perfMutex. Stop counters include input_buttons_seen, input_downs_seen,
input_accepted and input_diag_count. No input callback formats JSON or writes files.

process_memory retains PrivateUsage/working set and adds private_commit_bytes
from a timer-thread VirtualQuery walk of MEM_COMMIT/MEM_PRIVATE regions, its total
and matching region counts, duration in microseconds and completeness. Failed
or bounded-out walks emit null bytes and one diagnostic log. The walk ends at
the maximum application address, rejects zero/nonprogress/overflow and caps at
one million queries or a 100ms deadline checked between queries.

See [performance evidence](../../docs/evidence/p4/performance.md#dyl-172-proton-evidence-correction--2026-11-02)
for source citations, timing semantics, source selection and remaining live gates.
Use the existing portable test glob; it includes proton_policy_test.cpp.

## View samples (P5.5-01, view protocol 1; runtime NOT_RUN)

`view_sample` answers "where is the camera that renders, where does the player
appear on screen, and what geometry lies between them". It is **geometric
clearance, a diagnostic**: it is not rendered visibility and it never passes an
acceptance measure by itself (`docs/phases/PHASE_5_5.md`, "Acceptance
measures"; design and binary audit in `docs/evidence/p5.5/visibility-instrument.md`).
Rebuild the Windows probe in CI before use; earlier DLLs ignore these keys.

**Usage rule.** In the default mode the sample runs on a job thread (see
Dispatch). Use it for `@view` requests at **settled poses** on a disposable
profile: player standing, no equip change, not at a load door or a cell border.
Do not run periodic samples through movement, cell borders or load doors in the
default mode. For that reason the periodic timer is refused unless
`bViewPeriodicOffMain=1` (or `bViewMainThreadOnly=1`) is set explicitly.

### Settings

All keys are in `[Probe]`, read once at load, digits only.

| Key | Default | Meaning |
| --- | --- | --- |
| `bViewSamples` | absent = 0 | Exactly `1` enables the `@view ` request namespace, the view timer and the `probe_loaded` view fields. Anything else: the probe behaves exactly as before (an `@view x` request stays a raw `player_state` checkpoint tag). |
| `iViewIntervalMs` | 0 | Periodic samples; 0 or invalid = off; 1-99 are raised to 100; at most 2147483647. Ignored unless `bViewSamples=1`. |
| `bViewPeriodicOffMain` | 0 | Exactly `1` allows the periodic timer in the default (off-main) mode. Without it, or `bViewMainThreadOnly=1`, a nonzero interval creates no timer and one `view_sample` with `reason:"periodic_opt_in_missing"` is written at data load. |
| `iViewRingPoints` | 8 | Floor ring points, 0-16 (invalid = 8). |
| `iViewRingRadius` | 128 | Ring radius in game units, 16-1024 (invalid = 128). |
| `iViewMaxHits` | 8 | Distinct references recorded per ray, 1-16 (invalid = 8). |
| `bViewMainThreadOnly` | 0 | `1` samples only when the drain runs on `Main::threadID` (see Dispatch); otherwise retries and then reports `off_main_thread`. |

`probe_loaded` then also carries `view_protocol`, `view_interval_ms`,
`view_ring_points`, `view_ring_radius`, `view_max_hits`,
`view_main_thread_only`, `view_periodic_off_main` and `view_periodic`
(`"off"`, `"on"` or `"refused"`).

### Request grammar

`@view <label>` with **no trailing newline**, written through the same atomic
request file as checkpoints; `<label>` matches `[A-Za-z0-9_.-]{1,40}`. The
answer is one `view_sample` with `requested:true` and `tag` = the label. An
invalid label (empty, too long, other characters, extra text, trailing newline)
is answered with `available:false`, `reason:"invalid_view_request"` and `tag` =
the raw text after `@view `. `@view` with no space is an ordinary raw tag. Send
one request at a time and wait for the matching `tag`. Requests are polled by the
state timer, so `iStateIntervalMs=0` also disables `@view` requests.

### Event

Every queued request or periodic tick produces exactly one line; a sample that
cannot be taken is emitted with `available:false` and a `reason`, never omitted
and never clear. Field order is stable; the names below are the contract the
route generator uses.

| Field | Meaning |
| --- | --- |
| `view_protocol` | `1` |
| `seq` | 1, 2, ... over all `view_sample` lines of this process, in file order |
| `requested`, `tag` | `true` and the label for `@view`; `false` and no `tag` for periodic samples |
| `available`, `reason` | `reason` is `null` when available, otherwise one code from the table below |
| `frame` | Engine update counter (Address Library ID 411489) at sample start, or `null` before the binary guard passed. A label for correlating samples, not a consistency check: its only writer (RVA 100A02B) is reached from `Main` ID 36555, which the binary review found runs after the main thread has waited for the UI job, so it cannot change during a sample. |
| `since_last_ms` | Probe QPC milliseconds since the previous `view_sample` of any kind, `null` for the first |
| `since_last_frames` | `frame` minus the previous sample's `frame`, `null` if either is unknown |
| `coalesced_ticks` | Periodic ticks merged into this sample because the previous periodic sample was still outstanding; always 0 for requests |
| `hops`, `queued_ms` | Main-queue retries (only with `bViewMainThreadOnly=1`) and milliseconds from queueing to emission |
| `sample_ms` | Milliseconds spent inside the sample (from the start of the engine reads to emission), `null` when the sample never started; this time adds to the frame (see Dispatch) |
| `current_thread_id`, `captured_main_thread_id`, `main_thread_id` | As in `player_state` |
| `camera_state` | Only on `reason:"camera_state"`: the numeric camera state |
| `camera` | `state` (9 = third person), `pos {x,y,z}`, `yaw_deg` (0 = +Y/north, clockwise), `pitch_deg` (positive = looking down), `fov_deg`, `vfov_deg` (from the NiCamera frustum, `null` if orthographic or invalid), `world_fov_deg` (PlayerCamera::worldFOV), `ortho`, `node` (`"world_root_camera"`), `under_camera_root` (that NiCamera is a child of PlayerCamera's cameraRoot), `camera_root_pos`, `camera_root_delta` (distance), `rotation_forward_delta_deg` (angle between the projection's view axis and the node rotation's X axis; a consistency check, `null` if degenerate) |
| `player` | `pos {x,y,z}` (reference position, at the feet), `heading_deg`, `cell {form_id, editor_id}`, `interior` |
| `offset` | `camera.pos - player.pos` as `{x,y,z,distance}`; the rig-hold measure uses this, not a goal position |
| `points` | Five entries named `head`, `chest`, `feet`, `shoulder_l`, `shoulder_r` (shape below) |
| `ring` | `iViewRingPoints` entries `ring_0`... at `iViewRingRadius` around `player.pos`, `ring_0` on the camera's heading (far side), then clockwise seen from above |
| `player_box` | `{x_min,x_max,y_min,y_max}` over the in-front player points in normalized screen space, or `null` |
| `counts` | `{points:{clear,faded_only,blocked,indeterminate}, ring:{...}}` |
| `casts` | Havok casts used by this sample (at most 256) |
| `on_main_thread` | Whether the sample ran on `Main::threadID` |
| `config` | `ring_points`, `ring_radius`, `max_hits`, `layer` (41), `end_clearance` (2), `point_lift` (8), `main_thread_only`, `periodic_off_main` |

`camera`, `player`, `offset`, `points`, `ring`, `player_box`, `counts`,
`casts`, `on_main_thread` and `config` are present only when `available:true`.

Each point and ring entry:

```text
{"name":"head","world":{x,y,z}|null,"screen":{x,y}|null,"on_screen":bool|null,
 "ray":{"status":"clear|faded_only|blocked|indeterminate","transmittance":number|null,
        "hits":[{"ref":"0x........"|null,"base":"0x........"|null,"layer":N,"distance":units,"surfaces":N,
                 "treatment":"opaque|faded|hidden|unknown|volume","alpha":number|null,"reason":null|"..."}],
        "truncated":bool,"reason":null|"...","skipped_player":N,"casts":N}}
```

- Points: `head`, `chest`, `shoulder_l`, `shoulder_r` are the third-person
  skeleton nodes `NPC Head [Head]`, `NPC Spine2 [Spn2]`, `NPC L UpperArm [LUar]`
  and `NPC R UpperArm [RUar]` (case-insensitive name match, at most 1024 nodes
  searched). The search runs once per player 3D root and load; later samples read
  only the held nodes' world positions. A missing node (for example a beast
  skeleton) gives `world:null` and an indeterminate ray with reason
  `point_unavailable` (or `skeleton_cap`). `feet` and ring points are 8 units
  above `player.pos.z`; ring points assume a flat floor, so steps and slopes can
  add conservative hits.
- `screen`: normalized `x` 0..1 left to right and `y` 0..1 **top to bottom**
  (pixel = `x*width`, `y*height`). `on_screen` is true inside the 0..1 square,
  false for a point outside it or at/behind the camera plane (then `screen` is
  null), and `null` only when the point or the projection is unavailable. The
  math is the engine's `WorldPtToScreenPt3` with the world camera's matrix and
  port, except that the engine mirrors points behind the camera and the probe
  does not.
- Rays run from `camera.pos` towards the point and stop 2 units short of it.
  After each hit the probe casts again 1 unit past it. The player's own
  surfaces are skipped and counted (`skipped_player`, at most 8). A ray uses at
  most 48 casts.
- **One hit per reference.** Another surface of a reference already on the ray
  (several collision shapes, or the entry and exit faces of a closed mesh) is
  merged: its `surfaces` count rises, `distance` stays the nearest, and it uses
  no `iViewMaxHits` slot and no extra transmittance factor. Identity is the
  reference's form ID, or for a hit without a reference the resolved object (or
  collidable) address. After `iViewMaxHits` distinct references, one more
  reference before the point, or the 48-cast limit, sets `truncated:true`.
- `layer` is the hit collidable's collision layer. The ray uses layer 41
  (`L_LOS`). From the Skyrim.esm COLL records it collides with layers 0, 1, 2,
  4, 5, 6, 7, 8, 9, 10, 13, 14, 16, 17, 32, 38, 39, 40, 41, 43, 44 and 53. Unlike
  the camera's own layer `L_CAMERA` (39) it does **not** see `L_TRANSPARENT` (3),
  `L_DEBRIS_LARGE` (20), `L_TRANSPARENT_SMALL` (26), `L_TRANSPARENT_SMALL_ANIM`
  (28) or the character controller (30); neither sees invisible walls (27) or
  water (11). So `clear` here does **not** mean the camera's collision caster is
  clear, and drawn objects that exist only on those layers are invisible to the
  rays. A hit with no reference has `ref:null`.
- `treatment`, decided once per reference:
  - `volume` for hits on layers that are not drawn surfaces: `L_SPELL` (7),
    `L_CLOUDTRAP` (16), `L_CONEPROJECTILE` (38), the query layers 39, 40, 41,
    43, 44 and `L_DETECTION` (53). Volumes never block and are not walked.
  - `opaque` with reason `landscape` for terrain (layer 13) and any collidable
    in system group 1, without resolving its scene object (that lookup walks the
    TES cell grid).
  - Actor hits (form type ACHR) read only the root's app-culled flag: `hidden`,
    otherwise `unknown` with reason `actor` (actor 3D is not walked).
  - Other references: their 3D is walked (at most 256 nodes). `hidden` only
    when the reference root is app-culled, or when every drawn part is
    app-culled. A culled node below the root (the collision node, a switch
    branch) hides only its own subtree, so a drawn opaque sibling still makes
    the reference `opaque`. Otherwise `opaque` when any drawn part has no
    blending alpha property, or blends with a lighting material alpha of 1;
    `faded` with `alpha` = the highest lighting material alpha when every
    drawn part blends with an alpha below 1 (the DYL-201 fade); `unknown` with
    a `reason` (`unresolved`, `node_cap`, `non_lighting_blend`,
    `invalid_alpha`, `no_geometry`) when it cannot be read.
- `status`: `indeterminate` if the cast could not run (`reason`
  `invalid_geometry`, `no_havok_world`, `cast_budget`, `point_unavailable`,
  `skeleton_cap`) or `truncated`; otherwise `blocked` if any hit is `opaque` or
  `unknown`; otherwise `faded_only` if any hit is `faded`; otherwise `clear` (no
  hits, or only `hidden` and `volume` hits). A faded hit is never folded into
  `clear`.
- `transmittance`: 1 for `clear`; for `faded_only` the product of `1 - alpha`
  over the faded **references** (eight stacked 0.35 fades give 0.65^8, about
  0.032; one faded wall hit on entry and exit gives 0.65); 0 when an opaque hit
  blocks; `null` for an unknown block or indeterminate.

### Unavailable reasons

Checked in this order; each is logged once per process in `SkyCRPG_TestProbe.log`.

| `reason` | Meaning |
| --- | --- |
| `periodic_opt_in_missing` | Written once at data load when `iViewIntervalMs>0` without `bViewPeriodicOffMain=1` or `bViewMainThreadOnly=1`; no timer runs |
| `view_busy` | Four samples already outstanding |
| `invalid_view_request` | Bad `@view` label |
| `enqueue_failed` | SKSE task interface refused the task |
| `outside_task_drain`, `generation_changed`, `not_loaded` | As for `player_state` |
| `off_main_thread` | `bViewMainThreadOnly=1` and no main-thread drain within 600 retries or 5 s |
| `player_unavailable` | No player or 3D, or Main/Loading Menu open |
| `game_paused` | `UI::GameIsPaused()` (menus): the scene is stale |
| `camera_unavailable` | No PlayerCamera, camera state or cameraRoot |
| `camera_state` | Camera state is not 9 (third person); the state is in `camera_state` |
| `guard_binary` | A called engine function's entry bytes or address differ from the audited 1.7.104 values (logged with the ID) |
| `world_camera_unavailable` | `Main::WorldRootCamera()` returned null |
| `cell_unavailable` | No parent cell |
| `world_changed` | The load generation or the player's cell changed during the sample |
| `sample_failed` | A C++ exception during the sample |

Layout of the CommonLib accessors the sample uses is not checked at run time:
the accessors are `this` plus a constant, and those constants and the plain
member offsets are compile-time asserted against the binary audit.

### Dispatch, threads and cost

Samples run inside the same SKSE UI task drain as `player_state` and read nothing
outside it; the timers only queue. During unpaused play the drain runs inside
the main job list's `UI` job on a worker thread; around menu transitions and
while paused it runs in `Main::Update` on `Main::threadID` (probe history:
359 runs, see the evidence doc). Each Havok cast takes the world read lock the
way the engine's own `bhkWorld::PickObject` does and resolves the hit before
releasing it. The lock lets readers delay a writer but does not let a waiting
writer block a nested reader; the probe still never nests it.

Scene-graph reads (camera, held bone nodes, hit references) take no engine
lock: no lock protects them, and concurrent structural edits by other jobs of
the same job-list segment are not excluded. The probe holds a reference count
on every node it reads; if the engine drops its own reference meanwhile, the
probe's release runs that node's destructor on the job thread. Superseded bone
holds are kept until a load message or a menu event on `Main::threadID`
releases them (at most 10; beyond that they are released on the job thread).
This residual risk is stated in the evidence doc; `bViewMainThreadOnly=1`
removes it at the cost of no samples during unpaused play.

The sample runs while SKSE holds its UI queue lock and while the main thread
waits for the UI job, so its whole duration (`sample_ms`) adds to that frame's
time. A sample casts at most 256 rays in total (5 points + ring, at most 48
each) and walks at most 256 nodes per distinct hit reference, plus up to 1024
skeleton nodes when the player's 3D root changes. The real cost is unmeasured
(NOT_RUN); do not combine view samples with a performance capture whose numbers
matter.

### Cadence and what a sample can claim

A periodic sample is queued only while a game is loaded and only when the
previous periodic sample has been emitted; ticks in between are counted in
`coalesced_ticks`. A sample describes one moment: it says nothing about frames
between samples. `since_last_ms` and `since_last_frames` state how long the gap
was. "No interval longer than 1 s with the player hidden" can only be checked at
a cadence well under 1 s with no gap above it, and "zero blank frames" cannot
be claimed from view samples at all; it needs per-frame image evidence.

`view_policy_test.cpp` covers the request grammar and settings bounds, the
periodic opt-in, projection (including behind-camera points and non-unit
ports), pose angles and frustum FOV, ring and ray planning, every folding rule,
per-reference merging, transmittance (single, a two-faced wall, eight stacked
0.35 fades), treatment aggregation (including a culled proxy beside an opaque
sibling), volume and landscape layers, cadence coalescing and the exact JSON
shape. It does not compile the Windows adapter or prove any engine behaviour;
all in-game rows are NOT_RUN.
