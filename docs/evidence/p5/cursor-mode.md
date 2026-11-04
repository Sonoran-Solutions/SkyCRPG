# DYL-200 / P5-03 — world cursor mode candidate

## Sixth pass — overlay callback threads, from the first in-game route (2026-11-04)

**Route evidence on `503b27b`** (Camera DLL `fd5e9148…`, Selection DLL `3a12f12f…`, SkyrimSE.exe `846efccf…`):

| Run | Result | Evidence |
| --- | --- | --- |
| `route-cursor-1` hover | BLOCKED (harness) | Runner recovery refused the Fader Menu still open after `coc`. Cursor routes now settle for 12 s after `coc`, like the other route sets. |
| `route-cursor-2` hover | FAIL | All steps ran and restore passed, but cursor mode never stayed active. Each B showed `SkyCRPGCameraWorldPointer`, which closed 17–18 ms later. Camera.log: `Cursor refused/exit reason=thread`. |

**Cause.** An independent binary check of 1.7.104 found the following.

- In unpaused frames, `UI::ProcessMessages` (AL 82082, which also invokes menu creators) and `UI::AdvanceMenus` (AL 82083) run as the job-system "UI" job (AL 39042) on a worker thread, holding UI+0x158.
- `PostDisplay` runs on main from the UI draw (AL 82084), after main waits for that job's event.
- `Main::threadID` itself is correct: it is written once by the Main constructor on the WinMain thread.

The Overlay's constructor, `ProcessMessage` and `AdvanceMovie` refused off-main, so every entry ended on the next frame.

**Fix.**
- The constructor, `ProcessMessage` and `AdvanceMovie` no longer require main. They run under the engine's UI+0x158 and touch only this movie and the atomic stop.
- `create()` publishes a single process-lifetime instance through an atomic and reuses it, with an added reference, on later shows. Main therefore never holds a menu the UI job could free.
- Main-side movie and menu-stack access (`dimensions`, `hide`, `paint`, `ownMenu`, `nativeMenu`) takes the same reentrant UI+0x158 lock.
- `PostDisplay` skips drawing off main, which the binary check found happens only on the ServingThread during blocking loads.

Not verified: the cost of main waiting on UI+0x158 while the UI job holds it, and in-game behaviour.

## Fifth pass — fourth review fixes, orchestrator (2026-11-04)

A focused independent review of `531dcd5..3c9e896` found one blocker, one major and four minor issues. The orchestrator fixed them directly. This pass had no independent review of its own; it is covered by portable tests and Windows CI only.

| Finding | Resolution |
| --- | --- |
| **B1** ActivatePickRef re-resolves its local handle in 41003 for perk Activate entry points, so substituting only `out` let perks run on the centre pick X | `resolveActivation` writes the permitted handle back. It substitutes only when the incoming handle is the permitted handle or the current crosshair target, and refuses commanded-actor or furniture fallback handles while cursor mode is active. |
| **M1** Hysteresis kept A when the pointer sat on neighbour B | `cursor::hover` keeps A inside 1.25 × radius unless the chosen candidate is clearly nearer (1.25² × d(B) < d(A)). |
| m1 A withheld list blocked revocation | `clearAway` runs against the last projections while the list is withheld; only new binds wait for a ready list. |
| m2 Exit-refusal gap | The cursor handle is stored before `data->target` is written, and the inactive refusal no longer needs a live permit. |
| m3 Entry could start on aged data | Entry requires `Freshness::fresh`; only a running session gets the three-update grace. |
| m4 Extra diagnostic `choose` row on reset | Accepted: diagnostics only. |

Tests: all 10 Camera and 2 Selection portable executables PASS with the exact CI flags, including new hover and freshness cases. Game behaviour is NOT_RUN.

## Fourth pass — third independent review fixes (2026-11-03)

**PASS: focused code, portable tests and offline checks. NOT_RUN: this diff's Windows build and every in-game row below.** This section supersedes conflicting historical pass descriptions. Base `531dcd5` passes Windows CI per the owner's brief; that is not build evidence for this uncommitted diff. Host `dq-MEG-Z590-Trident-X-MS-B926`, Linux, g++15.2.0 (Ubuntu15.2.0-16ubuntu1), Python3.14.4. Read the required project context and connected DYL-200; supplied owner rulings supersede its obsolete BTPS text. No commits, pushes, PRs, game launches, downloads, messages, UI edits, shared-file or build/packaging changes.

| Finding | Resolution (code/portable scope only) |
| --- | --- |
| M1 | Entry viability always uses `passthrough(cursorContext(rawContext,true))`, matching continuation. Ordinary Selection still uses its unmodified context while cursor mode is off. Definite cursor-eligible combat/sneak passthroughs advance entry liveness even without an ordinary scan. |
| m1 / m7 | Register at DataLoaded under `processMessagesLock`; only registration loses the main-thread restriction, never creation. One fallback is available on main outside an already-owned lock. A missing UI or nested ProcessMessages tick defers that fallback; a name collision is terminal. Tests exercise deferral, once-only fallback, success and collision, replacing the unused-request assertion. |
| m2 | Publish the permitted native RefHandle before its token. The guarded resolver uses that stored handle while native Pick may have overwritten the incoming centre target, then rechecks token and FormID. Ordering is documented beside the resolver. |
| m3 | Append `CursorFrame::listReady`: a withheld viable list skips hover revocation/submission; a complete empty list still clears hover. Transient unknown Pick validation withholds the list. The exact-size API check requires rebuilding Camera and Selection together; legacy snapshot/list behavior is unchanged. |
| m4 | Only rejected replies/immediate failures enter refresh backoff. Pending identical submissions wait for their reply; successful A→B→A can proceed within one refresh. Authorized hover remains until it leaves 1.25× radius, including when the ordinary nearest choice would differ. |
| m5 | Time expiry starts a grace of three further `gUpdateCounter` advances without liveness. This handles a hitch even after several ordinary updates since the last scan. Repeated publications cannot renew liveness; counter wrap and actual session loss are tested. |
| m6 | Atomic `end()` reports whether cursor mode was active. `setCursorMode(0)` releases policy/pending external work only on that transition; failed entry/duplicate exit preserves legacy work. |
| m8 | A corpse uses its world-bound centre only with positive finite radius and finite XYZ; otherwise it follows the existing OBND centre path. |
| Exit nit | Inactive-mode activation refuses the last injected handle until the next Pick completes and clears/replaces its permit. No cross-thread CrosshairPickData write. The previously documented native hold-grab window remains: StartGrabObject bypasses this resolver. |
| Permit-test nit | The runtime Pick scope guard is now the portable-tested destructor itself. Cases exercise successful retention, early-return cleanup, wrong target, exit during Pick and ordinary noncursor behavior. Removed the duplicate permit assertion and the empty-frame initializer assertion. |

**Source:** [CommonLibSSE-NG7.2.0, revision7a60f4de794095d7b0f8928d1b930a52e9a7da83](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), inspected locally: `RefHandle` is uint32, `native_handle()` supplies it, spin-lock owner is DWORD0, UI lock is158, NiBound radius is0C. **Source (offline binary):** recomputed executable SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` and Address Library SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`. Static **PASS**: ProcessMessages1169E77 passes UI+158 to guard199FA0;199FC3 compares owner DWORD0 to current thread and199FC7 increments count+4 on recursion. Message dispatch116A3DA and MapMenu camera-update call99C6AC support the nested-update case. ActivatePickRef75159E reads crosshair+4; guarded call7515FE remains `E8 DD D5 A2 FF`. World-bound aggregationEE2944 clears radiusF0, with centreE4/E8/EC and radiusF0 copied atEE297F..EE29AD. Audit script/log: `/tmp/dyl200-pass4/audit.py`, `audit.log`; preliminary unrelated lock disassembly is retained in `audit-exploratory.log`, superseded by the corrected addresses. No binary material was added to Git and no GPL code was copied.

Jobs were declared before execution in `/tmp/dyl200-pass4/jobs.md`, owner this fourth-pass worker, with logs, PIDs, deadlines and task-only timeout cleanup. Actual results:

| Command | Actual result |
| --- | --- |
| `timeout 600 bash /tmp/dyl200-pass4/focused.sh` | **PASS**,2 cursor executables; `focused.log` and final `focused-final.log` after removing the remaining initializer-only assertion. |
| `timeout 600 bash /tmp/dyl200-pass4/portable.sh` | **PASS**,10 Camera +2 Selection executables; `portable.log`, tool session54348 exited0. |
| Camera, each `modules/camera/tests/*_test.cpp`: `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"` | **PASS**, exact CI commands; `RUNNER_TEMP=/tmp/dyl200-pass4`. |
| Selection, each `modules/selection/tests/*_test.cpp`: `g++ -std=c++20 -pthread -Wall -Wextra -Wshadow -Werror -pedantic "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"` | **PASS**, exact CI commands. |
| `timeout 600 python3 -B -m unittest discover -s tools/tests` | **PASS**,26 tests, `OK`; `python.log`. Produced archives are disposable synthetic test fixtures, not DLL/package evidence. |
| `timeout 120 python3 -B /tmp/dyl200-pass4/audit.py` | **PASS**, hashes/address assertions and inspected disassembly; `audit.log`. |
| `git diff --check` | **PASS**. |

Unverified: native MSVC /W4 /WX compilation/link, actual registration/callback timing, menuMap reader synchronization on the fallback, Pick/Activate concurrency, corpse projection, hover feel, native hold-grab, all runtime configurations and acceptance routes. No new DLL/package hashes or owner acceptance. Engineering decisions: explicit list readiness, terminal registration failure with one safe fallback, three-update grace after age expiry, atomic permit handle and exit guard; no owner ruling changed. **Owner questions:** none new; existing binding/feel proposals remain pending runtime evidence and owner review. **Proposed shared-file changes:** retain the existing proposals below; additionally document the paired cursor API rebuild and leave runtime gates open.

## Third pass — second independent review fixes (2026-11-03)

**PASS: implemented review fixes and portable/offline verification. NOT_RUN: this diff's Windows build and every in-game behavior.** This section is the current handoff and supersedes the historical first/second-pass design statements below. Base/head remains `bb86f473a178acbdb6c5827d82118af04faf176c`; the owner reports that base passed Windows CI, which is not build evidence for this uncommitted diff. Host: `dq-MEG-Z590-Trident-X-MS-B926`, Linux, g++ Ubuntu15.2.0-16ubuntu1, Python3.14.4. Read AGENTS, README, STATUS, MVP scope, PHASE_5/P5-03, targeting research, ARCHITECTURE and the connected DYL-200 issue. D026 and the supplied owner brief supersede the ticket's obsolete BTPS text.

No commits, pushes, PRs, Skyrim/Proton launches, deployments, third-party messages, downloads, UI edits, route edits, shared-file edits or build/packaging changes. No new DLL/package hashes exist. Python packaging-test archives are disposable mock fixtures, not candidate artifacts.

### Finding resolutions

| Finding | Resolution and scoped result |
| --- | --- |
| **B1 — registration never runs** | **PASS (code/policy):** DataLoaded now posts readiness and logs deferral. `service()` attempts registration on its first main-thread tick, before update coalescing, toggle consumption or the idle early-out. The `UI::processMessagesLock` guard remains. Missing UI logs once and retries without consuming a pending toggle; conflicting registration logs once and refuses. `prepareOverlay` asserts that no active/pending session precedes registration and also refuses that invalid state in release builds. Tests cover a toggle before service, deferred/retried registration, idle registration and no duplicate registration. Actual host registration/overlay creation **NOT_RUN**. |
| **M1 — freshness confused with usability** | **PASS (code/policy):** separate `aliveRefreshes` advances on definite finished attempts: complete/empty scans, exhausted eight-candidate validation, reference/visit caps and ray-budget exhaustion. Contention and other indeterminate engine reads do not advance it. `completedRefreshes` remains diagnostic. `CursorFrame` carries the validated `refreshSeconds`; Camera uses `max(0.5, 3*refreshSeconds)` (up to3s), with repeated reads unable to renew age. Unusable publications have `available=0,count=0`; they need not end the session. Known ray-budget failures in cursor mode wait for the configured refresh interval; ordinary noncursor retry behavior remains unchanged. Exhausted/ray-capped/capped versus contended tests and the1s configuration deadline PASS. |
| **M2 — unrelated passthrough ends mode** | **PASS (code/policy):** apply a cursor-only Context adjustment at scan and final injection validation, clearing only `nativeLiveActor` and `combat`. Sneaking itself is preserved for pointer/pickpocket ranking. Armed, telekinesis, grab/special, menus, death and every other state retain their original precedence. Outside cursor mode the original Context passes through unchanged. Tests cover both modes and preserved passthroughs. |
| **M3 — corpse exclusion** | **PASS (code/offline):** loaded dead actors project from their 3D root's `worldBound.center`, with finite XYZ checks. Other references retain the existing transformed OBND centre. Missing/deleted/disabled roots still refuse. New layout assertions pin worldBound/E4 and center/0; executable audit below confirms the offsets. Corpse hover/activation and ragdoll alignment **NOT_RUN**. |
| **Minor1 — stale hold-grab** | **PASS (documented residual risk, not a runtime fix):** retain the Selection-owned Pick/resolver architecture. StartGrabObject reads `CrosshairPickData+4` directly; it bypasses the ActivatePickRef guard. A hold crossing the native0.2s threshold after revocation but before the next Pick can still grab the stale reference. Pick clears it at its next outcome. No main-thread crosshair write was added because Pick runs on a job thread and that would introduce an unproven cross-thread mutation. Runtime window/impact **NOT_RUN**. |
| **Minor2 — clear before busy** | **PASS (code/policy):** all nonzero cursor requests run atomic `clear()` before trying `CursorAccess`, checking cycles or writing the mailbox. A busy/mismatched request cannot leave the old token authorized. The portable held-gate test proves old permission is already gone inside the failing submit callback and late publication cannot restore it. Zero remains gate-free. |
| **Minor3 — E dropped during identical Pick** | **PASS (code/policy):** removed unconditional permit zeroing at Pick entry. The prior matching token/FormID permit survives while validation runs; a refused outcome revokes it (also for null data), and a successful injection publishes its token. The resolver still checks token and resolved FormID before/after resolution; changed tokens/handles cannot borrow an old permit. Capture exclusive state before native Pick so an overlapping exit still reaches cleanup. The reported overlap/drop is a **Hypothesis**, not an observed game failure; exact Pick/Activate ordering **NOT_RUN**. |
| **Minor4 — delayed loss detection** | **PASS (code):** `look()` and `PostDisplay()` share `captureExit()`, including the single-atomic `setCursorMode(2)` query before capture/render. No snapshot polling was added to these paths. Live same-frame loss handling **NOT_RUN**. |
| **Minor5 — legacy API regression** | **PASS (code/policy):** restored the original gated v2 snapshot/candidate paths, unavailable on contention, `scanReady` requirement and frozen legacy candidate order/count. Also restored unavailable for a full legacy external mailbox, discovered by comparison with `398f427`. Cursor-extension calls alone expose busy; even legacy `setTarget` during cursor mode translates busy back to unavailable. Camera still uses the independent cursor publication. Every `reset()` now publishes the cleared state, covering all early returns, while preserving the legacy invalidWorld snapshot reason. Portable legacy result/readiness cases PASS; actual DLL calls **NOT_RUN**. |
| **Minor6 — per-frame rejected hover** | **PASS (code/policy):** a bounded16-ID attempt set permits each target at most once per observed alive refresh. Busy, rejected and missing-reply requests wait for the next definite scan; direct hops to a different target can proceed immediately, and hopping back does not bypass backoff. Explicit cycle changes reset the hover attempt set. Empty revocation bypasses throttling. Portable retry/direct-hop cases PASS. |
| **Minor7 — walking projection lag** | **PASS (code):** re-resolve/re-project the current cached candidate IDs once per engine update, independent of a new Selection refresh, with the existing16-reference/32-camera-child bounds. No extra discovery/LOS scan is added by projection. Selection's normal validation cadence remains its own. Live alignment/per-frame cost **NOT_RUN**; sampling before the outer SmoothCam transform can still leave up to one frame of visual lag. |
| **Minor8 — exit reasons** | **PASS (code):** stale age logs `stale`; lost/absent Selection logs `selection`; native focus logs `menu`; posted load/death/cell/thread/etc. reasons are preserved. `overlay` is reserved for actual overlay/stack/paint failure. Portable stale/lost policy cases PASS; log timing in game **NOT_RUN**. |

### Offline verification and source provenance

**Source:** [CommonLibSSE-NG7.2.0, revision7a60f4de794095d7b0f8928d1b930a52e9a7da83](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), read locally at `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0`. `TESObjectREFR::Get3D()` delegates to Get3D2, `NiAVObject::worldBound` is atE4, `NiBound::center` at0. `UI::processMessagesLock` is at158; `BSSpinLock::Lock` increments the lock count when the current thread already owns it, and `Unlock` decrements that count. `UI::Register` inserts into menuMap without acquiring this lock itself. These support keeping the explicit guard. No GPL/BTPS implementation was copied or used.

**Source (offline executable, no public binary URL):** exact local SkyrimSE.exe runtime1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; Address Library `versionlib-1-7-104-0.bin`, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`. Both hashes recomputed in this pass. All addresses below are RVAs (image base0x140000000).

- **PASS:** ID40552→751870 StartGrabObject;75189D reads `edx,[rax+4]`, followed by a separate handle resolver at7518B7. The existing guarded ActivatePickRef call remains7515FE with bytes `E8 DD D5 A2 FF`; it does not guard grab.
- **PASS:** ActivateHandler table ID208723→1935388, runtime slot6→7B2660. At7B26B8 it loads float from20B8640 (`0.20000000298023224`), compares held time at event+2C, then the qualifying path calls StartGrabObject at7B275D. This verifies0.2s, not a guessed hold threshold.
- **PASS:** NiNode table ID237098→1A46540, UpdateWorldBound slot2F→EE2920. The native aggregation copies child XYZ atE4/E8/EC into rootE4/E8/EC (EE297F..EE29A7), radiusF0, then combines further bounds via EFECA0. No new relocated function call is needed for corpse projection.

Audit script/log `/tmp/dyl200-pass3/audit.py`, `audit.log`; decoder `/tmp/cursor-review/al.py`; supplied full disassembly `/tmp/cursor-review2/full.asm`. Initial exploratory lookup used the pre-1.7.99 ActivateHandler slot4 (a no-op); source inspection selected the runtime-shifted slot6 before recording the verified conclusion. No game bytes/assets were added to Git. Static layout/call facts do not establish live root validity, UI callback timing or behavior.

### Jobs, commands and actual results

Pre-job records: `/tmp/dyl200-pass3/jobs.md`. Owner: this third-pass DYL-200 worker. Logs/PIDs and helper scripts stay under that directory. Each job had a timeout, expected exit0/PASS/OK completion and task-only timeout cleanup. All jobs completed normally; no task-created long-running process remains.

| Command | Actual result |
| --- | --- |
| `timeout 600 bash /tmp/dyl200-pass3/focused.sh` | **PASS**, both cursor executables, `focused.log`. Focused reruns after adding the release-build registration guard and translating legacy setter contention also **PASS**, `focused-final.log` and `focused-legacy.log`. |
| `timeout 600 bash /tmp/dyl200-pass3/portable.sh` | **PASS**, all10 Camera +2 Selection executables; exact CI commands below, `portable.log`, shell PID in `portable.pid`, tool session9128 completed exit0. |
| Camera, each `modules/camera/tests/*_test.cpp`: `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"` | **PASS**,10/10. `RUNNER_TEMP=/tmp/dyl200-pass3`. |
| Selection, each `modules/selection/tests/*_test.cpp`: `g++ -std=c++20 -pthread -Wall -Wextra -Wshadow -Werror -pedantic "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"` | **PASS**,2/2. |
| `timeout 600 python3 -B -m unittest discover -s tools/tests` | **PASS**,26 tests,0.641s, `python.log`; packaging outputs are mock fixtures. |
| `timeout 120 python3 -B /tmp/dyl200-pass3/audit.py` | **PASS**, hashes, direct-grab/hold and root-bound audit, `audit.log`. Expanded audit followed new offset evidence; no failing unchanged retry. |
| `git diff --check` | **PASS**, `diff-check.log`. |
| MSVC /W4 /WX build/link/package for this diff | **NOT_RUN**, no native toolchain here; orchestrator CI required. Min/max calls are parenthesized, API prefix/Snapshot unchanged. |
| Every in-game route, frame-cost sample and owner acceptance | **NOT_RUN**; no Skyrim/Proton process launched. |

Portable coverage is policy/gate/ABI behavior, not execution of engine hooks. The modified CursorFrame has an exact size check; old/new cursor pairs fail closed until both DLLs are rebuilt together. The original v2 interface prefix and Snapshot layout remain unchanged.

### Decisions, remaining verification and owner questions

Engineering decisions: document the hold-grab residual instead of racing a main-thread crosshair write; retain exact-size cursor-frame negotiation; retry a rejected ID once per alive refresh while allowing immediate different-target hops; keep legacy API behavior and all optional defaults unchanged. No owner ruling changed and no finding was dismissed.

**NOT_RUN runtime checklist for the orchestrator:** first main-thread registration with a toggle already pending; empty/exhausted/ray-capped/visit-capped scans persisting past500ms and `fRefreshSeconds=1.0`; sneaking while an NPC crosses centre, sheathed combat and all retained passthroughs; loaded corpses/ragdolls; repeated native E during overlapping unchanged Picks; stale hold-grab at0.2s; request-gate contention and rejected-hover backoff; walking projection/cycle latch; Selection loss between update and look/PostDisplay; precise exit logs; Camera-only/UI-only/combined and feature-off regressions; per-frame projection/push cost. These require the serialized game host and fresh DLL hashes/dependency manifest. No runtime result or owner acceptance is inferred from the base's reported CI PASS.

Owner questions: no new decision is required to review this diff. Existing binding/feel questions remain (B, tested controller proposal, sensitivity/radius/latch and visual scale). Review the documented hold-grab residual during the runtime gate; closing that separate native path would need its own tested synchronization or Selection-owned guard.

### Proposed shared-file changes

- Architecture/API: registration occurs on the first main service under the reentrant UI lock; Selection alone owns Pick and the ActivatePickRef resolver guard. Document the separate direct-grab residual, exact-size CursorFrame rebuild requirement, legacy unavailable semantics and alive-based `max(0.5, 3*refreshSeconds)` freshness.
- Controls/UX: cursor selection includes loaded corpses; centre-ray sneak actors and sheathed combat do not interrupt an active cursor session. Pointer projection updates each frame; retry of a rejected target is bounded to one per definite refresh. Empty/capped validation is distinct from stale/lost Selection.
- Status/compatibility: record this diff's portable/offline PASS separately from pending Windows CI and all NOT_RUN runtime rows. No shared file or ticket was changed by this worker.

## Second pass — independent review fixes (historical, 2026-11-03)

**Implementation and portable/offline verification PASS; Windows build and all runtime behavior NOT_RUN.** This section supersedes the first-pass design descriptions below. Reviewed base: `958ef77eb9aefb1eafe4db7938ffc01d05003ad9` (first pass `6c9f94c`, then the MSVC min-macro fix). Current changes are uncommitted on `phase5/dyl-200-cursor-mode`. Host: `dq-MEG-Z590-Trident-X-MS-B926`, Linux; g++ Ubuntu 15.2.0-16ubuntu1, Python 3.14.4. No game/Proton launch, deployment, binary/archive download, third-party contact, commit, push or PR. No UI files, shared documentation, build or packaging configuration changed. The specifically authorized CI scope files changed.

Read the assigned Linear DYL-200 issue through the connected read-only tool. Its BTPS/upstream/fork text is obsolete and superseded by the supplied owner rulings/D026 and second-pass brief. No finding was dismissed.

### Finding resolution

| Finding | Implemented resolution; runtime status is NOT_RUN throughout |
| --- | --- |
| M1 — busy versus lost; poll contention | Added append-only `Result::busy` and `getCursorFrame`. Snapshot plus candidate IDs are published together through a short independent try-gate. Readers never acquire Pick's `StateAccess`; a contended publisher skips publication, not native Pick/injection. Camera reads this publication only in ThirdPersonState update, guarded by the verified engine update counter. Look, input, AdvanceMovie and PostDisplay no longer read snapshots. One atomic session query at that same service detects loss. Busy/incomplete scans keep cached state until 500 ms without a **completed** refresh; busy pushes retry on later services with no busy-result loop. Request-gate contention defers request consumption, not ordinary Pick validation. Actual non-injecting/contended Picks still suppress activation/grab. |
| M2 — empty sessions | `sessionViable`/`CursorFrame.viable` describe world eligibility independently of `scanReady`, `available` and candidate count. Entry uses viability. Completed empty scans advance `completedRefreshes`; count zero produces no hover, not an exit. Walking away from the last reference preserves the cursor session. Incomplete/unknown validation does not refresh the 500-ms deadline. |
| M3 — immediate empty authorization | Every service re-evaluates cached projected points; look also checks immediately after integrating deltas. Leaving the authorized/pending target radius synchronously sends zero, except while a valid explicit cycle latch is retained. Zero revokes the atomic generation even when the request/publication/Pick gates are busy. Entry starts an empty generation. Added a Selection-owned, byte-checked guard at ActivatePickRef's handle-resolution call; it refuses an old crosshair handle before E can use it, without racing a main-thread write to CrosshairPickData. See ordering below. |
| Minor 1 | Camera's menu event sink returns immediately for exactly `SkyCRPGCameraWorldPointer`, before movement-config checks, debounce, suspension or lock-config refresh. |
| Minor 2 | Private CursorMenu uses native `kShowAll`. |
| Minor 3 | Queued hover is tracked separately from authorization. Camera reconciles `replies`, `externalTarget`, `externalResult` and live `authorizedTarget`; rejected/unknown pushes retry, and a missing acknowledgement retries after 200 ms. A new cycle is checked under the separate request gate before nonzero revocation and again on consumption. |
| Minor 4 | `leave()` calls only `setCursorMode(0)`; it cannot queue a legacy external release after Selection ended the session. |
| Minor 5 | Exact own-menu checks require `kOnStack` and `menuSystemVisible` before capture/paint. Missing stack/visibility refuses or exits. Pending creation waits at most 500 ms. |
| Minor 6 | Selection module/API resolution occurs once and is cached for process lifetime; no per-frame GetModuleHandleW in `apiLive`. Live DLL unload remains unsupported. |
| Minor 7 | Nonzero viewport origin is accepted. Projection maps normalized buffer coordinates to viewport-local pixels by subtracting left/top; drawing maps those local pixels through GetVisibleFrameRect. Bounds are checked against buffer dimensions. |
| Minor 8 | SelectionAPI.h changes select both Camera and Selection CI jobs. Added focused mapping tests; explicit dispatch and independent UI/testprobe behavior remain unchanged. |
| Minor 9 | Historical implementation attempted main-thread-only registration at off-main kDataLoaded and therefore failed on this host. **Corrected in third pass:** readiness only at DataLoaded; registration on the first main service under `UI::processMessagesLock`, before idle/request handling. |
| Minor 10 | `leave()` never clears the toggle atomic, preserving a press posted concurrently. Main look can consume an already-posted exit toggle before capturing deltas. Only right-stick events select controller visuals. Cursor hover exposes a refreshed candidate pool, releases the previous external latch before scanning, and explicitly rebinds a new eight-item latch on direct candidate hops; manual cycles retain frozen ordering and their movement threshold. |
| Behind-camera guard | Test signed homogeneous `w` from matrix row 3 before native projection. Native divides by **abs(w)**; reject nonpositive/nonfinite w without using a 0..1 depth assumption. Portable front/behind/plane/nonfinite cases PASS; live reversed-Z alignment NOT_RUN. |
| Windows macros | `(std::min)` / `(std::max)` and parenthesized numeric_limits min/max throughout owned code, including the remaining existing Selection and obstruction-test calls. |

### Authorization and ordering

1. Camera enters an empty cursor generation. A Pick from before entry cannot publish to it, and an old native crosshair handle has no matching cursor permit.
2. A nonzero hover request first checks observed cycle count under a separate nonblocking request gate, revokes the old generation, then posts `{generation, cycles, id}`. The latest request replaces older pending intent. A newer cycle wins. Pick's state gate is never taken by these API calls. Zero does not wait for either gate; its atomic CAS revocation cannot resurrect an inactive session. The CAS only retries a racing atomic publication, never an API `busy` response or a held gate.
3. Pick consumes hover at its next eligible opportunity (the legacy external mailbox's 100-ms throttle is not used for cursor intent), checks membership and fresh native range/body LOS, rebinds the hover latch, and publishes only if the generation still matches. Every successful injection records its exact cursor token as an activation permit. Non-injecting Pick invalidates that permit and clears native activation/grab data as before.
4. Camera detects pointer departure on every update and after look integration using cached projection. Zero cancels queued/in-flight authorization immediately. Projection itself remains refresh/cycle driven. A manual cycle's explicit small-motion latch is intentionally preserved.
5. At `ActivatePickRef+0x7E`, Selection's wrapper checks the active token and last valid Pick permit, then resolves through the **original call destination**. It checks token/permit again and requires the resolved FormID to equal the cursor target. A mismatch clears the local output NiPointer. Native code subsequently tests that pointer and cannot activate the old reference. This also covers ActivatePickRef's native command fallback before that lookup. The wrapper does not invoke activation, synthesize E, or mutate shared CrosshairPickData. Outside cursor mode it calls the original resolver unchanged. If hook bytes/allocation/protection/cache-flush verification fails, cursor entry is unavailable while ordinary Selection remains available.

The authorization decision linearizes at the guarded resolver. Input not yet delivered/integrated cannot affect an already executing native action. Thread scheduling, real input-to-activation timing and native menu gating require the serialized runtime route; this is not a runtime PASS. Native prompt removal can await Pick, but activation permission cannot rely on that visual delay.

### Offline evidence

**Source:** CommonLibSSE-NG 7.2.0 pinned revision [`7a60f4de794095d7b0f8928d1b930a52e9a7da83`](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), read locally at `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0`. Checked `UI`, `GPtr`, `NiSmartPointer`, `GViewport`, `ThumbstickEvent::IsRight`, `BSGraphics::State::GetFrameState1799`, NiCamera projection signature and native resolver ABI. No provider/GPL implementation copied.

**Source (offline executable, no public binary URL):** `/home/dq/.local/share/Steam/steamapps/common/Skyrim Special Edition/SkyrimSE.exe`, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`; Address Library `versionlib-1-7-104-0.bin`, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`.

- **PASS:** ID40548 → RVA751580, prologue `40 53 55 56 57 41 56 48 83 EC 50`; native target load at75159E reads `[rax+4]`. Call at7515FE (`+0x7E`) is exactly `E8 DD D5 A2 FF`, resolving to17EBE0. That resolver reads the uint32 handle through RCX and writes the reference smart pointer through RDX. The native caller uses that output at751604 and branches on null at751609. The guard preserves that exact destination rather than assuming another CommonLib lookup relocation is interchangeable.
- **PASS:** ID70640 → EF1180 computes row3 dot world + translation, then `andps` with the absolute-value mask atEF11BE before division. Signed w guard prevents behind-camera acceptance independently of depth.
- **PASS:** State ID411479 →33363A0; update counter at+58 →33363F8. Binary increments found at658D01 and6E4C85. The pinned CommonLib 1.7.99+ accessor exposes that exact field; Camera coalesces repeated services within one update count.
- Reviewer-verified IMenu/GFx layout, LoadMovie Advance(0), movie origin/depth100, native Pick/menu caller facts and original generation-token results are reused as supplied; they are not new runtime evidence.

Audit script/output: `/tmp/dyl200-pass2/audit.py`, `audit.log`, using the read-only `/tmp/cursor-review/al.py` decoder. No game bytes/assets or helper binaries were added to Git.

### Verification jobs and actual results

Pre-job declarations: `/tmp/dyl200-pass2/jobs.md` (purpose, command, budget, owner, completion/cleanup). Owner: this DYL-200 second-pass worker. All jobs completed; no task-created long-running process remains. Portable shell PID: `portable.pid`; tool handles63894 (initial failure),98686 (corrected PASS); logs retained. Python completed synchronously. No Windows DLL/package hash is claimed; Python packaging tests create disposable **mock fixtures**, not candidate artifacts.

| Command | Result |
| --- | --- |
| `timeout 600 bash /tmp/dyl200-pass2/portable.sh` | **PASS**, 10 Camera +2 Selection executables, final `portable.log`. Script uses the exact workflow commands below. |
| Camera: `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"`, each `modules/camera/tests/*_test.cpp` | **PASS**, all10. |
| Selection: `g++ -std=c++20 -pthread -Wall -Wextra -Wshadow -Werror -pedantic "$test" -o "$RUNNER_TEMP/$name"`, then `"$RUNNER_TEMP/$name"`, each `modules/selection/tests/*_test.cpp` | **PASS**, both2. |
| First portable attempt | **FAIL**, missing closing brace in the new `within` helper. Corrected code, then the full exact-command run passed. `portable-missing-brace-fail.log` retained; no unchanged retry loop. |
| `timeout 600 python3 -B -m unittest discover -s tools/tests` | **PASS**,26 tests, `python.log`. |
| `timeout 120 python3 /tmp/dyl200-pass2/audit.py` | **PASS**, pinned hash/address/call ABI/projection/update-counter inspection; expanded audit after adding update-counter evidence also PASS. |
| `git diff --check` | **PASS**. |
| Windows/MSVC compile/link/package; all in-game routes | **NOT_RUN**, orchestrator CI/serialized runtime required. |

New tests exercise freshness through busy/stale/lost states; empty eligible sessions; immediate pointer departure without refreshing projection; direct candidate hopping; signed-w rejection; empty-generation/E authorization and late-Pick rejection; superseded requests; direct latch rebinding from the old eight to a newly ranked ninth; deterministic concurrent publication busy without modifying output; publication while an independent Pick gate is held; and latest-request replacement. These are portable policy/concurrency tests, not an engine-hook execution.

### Reviewer uncertainties and remaining runtime gates

| Uncertainty | Defensive handling and remaining status |
| --- | --- |
| Constructor/AdvanceMovie/PostDisplay/ProcessMessage threads | Every callback checks Main thread ID and refuses/post-exits on mismatch. Wrong-thread destructor never reads the main-thread overlay pointer. **NOT_RUN:** actual callback thread identities and usable overlay on this host. |
| Whether hide destroys the menu | Keep a strong GPtr across hide; replace it only on main-thread construction. The final menu reference is intentionally retained for process lifetime to avoid engine-teardown destruction and dangling raw pointer use. **NOT_RUN:** actual native hide/reopen lifetime behavior. |
| Mouse.removeListener return | Require successful invocation and Boolean true before marking overlay usable; other result refuses. **NOT_RUN:** runtime return value. |
| Drawing/Display without Advance | Creation/drawing calls must succeed; PostDisplay checks stack, visibility, native focus and freshness. **NOT_RUN:** live rendered arrow/cross without further Advance and visual fidelity. |
| Activate with an extra menu on stack | Overlay has only the narrow passive flags; normal native activation dispatch/menu gates remain. Resolver guard only removes stale authority. **NOT_RUN:** native E reaches activation while this overlay is present. |
| Reversed-Z/behind-camera | Offline abs(w) and portable signed-w checks PASS. **NOT_RUN:** live camera matrix convention/alignment, reversed-Z, viewport origin/letterboxing, native child graph. |
| Functional/regression/performance | **NOT_RUN:** empty-corridor entry, leaving last chest, live contention, exact E/clear timing, rejected-hover recovery, keyboard/controller direct hops/latches, every interruption and same-frame yaw recovery, camera-only/UI-only/combined, feature-off comparison, actual frame/pick cost and memory. |

Additional concrete runtime checks are in `/srv/storage/Projects/SkyCRPG/session-p5/routes/cursor-review-pass2.md`; all remain NOT_RUN. Existing five JSON route drafts are unchanged. No owner playtest acceptance is inferred.

### Decisions and owner questions

Engineering choices: append-only frame API with an explicit busy result; coherent published list separate from Pick ownership; completed-refresh 500-ms deadline; 200-ms missing-reply retry; latest pending cursor intent; refreshed hover list versus frozen manual cycle list; Selection-owned native resolver guard rather than cross-thread crosshair writes; signed-w rejection; process-lifetime retained private movie; update-counter coalescing. No owner ruling changed. Cursor and external-target defaults remain OFF; keyboard B and controller-unbound proposals remain unchanged.

No new owner answer is needed to review these fixes. Existing questions remain: approve/revise B and the eventual tested controller binding; judge sensitivity/radius/latch/visual feel after the runtime routes. Build/review and runtime evidence belong to the orchestrator; this worker does not commit or launch.

### Proposed shared-file changes (second-pass additions)

- Architecture/API notes: Selection alone owns Pick and the byte-checked ActivatePickRef resolver guard for cursor authorization. Camera reads one published frame and uses an optional size-negotiated extension; no UI DLL dependency. Record busy versus lost, empty session viability and the 500-ms stale deadline.
- Controls/UX: remove the implication that an empty corridor or a transient busy scan exits mode. Preserve toggle semantics and manual cycle-latch behavior. Record ShowAll, viewport-local pointer coordinates and exact passive overlay visibility/stack requirements.
- Status/compatibility/dependency notes: both new DLLs require Windows CI and serialized routes; API header changes rebuild both consumers. No runtime result/acceptance or new provider dependency. Consider updating the stale shared CMake option description saying all policy adapters use no trampoline patches; no CMake setting was changed here.

## First-pass record (historical; design statements superseded above)

2026-11-03. **Code/offline checks PASS within the bounds below; Windows build and every in-game criterion NOT_RUN.** This is an uncommitted worker handoff, not owner acceptance. Base `398f4278e96409cbe504f3df7d5d49a9c45d8325`, branch `phase5/dyl-200-cursor-mode`, host `dq-MEG-Z590-Trident-X-MS-B926` (Linux). No Skyrim/Proton launch, deployment, download, third-party contact, commit, push or PR. No UI, shared docs, build or packaging files changed.

D026 replaces the obsolete BTPS dependency/upstream/fork portions of P5-03. The supplied DYL-200 brief supplies the revised implementation contract. D5-3 is toggle mode; D027 reserves R3. This work does not change either ruling or the Stage 1 boundary.

## Implementation

- `[CursorMode] bEnabled=0`. Off returns before cursor initialization, menu registration, API resolution or optional engine work. It installs **no new hooks**, even on: it uses Camera's existing ThirdPersonState update/look wrappers. Camera still never hooks Pick or requests camera control. SmoothCam remains the normal transform owner. Native activation, attacks and mouse buttons are never synthesized or swallowed.
- Keyboard proposal **B / DIK48 (`0x30`)**; controller remains `-1`. Both use fresh physical press edges and toggle semantics. Live native gameplay mappings and `QUserEvent` refuse entry. Fixed reserved Camera/Selection/UI keys and configured Camera toggle/recenter bindings are rejected during config validation. The entry command is not consumed as a native button.
- Main-thread entry checks the existing Camera context, all native focus, world/cell, drawn state, Selection readiness, fresh refresh counter (500 ms maximum idle), API extension and overlay. It requests the existing lock interruption path, waiting up to 500 ms for unlocked acknowledgement and overlay creation. No look capture while pending. A load success flag independently refuses failed post-load notifications. New game/load/cell transitions require a fresh toggle.
- The pointer recentres on entry, integrates **native look deltas**, and clamps to viewport dimensions, including after resize. Default sensitivity900 pixels/native look unit, radius48 physical pixels, latch threshold12 physical pixels; all are bounded config proposals. Active look chains the previous look implementation **once with zero deltas**. Outside mode the original branch path receives its original deltas. No OS cursor positioning or cursor-menu input injection.
- At most16 copied validated candidates, at most32 direct camera-root children inspected, one bounded projection pass per observed Selection refresh (an intervening cycle may cause a new observation). It resolves live references on main, rejects unloaded/deleted/disabled references; dead actors now use their finite loaded-root world-bound centre (third-pass correction), while other references transform the immutable base OBND centre using reference position/angles/scale, and projects with the exact native NiCamera. Failed/behind/offscreen projections are rejected. Smallest screen distance within radius wins; ties use world-centre distance to player, then FormID. No new world scan, reach extension or LOS relaxation.
- Changed choices only are queued, at most **one attempted push per Selection refresh**. A second snapshot rejects mixed list/cycle generations. The checked cursor setter additionally binds the mailbox request to `cyclesTotal`, closing the job-thread race between that snapshot and the push. A newer cycle wins, even if it arrives after enqueue. This uses Selection's existing external-target mailbox/validation, not another activation path.
- A Selection cycle records the chosen ID and current pointer position. Hover cannot replace it until displacement is **strictly greater than12px**, candidate/liveness invalidation, or exit. While exclusive, Selection's ordinary10-second/player-movement latch expiry is disabled; native eligibility/range/LOS still validate. Ordinary Selection clients retain the prior expiry behavior. Camera does not send another push merely to keep a latch alive.
- Drawn weapons **including melee** exit/refuse mode. This is the existing Selection sheathed-only contract, not a new combat or projectile-aim implementation. F7/configured recenter also exits before the existing recenter action, avoiding a deferred recenter on a later cursor exit. F6/configured POV, F8, U and R3 similarly exit before their existing behavior.

## Overlay: exact private menu, vanilla movie

Camera registers `SkyCRPGCameraWorldPointer` on the first main-thread service after DataLoaded, before the idle early-out or consuming a toggle, under `UI::processMessagesLock`. This corrects the earlier entry-time/DataLoaded registration descriptions. An already registered name refuses. Its only static flags are `kRequiresUpdate | kAllowSaving`; the expected engine `kOnStack` bit is allowed. `inputContext` stays `kNone`, depth2. No `kUsesCursor`, pause, modal, menu-context, cursor-assignment, movement-to-direction or input-focus flags. `focused()` exempts only the registered instance with that exact name/identity and exact allowed flag set. No exemption for the native `Cursor Menu` or a broad cursor flag/category. Native menu/pause/dialogue always wins. HUD/Fader/Mist remain passive base gameplay movies, and their focus flags still supersede through `focused()`.

The private instance loads vanilla **CursorMenu**, with ExactFit/transparent background, verifies `_root.mc_Cursor`, removes the vanilla `Mouse` listener (checks the Boolean result), sets its mouse-cursor count to0, and never forwards Scaleform/input messages. Keyboard/mouse uses the vanilla arrow. Controller uses an original four-stroke white cross drawn into `skycrpgReticle` with the existing ActionScript MovieClip drawing API; every creation/drawing call must succeed. Position is mapped from physical viewport pixels into the visible movie rectangle. No font, new SWF, copied game asset, native cursor-menu manipulation or UI DLL is shipped. A loose-file replacement without the expected contract refuses entry.

**Source (offline local asset):** installed `Data/Skyrim - Interface.bsa`, format105, flags0x3; `interface/cursormenu.swf`, offset81902876,949 bytes, SHA-256 `069c85fcce23de69f12e3c3273832acd19e28aa187ca62daf52d0b906f6c68bd`. CWS v15 expands to1723 bytes. Frame rectangle0..640 ×0..480 pixels,24fps,1 frame. Tags: DefineShape3, DefineSprite, named PlaceObject2 `mc_Cursor`, and a183-byte DoAction with constants `mouseListener`, `Object`, `onMouseMove`, `mc_Cursor`, `_x`, `_y`, `Mouse`, `addListener`. The action reads mouse-property indexes20/21 and assigns the clip position. This establishes the arrow and listener contract offline. It does **not** prove runtime GFx method success, appearance, focus order, UI callback thread, aspect scaling, clip origin/hotspot or mouse/controller feel. Those remain **NOT_RUN**; entry fails closed on detectable contract failure.

## Selection API delta (only exception to Camera file ownership)

Changed only `modules/selection/include/skycrpg/SelectionAPI.h`, `include/cursor_override.h` (new), `src/plugin.cpp`, and `tests/cursor_override_test.cpp` (new). No Selection INI/default or build/packaging change.

API version stays2. The original interface prefix and Snapshot layout are unchanged. Two pointers are appended and Camera checks the complete `size`, version and functions before use:

1. `setCursorMode(mode)`:0 synchronously releases exclusivity and pending requests;1 enters **empty** exclusive mode if external targets/injection are available;2 queries whether the session survived lifecycle invalidation. Mode0 works during menus/load. Entry returns `ok`, not a promise of a chosen/validated object.
2. `setCursorTarget(formID, observedCycles)`: the cursor form of `setTarget`, using its existing mailbox, with an expected cycle generation. Zero means explicitly empty. Nonzero still needs membership plus fresh native eligibility/range/body LOS. `queued` is not a successful activation/validation. The second pointer is needed to avoid a stale hover overwriting a cycle on Pick's job thread between Camera's snapshot and push. An obsolete cycle generation returns `invalidTarget`; Camera waits for a new observation. All other unavailability exits mode. Legacy `setTarget` behavior outside exclusive mode is unchanged.

Queued cursor requests clear authorization for the **previous** hover immediately, so a failed new push cannot activate an old object. Within exclusive mode, every non-injecting Pick (including contention/reentrancy/scan failure/invalid target) clears `target`, `grabPickRef` and stale collider; `targetActor` and collisionPoint remain native. An empty cursor never falls back to native or nearest. A Pick that began exclusive also clears on early lifecycle refusal even if that refusal ends its session. Lifecycle reset ends exclusivity; Camera's query detects that loss. Clear/exit drops pending mailbox work immediately, and Pick releases its policy latch at its next opportunity. No engine pointers or STL cross the ABI; calls remain any-thread/nonblocking, no API scan/engine call.

Snapshot.target remains the underlying Selection policy choice (it can be nonzero in exclusive-empty mode); actual empty Pick rows use form0/source external, and Camera trace target0 denotes its empty pointer. Do not infer an activation from Snapshot.target alone.

The portable override uses an atomic generation so a Pick cannot publish a target into an exited/re-entered session. Cursor cycle requests beat pending hover requests. The ordinary noncursor cycle/mailbox order is retained. Selection remains its own DLL and still ships **`bAllowExternalTargets=0`**. Recommendation: keep that default OFF through the runtime gate; use an explicit isolated-profile override1 for this opt-in experiment. A future installer may offer a coordinated opt-in, subject to owner review. No default was changed here.

## Exit and thread table

All exit rows are **NOT_RUN in game**. On main, one `leave` path sets inactive first, clears local latch/pending entry/toggle, releases Selection exclusivity/request, sets the private movie root invisible immediately, then queues its hide/removal. `PostDisplay` also refuses to draw inactive sessions. Look capture is a per-call decision, with no persistent native input-disable bit to restore. Queue removal need not finish synchronously for the overlay to be invisible and noninteractive.

Other threads only post atomics for the new feature; they never mutate its UI or local pointer state. Main consumers are the existing input/update/look hooks and the private menu's PostDisplay (AdvanceMovie only checks thread identity). They drain exit flags **before** another capture/draw. A same-frame guarantee means that first main-thread opportunity, not a worker calling Scaleform. An unexpected off-main look/overlay callback posts a refusal and captures/draws nothing. No SKSE task is assumed to be main. kDataLoaded posts readiness only for cursor mode, after the existing Camera hook checks; the first main service registers under the UI lock before its idle early-out, and API resolution/UI work also happen on main. Selection's job-thread Pick/API state remains protected by its existing nonblocking StateAccess gate plus atomics.

| Trigger / log reason | Main-thread action and recovery |
| --- | --- |
| Toggle / `toggle` | Immediate leave; next fresh toggle may enter. |
| Feature or Camera disable / `disabled` | No entry; leave if active. Feature flag is restart configuration. |
| Provider/hash/hook/ownership/world failure / `lifecycle` | Leave/refuse; never capture with invalid Camera prerequisites. |
| Any native menu, console, dialogue, pause / `menu` | Opening posts exit; stack/focus checked on each service. No automatic re-entry on close. Native Cursor Menu is not exempt. |
| Preload, postload, new game, load event / `load` | Exit posted, load-success flag checked. No pointer/handle serialized. Failed postload refuses. |
| Death / `death` | Leave via player state; fresh toggle only after valid live world. |
| Player cell attach/detach or changed parent-cell ID / `cell` | Exit on event or main cell check; destination does not restore mode. |
| First person/native POV / `first-person` | Exit before capture; native state and POV button checked. |
| Furniture/mount/swim/killmove/script/beast/tween/slide or next camera state / `special` | Existing context priority exits; no capture in special states. |
| Bow/spell/drawn weapon / `precision` | Leave before look filtering; no aiming implemented. |
| U/R3/observed target lock / `lock` | Leave before lock input; entering while locked first requests existing interrupt, then requires unlocked acknowledgement. |
| F6 or configured Camera toggle / `F6` | Exit before native/Camera POV request. |
| F8 / `F8` | Exit before provider escape handling. |
| F7/configured recenter / `lifecycle` | Exit before recenter, preventing delayed rotation. |
| Selection absent, missing API extension, disabled external targets, unavailable/stale snapshot, session loss, module-handle loss / `selection` | Refuse or leave; clear when API remains available. Specific absent/extension/external-off reasons log once. DLL live unloading is unsupported; no cached call after observed module loss. |
| Movie missing/incompatible/hidden, bad flags, absent camera or bad viewport / `overlay` or `lifecycle` | Refuse/leave; no invisible capture. Pending creation timeout500ms. |
| Unexpected callback thread / `thread` | Post refusal; pass through look/no render; main drains before next capture/draw. |

## Offline engine audit

**Source:** CommonLibSSE-NG7.2.0 checkout revision [`7a60f4de794095d7b0f8928d1b930a52e9a7da83`](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), local `/srv/storage/Projects/SkyCRPG/session-p2-20261026/commonlib-7.2.0`. Read headers AND implementations for NiCamera, NiNode/TESCamera, TESObjectREFR/TESForm, ControlMap, Main, UI/IMenu/UIMessageQueue, BSScaleformManager, GFxMovie/View/Value and GViewport. No GPL implementation was read/copied for this change. Original in-repo reference-pose convention agrees with Selection; no BTPS material was used.

**Source (binary):** exact local SkyrimSE.exe SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`, runtime1.7.104.0; Address Library `versionlib-1-7-104-0.bin` SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842`, format5. RVAs below are relative to image base0x140000000. The existing pinned Camera prerequisite hashes remain mandatory; loaded SmoothCam/TDM/SKSE versions were **not** observed in a new game session.

| Access / offline result | Binary evidence (PASS for described static fact only) |
| --- | --- |
| Existing update/look wrappers | ID50896 RVA8FD590 prologue `40 57 48 83 ec 70`; ID50914 RVA8FEB00 `40 57 48 83 ec 30`. No new wrapper address/hook. |
| Main thread identity | Existing Main singleton ID400315 RVA219E570; inherited static_assert `Main::threadID=0x28`. Existing target-lock audit reused. New UI calls check this ID, never the data-load callback/TDM API requester thread. |
| Player root and bounded NiCamera lookup | PlayerCamera vtable ID214859 slot1 points RVA8F7CF0; root setter loads/stores +20 at8F7D37/8F7D4E. Player update8F88D2 reads root+20. NiNode child accessor uses flat+110; only direct children with **exact** native NiCamera vtable ID237191 RVA1A485B8 qualify; no unchecked RTTI downcast or guessed nested child. NiCamera constructor writes that table atEEF9C1/EEF9C8. An absent/different graph refuses, rather than assuming child0 is a camera. Actual provider graph attachment remains a runtime check. |
| Matrix and viewport | ID70617 RVAEEF050 copies matrix+110..140, frustum+150, port+174..180 and lod+184. Constructor EEF9A0 initializes the same fields. Projection takes these members, not cameraRoot rotation as an approximate projection. |
| Projection ABI | ID70640 RVAEF1180: RCX matrix, RDX NiRect, R8 point, R9 x-output; stack y/z/tolerance. Reads matrix row3 and float tolerance, writes float outputs, returns bool AL atEF12E6/EF12F5. Native caller939679 sets all seven args. Port x uses left/right, y uses bottom/top; Camera converts y-up normalized output to screen y-down. z outside0..1 rejects. |
| Reference centre/liveness | GetScale ID19664 RVA2E6F30 reads ref+98, base+40, returns scalar in XMM0 at2E6F67 (NPC multiplier preserved). GetPosition/GetAngle/GetBaseObject are source member accessors; base+40 is also read by this native routine. OBND+20 reused from Selection's audited boundData contract. IsDeleted/IsDisabled read form flags; no new guessed engine routine. Ref/actor Get3D2 slot70→2EBCE0 (TLS cache then ref+68 loaded data); player slot70→747AF0. Actor/player IsDead slot99→6879A0 reads life-state bits at+C8 and Boolean parameterDL; no wrong HasActorDoingCommand/ApplyEffectShader accessor introduced. |
| Form resolution | Existing CommonLib form-map/lock IDs400507/400516→21A3808/21A3C90, same audited lookup path as Selection. Main-thread lookup returns a borrowed reference used only in that bounded pass; none retained across load/cell. |
| Private movie load | BSScaleformManager singleton ID402775→369B060; LoadMovie ID82325→116FDA0. Native CursorMenu constructor at929FE2/929FE9 passes manager, menu, `uiMovie` at+10, filename, scale mode on stack and background float. Confirms this exact overload/Boolean return path, not LoadMovieEx. Native menu sets depth+18 and flags+1C at92A005/92A009; new IMenu uses same fields. |
| GFx calls / viewport | Native movie loader calls GetVariable slot11 at116FF9F, scale-mode slot1B at116FF7D, visible-frame-rect slot1F at116FFE3, SetViewport slot19 at1170114 and Advance slot25 at1170139. Native CursorMenu calls mouse-count slot38 at929FFF, matching SetMouseCursorCount. GViewport width/height+10/+14 and origin+8/+C agree with loader construction11700D0..11700E9. Recovered movie vtable RVA1A2ADE8: GetViewport slot1A→D49870 copies the full viewport fields from movie+78 through+A8 to caller output; visible-frame-rect slot1F→D498C0 returns four coordinates divided by20; SetMouseCursorCount slot38→D523E0 clamps EDX to4 and stores+A68. GetVariable slot11→D49060, SetVariable slot10→D529F0, Invoke slot17→D4B150 (forwards RDX path/R8 result/R9 args and stack count), Display slot26→D45C50 agree with the pinned interface and the native call sites; GFxValue ObjectInterface invoke ID82256→CF6160, display info ID82287→CF7330, add/release IDs82269/82270→CF6830/CF6890 prologues/signatures inspected. ActionScript methods are dynamically checked; offline method presence does not claim runtime success. |
| UI menu/message operations | UI::Register/GetMenu/IsMenuOpen and pause/focus checks are local CommonLib implementations over menuMap/stack/flags already used by Camera. Queue singleton ID400445→21A05C0; AddMessage ID13631→1B47F0 prologue `40 56 57 41 56 48 83 ec 30`. IMenu default ProcessMessage is deliberately bypassed because it dispatches Scaleform input; default inputContext is kNone. |
| Binding | Installed `interface/controls/pc/controlmap.txt` SHA-256 `54886af6a32c18fa5a8b5805ab3070b6af1b9ddc92f777206494099d4e3353ee`. All contexts/alternative/chord columns scanned: B0x30 has no mapping. O0x18 failed (Options line203); V0x2F also maps Filter. B does not overlap F6/7/8, U, brackets, K/L,1..8 or keypad hotbar aliases. Source UI hotbar dispatches native Hotkey1..8 events. D-pad Left0x4 is native Hotkey1: controller proposal requires a deliberate remap, not a free default. |

Audit scripts and disassembly are external scratch files `/tmp/dyl200/{extract.py,audit.py,audit_extra.py,accessors.py,gfx_vtable.py,*.asm}`. No game bytes/assets were added to Git. These checks resolve new address/asset questions offline; they do not establish actual hook timing, live graph contents, callback threads or menu behavior.

## Tests and job record

Pre-job declaration: `/tmp/dyl200/jobs.md` (owner DYL-200; command, expected output, timeout, completion and cleanup). Builds/tests run under `timeout`; shell handle/PID and logs retained. No long-running task process remains. Native Windows compilation/link and packaging **NOT_RUN**: no MSVC/CMake here; orchestrator CI required after review/commit. No DLL or package hashes exist for this diff.

| Command / purpose | Actual result |
| --- | --- |
| `timeout 600 bash /tmp/dyl200/portable.sh` | **PASS**,10 Camera +2 Selection C++ executables with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include`; full output `portable.log`. Existing CI globs include both new tests; build config unchanged. |
| First expanded latch test | **FAIL**, test fixture left Selection's default dead/menu/first-person/special flags set, so cycling correctly refused. Fixed the fixture to an eligible world and ran once with changed input; `portable-latch-fixture-fail.log` retained. Initial policy/ABI suite before that extension also passed (`portable-first.log`). |
| `timeout 120 python3 /tmp/dyl200/audit.py` plus `audit_extra.py`, `accessors.py`, `gfx_vtable.py` | **PASS**, pinned hashes, address/caller/member audit and stock B binding. Initial O binding assertion **FAIL**; changed proposal/input to B and reran, retaining `audit-first.log`. |
| `timeout 60 python3 -B /tmp/dyl200/validate_routes.py` | **PASS**,five drafts, budgets900s,24/39/65/33/39 steps; all key names supported. Initial validator **FAIL** for wrong helper name, then **FAIL** for duplicate fixture section/state tag. Corrected helper to `iter_steps` and state tag to `fixture-state`; changed-input final pass in `routes.log`, earlier logs retained. No dry-run/profile preparation/real execution. |
| `timeout 60 bash /tmp/dyl200/focused.sh` | **PASS**, both new tests under additional `-Wshadow` (Selection CI flag), after the final checked-cycle/latch changes. `focused.log`. |
| `git diff --check` | **PASS**. |

New portable coverage: toggles and all eligibility exit priorities, F6/F8/lock/native-POV input classification and native Activate exclusion, pointer centring/clamping/sensitivity/nonfinite input, screen-distance/radius/world-distance choice, threshold boundary/invalidation, reserved bindings, external-absent/disabled refusal, feature-off policy, append-only ABI prefix, empty/failed/invalid Pick suppression, exit/re-entry generation, stale hover versus newer cycle, cursor latch surviving ordinary expiry and ordinary noncursor expiry remaining intact. Portable policy tests do not execute IMenu, GFx, MSVC linking or the actual job-thread Pick.

## Acceptance and routes

| DYL-200 acceptance / required runtime evidence | Result |
| --- | --- |
| Keyboard/mouse hover indicates intended reference; one native E activates it | **NOT_RUN** |
| Right-stick reticle; controller toggle and one native Activate | **NOT_RUN**, controller remains owner hands-on |
| No camera rotation from cursor deltas; native look/combat outside mode | **NOT_RUN** |
| Cycle latch survives small drift/time, releases past threshold/invalidation | **NOT_RUN** |
| Empty pointer never activates native/nearest/previous hover | **NOT_RUN** |
| Every exit restores look/hides overlay in same frame; no menu recapture | **NOT_RUN** |
| Load/death/cell/first-person/special/lock/precision interruption matrix | **NOT_RUN** |
| Selection absent/old API/external-off/unavailable/session loss refusal | **NOT_RUN** |
| Camera-only, UI-only, combined; feature-off comparison | **NOT_RUN** |
| Per-frame projection/push cost and render/UI overhead | **NOT_RUN** |
| MSVC /W4 /WX build/link, exact internal candidate hashes | **NOT_RUN** |
| Owner binding/visual/feel acceptance | **NOT_RUN** |

Added outside the worktree under `session-p5/routes`: `write_cursor_routes.py` and `gen/cursor/p5-cursor-{hover,off,latch,interrupt,activate}.json`. Each is draft/blocked with explicit CI/stage/calibration prerequisites, a copied save and the runner's cleanup contract. Hover/off use identical relative input steps for comparison. Latch includes small drift, reverse cycle,11s hold and large movement. Interrupt covers toggle/F6/F8/Tab/console; feature-off comparison is separate. Activation draft sends exactly one E only after the orchestrator clears the reviewed-target/occupancy/calibration prerequisite. All actual runs remain NOT_RUN. The runner and existing routes were not changed.

Runner limits: relative mouse deltas are not calibrated pointer pixels; its OS/menu pointer helpers cannot position this world pointer. It cannot assert hovered identity/ownership/occupancy, activation count or target-health outcomes. Selection.log is outside its assertion allowlist and must be collected/reviewed separately. Reload is initial setup or recovery, not an ordinary route step; console-based reload already interrupts mode. Dialogue/death while continuously active, controller, unexpected thread/API failures and exact same-frame timing require hands-on/instrumented fixtures. Do not treat a route/schema PASS or prompt as activation proof.

## Proposed shared-file changes

**docs/CONTROLS.md (proposal, not approved binding):** “Cursor mode is an opt-in toggle, default OFF. B/DIK48 is the tested-offline keyboard proposal; live remaps refuse. Move its pointer with mouse look deltas; right stick moves the controller reticle. Native Activate remains the only activation button. Selection [ / ] cycle and latch until more than12px of pointer motion, invalidation or exit. R3 remains D027 lock/POV; cursor gamepad toggle defaults unbound. D-pad Left can be tested only with an explicit owner-reviewed native Hotkey1 remap. F6, F8, recenter, lock, precision/drawn weapons and native menus exit; closing a menu does not resume it.”

**docs/UX_SPEC.md (proposal):** “Camera owns a nonfocusing `SkyCRPGCameraWorldPointer` overlay independent of UI. It loads a private vanilla CursorMenu instance and disables its OS mouse listener; a vanilla arrow/original drawn cross follows integrated native look deltas. Pointer starts centred and clamps to viewport pixels. Selection alone validates candidates and supplies native activation picks; empty hover explicitly suppresses fallback. Default hover radius48px and cycle-release distance12px are unaccepted feel proposals. All native focus and lifecycle states supersede mode. No click-to-move, projectile aim, RTS panning, drag/drop or held-right-mouse behavior.”

**Dependency/status/API note for integrator:** Camera's optional cursor path requires Selection v2 with the append-only cursor extension and explicit external-target opt-in. No dependency exists when CursorMode is off, and UI never depends on Camera. Keep Selection's external-target defaultOFF pending runtime validation. Replace obsolete BTPS assumptions in P5-03/architecture/status only in the integrator's shared-file pass; leave runtime acceptance open.

## Decisions and owner questions

Engineering choices: B proposal after all-context stock scan; D-pad Left remap proposal with default unbound; sensitivity900/radius48/latch12 bounded knobs; same private movie for both devices with an original drawn cross;500ms entry/staleness limits; exact native NiCamera child requirement; drawn-weapon/F7 exits; append-only exclusive/checked-cycle mailbox extension; keep Selection defaultOFF. These are candidate implementation choices, not owner acceptance.

Owner review after CI/game evidence: approve/revise B and the controller remap proposal; judge pointer/cross scale, sensitivity,48px radius and12px latch threshold; confirm whether an opt-in installer should coordinate external-target enablement. No owner decision was silently changed.

Remaining risks: MSVC/API/link errors despite source-signature review; provider camera child graph mismatch; projection sampled before the outer SmoothCam transform (up to a frame of visual lag); low-frame-rate/raw mouse/stick scaling; loose CursorMenu replacements; aspect/reticle sizing and pointer hotspot; UI creation/render thread or menu-stack differences; Pick/Activate ordering, target invalidation and stale native prompt timing. The checked cycle token prevents a known hover/cycle race but actual cross-thread lifecycle behavior still needs the serialized game routes. Selection's bounded candidate prefix means an object outside that prefix cannot be hovered. No claim of complete world enumeration, dynamic DLL unload support, performance acceptance or in-game behavior is made.
