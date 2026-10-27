# DYL-159 lifecycle audit

Baseline `ef1a7c5bdd2166b189942303d2700a5b3976444f`; line references below refer to that baseline's `modules/camera/src/plugin.cpp`. This is a source audit, not runtime evidence. SmoothCam remains the transform writer. **C4/C5 runtime acceptance: NOT_RUN.**

| Transition | Baseline behavior and relinquish/return | Gap |
| --- | --- | --- |
| Dialogue | `focused` recognizes Dialogue Menu (139–145); `select` suppresses look/update (215–219, 239–243). Menu sink suspends keys (253–258). Return on normal context. | No proof of scene or ownership interruption hidden by focus. |
| Menus | Same focus and suspension; precision retains ownership through focus (269–272; `precision_policy.h:13–16`). | Input-only revalidation; menu-close tween is deliberately tolerated. |
| Scripted scenes | Non-third-person camera and script POV mode are special (166–178); normal context required to act. | Third-person scripted scene without those signals may be missed. |
| Death/reload | Dead/invalid world blocks (164–165); preload resets ownership and keys, postload sets zoom (386–388). | No explicit deferred pitch restoration; transient key reset incomplete. |
| Fast travel | Loading menu/world guard and load messages as above. | No dedicated travel event; validity depends on observed state. |
| Interior/exterior and load doors | Parent cell/3D guard (164); normal context required. | No cell event; transient interruptions without input can be missed. |
| First person | Native first person blocks normal policy; F6 ownership enables deliberate return (166, 215–219, 318–337). | Ownership only revalidated on input. |
| Mount | `IsOnMount` hazard (169), policy resumes in normal state. | Input-only ownership observation. |
| Swim | `IsSwimming` hazard (169), policy resumes in normal state. | Input-only ownership observation. |
| Werewolf/vampire lord | Beast POV mode and non-third-person state guarded (168–172). | No independent race/form signal verified. |
| Furniture sitting/crafting | Focus/camera state can block (139–145, 168–178). | No explicit sit/sleep guard. |
| Killcams | `IsInKillMove` and special camera guarded (168–169). | Input-only ownership observation. |
| Bleedout | Dead or non-third-person camera may block (164, 168). | No explicit bleedout guard. |

The adapter stores no actor or camera pointers between callbacks. Existing provider API pointers are process-lifetime interfaces, not world objects. Pitch is loaded from INI and corrected in `look` (110–128, 228–231); `savePreferences` writes enabled/zoom but not pitch (129–136). A valid hooked look must actually run for pitch correction, so the Phase 1 load-pitch failure is not explained or proven fixed by source alone.

## Candidate changes and limits

- Pre-load, post-load, new-game, `TESLoadGameEvent`, and player reference attach/detach reset ownership, recenter and pending zoom, and require release only for keys held across the reset (idle keys fire on the next press), without caching world pointers. The saved INI pitch remains the target of the normal `look` correction; the in-memory preference for zoom is reapplied only by a normal current third-person update. Neither writes a transform. These paths are **NOT_RUN** in game, including the Phase 1 load-pitch failure; the candidate does not establish that the engine calls `look` when needed after load.
- Third-person hook observations revalidate ownership and suspend controls without requiring a new input event. Each hook still calls its predecessor exactly once; `update` calls it first. This only detects states while one of those hooks actually executes. First-person or menu-hidden interruptions with no hook observation remain unobservable; menu deferral and hazard-free tween retention preserve the owner-passed F6 behavior.
- Sit/sleep and actor bleedout join mount, killmove, swim, script and beast POV signals as hazards. Beast classification still relies on engine POV/camera signals; no independent race check has been validated. A script retaining ordinary camera and control flags can likewise escape detection. Cell attach/detach events are filtered to the player reference; their timing and coverage are **NOT_RUN**.
- `[Debug] iLifecycleTrace=1` enables transition decision lines with camera state ID and a 128-line process-wide cap. Off by default. It is not a complete event trace.
- Current recovery is F8 provider disable, native POV, and F6 re-enable in valid third person; F7 recenters but is not a factory reset. The closed-game INI reset procedure is in [installation](../../INSTALLATION.md). No new long-hold shortcut is claimed.

## Validation

| Check | Result |
| --- | --- |
| Four portable C++ policy and trace tests | **PASS**, Linux g++ with `-std=c++20 -Wall -Wextra -Werror`; [job](lifecycle/test-job.json), [log](lifecycle/test.log). These exercise pure decisions, not engine event delivery or actual camera correction. |
| Windows native build | **NOT_RUN**; CI-only. |
| C4/C5 in-game lifecycle and copied-save removal/restart | **NOT_RUN**. No game, profiles or saves touched. Record runtime, dependencies, source commit and artifact hashes with the eventual owner run. |

Orchestrator review correction: the initial reset marked every custom key as held. Since no release event arrives for an idle key, F6/F7 would have ignored the first press after every load or player cell attach (including load doors). The reset now uses the existing `FreshPress::suspend()` semantics, and the focused test covers both idle and held keys. All four focused camera tests **PASS** after the correction.

## Combined lifecycle + obstruction owner route — 2026-10-26

Integration build `integration/p2-camera-wave5` source `155a3e3671e708de06cd7f3db10bc36fcfae872c` (DYL-159 `85f4338` merged with DYL-157 `f8ff6ad`), Windows build [40442507358](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40442507358) **PASS**; Camera DLL SHA-256 `0eeef6bbb748a9a9ddee0aa8a2c882817028a25b9c6871a6c7d9c2ef62c2e5b2`, archive SHA-256 `05c369c2eee02c6192e9e13093ee80d92f6270eea139b380c0f4bdd3ee38b833`; UI `ba10da6`. For this run only, the deployed INI enabled `[Obstruction] bEnabled=1` and `[Debug] iLifecycleTrace=1`; the INI was preserved and removed on restoration. Controller off. Run `6456672598ca49c684ce42b5cd5a90d0` reached the 20-minute bound and was stopped by the profile timeout (`TIMEOUT`, no leftover process); restoration **PASS**. Camera log SHA-256 `e5be49999143efba60b9734b5639a4d1919a20be9b28af523c9cd9c98a8b25ee`; records under `/srv/storage/Projects/SkyCRPG/session-p2-20261026/wave6-lifecycle-obstruction/`.

**Owner-reported: the requested lifecycle and obstruction route PASS** ("everything else passes"), with the following feedback:

- **Furniture activation from the elevated view — usability concern.** In the Bannered Mare, the owner could not select a bench from the default camera because the crosshair could not be moved lower from that framing. The sit/stand transition itself was not reported as failing. P2-03 activation acceptance should note that the default framing cannot always reach low nearby objects; F6 native first person remains the documented fallback.
- **Obstruction on ceiling beams — works, but is a nuisance.** In the Bannered Mare, moving forward slightly under the ceiling beams makes the camera tighten. This is the intended compression response, but frequent tightening in beamed interiors is a usability concern.
- **Diagnostic finding.** The lifecycle trace exhausted its 128-line cap immediately after load. 124 `observe` lines alternated between `decision=8` (normal) and `decision=2` (special) in third person (`state=9`). A likely cause is that `look()` uses `context(false)` while `update()` uses `context(true)` (`allowZoomSlide`), so each frame reports a different state while `fovSlideMode` is set. A `look()` that observes special state also suspends and resets obstruction. This needs investigation before obstruction tuning, and the trace should not record per-hook alternation.

The owner also observed that NPC dialogue is silent. The same `winegstreamer transform` failure appears in baseline (no-mod) runs, so this is a launcher environment issue (`tools/profile.py` does not pass Steam's `STEAM_COMPAT_MEDIA_PATH` / `STEAM_COMPAT_TRANSCODED_MEDIA_PATH`), not a module defect.

Owner feature requests (future phases, not Phase 2 blockers): proximity "smart" targeting with tab-style cycling on controller; mouse-pointer hover targeting on keyboard/mouse; a controller binding for a right-stick reticle/mouse mode; and a fixed camera that fades or makes obstructing geometry (e.g. beams) see-through instead of pulling in.

## Slide classification and trace dedup candidate

On integration base `155a3e3`, this uncommitted Camera candidate classifies native zoom slide separately from lifecycle hazards. The update hook alone observes lifecycle; slide passes native look input through, pauses obstruction sampling without discarding its latch or recenter, and continues the existing bounded zoom request. F6 remains suppressed during slide. Genuine unsafe states and cell changes still reset as before. Observe traces require ten consecutive update frames with the same decision and camera state before logging a change; load and attach events remain immediate, with the existing 128-line cap.

Focused Linux tests **PASS**: all six Camera policy, precision, trace, lifecycle, obstruction policy and obstruction probe suites, g++ 15.2.0, Linux 7.0.0-38; [job](lifecycle/slide-candidate/job.json) and [log](lifecycle/slide-candidate/test.log). The [initial run](lifecycle/slide-candidate/initial-test.log) **FAIL** was a test fixture warmup sample-count error; the corrected fixture passed. These portable tests do not establish in-game behavior. Windows CI build **NOT_RUN**; owner retest **NOT_RUN**. Runtime behavior of slide timing, ownership, obstruction and trace volume remains to be verified on the pinned host.
