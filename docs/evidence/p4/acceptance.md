# DYL-175 — owner acceptance

Phase 4 internal candidates; automated technical results are not owner acceptance. Public redistribution and related rights clearance are deferred by [D023](../../DECISIONS.md).

| Archive | SHA-256 | Source | CI run | Distribution |
| --- | --- | --- | --- | --- |
| `SkyCRPG_UI-0.1.0-rc.1-6b4c0fc.zip` | `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb` | `6b4c0fc6a6da4effd9faeafc9a2c59874a612271` | [41376680950](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41376680950) | `internal-candidate` |
| `SkyCRPG_Camera-0.1.0-rc.1-6b4c0fc.zip` | `e3f6d61e2d2075a10d8a31c07ccfbe9c78e324230f5c399cadbc2c3c39c86838` | `6b4c0fc6a6da4effd9faeafc9a2c59874a612271` | [41376680950](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/41376680950) | `internal-candidate` |

## Support matrix

| Component | Evidence / status |
| --- | --- |
| Host/runtime | Ubuntu 26.04.1, RTX 3080, Proton Experimental `experimental-11.0-20261027b-x86_64`, Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, Address Library 13; keyboard/mouse automated **PASS** bounded routes. Other hosts/runtimes **NOT_RUN**. |
| Providers | CommonLibSSE-NG 7.2.0, SkyUI Community 6.11, SmoothCam 1.8.1, TDM 2.3.1, MCM Helper 1.6.3, Infinity UI NG 2.1.3, LMU 1.3.4, DEM 1.7.5. [Dependency lock](../../compatibility/dependency-lock.md). |
| Managers / device | MO2 2.5.2 portable under Proton automated routes **PASS**; Vortex and other managers **NOT_RUN**; controller automated **NOT_RUN** by host policy. |

## Technical gates

| Ticket | Status |
| --- | --- |
| DYL-170 | Scoped automated **PASS**; owner approved screenshots. [UI](blocker-resolution-ui.md), [Camera](blocker-resolution-camera.md). |
| DYL-171 | Strict assembly/reproduction and MO2 automated 5/5 **PASS**, with open rows in [packaging](packaging.md). |
| DYL-172 | Frame cost **PASS** (combined p95 +0.58% standing, +0.72% turn) and warm inventory latency **PASS** (max 203 ms); memory **FAIL** with the optional minimap stack, also in the baseline without SkyCRPG modules and not flagged with the stack omitted. Accepted as an external-provider limitation by owner decision 2026-11-02 ([D024](../../DECISIONS.md)); not a pass. [Performance](performance.md). |
| DYL-173 | Automated packaged regression 22/22 **PASS**, bounded to [routes](regression.md); controller **NOT_RUN**. |
| DYL-174 | Final documentation merged in #32; package identities cross-checked; public rights clearance deferred. [Review](docs-review.md). |
| DYL-175 | **ACCEPT, owner decision 2026-11-02** ([D025](../../DECISIONS.md)). |

Known limitations: [release notes](../../RELEASE_NOTES.md). Owner route: [PLAYTEST.md](../../PLAYTEST.md); The owner confirmed on 2026-11-02 that they played the route with both keyboard/mouse and controller and accept the Phase 4 MVP. Their Linear comment on DYL-175 reads: “Recording that owner playtest approved, recording as GO to phase 5”. The per-area device split, per-area observations and any unreached steps were not recorded. The ACCEPT does not turn any FAIL, BLOCKED or NOT_RUN row in the linked evidence into a pass, and it is not a public release ([D023](../../DECISIONS.md)).

| Owner result | Observation / disposition |
| --- | --- |
| Camera controls and aiming | ACCEPT, covered by the overall ruling; no separate observation recorded. |
| Inventory readability and actions | ACCEPT, covered by the overall ruling; no separate observation recorded. |
| Minimap and navigation | ACCEPT, covered by the overall ruling; no separate observation recorded. |
| Module independence and recovery | ACCEPT, covered by the overall ruling; no separate observation recorded. |
| Overall ACCEPT / revision request | **ACCEPT**; keyboard/mouse and controller; no revision request. |
