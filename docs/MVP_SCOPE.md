# MVP scope and acceptance

**Planning baseline: 2026-10-22. Nothing in this file is a claim of implemented functionality.**

MVP is complete when two packaged candidates meet the following criteria on the single Phase 1 support baseline, separately and together, and the owner has completed the playtest. Required fallbacks count as correct behavior; unresolved data loss, control lock or unusable navigation does not.

## Camera acceptance

| ID | Requirement | Evidence |
| --- | --- | --- |
| C1 | Elevated perspective follow camera with bounded zoom/distance, held pitch, yaw/recenter controls and configurable defaults. | Camera-only outdoor route and config recovery. |
| C2 | Obstruction preserves wall/floor collision and brings the camera closer or returns to a safe view in tight interiors. | Corridor, doorway, stairs and room fixtures. |
| C3 | Real-time melee, bow and targeted magic are usable; normal aiming/interaction view is available when required. | Hits and activations in the agreed encounters. |
| C4 | Dialogue, menus, loading, fast travel, death/reload and scripted scenes hand camera ownership back correctly. | Lifecycle table with pass/fail per transition. |
| C5 | Emergency disable/reset works; first person, mounts, swimming, transformations, furniture and killcams safely hand off unless deliberately supported. | Handoff/return and copied-save restart checks. |

## UI acceptance

| ID | Requirement | Evidence |
| --- | --- | --- |
| U1 | Readable player resources and one recruited vanilla humanoid follower's portrait/status or neutral silhouette; additional followers depend on the tested provider. | Live HUD, recruit/dismiss, unload/death checks. |
| U2 | Up to eight favorite/action slots show actual state and operate through native equip/use behavior; combat remains continuous. | Weapon, spell, consumable and depleted-slot checks. |
| U3 | Player inventory uses a scrollable uniform grid, categories/search/sort, counts, item details, equipment slots and carry-weight/gold display. | Real item workflows, not a static mockup. |
| U4 | Inventory actions preserve count, ownership/instance state, equipped/favorite/quest rules and native weight/encumbrance. | Plain/enchanted/tempered/stolen/quest/stack fixtures before and after. |
| U5 | A functioning existing minimap occupies the intended corner with no duplicate horizontal compass. Disable/provider failure restores usable native navigation. | Exterior/interior and provider-absence cases. |
| U6 | Input/focus/pause behavior, native menu fallbacks, save/load and both input modes work at 1280x720 and 1920x1080, including one scaled-text case. | Menu/controller/layout matrix. |

The grid MVP covers the **player inventory**. Containers, barter, pickpocketing, follower trade, crafting, magic, journal and world/local-map menus remain available through native/SkyUI screens. Equipping an item can use an equipment-slot silhouette; a 3D paper doll is optional if a proven component makes it inexpensive.

Normal paused inventory behavior is acceptable. Real-time action combat does not mean an unpaused inventory requirement. Skyrim Souls-style unpaused menus need a separate compatibility task.

Enemy, follower and quest markers follow the selected minimap's real capabilities and configured detection rules. The concept art is not permission to invent a wallhack or require an unimplemented provider API.

## Module and release acceptance

| ID | Requirement |
| --- | --- |
| M1 | Camera-only installation works with ordinary UI; UI-only works with ordinary camera; the pair works together. |
| M2 | Separate archives, disjoint settings/package ownership, explicit dependencies, recoverable disable/reset, and documented removal/restart behavior. |
| M3 | One exact runtime/host/dependency stack is documented as tested; all others remain untested unless independently verified. |
| Q1 | No known critical crash, item loss/duplication, persistent input lock or blocked progression in the required fixtures. |
| Q2 | Performance meets the recorded baseline-relative budgets in VALIDATION.md, or a specific revised tradeoff is accepted and recorded before release. |
| Q3 | Source/package hashes, reproduction steps, installation instructions, attribution, limitations and owner acceptance are attached to the final candidate. |

## Explicitly deferred

- Turn-based combat, tactical pause, party command AI or character switching.
- Click-to-move, pathfinding and detached RTS camera navigation.
- True orthographic projection and universal roof/wall cutaways.
- Tetris item footprints, rotation, nested bags and replacement capacity rules.
- New minimap renderer or map/world generation.
- Full redesign of all game menus, a complete combat log and live portrait capture.
- Universal follower frameworks, runtime versions, graphics stacks or combat-overhaul compatibility.
- Public distribution to Nexus or release publication as part of this planning request.

## Change control

The owner's stated direction is authoritative. Concrete technical defaults in this document are proposed scope choices. If a spike proves one infeasible, document the evidence and select a bounded alternative; do not quietly remove the grid, minimap, module independence or playable camera to make a gate green. Escalate only a material product tradeoff after presenting the tested alternatives.
