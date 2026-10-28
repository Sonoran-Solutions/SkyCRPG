# Internal candidate notes

Phase 4 is in progress. These are internal candidates, not a public release or owner acceptance. Final candidate source `<candidate SHA>`, CI `<candidate CI run>`, Camera `<candidate Camera archive hash>`, UI `<candidate UI archive hash>`. [Install](INSTALLATION.md) · [controls](CONTROLS.md) · [playtest](PLAYTEST.md).

## Known limitations

- Camera hands special states, including first person, mounts, swimming, furniture, transformations, killcams and scripted states, back to native behavior; full transition coverage is **NOT_RUN**.
- Overhead ranged aim **FAIL** and target lock **BLOCKED** are permanent MVP limitations by owner decision. Use F6 native first person for precision; target lock is planned for [Phase 5](phases/PHASE_5.md).
- Obstruction framing defaults OFF. Ceiling beams and low furniture remain usability limits; opting in and the full route need validation.
- The grid covers player inventory only. Container, barter, crafting and other non-player menus remain native/SkyUI. The favorite toggle is native.
- One follower is tracked; multiple followers are unsupported. True non-essential death remains **NOT_RUN**.
- The minimap provider describes actor, location and quest markers and a quest pointer; actual marker coverage and settings are **NOT_RUN**. Survival temperature disappears with the hidden compass. The provider has no fresh-render signal; the owner accepted stale-frame risk, not lost navigation. See [DYL-166](evidence/p3/minimap.md) and [UI audit](evidence/p4/blocker-resolution-ui.md).
- Dialogue audio was silent on the recorded Proton Experimental host even without mods; this is a documented host limitation. Suggested workarounds are **NOT_RUN**.
- Patched movies target SkyUI Community 6.11 only; build inputs are hash-checked, but the DLL cannot check the loaded movie at runtime. Other versions are unsupported.
- Only the recorded Steam 1.7.104 / Proton host has gameplay evidence, and that evidence is bounded to earlier candidates. Other runtimes, Windows hosts, mod stacks, follower frameworks, combat overhauls and display combinations are untested. See [runtime matrix](compatibility/runtime-matrix.md).
