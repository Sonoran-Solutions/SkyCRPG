# SkyCRPG

A modular attempt to make Skyrim feel like a classic CRPG while retaining real-time action combat.

**Status: planning initialized; no playable SkyCRPG release exists yet.** The first four phases target two independently installable MVPs:

| Module | Intended result |
| --- | --- |
| **SkyCRPG Camera** | Elevated, pulled-back perspective, configurable framing, reliable aiming and safe recovery in tight interiors and scripted states. |
| **SkyCRPG UI** | A classic CRPG HUD, player/follower status portraits, an eight-slot hotbar, player grid inventory and a minimap in place of the standard compass. |

Installing either module must not require the other. External dependencies are allowed when explicitly selected and tested. The planned inventory grid preserves Skyrim's item and carry-weight rules; this is a working product assumption, not a recorded owner decision about Tetris-style storage.

## Start here

- [Vision and scope](docs/VISION.md)
- [MVP acceptance criteria](docs/MVP_SCOPE.md)
- [Four-phase roadmap](docs/ROADMAP.md)
- [Linear project](https://linear.app/devils17/project/skycrpg-3bba43f5930f)
- [Current status and ready work](docs/STATUS.md)
- [Orchestrator handoff](docs/ORCHESTRATOR_START.md)

## Engineering and design

- [Architecture and ownership boundaries](docs/ARCHITECTURE.md)
- [Interface and camera design](docs/UX_SPEC.md)
- [Existing mods and reuse research](docs/REUSE_RESEARCH.md)
- [Decisions and open choices](docs/DECISIONS.md)
- [Risks](docs/RISKS.md)
- [Build/test setup](docs/BUILD_AND_TEST.md)
- [Validation strategy](docs/VALIDATION.md)
- [Runtime matrix](docs/compatibility/runtime-matrix.md)
- [Dependency selection record](docs/compatibility/dependency-lock.md)
- [Controls](docs/CONTROLS.md)
- [Installation planning](docs/INSTALLATION.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)
- [Third-party attribution policy](docs/THIRD_PARTY.md)

## Delivery goal

Phase 1 produces two in-game proofs and selects the stack. Phase 2 produces the camera MVP candidate and a usable HUD. Phase 3 completes the UI MVP candidate and a combined gameplay route. Phase 4 stabilizes, packages and validates both, ending with an owner playtest.

Turn-based combat, click-to-move, tactical party control, universal roof removal and a new minimap renderer are outside this MVP. Native camera/menu fallbacks are deliberate parts of the design.

Repository documents define the intended product and acceptance criteria. Linear tracks execution, blockers and evidence. A checked planning document is not evidence that the corresponding game feature works.
