# Phase 4 — Stabilization and MVP acceptance

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

## Outcome

Exit: separately packaged camera/UI MVP candidates pass the pinned camera-only, UI-only and combined matrix, item integrity, bounded performance checks and owner playtest. Known limitations and install/rollback instructions are published; game builds are not claimed tested without evidence.

## Work items

### P4-01 — Resolve integration blockers and verify failure/disable behavior

**Linear:** [DYL-170](https://linear.app/devils17/issue/DYL-170/p4-01-resolve-integration-blockers-and-verify-failuredisable-behavior).

**Lane:** integration. **Dependencies:** P3-07.

Make the pair robust when a module or optional provider is absent or turned off.

**Scope**

- Fix the finite blocker list from P3 and dependency conflicts on the supported stack.
- Exercise camera-only, UI-only and combined startup, missing optional providers, incompatible version guards and independent disable/reset.
- Keep feature changes limited to issues required for existing MVP acceptance; create future backlog entries for enhancements.

**Acceptance criteria**

- [ ] Each module works independently and has a clear failure message or safe fallback when dependencies are wrong.
- [ ] Camera disable does not disable UI; UI disable does not strand the camera or leave the compass hidden.
- [ ] No unresolved critical crash, data integrity, input-capture or blocked-gameplay defect remains.

**Focused validation:** Targeted reproductions for each fix plus the three startup/disable profiles; do not rerun the entire matrix per fix.

**Evidence/deliverables:** `docs/evidence/p4/blocker-resolution.md`, `docs/compatibility/runtime-matrix.md`.

**Out of scope:** No new runtimes, combat overhauls or follower framework integrations unless needed for the locked baseline.

### P4-02 — Package reproducible independent camera and UI release candidates

**Linear:** [DYL-171](https://linear.app/devils17/issue/DYL-171/p4-02-package-reproducible-independent-camera-and-ui-release).

**Lane:** release. **Dependencies:** P4-01.

Deliver two installable archives, not just a development folder that happens to work.

**Scope**

- Produce Camera and UI versioned archives with manifests, exact dependency requirements, config defaults, checksums and source commit.
- Make shared source/build assets safe for independent installation and detect conflicting file ownership.
- Validate fresh installs and updates in the selected mod manager; document removal/restart requirements and keep third-party downloads external unless redistribution is permitted and intentional.

**Acceptance criteria**

- [ ] Fresh camera-only, UI-only and combined installs work using the written instructions.
- [ ] Package contents have no accidental source/build/debug junk, game binaries, personal paths or duplicate conflicting config ownership.
- [ ] The same source and lock data reproduce the candidate build process; byte-identical output is claimed only if demonstrated.

**Focused validation:** Archive inspection plus fresh-profile installs for all three combinations; use package hashes in subsequent gates.

**Evidence/deliverables:** `docs/evidence/p4/packaging.md`, `docs/INSTALLATION.md`, `docs/compatibility/dependency-lock.md`.

**Out of scope:** No public Nexus/GitHub release publication without a separate release instruction.

### P4-03 — Measure frame cost, inventory latency and resource stability

**Linear:** [DYL-172](https://linear.app/devils17/issue/DYL-172/p4-03-measure-frame-cost-inventory-latency-and-resource-stability).

**Lane:** performance. **Dependencies:** P4-01.

Keep the CRPG interface and camera from making a playable Skyrim setup sluggish.

**Scope**

- Compare baseline, camera-only, UI-only and combined on the same deterministic route/settings using warmed short captures.
- Measure frame-time median/p95, inventory-open and grid-scroll responsiveness, and memory trend across repeated menu and cell transitions.
- Use initial planning budgets of <=10% p95 frame-time overhead for the pair, <=250ms warmed inventory-open at 500 entries and no unbounded retained-state growth; calibrate explicitly from P1 baseline, never silently relax after failure.

**Acceptance criteria**

- [ ] Reproducible measurements identify hardware, resolution, load order, sample duration and artifact hashes.
- [ ] Regressions above the chosen budgets are fixed or recorded as an explicit release blocker/product tradeoff.
- [ ] Diagnostics and polling remain bounded; the game loop performs no network calls or repeated full inventory/actor scans per frame.

**Focused validation:** One baseline and one candidate capture per profile, plus a bounded 10-minute resource check; repeat only a noisy/failing comparison once.

**Evidence/deliverables:** `docs/evidence/p4/performance.md`, `docs/VALIDATION.md`.

**Out of scope:** No multi-hour soak loop, endless benchmark tuning or broad hardware survey.

### P4-04 — Run the final supported-profile regression and inventory integrity matrix

**Linear:** [DYL-173](https://linear.app/devils17/issue/DYL-173/p4-04-run-the-final-supported-profile-regression-and-inventory).

**Lane:** qa. **Dependencies:** P4-02, P4-03.

Verify the packaged candidates across the exact configurations the release will claim.

**Scope**

- Execute the release matrix for the pinned runtime: camera-only, UI-only and combined, plus the documented disable/removal recovery procedure.
- Recheck core camera transitions, item identity/counts/weight, hotbar, follower lifecycle, minimap/compass and menu input.
- Record pass/fail/not-run per acceptance ID and distinguish code, package and evidence-only commits.

**Acceptance criteria**

- [ ] All required MVP acceptance IDs pass on the exact packaged candidate or are explicitly blocking.
- [ ] Before/after item fixtures show no unexplained loss, duplication, identity change or weight-rule change.
- [ ] The test report supports every published compatibility claim and names all untested runtimes/hosts/providers.

**Focused validation:** One bounded release matrix; failures trigger targeted fixes and only affected rechecks plus a short integrated smoke.

**Evidence/deliverables:** `docs/evidence/p4/regression.md`, `docs/compatibility/runtime-matrix.md`, `docs/STATUS.md`.

**Out of scope:** No exhaustive mod-list certification or endless reruns of unchanged full suites.

### P4-05 — Finalize installation, controls, limitations and third-party attribution

**Linear:** [DYL-174](https://linear.app/devils17/issue/DYL-174/p4-05-finalize-installation-controls-limitations-and-third-party).

**Lane:** docs. **Dependencies:** P4-02.

Make the MVP candidates understandable and reproducible for someone other than their implementer.

**Scope**

- Write actual install/update/disable/removal steps, runtime/dependency requirements, control bindings, troubleshooting and known limitations from the validated packages.
- Document which screenshots are concept art versus real evidence and the precise player-inventory/follower/minimap scope.
- Complete source/asset attribution and selected licensing requirements; add release notes and a short owner playtest script.

**Acceptance criteria**

- [ ] Instructions install each module alone and together and clearly identify required external downloads.
- [ ] Known limits include native fallback for special camera states, native container/barter menus and unsupported stacks.
- [ ] No proposed feature or concept image is presented as a shipped implementation; attribution matches actual reused files.

**Focused validation:** Documentation/package cross-check and link review; no game rerun for text-only corrections unless instructions changed actual behavior.

**Evidence/deliverables:** `README.md`, `docs/INSTALLATION.md`, `docs/CONTROLS.md`, `docs/RELEASE_CHECKLIST.md`, `docs/THIRD_PARTY.md`.

**Out of scope:** No unverified compatibility marketing, made-up playtest results or automatic release publishing.

### P4-06 — Phase 4 gate — owner playtest and camera/UI MVP acceptance

**Linear:** [DYL-175](https://linear.app/devils17/issue/DYL-175/p4-06-phase-4-gate-owner-playtest-and-cameraui-mvp-acceptance).

**Lane:** gate. **Dependencies:** P4-04, P4-05.

End the planned four phases with two working mod MVPs and an honest completion decision.

**Scope**

- Give the owner exact package hashes, support matrix and the short playtest route.
- Record the owner's usability assessment of camera controls/aiming, inventory readability, minimap and module independence; resolve acceptance-blocking defects.
- Publish a final disposition, candidate links and prioritized post-MVP backlog. Preparing the candidates is required before requesting this playtest.

**Acceptance criteria**

- [ ] Camera-only, UI-only and combined packaged builds pass the required technical criteria.
- [ ] The owner records ACCEPT or a specific revision request after actual play; absent playtest remains OWNER_PLAYTEST_PENDING.
- [ ] The roadmap ends at MVP acceptance with no hidden dependency on a fifth implementation phase.

**Focused validation:** Owner executes the bounded route once; repeat only a reported defect after a fix. Technical verification is not replaced by a taste vote.

**Evidence/deliverables:** `docs/evidence/p4/acceptance.md`, `docs/STATUS.md`, `docs/RELEASE_CHECKLIST.md`.

**Out of scope:** No claim that documentation alone delivered the mod, no automatic public release and no expansion of MVP scope.
