# DYL-174 documentation review

Base `c7333c7`, worktree base `8f1dc119b6abc648205c8f0714a3217dfaf41631`; head `<head SHA>`, CI `<CI run>`, DLL hashes `<DLL sha256>`, archives `<candidate archive hashes>`. Owner: docs worker. No game, Steam or Proton launch; new runtime acceptance **NOT_RUN**. Candidate files must be rebuilt from a clean committed source before final assembly.

| Item | Audit and result |
| --- | --- |
| Install, controls, limitations | [Installation](../../INSTALLATION.md), [controls](../../CONTROLS.md), [notes](../../RELEASE_NOTES.md): package paths, overrides, defaults and exact module messages cross-checked against source. Final manager installs, update, disable and removal **NOT_RUN**. |
| Evidence images | Inventory below identifies images in existing evidence; no concept image is presented as shipped. Historical evidence files are outside this lane's write ownership, so labels are indexed here. |
| Rights and attribution | [Credits and draft source statement](../../THIRD_PARTY.md); legacy CommonLib MIT text copied from pinned local source and added to both package inventories. **BLOCKED:** local searches found no spdlog, fmt, DirectXTK or rapidcsv license text; link composition per DLL is unverified. Supply the applicable original notices before distribution; do not treat the notice set as complete. Public SWF distribution and HUD asset provenance await owner review. |
| Release flow and route | [Checklist](../../RELEASE_CHECKLIST.md) and [playtest](../../PLAYTEST.md); owner acceptance **NOT_RUN**. |
| Links and package/config cross-check | See results below. |

## Image classification

All listed are **real game evidence**, not concept art; stills do not prove actions. No README images or Markdown concept-image links were found. Historical references in `docs/evidence/p3/input-and-layout.md` are real owner screenshots from `p3-input-168` and `p3-input-168-r3`; `docs/evidence/p2/hotbar.md` is the real owner screenshot from `wave2-hotbar-route`, and `docs/evidence/p2/obstruction.md` has real before/after images from `wave2-obstruction-route`. The temporary clipboard captures in `docs/evidence/p1/ui-spike.md` are owner-supplied real evidence; run `20261026T071034`. The Party Sheet asset names in `docs/evidence/p3/follower-hud.md` describe external provider files, not project screenshots.

- Phase 1 baseline interior/exterior, `docs/evidence/p1/session-2026-10-26.md`: real baseline evidence, run `20261026T070429`.
- Phase 1 combined grid and overhead/minimap, referenced by `ui-spike.md`, `camera-spike.md`, `session-2026-10-26.md` and Phase 2 camera controls: real evidence, run `20261026T075556`.
- Phase 1 Bleak Falls room, `gate.md`: real evidence, run `20261026T100826`.
- Phase 2 Camera-only exterior and combined HUD, `camera-controls.md`: real evidence, runs `20261026T115627` and `20261026T123755`, respectively.
- Phase 2 HUD combined, `core-hud.md`: real evidence, run `20261026T123755`; before/after sprint: real UI-only evidence, route 3 (`runtime-route-3`). Failed route 2 screenshot is real evidence from `ui-route-2`.

## Package and configuration audit

`expected_paths(full=True)` and `OVERRIDES` were compared with the explicit file inventory in [installation](../../INSTALLATION.md): each module DLL and INI, three UI movies, three Camera provider overrides and all eight per-module notice/build files are named there. Camera and UI have disjoint install paths. Documented INI keys were checked against their respective shipped module INIs; the generic `bEnabled` in the release checklist refers to the respective module sections in installation. The package manifest additionally carries external requirements and optional minimap providers; those are documented. The missing license notices above are not yet in `expected_paths` and remain a blocker.

## Validation and runtime route

Link checker and test commands, plan, and logs live at `/srv/storage/Projects/SkyCRPG/session-p4/logs/docs/`. Both commands are bounded at 600 seconds; expected success is exit 0. **PASS:** 22 package/tool tests, link targets, package/config cross-check and `git diff --check`. The first unit run failed because the new notice is uncommitted and the fixture read `git show` at the old HEAD; the test now supplies the new notice as a fixture, and the final run passes. The first cross-check incorrectly capitalized UI in its file name; it was corrected before the passing run. The initial link check reported this review file before it had been created; the final run passes. `git diff --check` checks whitespace. No SWF was built, no engine APIs changed.

For the eventual owner route: on the exact packaged candidate, separately record keyboard/mouse and controller inputs and verify fresh Camera-only, UI-only and combined installs, F6/F7/F8, zoom and native aim, inventory/filter/hotbar, minimap/compass, and disable/update/removal with copied saves; see [playtest](../../PLAYTEST.md) and [packaging route](packaging.md). All are **NOT_RUN** here. Record executable, dependencies, host, source, CI and archive/DLL hashes before each run. Owner acceptance remains pending.
