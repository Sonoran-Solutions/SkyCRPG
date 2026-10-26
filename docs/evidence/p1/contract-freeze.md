# DYL-154 contract-freeze evidence

2026-10-26. Documentation-only reconciliation of Phase 1 evidence; no new game run, native build, provider upgrade or Phase 2 code. Base main: `87d91626815dca145c6a876664f1ac7d0ffeafdf`. PR head is recorded in the PR/Linear handoff to avoid a self-referential source hash.

## Source review and state reconciliation

| Source | Verified state / relevant finding |
| --- | --- |
| [PR3](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3) | Merged2026-10-26; reviewed head`1f272ca80e4f207a5a13a97506b44a5fc8ddb1fe`, merge`a3edd163bb31491427b81eb860f7d440a4379ce8`. [Senior review comment](https://github.com/Sonoran-Solutions/SkyCRPG/pull/3#issuecomment-6419920725): bounded code/config PASS; obstruction NOT_RUN, ranged aim FAIL, pitch FAIL/owner-deferred. |
| [PR4](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4) | Merged2026-10-26; reviewed head`2f9d335f4be7f70c5b4116cd5d38f9aa0fbb7340`, merge`87d91626815dca145c6a876664f1ac7d0ffeafdf`. [Senior review comment](https://github.com/Sonoran-Solutions/SkyCRPG/pull/4#issuecomment-6419920095): bounded native-menu/code PASS; absent-provider startup NOT_RUN,19+ layout concern, compass replacement/follower/hotbar incomplete. |
| GitHub review semantics | Senior reviews are issue comments, not formal submitted review objects. Historical exact-head Windows CI PASS covers skeleton compilation/package only, not Scaleform/preset/gameplay. |
| Linear DYL-152/153 | Actual status **In Progress**, despite older In Review comments. Source merges reconciled in new comments; no automatic Done transition. Missing evidence explicitly retained. |
| Linear DYL-154/155 | DYL-154 contract work submitted for review; DYL-155 remains Backlog with its dependency on DYL-154 intact. No gate execution or Phase 2 start. |
| Linear DYL-164 | Remains Backlog; senior-review19+ row-alignment concern and19/20+/500-entry fixture obligation added as follow-up comment. |

The [session](session-2026-10-26.md) and separate [camera](camera-spike.md)/[UI](ui-spike.md) reports supply owner observations and indexed logs/captures. Later corrections supersede historical pending statuses without erasing failed attempts. The [runtime matrix](../../compatibility/runtime-matrix.md) preserves exact source/package pins and the [acceptance matrix](../../compatibility/acceptance-matrix.md) preserves every remaining FAIL/NOT_RUN/BLOCKED obligation.

## Independent audit

A read-only Luna6 worker inspected package/source ownership, profile manifests, both senior reviews, licenses and CI path filters, then audited the contract drafts. No files were edited by the worker and no builds/game jobs were launched. It found one ambiguous UI prerequisite sentence; corrected to state common SKSE/Address Library/SkyUI requirements explicitly for both profiles.

Native package inputs have two disjoint files each and no cross-module DLL imports/requirements. Whole-profile overlap is130 paths,129 equal hashes and one profile-specific save-path INI; it does not imply that modules own shared external files. The contracts distinguish provider-owned material, module-owned deltas and integrator-owned profile state. File names/paths match the committed resolved manifest, including the two minimap SWFs.

## Focused validation

Validation job: owner orchestrator; purpose document consistency, local links/anchors, ticket/acceptance coverage, exact hashes and package overlap. Command `timeout --signal=TERM --kill-after=5s 600s python3 /srv/storage/Projects/SkyCRPG/session-dyl154-20261026/check_contracts.py`; expected output a JSON report and exit0; deadline600s; local PID/job record and log in that session directory. Cleanup: terminate only this validator process on timeout; it launches no game/build/child jobs. The read-only validator is a task-local audit, not new repository tooling or CI enforcement.

Result: **PASS**. All17 acceptance IDs,13 camera state rows,26 roadmap ticket references, local links/anchors,34 baseline hash/source pins, all five canonical profile digests and disjoint native payloads passed. `git diff --check` passed. The [machine-readable check summary](contract-validation.json) records the final file/link counts and audit output. Native Windows build, profile fixture suite, external dependency recompilation, full compatibility matrix and gameplay: **NOT_RUN for DYL-154**, intentionally reused unchanged evidence. No executable repository tooling is modified. Existing workflow path filters exclude documentation-only changes, so no Windows build should be launched by this PR.

## DYL-154 acceptance and gate boundary

- ADR records the selected route, rejected alternatives, rights obligations and exact tested stack.
- Camera states and UI/inventory/HUD/provider lifecycles specify owners, entry/exit, input, restoration and fallback; no raw runtime pointer is durable identity. Planned adapters and public APIs are not invented as implemented.
- All C1–C5, U1–U6, M1–M3 and Q1–Q3 map to evidence, implementation and validation tickets. D015 license remains an owner decision; pitch defer remains narrow; other missing evidence remains visible.

These documentary deliverables satisfy the task's acceptance content, pending senior review. DYL-155 can begin after that review/acceptance; it cannot automatically pass while missing critical Phase 1 proof is merely assigned to later tickets. This PR remains unmerged with auto-merge disabled.


## Subsequent gate evidence — 2026-10-26

The [DYL-155 gate](gate.md) supersedes the historical missing critical obstruction and whole-provider-absence checks with two bounded owner-observed PASS routes. Earlier results above remain historical; full MVP coverage and recorded aiming/pitch failures are not promoted. PR5 architecture is now reviewed and merged; the gate GO recommendation awaits its own senior review and acceptance.
