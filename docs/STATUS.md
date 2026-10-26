# Current status

Updated: 2026-10-26 (P1-03/P1-04 execution).

PR #1 (foundation) and PR #2 (dependency research) are merged. DYL-150 and DYL-151 are Done in Linear. Independent load-only DLLs and a provisional provider stack exist; no playable SkyCRPG MVP or frozen support claim exists.

## Active feasibility work

| Ticket | Branch / review | State |
| --- | --- | --- |
| DYL-152 / P1-03 | `spike/dyl-152-camera`, [draft PR #3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3) | In Progress. Original SmoothCam framing candidate, source-derived schema checks and deterministic config archive. Recovered owner route: movement/melee usable, first-person precision and camera recovery PASS; top-down third-person ranged aiming FAIL. Existing targeting alternative under investigation; held pitch/recenter remain open. |
| DYL-153 / P1-04 | `spike/dyl-153-ui`, [draft PR #4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4) | In Progress. Native SkyUI-backed inventory-grid/HUD candidate compiled against acquired Nexus 6.11 assets; owner verified inventory actions, counts/weight, minimap and compass fallback; controller grid navigation failed and is being fixed. |

Both PRs remain unmerged, auto-merge disabled, for senior review. Workers own separate module/evidence files. The orchestrator owns dependency staging, shared docs, plugin activation and the serial game-test schedule.

## Foundation follow-up

- Rewritten `tools/profile.py` real-host deploy/load/undeploy: **PASS (load only)** on merged main `c5297d5`, run `20261026T065153`. No leftover game processes; original save/configuration hashes preserved.
- Original 2021 copied fixture: **FAIL** to load; owner identified it as heavily modded. The application's clean exit was not gameplay acceptance.
- Owner-requested replacement vanilla1.7.104 Helgen cave/exterior fixtures acquired externally. Bounded baseline route: **PASS (owner-observed)** with interior/exterior captures and clean exit, run `20261026T070429`.
- Controller is available through owner playtesting. UI horizontal grid navigation **FAIL**, fix/retest pending; complete baseline controller coverage remains unverified. DYL-150 is not reopened.

See [session evidence](evidence/p1/session-2026-10-26.md), the separate camera/UI spike reports on their branches, and [dependency lock](compatibility/dependency-lock.md). Package acquisition, compilation and plugin loading do not prove camera or inventory behavior.

## Phase gates

| Gate | State |
| --- | --- |
| Phase 1 — prototypes and stack | In progress; feature routes and review pending |
| DYL-154 / P1-05 — freeze contracts | BLOCKED pending both reviewed spikes and responsible integration choices |
| Phase 2 — camera candidate and playable HUD | NOT_RUN / not started |
| Phase 3 — UI candidate and combined route | NOT_RUN / not started |
| Phase 4 — packaged MVP acceptance | NOT_RUN / not started |

No runtime downgrade, inventory-capacity redesign, second camera writer, broad provider fork or minimap removal has been selected. ADR-001 remains proposed. Native compass stays visible in the UI spike until reliable map readiness/restoration is established; this is a safety default and an open MVP requirement, not acceptance of duplicate navigation for the final UI.
