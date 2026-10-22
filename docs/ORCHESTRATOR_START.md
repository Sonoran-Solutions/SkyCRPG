# Orchestrator kickoff

Use this handoff for the first implementation session. This is an execution brief, not a claim that environment setup is already available.

## Mission

Execute Phase 1 of SkyCRPG from the [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f). Deliver two independent in-game proofs and a chosen implementation stack for the four-phase camera/UI MVP. Read AGENTS.md, STATUS.md, MVP_SCOPE.md, ROADMAP.md, REUSE_RESEARCH.md and PHASE_1.md before writing code.

## Start with the ready work

1. [DYL-150 / P1-01](https://linear.app/devils17/issue/DYL-150/p1-01-establish-reproducible-test-profiles-and-independent-module): inspect the actual game/build environment, create disposable profiles and independent module skeletons.
2. [DYL-151 / P1-02](https://linear.app/devils17/issue/DYL-151/p1-02-choose-reusable-mod-components-and-a-compatible-runtime): audit existing components, permissions and the runtime intersection.

These may run in parallel with disjoint ownership. After they converge, run the camera spike (P1-03) and UI spike (P1-04) in parallel. One integrator owns ADR-001 and contracts (P1-05). Complete the phase gate (P1-06) from actual evidence.

Use the user's active worker/model preferences. Do not delegate expensive reasoning merely to parallelize trivial work. Give each worker its own files, ticket, concrete output and verification budget.

## Required direction

Keep action combat and independent Camera/UI installs. Use existing movement/minimap/follower/HUD capabilities where they pass the contract. The grid baseline is player-inventory presentation with native item/weight rules. The minimap replaces the compass in the intended UI, but failures must restore usable navigation.

Prefer a tested preset/configuration/adapter over a broad fork. A small custom camera or inventory view is acceptable when the existing component cannot meet the scope. Do not silently replace a required feature with an unimplemented mockup.

## Job and evidence discipline

Follow the bounded-job rules in AGENTS.md and VALIDATION.md. No duplicate quiet jobs, no full suite for every small PR, no unbounded retry/soak loops. Keep process handles, logs and cleanup. Distinguish code validated in game from compile-only evidence.

If no Skyrim host is available, complete all useful setup/code/docs work, build a reviewable candidate where possible, and write the exact test handoff. Keep the runtime gate BLOCKED/NOT_RUN; do not ask for a general permission reset or mark the phase complete.

## Session handoff

Provide PR/commit, ticket states, files changed, selected stack and reasons, focused validation, game evidence or explicit NOT_RUN gaps, remaining blockers and next ready tasks. Keep PRs reviewable; implementation merge and public release need their own authorization. Do not begin Phase 2 until the Phase 1 criteria actually pass.
