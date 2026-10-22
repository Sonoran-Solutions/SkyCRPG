# Architecture proposal

## Module ownership

This is a proposed design until ADR-001 is completed by Phase 1.

| Component | Owns | Must not own |
| --- | --- | --- |
| Camera module | Camera mode, framing, obstruction recovery, camera input and lifecycle handoff. | Inventory, follower AI, global HUD replacement or UI-only configuration. |
| UI module | HUD presentation, player-inventory presentation/actions, menu focus, provider adapters, minimap/compass presentation. | Player movement physics, camera transform, follower commands or independent item ownership. |
| External providers | Their existing movement, mapping, HUD or menu functionality. | An assumed API that has not been inspected and exercised. |
| Shared source | Small immutable types/utilities, versioning or build helpers. | A mandatory runtime service coupling Camera and UI. |

Use separate DLLs only where new native code is actually necessary. A Camera package built from a tested preset plus a small adapter is valid if it meets the camera criteria. A UI package may combine original assets, configuration and adapters to independently installed providers. A source fork needs an explicit maintenance and permission decision.

## Candidate implementation paths

Camera: first test a supported preset/API or cooperative handoff to an existing camera system. Use a small SKSE/CommonLib controller when that route cannot provide held pitch, recovery and lifecycle behavior. Do not enable two transform writers.

UI: prefer existing HUD and Scaleform/Infinity UI surfaces when they satisfy the design; consider an SKSE/native overlay for a bounded player-inventory surface if the spike proves native actions, focus, pause, controller operation and graphics compatibility. The renderer is a Phase 1 decision, not predetermined by the screenshots.

The build targets the game's Windows x64 plugin ABI. A Linux development host does not imply a native Linux Skyrim plugin. Choose a verified build host/runner and record Proton separately if used for game tests. SKSE/CommonLib versions and runtime address support must be pinned; an upstream library's broad support statement is not a SkyCRPG compatibility result.

## Camera state precedence

Highest priority first:

1. Disabled, unsupported runtime, missing prerequisite or explicit emergency reset: release ownership and expose the safe underlying camera.
2. Loading, invalid player, death transition or scripted/special camera state: suspend; never retain stale scene pointers.
3. Menu/dialogue/first-person/special locomotion state: hand off according to the selected backend.
4. Precision aiming or interaction: use the tested closer/normal camera when needed.
5. Obstructed exploration: shorten or lower the view smoothly with hysteresis.
6. Normal exploration/combat: use the selected elevated preset.

Capture only the settings/state that SkyCRPG changes. Restoration must account for intervening user/provider changes, not blindly overwrite the whole camera configuration. Returning from a handoff must revalidate player, cell and camera state.

## UI data and mutation contract

The engine is the source of truth. Render immutable snapshots tagged with a generation. A selected row references a resolvable item/instance identity, never merely a displayed name, row index or base FormID when extra state distinguishes copies.

Before an equip/use/drop request, resolve the current item again on the appropriate game thread, validate quantity and quest/equipment restrictions, perform one supported action, then refresh the snapshot. Menu closure, load/new game, removed items and changed inventory generations invalidate stale commands. Do not hold an engine lock while waiting for render work or another thread.

Favorites and hotbar reference native state. Spell selection does not imply casting, and the UI must not invent cooldowns or command the actor through duplicate inputs.

Follower widgets use resolvable handles, bounded observation and provider capabilities. Unknown/unloaded state is displayed safely. A static silhouette is an acceptable portrait fallback.

## Menu and input ownership

Exactly one layer consumes an action: gameplay, inventory, modal quantity/confirmation dialog, or another native menu. Captured mouse/controller input must not leak to the camera or consume a potion behind a dialog. On close, load and reset, release held/captured state.

Pause behavior follows the chosen inventory/menu route. Opening a native fallback menu suspends the custom surface as necessary. Disabling the UI returns all owned HUD/compass changes and leaves native menu access intact.

## Minimap contract

Use the selected provider's actual local map and controls. Read capability/version information where offered; otherwise validate the installed files/configuration and use conservative fallback. The integration owns placement/style/configuration, not the game-world camera or map rendering.

Hide the standard compass only when replacement navigation is enabled and functioning under the selected provider contract. A missing provider or deliberate minimap disable must restore native navigation. The provider's optional compass ring is not proof that it hides Skyrim's original horizontal compass.

## Persistence and packaging

Prefer versioned configuration files for presentation preferences. Do not persist raw pointers or item replicas. If co-save data becomes unavoidable, define versioning, invalidation and removal consequences before shipping.

Suggested implementation tree after Phase 1: modules/camera, modules/ui, shared, assets, presets, packaging, tests and tools. These directories are not created as pretend implementation now. Separate logs, filenames and config ownership matter more than the exact directory names.

Two install archives can share build-time code but cannot overwrite each other's settings or require the other module. External dependencies stay external unless permission and intentional packaging justify inclusion.

## Failure policy

Unsupported executable: reject/suspend the affected feature before unsafe address access and give a useful log. Missing optional portraits/minimap: degrade safely. Invalid config: defaults. Invalid item command: refresh and refuse. Another camera owner: deterministic configured coexistence or documented incompatibility. No silent repeated retries, unconditional full scans or game-loop network work.
