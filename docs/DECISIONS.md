# Decisions and open choices

Status labels distinguish the user's direction from implementation proposals. A roadmap assumption is not retroactive owner approval.

| ID | Status | Decision or assumption | Basis / next action |
| --- | --- | --- | --- |
| D001 | User direction | A top-down/isometric-style Skyrim experience with a classic CRPG interface. | Current project request and concept discussion. |
| D002 | User direction | Camera and UI should be usable independently as parts of a set. | User explicitly described separate mods. |
| D003 | User direction | Keep action combat; turn-based conversion is outside the current goal. | User's stated concept. |
| D004 | User direction | Include a grid inventory and minimap instead of Skyrim's standard compass. | Original concept and minimap screenshot revision. |
| D005 | User direction | Create approximately four implementation phases ending in camera and UI MVPs. | Planning request. |
| D006 | Working baseline | Use elevated perspective, with native/closer fallback for aiming and difficult states. | MVP feasibility and bounded scope; P1-03 proves behavior. |
| D007 | Working baseline | Uniform display grid with native weight and item rules; player inventory first. | P1-02/P1-04 must confirm route. Tetris capacity is a separate product choice. |
| D008 | Working baseline | One tested runtime/dependency stack; separate package support from broad SE/AE claims. | P1-01/P1-02 select from actual available installation. |
| D009 | Working baseline | Integrate a minimap; Dragon's Eye fork is the first candidate. | REUSE_RESEARCH.md; P1-04 supplies runtime proof. |
| D010 | Working baseline | Keyboard/mouse and controller paths, 720p/1080p; at least one vanilla follower. | Scope limits to produce a usable UI MVP. |
| D011 | Working baseline | Normal paused inventory is acceptable; combat stays real-time. | Explicitly distinguishes action combat from unpaused-menu compatibility. |
| D012 | Open engineering choice | Camera preset/adapter versus a small custom controller. | ADR-001, P1-03. |
| D013 | Open engineering choice | Scaleform/Infinity UI versus another native overlay for the inventory surface. | ADR-001, P1-04. |
| D014 | Open engineering choice | Exact follower/hotbar providers and reusable equipment display. | P1-02/P1-04; do not assume API availability. |
| D015 | Open release choice | Source/art license for original SkyCRPG work. | Complete before source reuse/distribution; no license grant is invented by this planning commit. |
| D016 | Working baseline | Windows x64 plugin ABI; owner may test through Proton if selected and evidenced. | Record host separately; no untested Linux/Windows parity claim. |

## Recording changes

Append date, affected decision, evidence, selected option and authority. Record whether the change is an engineering resolution within scope or an explicit owner product decision. Update MVP_SCOPE, relevant phase/tickets and compatibility documents together.

Changes to native weight, omission of grid/minimap, loss of module independence or replacing action combat are material product changes. Bring a concrete tested alternative to the owner. Ordinary implementation details that preserve the contract do not require repeated permission.

## Initial history

2026-10-22: planning initialized from the user's concept and minimap revision. No implementation test, component selection or owner playtest result has been recorded.
