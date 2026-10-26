# Current status

Updated: 2026-10-26 after owner Phase1 spike playtests.

PR1 (foundation) and PR2 (research) are merged. DYL-150/151 remain Done. DYL-152/153 are **In Review**, with two independent unmerged PRs and auto-merge disabled. This is a playable feasibility result with explicit limits, not a complete MVP or support certification.

| Ticket / PR | Current evidence |
| --- | --- |
| DYL-152 / [PR3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3) | **PASS (owner-observed)** pulled-back exploration, qualified melee usability, first-person precision fallback, activation and ten F8 pairs. **FAIL** top-down third-person bow/spell aim. **FAIL** pitch persistence across loading screens; zoom persists. Owner explicitly defers pitch repair without blocking these PRs. |
| DYL-153 / [PR4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4) | **PASS (owner-observed)** UI-only ordinary camera, live grid, native equip/unequip/counts/weight, one action per activation, corrected controller grid/category navigation, minimap indoors/outdoors and compass fallback. Initial two controller attempts failed; imported-class correction passed. |
| Combined Camera + UI | **PASS (bounded startup and owner checkpoint)** run20261026T075556, all8 DLLs loaded, clean exit, top-down/minimap and selected-grid captures. Pitch-persistence limitation above remains. |

## Architecture and remaining decisions

Reuse SmoothCam1.8.1 framing/collision and TDM2.3.1 directional movement; keep lock-on disabled. Native first-person aiming is proven. A small original adapter is recommended for persistent pitch and explicit exclusive targeting handoff, but is not implemented/proven. TDM lock-on writes camera rotations and does not request SmoothCam camera ownership, so enabling both is not an accepted route. No large custom camera is selected.

Reuse native InventoryMenu/Scaleform/SkyUI backing entries and action callbacks, including both imported InventoryLists classes. Native counts and weight remain authoritative. Keep the map trio and visible native compass fallback. Automatic compass replacement, complete follower data and eight-slot hotbar remain later implementation requirements; follower/hotbar probes are not full acceptance.

Senior review must ratify these boundaries and outstanding checks before DYL-154 freezes contracts. Pitch persistence is tracked on DYL-159 and aiming on DYL-158, both still Backlog. DYL-154 can start after that review; it and Phase2 have **NOT_RUN / not started**. No MVP requirement is silently removed.

## Foundation and restoration

- Rewritten profile tool real-host deploy/load/undeploy: **PASS**. Replacement clean Helgen cave/exterior route: **PASS (owner-observed)** with captures. Old heavily modded2021 fixture: **FAIL** to load, preserved separately.
- Controller hardware is available through owner playtesting; grid navigation was actually exercised. Complete baseline/controller and supported-profile matrices remain **NOT_RUN**, not assumed from this route. DYL-150 is not reopened.
- Final preservation: **PASS**, all146 pre-existing save/copy hashes unchanged, original INIs and Plugins.txt restored, no active deployment or Skyrim process. Test fixtures and archives remain external.

See [session evidence](evidence/p1/session-2026-10-26.md), [exact tested artifacts](evidence/p1/tested-artifacts.json), [package hashes](compatibility/dependency-lock.md), the camera/UI spike reports on their PR branches, and [ADR-001](decisions/ADR-001-stack.md). Tight-ceiling/doorway recovery, full controller coverage, native Windows, unsupported camera states and the full supported-profile matrix are not certified by these bounded runs.
