# Phase 1 — Foundation and feasibility

P1-05 contract reference: [ARCHITECTURE](../ARCHITECTURE.md), [ADR-001](../decisions/ADR-001-stack.md), [acceptance traceability](../compatibility/acceptance-matrix.md). PR3/4 source is merged; remaining gameplay gaps are not closed by that merge. Phase gate checkboxes below remain acceptance criteria, not current results. The [DYL-155 gate report](../evidence/p1/gate.md) maps all 21 criteria and records owner-accepted GO on 2026-10-26, with the full-MVP limitations retained.

[Roadmap](../ROADMAP.md) · [MVP scope](../MVP_SCOPE.md) · [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)

## Outcome

Exit: two independently installable in-game prototypes; pinned executable and dependency stack; camera obstruction/aiming proof; one real inventory action under native weight rules; chosen minimap and UI reuse route. Runtime results are required, not just compiled mocks.

## Work items

### P1-01 — Establish reproducible test profiles and independent module skeletons

**Linear:** [DYL-150](https://linear.app/devils17/issue/DYL-150/p1-01-establish-reproducible-test-profiles-and-independent-module).

**Lane:** foundation. **Dependencies:** None — ready at kickoff.

Give every implementation task an actual build/install/playtest path and prevent a two-module concept from becoming one coupled plugin.

**Scope**

- Inventory the owner's available Skyrim install, store/executable version, SKSE, Address Library, OS or Proton version, GPU, mod manager, input device and build host; use a disposable test profile and copied saves.
- Create separate camera and UI package targets with isolated config/log paths. Add only the build/tooling needed by the provisional stack; do not commit game binaries, saves, third-party mod archives or credentials.
- Record baseline-only, camera-only, UI-only and combined profiles plus a short outdoor/interior route. Native Windows x64 DLL builds may use a Windows runner even when the playtest host is Linux/Proton.

**Acceptance criteria**

- [ ] Both empty or minimal module packages are reproducibly produced and loaded independently into the game on the recorded provisional runtime.
- [ ] A baseline play session and the two load logs identify executable, dependency versions and code/artifact hashes.
- [ ] The environment manifest distinguishes tested host/runtime combinations from candidates and records any absent local game access as NOT_RUN, never PASS.

**Focused validation:** Build once, perform the two minimal plugin-load checks and record the baseline route; no full mod-compatibility sweep.

**Evidence/deliverables:** `docs/BUILD_AND_TEST.md`, `docs/compatibility/runtime-matrix.md`, `docs/evidence/p1/environment.md`.

**Out of scope:** No gameplay implementation, blanket SE/AE support claim, automated game downloads, or irreversible edits to the normal mod profile.

### P1-02 — Choose reusable mod components and a compatible runtime intersection

**Linear:** [DYL-151](https://linear.app/devils17/issue/DYL-151/p1-02-choose-reusable-mod-components-and-a-compatible-runtime).

**Lane:** research. **Dependencies:** None — ready at kickoff.

Avoid rebuilding existing systems while ensuring the chosen dependencies can meet SkyCRPG's actual MVP.

**Scope**

- Audit primary sources and exact downloadable versions for TDM, SmoothCam or another single camera owner, Party Sheet, hotbar candidates, Grid Inventory, Inventory Reforged/SkyUI, and minimap candidates.
- Classify each as external dependency/preset, documented adapter, permitted source reuse, reference only, or rejected. Record license/permission, API availability, package footprint, runtime support, conflicts and maintenance cost.
- Choose one runtime intersection with P1-01, one minimap route and one UI rendering route. Prefer existing-provider integrations. Native-weight display grid remains the planning baseline; a Tetris capacity change requires an explicit product decision.
- The desk audit can start alongside P1-01. Final runtime selection waits for P1-01's actual environment facts; both tickets must converge before either in-game spike starts.

**Acceptance criteria**

- [ ] A decision matrix identifies exact versions, primary URLs and a rationale for every MVP component.
- [ ] Camera ownership has exactly one writer; conflicting camera packages are not silently stacked.
- [ ] A real minimap candidate and workable grid approach exist for the same target runtime; source-reuse obligations and unavailable APIs are explicit.
- [ ] No borrowed code or assets are shipped on the strength of a public repository alone.

**Focused validation:** Bound the desk audit and two promising alternatives per major unknown; use P1-03/P1-04 for in-game proof rather than claiming docs establish compatibility.

**Evidence/deliverables:** `docs/REUSE_RESEARCH.md`, `docs/compatibility/dependency-lock.md`, `docs/decisions/ADR-001-stack.md`.

**Out of scope:** No new minimap renderer, movement system, party-control framework, or broad speculative fork.

### P1-03 — Prove overhead camera, aiming and obstruction behavior in game

**Linear:** [DYL-152](https://linear.app/devils17/issue/DYL-152/p1-03-prove-overhead-camera-aiming-and-obstruction-behavior-in-game).

**Lane:** camera. **Dependencies:** P1-01, P1-02.

Find out early whether an elevated camera is playable in the places that break simple presets.

**Scope**

- Prototype a pulled-back perspective view with fixed or held pitch, recenter and an emergency return to normal third person.
- Exercise an open road, tight Nordic room/corridor and doorway, melee, bow, aimed spell and activation prompts.
- Compare an existing camera preset/API route with the smallest custom controller only if necessary; establish safe close-camera fallback when the desired overhead position is obstructed.

**Acceptance criteria**

- [ ] A camera-only build demonstrates controllable movement, readable melee, and an accurate usable aiming fallback.
- [ ] Walls and floors continue to block the camera; difficult interiors recover to a usable nearer view without getting stuck.
- [ ] Ten toggle/handoff cycles and one interior/exterior transition leave input and camera usable.
- [ ] The report names unimplemented states and recommends one ownership route; a desktop visualization is not the proof.

**Focused validation:** One bounded route with timestamped captures and logs; repeat only a failed case after a specific fix.

**Evidence/deliverables:** `docs/evidence/p1/camera-spike.md`, `docs/decisions/ADR-001-stack.md`.

**Out of scope:** No orthographic projection, universal roof cutaway, click-to-move or new combat rules.

### P1-04 — Prove the UI, native inventory action and minimap integration in game

**Linear:** [DYL-153](https://linear.app/devils17/issue/DYL-153/p1-04-prove-the-ui-native-inventory-action-and-minimap-integration-in).

**Lane:** ui. **Dependencies:** P1-01, P1-02.

Prove that the proposed interface can drive real game data and coexist with the ordinary camera before building a full skin.

**Scope**

- Render a minimal UI-only HUD and a small inventory grid populated from live inventory.
- Select a real weapon and equip/unequip it through an authoritative game/menu action; verify normal counts, weight and equipped state.
- Load the chosen minimap and test positioning, compass handoff, interior map behavior and missing-provider recovery. Probe the chosen follower/hotbar provider interfaces without assuming APIs exist.

**Acceptance criteria**

- [ ] The UI prototype works without SkyCRPG Camera and with standard third person.
- [ ] At least one inventory action changes the intended item exactly once; no fake preview is accepted as an implementation.
- [ ] A working minimap is visible in the target stack; losing or disabling it leaves navigation usable.
- [ ] The renderer/input/pause approach is chosen from evidence, including controller feasibility and a fallback for unavailable follower data.

**Focused validation:** One UI-only install, one real inventory action round trip, minimap present/absent checks, and a short combined-load smoke.

**Evidence/deliverables:** `docs/evidence/p1/ui-spike.md`, `docs/decisions/ADR-001-stack.md`.

**Out of scope:** No full menu replacement, container/barter overhaul, portrait-capture engine or inventory capacity redesign.

### P1-05 — Freeze module boundaries, control contracts and the tested support baseline

**Linear:** [DYL-154](https://linear.app/devils17/issue/DYL-154/p1-05-freeze-module-boundaries-control-contracts-and-the-tested).

**Lane:** architecture. **Dependencies:** P1-03, P1-04.

Let camera and UI implementation proceed in parallel without competing for input, camera state or shared files.

**Scope**

- Finalize the selected camera owner, UI backend, dependency versions and one primary runtime/host from the spikes.
- Document camera state precedence, input ownership, inventory transaction identity, provider capability detection, menu pause behavior, settings persistence and failure recovery.
- Define two archives, disjoint install/config paths and independent enable/disable behavior; shared source is allowed but neither package may require the other at runtime.

**Acceptance criteria**

- [ ] ADR-001 records the chosen route, rejected options, license implications and exact tested stack.
- [ ] Every transition has an owner and exit rule; no raw runtime actor/item pointer is treated as a durable save identifier.
- [ ] The acceptance IDs in MVP_SCOPE.md map to tests and all remaining decision items have a ticket or explicit defer disposition.

**Focused validation:** Review contracts against the two spike logs and inspect package manifests; do not rerun unchanged gameplay proofs.

**Evidence/deliverables:** `docs/ARCHITECTURE.md`, `docs/decisions/ADR-001-stack.md`, `docs/compatibility/runtime-matrix.md`, `docs/DECISIONS.md`.

**Out of scope:** No speculative shared core service or broad plugin API.

### P1-06 — Phase 1 gate — verify two playable prototypes and commit the implementation route

**Linear:** [DYL-155](https://linear.app/devils17/issue/DYL-155/p1-06-phase-1-gate-verify-two-playable-prototypes-and-commit-the).

**Lane:** gate. **Dependencies:** P1-05.

Exit research with executable evidence and a clear implementation path for both MVPs.

**Scope**

- Review all P1 deliverables and the runtime intersection, then close only completed requirements.
- If a critical route fails, choose one bounded fallback already evaluated and update the affected tickets; record a blocker if no route works.
- Publish the next ready camera and UI tasks with ownership and validation budgets.

**Acceptance criteria**

- [ ] Independent camera and UI prototypes run in the recorded game environment.
- [ ] Overhead interior/aiming recovery, a real native inventory action and a working minimap are evidenced.
- [ ] The chosen stack, module contracts and known limitations are documented; no unresolved critical feasibility item is disguised as completed.

**Focused validation:** Inspect existing evidence first; perform only missing critical checks, with a single combined smoke if the final stack changed.

**Evidence/deliverables:** `docs/evidence/p1/gate.md`, `docs/STATUS.md`, `docs/DECISIONS.md`.

**Out of scope:** No phase completion from compilation alone and no open-ended re-research.
