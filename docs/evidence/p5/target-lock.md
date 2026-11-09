# DYL-197 / P5-01 — fourth-pass link and senior-review fixes

2026-11-02. Source base `0d8083af15f2e6392592f26a76eac5d008fbe938` (PR #34), with uncommitted fourth-pass changes. The assigned Linear ticket and phase/research/architecture documents were read; the later owner brief governs. No commit, push, PR, third-party message, Skyrim or Proton launch. No UI or shared product-document edits. The explicit F12 request authorizes the sole shared build-file edit, `.github/workflows/build.yml`; packaging is unchanged.

**Current result:** portable/offline checks **PASS**. The supplied candidate's Windows link is **FAIL** ([CI 41475236195](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41475236195), owner-supplied failure: BSKeyboardDevice/BSWin32KeyboardDevice/BSInputDevice symbols). The corrected Windows target is **NOT_RUN** here: no cmake/MSVC. Every in-game criterion and owner acceptance remains **NOT_RUN**. The earlier claim that the shader return ABI was validated was incorrect; the binary contradicts CommonLib's return declaration.

## Pre-merge re-review fixes (2026-11-08)

Base: `151a2c62e9b8d77c242f6a8f093cdc44fa6dbcbf`, branch `phase5/dyl-197-target-lock`. The owner sessions below ran `63e992e`; `151a2c6` has identical code. These fixes are **NOT_RUN** in game and do not extend that owner acceptance. Feature OFF paths are unchanged in source; no INI, external API, UI, packaging or transform-ownership change. SmoothCam remains the sole normal third-person transform writer.

Verification job declared before execution: owner DYL-197 pre-merge worker; purpose regression-check all eight Camera portable tests, beginning with target-lock policy; command per test `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include modules/camera/tests/<name>.cpp -o /tmp/lock5-<name> && /tmp/lock5-<name>`, driven by `timeout --kill-after=5s 600s bash /tmp/lock5-tests.sh`; expected eight PASS lines and exit 0; deadline 10 minutes; completion process exit plus `/tmp/lock5-tests.log`; job PID recorded in `/tmp/lock5-tests.pid`. Timeout owns/cleans child compilers/tests; artifacts remain under `/tmp`. Native MSVC build and game launch are **NOT_RUN**.

| Finding | Change | Verification |
| --- | --- | --- |
| 1: off-main false prerequisite failure | Gate failure reporting when lock is enabled on a main-thread observation; keep Phase 4 reporting when OFF. | **PASS** portable truth table, repeated off-main/first-person failure-debounce model and native source review. |
| 2: passive-menu INI reads | Request refresh only after the event filter. Ignore HUD/Cursor/Fader/Mist on both edges; refresh on Dialogue/Console/Loading on either edge; other menus neither refresh nor interrupt from the event thread. | **PASS** portable menu-filter cases and source request/consume review. |
| 3: event-thread menu lifetime race | Replace `UI::GetMenu`/flag reads in the event filter with name/opening-only policy. Every other menu is left to the main-thread `focused()`/`c.menu` path, whose eligibility drop releases the lease and requires fresh input (orchestrator narrowed the worker's close-interrupts variant, which would have ended a lock on any provider widget menu close). | **PASS** portable rapid-menu/threaded interruption, acknowledgement/idle/release-race cases and source ordering review. |
| 4: misleading/missing refusal HUD | Require `ready && lockHooksReady`; report unsafe config or unavailable indicator. | **PASS** native source review; HUD behavior **NOT_RUN**. |
| 5: idle-only state-log churn | Remove input-idle from change detection; retain its value in emitted diagnostic lines. Correct the absolute logging guarantee in the comment. | **PASS** diff review; 256-line cap retained. |
| 6: process-list layout pins | Assert `magicEffects == 0x108` and `magicEffectsLock == 0x120`. | **PASS** source match to CommonLibSSE-NG 7.2.0 `ProcessLists.h`; supplied independent binary audit reused (1.7.104 `0x7888CB`/`0x7888F0`). Native assertion compilation **NOT_RUN**. |
| 7: vacuous save/load test | Delete arithmetic-only assertion; no replacement save/load claim. | **PASS** diff review; actual save/load remains **NOT_RUN** for this patch. |

Decision: use the menu filter instead of filesystem timestamps, keeping the fix small and retaining menu-close MCM refresh without new file state. Unknown menu closes conservatively interrupt; unknown openings rely on the existing main-thread focus path. No owner decision or dependency changed; no questions for the owner.

Actual job result: **PASS**, 8/8, exit 0 (g++ Ubuntu 15.2.0-16ubuntu1); completed tool session `58739`, sandbox job PID `4`. No retry or timeout; the timeout-owned job and children exited. Per-test results from `/tmp/lock5-tests.log`:

```text
PASS target_lock_policy_test exit=0
PASS lifecycle_policy_test exit=0
PASS lock_frame_trace_test exit=0
PASS obstruction_policy_test exit=0
PASS obstruction_probe_test exit=0
PASS policy_test exit=0
PASS precision_policy_test exit=0
PASS precision_trace_test exit=0
```

Source review **PASS**: lease ordering is unchanged. The diff preserves `targetLockLease`, interrupt consumption before eligibility/release, `c.menu` making the state ineligible, unlocked acknowledgement, idle-input barrier and mutex-protected `releaseIfClear`; portable tests cover the pending-interrupt race and the new unlisted rapid-menu close. Every changed native line was re-read for `/W4 /WX`: no new unused variables/parameters, signed/unsigned conversion or C4456/C4457/C4458 shadowing hazards found; the diagnostic `%u` argument is explicitly unsigned. CommonLib confirms the member names/offsets and non-null `BSFixedString::c_str()` contract. `git diff --check` **PASS**. This is source/portable evidence only: `plugin.cpp` compilation and all gameplay for these changes remain **NOT_RUN**. No commit, push, PR or game launch.

## Findings and implementation

| Finding | Confirmation and change | Verification boundary |
| --- | --- | --- |
| Windows link | Removed keyboard AND gamepad polling from `pollLockInput`. A dispatch starts idle, matching `ButtonEvent::IsPressed()` events mark it held, and matching release events reset a not-yet-ready barrier. A null event head is an idle dispatch. Two subsequent idle dispatches are still required. | **PASS** source/portable barrier tests; corrected Windows link **NOT_RUN**. Event delivery semantics supplied in the owner brief; native order **NOT_RUN**. |
| F1 crash/queued apply | Ignore `ApplyEffectShader`'s return. Snapshot target+shader effect IDs before apply; select a new unfinished match through `ForEachShaderEffect`. Retain the attempt for up to 500 ms while registration is pending, then refuse it if absent. Revalidate the exact owned ID on every locked observation. | **PASS** offline return-path inspection and ID/window policy tests. First lock/queued delivery **NOT_RUN** in game. |
| F2 trace | Keep a 16-row pre-lock ring without spending the budget; emit locked rows and 16 post-lock rows, capped at 2048 per process. A dedicated writer opens `SkyCRPG_Camera_TargetLock.log` once and drains/flushes every 100 ms off the input thread. Each row includes accumulated original look-hook x/y input and call count for the last-rendered interval. | **PASS** 10,000 idle-poll, pre/post-window, ordering, cap and writer-drain tests. Normal crash loss is the not-yet-flushed interval; a stalled/failed disk can lose the bounded capture (maximum 2048 rows). Timing/angle interpretation **NOT_RUN** in game. |
| F3 fail open | Request the disable lease at data load, independently of Main/TDM thread equality; retain/retry on hook-order or thread failure. Only effect mutation and lease release require the verified thread. First mismatch logs current, Main and TDM IDs once. Remove the input-sink early return; native F6 escape remains available under policy refusal, F7 remains subject to safe-framing policy, and F8 remains SmoothCam-owned. | **PASS** source/API audit. A competing API owner/MustKeep cannot be overridden; refusal suspends policy and is reported. Actual provider acknowledgement/thread mismatch and recovery **NOT_RUN**. |
| F4 dead target | With no disable lease, observe a dead target as unlocked/awaiting TDM for one second; stop its indicator and permit a new live target to acquire its own indicator. Expiry fails closed. A held lease still requires TDM's actual unlocked acknowledgement, even if the target is dead. | **PASS** grace/dead-retarget lease policy tests. Auto-retarget **NOT_RUN**. |
| F5 save | Apply for two seconds; refresh only the owned effect's `lifetime = age + 2`. Native save serializes both fields. Off-thread save requests interrupt without touching effects; a serialized stale or canceled queued effect expires after at most two further seconds of effect updates rather than persisting indefinitely. | **PASS** binary save/load/update inspection. Actual save/load, pausing and visual fade **NOT_RUN**; two seconds means unpaused engine effect time, not wall time. |
| F6 log flood | Lease-request result logged only when the result changes in the opt-in path; release resets the observation. Includes result number for AlreadyTaken/MustKeep diagnosis. Feature-off messages retain Phase 4 text. | **PASS** 10,000 repeated-state test and source review. |
| F7 shader conflict | Default becomes `LifeDetectedUndead`, `0x000AAEB3` / decimal `700083`. Scan found no references in any vanilla MGEF/SPEL/ENCH/ALCH/INGR/SCRL record. Existing same-shader instances no longer refuse locks or get finished/refreshed by Camera; only the new ID is owned. | **PASS** offline scan/ID exclusion tests. Appearance and concurrent external applications remain **NOT_RUN**. |
| F8 overlays | Refresh configuration on every menu event, but interrupt only for blocking flags or Dialogue/Console/Loading. Explicitly ignore HUD/Cursor/Fader/Mist events; non-pausing third-party overlays without focus/modal flags do not interrupt. | **PASS** source/flag audit. Actual menu flags/order **NOT_RUN**. |
| F9 yaw | World yaw now uses `atan2(forward.x, forward.y)`, matching the engine's clockwise +Z yaw convention and the net-yaw column. | **PASS** pinned matrix source and positive/negative-angle round-trip tests. SmoothCam world/net equality **NOT_RUN**. |
| F10 NaN | Use `std::numeric_limits<float>::quiet_NaN()`. | **PASS** source review. |
| F11 notice/parity | A refused lock configuration (including enabled-without-overlay) produces one HUD notice per session via the existing `SendHUDMessage` path. Overlay comment explicitly warns that a held lease can swallow short R3; feature OFF equals base only after restoring/removing the overlay. | **PASS** source review; HUD visibility **NOT_RUN**. |
| F12 CI/evidence | Add a Camera portable job, selected when Camera is in the scope matrix, following the probe job. No change to packaged files or overlay distribution. Correct superseded evidence below and retain historical job records as history. | **PASS** local Camera/probe/package/scope tests; hosted workflow **NOT_RUN**. |

`[TargetLock] bEnabled=0` remains the default. Feature-off code bypasses the new policy; the Phase 4 fragment and parser are byte-identical to `20e215d`, and the reduced feature-off `context()` still compares equal. Portable state/aggregate/lease parity checks remain in place. Runtime equivalence is **NOT_RUN**, and requires the opt-in overlay removed. SmoothCam remains the sole normal third-person transform writer; no RequestCameraControl, crosshair hook, UI DLL requirement or GPL implementation was added.

The overlay retains all 19 guarded keys. Provider config then MCM overrides are read without rewriting either file. U=DIK 22 and R3 hold=0.4 seconds remain unapproved test parameters; keyboard/gamepad conflicts refuse the attempt. Dedicated switching/cursor/cycle bindings remain unbound pending an owner-reviewed tested proposal. Unknown/missing providers or state fail closed where the API permits; no competing lease is forcibly taken.

## Binary and CommonLib audit

**Source (offline):** exact supplied SkyrimSE.exe and format-5 Address Library, 1.7.104.0, pointer size 8, 565759 offsets after the 96-byte header. Disassembly logs are `/tmp/skycrpg-p5-fourth-{offline,followup,abi}.log`. Native return values were traced through callees, not inferred from declarations.

| Path | Verified facts |
| --- | --- |
| ApplyEffectShader, AE ID 19872, RVA `0x2F7420` | Tail jump at `0x2F7602` to `0x5DB480`; constructor at `0x5DE270` initializes the shader object. Registration dispatch `0x784AD0` leads to direct list insertion `0x788830` or queued `0x669340`; direct path leaves RAX=1 at spinlock release `0x7889FE`. Init failure tail-jumps through deleting destructor at `0x5DB4D3`. None is a returned effect pointer. The argument locations/duration remain valid; the return is ignored. |
| Lifetime/update | `0x5DB48D` stores duration at +0x10; `0x5DB4FC` increments age at +0x20; `0x5DB570`–`0x5DB58C` checks finite lifetime against age and sets finished +0x40. Compile-time asserts cover both fields. |
| Save/load | Shader vtable AE ID 206059 resolves RVA `0x18FB610`: slot 0x2D → `0x5DFB20` → `0x5DB810` → `0x4A9940`. That base save writes lifetime +0x10 and age +0x20 (also initialized +0x24 and ID +0x28); `0x5DB810` additionally writes finished +0x40 and target. Shader load slot 0x2E → `0x5DFC20` → `0x5DB910` → `0x4A99E0`, restoring lifetime and age. Therefore saving an unfinished effect is possible, but finite remaining time survives serialization. |
| ForEachShaderEffect | Pinned CommonLib implementation locks `magicEffectsLock` and iterates `magicEffects`; `TempEffectTraits::As` dispatches `GetType()`. Exact shader vtable slot 0x2C → `0x5E2310` returns EAX=10, matching kRefShader. Registration uses the same list +0x108 and lock +0x120. Callback uses asserted effect ID +0x28, target +0x38, finished +0x40, shader +0x108. No native function return is interpreted as an effect pointer. |
| Singleton/data access | ProcessLists ID 400315 → `0x219E570`; Main ID 403449 → `0x3230530` (prior offline Main+0x28/GetCurrentThreadId check rerun); TESDataHandler ID 400269 → `0x219DED8`; form map ID 400507 → `0x21A3808`, map lock ID 400517 → `0x21A3C98`. These are data, not function return ABIs. |
| Form/input/handle helpers | `TESDataHandler::LookupForm`/`LookupModByName` and `TESForm::LookupByID` implement C++ file-list/form-map lookup; `ControlMap::GetMappedKey` and `GetUserEventName` implement C++ mapping traversal. `ButtonEvent` accessors are existing linked Camera calls. ActorHandle→ObjectRefHandle now copies the handle through its C++ converting constructor (no CreateRefHandle call). Actor handle resolution, IsDead, Is3DLoaded, UI/GetMenu and HUD notice use existing Camera/probe paths; no device-class method remains. Existing third-person hooks remain IDs 50896/50914. |

**Source:** CommonLibSSE-NG 7.2.0 revision [`7a60f4de794095d7b0f8928d1b930a52e9a7da83`](https://github.com/alandtse/CommonLibSSE-NG/tree/7a60f4de794095d7b0f8928d1b930a52e9a7da83), particularly `TESObjectREFR.cpp`, `ProcessLists.cpp`, `TempEffectTraits.h`, `TESDataHandler.cpp`, `TESForm.h`, `ControlMap.cpp`, `BSPointerHandle.h`, `IMenu.h` and the effect headers. The misleading ApplyEffectShader return declaration is explicitly overridden by the binary evidence. No local Windows link proof is claimed. Two guessed optional source-file paths returned HTTP 404; the tree located the actual template implementation in `TempEffectTraits.h`.

**Source:** [`NiMatrix3.cpp`](https://github.com/alandtse/CommonLibSSE-NG/blob/7a60f4de794095d7b0f8928d1b930a52e9a7da83/src/RE/N/NiMatrix3.cpp) at that same revision: the Y/forward column has positive sin(z), cos(z); ZXY composition scales both by cos(pitch). Thus negating x inverted yaw. Portable tests cover both yaw signs; no runtime world-angle stability is inferred from this algebra.

**Source:** TDM revision [`57b913ae17fed857c9e281799174898cd66a3e82`](https://github.com/ersh1/TrueDirectionalMovement/tree/57b913ae17fed857c9e281799174898cd66a3e82), `ModAPI.cpp`, API header, `Hooks.cpp` and settings: disable-request acquisition uses the owner atomic and handler force-disable flag without a thread-ID precondition. Startup/failure acquisition is allowed; effect mutation and release remain restricted to Main/current/TDM equality. A mismatch publishes an interrupt and retains suppression instead of leaving the overlay active without policy. Public source correspondence to the pinned installed TDM DLL remains **NOT_RUN**; no provider implementation copied. Relaxed mode/zero adjust speeds preserving orientation remains a **Hypothesis** until the trace route.

## Shader scan and ownership

**Source (offline) / PASS:** decompressed/parsed all 869688 Skyrim.esm records, including all 169 EFSH records. For each shader, conservatively searched every byte position of MGEF, SPEL, ENCH, ALCH, INGR and SCRL payloads for its four-byte FormID. This can overcount accidental references; zero matches rules out direct references in these record types. LifeDetectedEnemy `0xDC209` had nine MGEF matches (DetectLifeEnemy interior/exterior, Aura Whisper and Werewolf detect-life effects). LifeDetectedUndead `0xAAEB3` had zero across all six types. Existing scan records its SmallGlowSwirls texture and GradGhostEtherShader gradient. Script/quest/mod applications are not ruled out. No extracted game records or artwork were committed.

**Hypothesis:** the alternate detection glow is a suitable minimal Camera-only indicator. Appearance, occlusion and acceptability are **NOT_RUN** and remain an owner decision. Original Camera code owns only a new effect ID for the target/shader. It never stops or refreshes preexisting matches. The bounded queued path necessarily identifies the first new match; another source creating the exact same shader on the same target during that window is not distinguishable through this API and needs compatibility testing. Interrupt/retarget finishes an owned observed ID; canceled unobserved queued instances are finite and expire without refresh. No raw effect pointer is retained across calls or loads.

## Input identities and validation

| Read-only input | SHA-256 |
| --- | --- |
| SkyrimSE.exe | `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` |
| versionlib-1-7-104-0.bin | `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842` (prior pinned evidence reused; offsets re-read) |
| Skyrim.esm | `e198c3b85e5e48e0c92a6580d8f66e644256b68d812ee32b61735cf9b753df73` |

Host `dq-MEG-Z590-Trident-X-MS-B926`; g++ Ubuntu 15.2.0-16ubuntu1. Exact executable/ESM hashes rechecked this pass. No new Windows DLL or native package exists, so there are no new DLL/package hashes. Python package tests produce synthetic fixtures only, regardless of their printed candidate labels.

| Check | Actual result |
| --- | --- |
| Camera portable matrix, `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -I modules/camera/include` | **PASS**, 8/8 binaries; lifecycle, trace window/writer, obstruction policy/probe, base policy, precision policy/trace, target-lock policy. |
| Testprobe portable matrix (unchanged CI shape) | **PASS**, 8/8 binaries. |
| `python3 -B -I -m unittest discover -s tools/tests -v` | **PASS**, 23/23 package/scope tests. |
| Working-tree assembly + baseline parser/fragment/reduced-context checks | **PASS**, separate synthetic assembly assertion and one focused unittest; overlay omitted, unrelated keys preserved. |
| Offline shader scan, apply/save/vtable audits | **PASS**, exact binary/record facts above; no game run. |
| Supplied Windows candidate CI | **FAIL**, unresolved device symbols in owner-supplied CI 41475236195. |
| Corrected Windows build/hosted portable job | **NOT_RUN**, orchestrator CI after review/commit. |
| All runtime rows | **NOT_RUN**: first/queued lock, indicator coexistence/appearance/retarget/expiry/save-load, menu/thread/hook failure paths, HUD notice, U/R3 hold/switch/yaw/zoom, stable world angles, bow/spell hits, F6/F8, Camera-only/combined/UI-only and owner acceptance. |

## Proposed shared-file changes

- `docs/CONTROLS.md`: document overlay restoration, swallowed short R3 while lease-held, U and 0.4 s as test parameters, and pending switch/cursor/cycle proposal.
- `docs/ARCHITECTURE.md`: record startup fail-closed lease, two-idle input barrier, bounded apply/dead-retarget windows and finite effect-ID ownership; Camera-only remains independent of UI.
- `docs/STATUS.md` and affected tickets: supplied Windows candidate FAIL, corrected build/game/acceptance NOT_RUN, portable/offline PASS. Do not equate compilation with gameplay acceptance.
- `docs/DECISIONS.md`: retain D026 and D5-2–D5-6; engineering defaults below are not new owner decisions.
- `docs/compatibility/dependency-lock.md`: no new provider; selected vanilla shader is `0xAAEB3`. Keep existing executable, TDM and SmoothCam pins.
- Packaging: unchanged; P5-06 still decides overlay delivery. The requested Camera portable CI job is implemented in this diff; no proposed packaging mutation.

## Decisions made and questions for the owner

Engineering decisions: 500 ms apply-registration window; one-second TDM death-retarget window; two-second refreshed finite effect; LifeDetectedUndead configurable test default; 16 pre/post trace polls, asynchronous 100 ms flush, 2048-row hard cap; refuse live settings rather than rewrite them. No owner ruling changed.

After the serialized route: approve/replace U and the measured R3 hold threshold; approve/replace the shader's actual appearance; select the tested switch/cycle/cursor proposal; decide overlay delivery at P5-06. No answer is required to review this diff.

## Orchestrator runtime route — NOT_RUN

Record source/DLL/package hashes, runtime, host, loaded dependencies and effective INIs before a bounded Camera-only run on an isolated profile/copied save. One serialized Skyrim PID, log, 20-minute deadline, exit/cleanup signal; only the orchestrator launches. Test first lock and queued apply, two enemies, target death/auto-retarget, already-glowing target, failed/ended shader and fresh retry. Save while locked (including off-thread notification), reload with Camera enabled and absent, and verify finite stale glow expiry. Test no overlay, wrong settings, unknown shader, thread mismatch, hook failure and held-input interruption; retain F6 native escape and SmoothCam F8. Use the separate TargetLock log for idle lock and deliberate yaw, then medium-range bow/spell hits, zoom and R3 short/hold. Verify native blocking menus interrupt and passive overlays do not; test rapid pause open/close, dialogue, loading, death and cell changes. Feature-off parity requires removing/restoring the overlay. All these rows are NOT_RUN.

## Historical jobs and handoffs — superseded behavior claims

The following records preserve previous commands/results. Their return-pointer safety, permanent-effect save safety and startup-without-lease claims are **incorrect and superseded above**. Historical Windows NOT_RUN does not erase the subsequent supplied CI FAIL.

## Job records

The following declarations were written before the corresponding jobs ran. Every timeout-owned job completed; all retained tools are under /tmp. No game process was created. Completed tool handles: portable matrix `2285`, corrected address inspection `52250`, indicator inspection `39808`.

Second-pass offline job declared before execution: scan local Skyrim.esm EFSH records and inspect pinned executable addresses (no game); commands `python3 /tmp/skycrpg-p5-lock-source/scan_shaders.py` and bounded Python/objdump inspection; expected record IDs, API address/prologue/call-site evidence; timeout 60 seconds each; owner DYL-197 worker; completion process exit; logs `/tmp/skycrpg-p5-lock-shaders.log` and `/tmp/skycrpg-p5-lock-addresses.log`; timeout owns/cleans children.

Second-pass portable job declared before execution: purpose lease/fresh-input/precedence/fragment guard and Camera regression; command `timeout --kill-after=5s 600s bash -c 'set -e; for test in modules/camera/tests/*_test.cpp; do name=$(basename "$test" .cpp); g++ -std=c++20 -Wall -Wextra -Werror -I modules/camera/include "$test" -o /tmp/skycrpg-p5-second-$name; /tmp/skycrpg-p5-second-$name; echo "PASS $name"; done'`; expected seven PASS lines/exit 0; timeout 10 minutes; owner DYL-197 worker; completion process exit; log `/tmp/skycrpg-p5-lock-tests.log`; timeout owns/cleans child jobs, binaries remain in /tmp only.

Offline address attempt 1: **FAIL** in inspection helper after resolving ApplyEffectShader: it treated the ProcessLists singleton's zero-initialized virtual storage as file-backed code. New hypothesis/input: classify singleton ID 400315 as data/BSS and inspect its actual RIP-relative users; rerun the corrected script once, same 60-second deadline/log ownership, log `/tmp/skycrpg-p5-lock-addresses.log`. This was a helper error, not a game result.

Packaging focused job declared before execution: purpose verify the current working-tree fragment through `tools/package.py::merge_ini`, preserve unrelated keys, and run the existing Camera assembly test (which deliberately reads committed config, so it is regression-only). Commands `PYTHONDONTWRITEBYTECODE=1 python3 /tmp/skycrpg-p5-lock-source/check_package.py` and `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_package.PackageTests.test_camera_assembly_preserves_provider_keys_and_independence`; expected assertions/one unittest PASS; timeout 60 seconds each; owner DYL-197 worker; completion exit 0; log `/tmp/skycrpg-p5-lock-package.log`; timeout owns/cleans children. No package or tool source mutation.

Additional offline job declared before execution: inspect shader-constructor/registration callees and two aligned native call sites, confirm process-list singleton RIP-relative references, and hash Skyrim.esm; purpose validate the newly used ABI/data lookup without a game; command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-lock-source/inspect_indicator.py`; expected disassembly/offset references and one ESM SHA-256; owner DYL-197 worker; completion exit 0; log `/tmp/skycrpg-p5-lock-indicator.log`; timeout owns/cleans children.

Final focused portable recheck declared before execution: purpose validate explicit integer conversion in shared guard header and final documented fragment; command `timeout --kill-after=5s 60s bash -c 'set -e; g++ -std=c++20 -Wall -Wextra -Werror -I modules/camera/include modules/camera/tests/target_lock_policy_test.cpp -o /tmp/skycrpg-p5-second-target_lock_policy_test; /tmp/skycrpg-p5-second-target_lock_policy_test'`; expected exit 0; timeout 60 seconds; owner DYL-197 worker; completion process exit; log `/tmp/skycrpg-p5-lock-final-test.log`; timeout owns/cleans children. Broader tests reuse the seven-binary PASS; other portable sources unchanged.

## Second-pass worker handoff (historical; superseded above)

Files changed (including the supplied uncommitted first-pass files): `modules/camera/src/plugin.cpp`, `modules/camera/include/target_lock_policy.h`, `modules/camera/include/lifecycle_policy.h`, `modules/camera/include/policy.h`, `modules/camera/config/SkyCRPG_Camera.ini`, `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini`, `modules/camera/tests/target_lock_policy_test.cpp`, `docs/evidence/p5/target-lock.md`.

Actual results: portable g++ matrix 7/7 **PASS**, focused final guard test **PASS**, working-tree package merge **PASS**, existing Camera assembly unittest **PASS**, EFSH scan **PASS**, corrected address inspection **PASS** (first helper attempt **FAIL**), constructor/registration/singleton inspection **PASS**, `git diff --check` **PASS**. Commands and logs are recorded above. Windows compilation and every game/owner row remain **NOT_RUN**. Source review rejects a blanket Phase 4 native-POV equality claim for the full new fragment; the integrator must select profile-specific settings. Engineering decisions and owner questions are listed above. No running job remains; no commit/push/PR or game launch was performed.

## Third-pass verification jobs (declared before execution)

Owner for all jobs: DYL-197 third-pass worker. No game/Proton process. Completion signal: process exit and recorded log; `timeout --kill-after=5s` owns the process tree and cleans children at its deadline. Artifacts/logs under `/tmp`; no native package identity implied.

- Portable Camera matrix: purpose verify Phase 4 prerequisite/state equivalence, lease/fresh-input interruption ordering, trace cap, overlay guard and existing Camera policies. Command `timeout --kill-after=5s 600s bash /tmp/skycrpg-p5-third-tests.sh`; expected seven PASS lines, exit 0; deadline 10 minutes; log `/tmp/skycrpg-p5-third-tests.log`.
- Packaging/source focused check: purpose run unchanged Camera assembly unittest and inject working-tree config reads into its synthetic fixture to verify the extra overlay is ignored, Phase 4 merged POV/key values preserved, unrelated keys retained; verify exact baseline parser and fragment. Command `timeout --kill-after=5s 60s env PYTHONDONTWRITEBYTECODE=1 python3 /tmp/skycrpg-p5-third-package.py`; expected assertions and unittest PASS, exit 0; deadline 60 seconds; log `/tmp/skycrpg-p5-third-package.log`. No packaging/test source edits or commits.
- Offline Main-thread verification: purpose resolve newly read Main singleton (AE ID 403449), inspect RIP-relative users and thread-ID accesses in the exact binary. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-main.py`; expected resolved address and disassembly, exit 0; deadline 60 seconds; log `/tmp/skycrpg-p5-third-main.log`. Existing unchanged shader ABI evidence above is reused.

Third-pass final source/ABI review declared before execution: after tightening startup interrupt retention and bypassing lock input traversal when off, compare reduced feature-off `context()` byte-for-byte against `20e215d`, inspect every lease/effect call site, and resolve the imported function called beside the observed Main+0x28 comparison. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-review.py` plus `git diff --check`; expected assertions PASS, IAT name `GetCurrentThreadId`, clean diff; owner DYL-197 worker; exit completion, log `/tmp/skycrpg-p5-third-review.log`; deadline 60 seconds, timeout cleanup. Portable sources unchanged since the seven-binary PASS; reuse that evidence.


## Third-pass final handoff

Files changed from the supplied second-pass candidate: `modules/camera/src/plugin.cpp`, `modules/camera/include/policy.h`, `modules/camera/include/target_lock_policy.h`, `modules/camera/tests/target_lock_policy_test.cpp`, `modules/camera/config/SkyCRPG_Camera.ini`, `modules/camera/config/TrueDirectionalMovement-settings.fragment.ini` (restored exactly to Phase 4, therefore no diff from HEAD), new `modules/camera/config/TrueDirectionalMovement-target-lock.fragment.ini`, and this evidence file. Inherited `modules/camera/include/lifecycle_policy.h` changes remain part of the final candidate; no third-pass edit needed there.

Actual results: `timeout --kill-after=5s 600s bash /tmp/skycrpg-p5-third-tests.sh` **PASS**, 7/7 portable binaries (g++ C++20, `-Wall -Wextra -Werror -pthread`); package/source fixture command above **PASS** (one unchanged unittest plus working-tree assembly/byte-identity assertions); Main singleton/member/import inspection **PASS**; reduced feature-off context comparison **PASS**; `git diff --check` **PASS**. Logs: `/tmp/skycrpg-p5-third-{tests,package,main,review}.log`. Completed tool handles: portable `65798`, offline `42032`; package and final review exited within their initial call. The first import-label helper audit was **FAIL** (overbroad descriptor attribution); one corrected terminated-thunk scan **PASS**, uniquely identifying KERNEL32.dll. No timeout, running job, game process, commit, push or PR created by this pass.

Unverified: Windows/MSVC build, actual TDM/main callback identity/order, indicator appearance/persistence, controller hold and switching, world-angle stability, ranged hits, all game regressions and owner acceptance remain **NOT_RUN**. Prior shader ABI/record evidence is reused, not relabeled as a new game result. Decisions and owner questions are in their section above; no owner ruling changed.

Final import-label audit: **FAIL** in the first helper's attribution despite exit 0: it did not bound the IAT slot by each descriptor's terminating thunk and printed extra DLL names for the same import. The opcode/address/member observations are unaffected. One diagnostic correction uses terminated thunk enumeration and requires exactly one matching descriptor. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-review.py`; expected exactly `KERNEL32.dll!GetCurrentThreadId` plus existing source assertions; owner DYL-197 worker; log `/tmp/skycrpg-p5-third-review.log`; exit completion, 60-second deadline/timeout cleanup. Also make the hook-failure release explicitly feature-off-only: opt-in has no lease before hook installation; Phase 4 release remains unchanged when OFF.

Corrected final import review: **PASS**, exit 0, unique `KERNEL32.dll!GetCurrentThreadId`; reduced context/startup assertions and `git diff --check` also **PASS**. No helper process remains.


## Fourth-pass jobs (declared before execution)

Owner: DYL-197 fourth-pass worker. Offline job: verify effect return paths, finite lifetime/save fields, yaw and unused EFSH candidates from the supplied read-only executable/ESM. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-fourth-offline.py > /tmp/skycrpg-p5-fourth-offline.log 2>&1`; expected disassembly, hashes and shader reference scan; completion process exit; 60-second deadline, timeout owns/cleans children. No game job authorized or started.

Offline follow-up: inspect the apply tail, registration dispatch, base save/load callees and native third-person yaw composition identified in the first inspection. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-fourth-followup.py > /tmp/skycrpg-p5-fourth-followup.log 2>&1`; expected aligned disassembly; owner fourth-pass worker, 60-second deadline, exit completion, timeout cleanup. This is additional address coverage, not a retry.

Fourth-pass automated job: run all eight Camera portable tests (new trace/writer test included), all eight probe portable tests to mirror the unchanged CI job, and all package/scope Python tests because the workflow changed. Command `timeout --kill-after=5s 600s bash /tmp/skycrpg-p5-fourth-tests.sh > /tmp/skycrpg-p5-fourth-tests.log 2>&1`; expected 16 PASS lines and Python suite OK, exit 0; owner fourth-pass worker; 10-minute deadline; completion exit; timeout owns/cleans child compilers/tests. Binaries/log under /tmp; no Windows/game proof.

Fourth-pass baseline/assembly review: command `timeout --kill-after=5s 60s env PYTHONDONTWRITEBYTECODE=1 python3 /tmp/skycrpg-p5-third-package.py > /tmp/skycrpg-p5-fourth-package.log 2>&1` and `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-review.py > /tmp/skycrpg-p5-fourth-review.log 2>&1`; purpose recheck unchanged Phase 4 fragment/parser/reduced context and working-tree overlay exclusion; expected assertions PASS/one assembly test OK/clean diff; owner fourth-pass worker, 60 seconds each, exit completion, timeout cleanup.

Fourth-pass CommonLib ABI audit: resolve all new singleton/form-map data IDs and shader vtable, check the iterator's GetType slot, shader save/load overrides and registration/list offsets. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-fourth-abi.py > /tmp/skycrpg-p5-fourth-abi.log 2>&1`; expected IDs, GetType=10 assertion and disassembly; owner fourth-pass worker, 60-second deadline, exit completion, timeout cleanup. Source implementations (pinned CommonLib revision) distinguish C++ container traversal from native relocated/virtual calls.

Fourth-pass final review: after the native-only dead-acknowledgement, bounded-discovery and handle-copy refinements, inspect the final diff and rerun baseline reduction plus a clean whitespace check. Command `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-review.py > /tmp/skycrpg-p5-fourth-final-review.log 2>&1` and `git diff --check`; expected PASS, owner fourth-pass worker, 60-second deadline, exit completion, timeout cleanup. Reuse the passing portable suite: no portable code/test changed after that run. Input matching uses physical U=22/R3=128 IDs, so the barrier does not depend on native user-event naming order.

## Fourth-pass final handoff

Files changed: `.github/workflows/build.yml`; `modules/camera/src/plugin.cpp`; `modules/camera/include/target_lock_policy.h`; new `modules/camera/include/lock_frame_trace.h`; `modules/camera/tests/target_lock_policy_test.cpp`; new `modules/camera/tests/lock_frame_trace_test.cpp`; `modules/camera/config/SkyCRPG_Camera.ini`; `modules/camera/config/TrueDirectionalMovement-target-lock.fragment.ini`; `docs/evidence/p5/target-lock.md`.

Actual commands/results:

- `timeout --kill-after=5s 600s bash /tmp/skycrpg-p5-fourth-tests.sh`: **PASS**, exit 0; Camera 8/8, testprobe 8/8 and `python3 -B -I -m unittest discover -s tools/tests -v` 23/23. Log `/tmp/skycrpg-p5-fourth-tests.log`; completed exec handle `88702`.
- `timeout --kill-after=5s 60s env PYTHONDONTWRITEBYTECODE=1 python3 /tmp/skycrpg-p5-third-package.py`: **PASS**, exit 0; focused assembly unittest plus working-tree overlay exclusion/baseline assertions. Log `/tmp/skycrpg-p5-fourth-package.log`.
- `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-third-review.py`: **PASS**, exit 0 both initial and final source reviews. Logs `/tmp/skycrpg-p5-fourth-review.log`, `/tmp/skycrpg-p5-fourth-final-review.log`.
- `timeout --kill-after=5s 60s python3 /tmp/skycrpg-p5-fourth-offline.py`, and corresponding `-followup.py`, `-abi.py`: **PASS**, exit 0 each; shader scan, hashes, native return/save/lifetime/list/type inspection. Matching `.log` paths; completed offline exec handle `67762`.
- `git diff --check`: **PASS**. All jobs exited; no timeout or child job remains. One shell-script preparation delimiter typo was corrected before execution; it launched no test. There was no failing build/test retry.

Unverified: corrected Windows build/link and hosted CI; all in-game indicator/save/retarget/menu/input/thread/hook/notice/aim/independence behavior; owner acceptance. Prior Windows CI remains **FAIL**, not replaced by portable PASS. No native package hash invented. Decisions and owner questions appear above. No commit, push, PR, game process or packaging change made by this pass.

## In-game run p5-b — 2026-11-02

Camera DLL `3b6ff9cf…` from `6b42f35`, camera-only, keyboard/mouse, unattended.

| Route | Result | Note |
| --- | --- | --- |
| `p5-cam-off-main`, bEnabled=0, no overlay | **PASS** | Same steps as `dyl173-cam-main`. |
| `p5-cam-off-disabled`, bEnabled=0, no overlay | **PASS** | Same steps as `dyl173-cam-disabled`. |
| `p5-lock-main`, bEnabled=1 | **FAIL** | The lease was never released, so no lock happened. It failed closed. |

Cause: the log shows `current=620 Main=368 TDM=620`. `GetTDMThreadId` returns the thread that first requested TDM's API (data load), while TDM logs its updates on 368, the main thread. Requiring all three IDs to match could never pass. The gate now requires only `Main::threadID`. Runs `batch-p5-a-00` and `-01` failed because of a stage builder bug that deployed JSON as the DLL; they tested no plugin code.

## In-game run p5-d — 2026-11-02

`p5-lock-main` on Camera DLL `9a5a2278…` from `82b2121` was **FAIL**: the lease was still never released, with no refusal logged, so no lock happened and it failed closed. The update hook reached the normal state, so the likely cause is the release-input tracker, which only advanced on the main thread and counted input dispatches. Probe records show menu events running on several worker threads as well as main; the input sink's thread had not been logged.

The tracker now uses atomics and a 0.25 s quiet window, because a held key emits an event on every poll, and works from any sink thread. Off-main `context()` reuses the last main-thread decision, so F6/F7/F8 aren't affected. A bounded line logs each lock state change and the sink's thread. Whether this cause is right is **not yet confirmed in game**.

## Owner hands-on run owner-lock-1 — 2026-11-02

Camera DLL `8be698a5…` from `63e992e`, camera-only, keyboard, Bleak Falls Barrow save with placed bandits, isolated profile; restore PASS, no leftover processes. The owner said: "The lock worked, I'm satisfied with the implementation". This is owner feedback on the lock, not the P5-06 gate.

| Check | Result | Evidence |
| --- | --- | --- |
| U locks a hostile from the elevated view | **PASS, keyboard** | `locked=1`, decision 9, target handle `0010FBEF`, one lock of 18.8 s. |
| Lease released only after acknowledgement, retaken after | **PASS, bounded** | State log; retaken after the lock ended. |
| Stable world pitch while locked | **PASS** | 1148 locked trace rows, all at 55.0 degrees. |
| Stable yaw with no look input | **PASS** | 0.0001 degree drift over 18.8 s, confirming the zero-speed hypothesis for this window. |
| Visible indicator in Camera-only | **PASS, owner** | Owner: visible, looks fine. Shader `LifeDetectedUndead`. |
| Target death ends lock | **PASS, owner** | Owner: the enemy died; no stale target in the log. Auto-retarget with a second hostile **NOT_RUN**. |
| Nothing wrong while locked | **PASS, owner** | Owner: all normal. |
| User yaw and wheel zoom while locked | **NOT_RUN** | No mouse input logged during the lock. |
| Bow and aimed-spell hits while locked | **NOT_RUN** | |
| Target switching | **NOT_RUN** | No binding yet. |
| Menu, dialogue, load, F6, F8 interrupting a real lock | **NOT_RUN** | The handoff across these passed in unattended routes with no lock held. |
| Controller R3 short press and hold | **NOT_RUN** | |

Unattended runs `p5-f`, `p5-g`, `p5-h` and `p5-i` reported runner PASS but never locked (`locked=1` absent), so they are not lock evidence. Their setups gave no confirmed valid target: the Whiterun exterior had locals in the way, and spawned bandits weren't seen in QASmoke. `session-p5/routes/check_lock_log.py` now guards against counting such a run. A correction to run p5-d above: the input sink ran on the main thread here, so the off-main sink theory didn't hold. The dispatch-counting tracker was replaced anyway, and the lease has released since.

## Owner hands-on run owner-lock-2 — 2026-11-02

Same build, profile and location as `owner-lock-1`, keyboard/mouse; restore PASS. The owner reported: "All checks passed except the last one because there was only one bandit."

| Check | Result | Evidence |
| --- | --- | --- |
| Arrows hit the locked enemy from the elevated view | **PASS, owner** | Owner report; hits are not independently logged. Locked case only: unlocked overhead aim is not fixed, and F6 remains that fallback. |
| Firebolt hits the locked enemy | **PASS, owner** | Owner report; not independently logged. |
| Menu interrupts a held lock; only a new U re-locks | **PASS, owner** | Three locks on one target (5.9, 12.0, 4.6 s), with lease cycles in the state log. |
| F6 interrupts a held lock; only a new U re-locks | **PASS, owner** | As above. |
| Stable pitch across all locks | **PASS** | 1372 locked rows at 55.0 degrees. |
| Mouse yaw and wheel zoom while locked | **UNRESOLVED** | The owner reported all checks passed, but the trace shows no look input and no meaningful yaw change while locked. Zoom isn't traced. To be confirmed. |
| Auto-retarget on target death | **NOT_RUN** | Only one bandit was available. |
| Controller R3 | **NOT_RUN** | Separate run. |

The owner made a quicksave next to the bandit for repeat tests. Restore moved it to the run's preserved outputs, and it's backed up as `session-p5/owner-saves/Quicksave0_…_20261102205255_1_1` outside the repository. In that session yaw moved only 0.06 degrees overall, locked or not, so the mouse may not have reached the game at all; Deskflow is known to block mouse look on this host.

## Owner hands-on run owner-lock-3 — 2026-11-02

Same build, camera-only, controller on, from the owner's quicksave next to the bandit; restore PASS. The owner said: "It worked and felt right, I approve". Recorded as owner ruling D027.

| Check | Result | Evidence |
| --- | --- | --- |
| R3 short press toggles lock; hold reaches native POV at 0.4 s | **PASS, owner** | Owner approval; lock segments in the trace. |
| No double fire from one R3 press | **PASS, owner** | Owner report; not independently traced. |
| Right-stick look turns the camera while locked, pitch held | **PASS** | 114 locked rows with look input; yaw range 240.4 degrees; pitch 55.0 degrees throughout. |

This shows user look works while locked on controller. Mouse look uses the same look hook but was not exercised while locked (owner-lock-2 logged no look input), so the mouse row stays **NOT_RUN** rather than PASS. **Still NOT_RUN:** wheel or controller zoom while locked, target switching, auto-retarget, and dialogue, load and F8 interrupting a held lock.
