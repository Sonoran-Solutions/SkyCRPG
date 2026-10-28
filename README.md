# SkyCRPG

A modular attempt to make Skyrim feel like a classic CRPG while retaining real-time action combat.

**Status: Phase 4 is in progress. Internal candidates only; no public MVP release or owner acceptance exists.** The first four phases target two independently installable MVPs:

| Module | Intended result |
| --- | --- |
| **SkyCRPG Camera** | Elevated, pulled-back perspective, configurable framing, reliable aiming and safe recovery in tight interiors and scripted states. |
| **SkyCRPG UI** | A classic CRPG HUD, player/follower status portraits, an eight-slot hotbar, player grid inventory and a minimap in place of the standard compass. |

Installing either module must not require the other. External dependencies are allowed when explicitly selected and tested. The player-inventory grid preserves Skyrim's native item, ownership and carry-weight rules; a Tetris-style capacity system is outside the current scope.

## Start here

- [Vision and scope](docs/VISION.md)
- [MVP acceptance criteria](docs/MVP_SCOPE.md)
- [Acceptance evidence and ticket mapping](docs/compatibility/acceptance-matrix.md)
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
- [Installation and recovery](docs/INSTALLATION.md)
- [Known limitations](docs/RELEASE_NOTES.md)
- [Owner playtest route](docs/PLAYTEST.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)
- [Third-party attribution policy](docs/THIRD_PARTY.md)

## Delivery goal

Phase 1 produces two in-game proofs and selects the stack. Phase 2 produces the camera MVP candidate and a usable HUD. Phase 3 completes the UI MVP candidate and a combined gameplay route. Phase 4 stabilizes, packages and validates both, ending with an owner playtest.

Turn-based combat, click-to-move, tactical party control, universal roof removal and a new minimap renderer are outside this MVP. Native camera/menu fallbacks are deliberate parts of the design.

Repository documents define the intended product and acceptance criteria. Linear tracks execution, blockers and evidence. A checked planning document is not evidence that the corresponding game feature works.

## License

Original code/configuration/documentation: [MIT](LICENSE). Original artwork: [CC BY4.0](LICENSES/CC-BY-4.0.txt). Rights holder: **devils_17**. See [licensing scope and third-party exclusions](docs/LICENSING.md).
