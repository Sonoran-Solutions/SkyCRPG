# Decisions and remaining owner choices

Updated 2026-10-26 for DYL-154. “Selected engineering contract” records the evidence-backed implementation route submitted for senior review; it does not claim unimplemented behavior passes. User direction and explicit owner defers are separate. [ADR-001](decisions/ADR-001-stack.md), [architecture](ARCHITECTURE.md) and [acceptance matrix](compatibility/acceptance-matrix.md) give precise scope and obligations.

| ID | Authority / disposition | Decision | Evidence / follow-up owner |
| --- | --- | --- | --- |
| D001 | User direction | Elevated/isometric-style Skyrim with classic CRPG interface. | C1–C5/U1–U6 remain required. |
| D002 | User direction | Independently installable Camera and UI. | M1/M2; separate routes PASS, final packaging DYL-171. |
| D003 | User direction | Preserve real-time action combat. | No turn-based or new combat backend. DYL-158/161. |
| D004 | User direction | Functional grid and minimap replacing native compass. | Current duplicate compass is incomplete U5, DYL-166; no decorative-grid/minimap omission waiver. |
| D005 | User direction | Four phases ending in Camera/UI MVPs. | DYL-155/162/169/175 gates; no phase completion from source merges. |
| D006 | Selected engineering contract | Elevated perspective with native/closer fallback; first-person precision is the proven fallback. | Camera owner run073401; top-down ranged FAIL still DYL-158. |
| D007 | Explicit current task boundary | Uniform player-inventory presentation, native item identity/weight/ownership/capacity. | UI native round-trip PASS; DYL-163/164 complete integrity/layout. Tetris capacity is outside scope. |
| D008 | Selected engineering contract | One exact tested runtime/host/dependency set only. | Steam1.7.104/SKSE2.3.1/Address Library13 on Ubuntu26.04.1/Proton Experimental11/RTX3080; runtime matrix and lock. DYL-173 covers final acceptance. |
| D009 | Selected engineering contract | Existing DEM1.7.5 + Infinity UI NG2.1.3 + LMU1.3.4 minimap. | Rendered in-game; readiness/compass replacement and absence proof remain DYL-166. |
| D010 | Retained MVP requirement | Mouse/keyboard and controller,720p/1080p/scaled text, one vanilla humanoid follower. | Controller grid subset PASS only; DYL-165/168 and DYL-169 gate. No scope reduction. |
| D011 | Selected engineering contract | Native paused InventoryMenu; combat remains real-time outside menu. | Native backend retained. Unpaused-menu compatibility not selected; DYL-167 validates lifecycle. |
| D012 | Resolved engineering choice | SmoothCam1.8.1 sole normal third-person writer + TDM2.3.1 directional movement, future small policy/handoff adapter. | Existing preset works within limits; adapter unimplemented. Lock/yaw competition disabled. DYL-156/157/158/159; no large custom controller selected. |
| D013 | Resolved engineering choice | SkyUI Community6.11/native Scaleform grid with bounded input routing. | Live action/controller proof, both imported class copies patched. No new inventory backend/overlay required. DYL-163/164/167. |
| D014 | Bounded engineering follow-up | Native favorites/equipment for future eight-slot bar; Party Sheet3.7 optional follower candidate, no assumed API. | DYL-161 validates HUD dispatch route; DYL-165 validates follower capability or narrow original observer. Silhouette/unavailable fallback required, working follower still required. |
| D015 | **Open owner decision** | Original SkyCRPG source/art license. | DYL-174 before DYL-171 distribution; rights/attribution of derivatives assessed separately. Architecture can be reviewed without selecting a license. |
| D016 | Selected engineering contract | Windows x64 plugin ABI; only recorded Proton host has gameplay evidence. | Native Windows CI does not prove native Windows gameplay. DYL-173. |
| D017 | **Explicit owner defer** | Pitch resets after loading while zoom persists; nonblocking for spike PRs, fix later. | Combined075556 owner report; **FAIL** remains, assigned DYL-159. Does not waive C2 obstruction, absent-provider proof or any unrelated requirement. |
| D018 | Selected engineering contract | One input consumer per event; native menu/modal focus precedes gameplay. | Scoped controller correction PASS; Camera checks native focus independently. DYL-159/161/167/168. |
| D019 | Selected engineering contract | UI alone owns its native compass suppression, only after verified replacement readiness. | Current compass always visible; no public readiness API proven. DYL-166 must establish the capability and absence/partial-install recovery. |
| D020 | Selected engineering contract | External prerequisites remain external; modules own disjoint payload/config namespaces, source utilities only. | Native manifest audit PASS; generated SWFs/config need final packaging and license review. DYL-170/171/174. |
| D021 | Explicit engineering debt | 18-cell spike layout is not a completed scrollable inventory; correct row alignment at19+ items. | Senior UI review source finding, gameplay NOT_RUN. DYL-164 with19/20+/500-entry fixtures. |

## Gate decisions still outstanding

DYL-152/153 code is merged after bounded senior review, but their gameplay obligations remain open. DYL-155 must assess detailed obstruction proof and clean missing-provider startup, both NOT_RUN, and the proven first-person aiming fallback before passing Phase 1. Assigning later implementation tickets is not an owner-approved waiver. If satisfying the gate requires a product-scope concession rather than an implementation/fallback choice, request an owner ruling with the specific unmet criterion and evidence. No such concession is assumed here.

DYL-172 must measure the proposed performance budgets; changes require an explicit tradeoff before release. DYL-174 must obtain D015 and resolve derivative notices/asset permissions before publication. These are owned follow-ups, not reasons to invent a license or broaden support now.

## Decision history

- 2026-10-22: concept/planning initialized; no implementation acceptance at that time.
- 2026-10-25: DYL-151 desk research proposed external providers and native-menu route; exact source/file/license audit retained in dependency lock.
- 2026-10-26: bounded Camera/UI owner routes and combined checkpoint supplied playable evidence; two controller routing failures preceded final PASS. Top-down aim and load-pitch remained FAIL; only pitch received explicit owner defer.
- 2026-10-26: PR3/4 senior issue-comment review accepted bounded source/configuration work with gaps. User authorized merges; source merged. DYL-154 records the implementation contracts and traceability for review, without closing DYL-152/153 or starting DYL-155/Phase 2.

Record future changes with date, authority, evidence, impacted acceptance IDs and tickets. Runtime changes, incompatible licensing obligations, removing grid/minimap/independence, native-weight changes or a large replacement camera need an owner ruling; routine contract-preserving implementation choices do not need repeated permission.
