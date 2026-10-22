# Working on SkyCRPG

Read README.md, docs/STATUS.md, docs/MVP_SCOPE.md, the assigned Linear ticket and its phase document before implementation. Respect later owner instructions over planning assumptions. The repository currently contains planning only.

## Product boundaries

- Preserve two independently installable modules. Camera-only, UI-only and combined are required configurations.
- Preserve real-time Skyrim combat and native item ownership, counts and carry weight under the current planning baseline.
- Use one camera owner. Integrate existing movement, minimap and follower/HUD providers where they meet the contract.
- Do not silently replace the MVP with screenshots, an HTML mockup, a mod-list recommendation, or four phases of research.
- Record scope or dependency changes in docs/DECISIONS.md and the affected tickets; distinguish owner decisions from engineering recommendations.

## Execute efficiently

Choose the smallest next ready ticket. Give parallel workers disjoint files and contracts. A camera worker and a UI worker may work concurrently after Phase 1; one integrator owns common contracts and package changes.

Before any build, test, benchmark or game job, record its purpose, command/profile, expected output, timeout, owner and completion signal. Reuse valid evidence when only documentation changes. Run the relevant target and focused checks first; run the complete supported-profile matrix at phase/release gates or for a concrete shared-state risk.

Every long job must have a handle/PID or CI URL, a log, a deadline and cleanup. Allow at most one Skyrim process per test profile. Never start a duplicate because a command is quiet. Inspect the existing job first. Cancel only the processes created for this task, including their children, and verify cleanup. A timeout is a result to diagnose, not permission for an identical retry loop.

Initial limits: 15 minutes for a warm build, 30 minutes for the first dependency build, 10 minutes for focused automated tests, 20 minutes for a scripted/manual route, and 10 minutes for a performance/resource sample. Adjust a budget explicitly when measured evidence requires it. Limit an unchanged failing attempt to one diagnostic retry; further attempts need a new hypothesis or changed input.

## Evidence and completion

Use PASS, FAIL, BLOCKED or NOT_RUN. Compilation and mock tests do not prove in-game behavior. Keep executable/runtime, dependency versions, host, source commit and package hashes with game evidence. If Skyrim or a required host is unavailable, finish the code/docs work possible and leave the runtime gate open.

Use copied saves and isolated mod profiles. Do not commit personal saves, game binaries, dependency archives, credentials or personal configuration. Keep source/license notices with reused material. Follow the actual permissions of code and art separately.

Implementation work should use focused branches and reviewable PRs. Do not merge, publish a public mod release, change owner decisions or claim owner playtest acceptance without the corresponding authorization/evidence. Creating internal candidate artifacts and collecting evidence should proceed without unnecessary permission requests.
