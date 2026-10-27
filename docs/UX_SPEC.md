# Interface and camera design

## Visual direction

Use dark charcoal, aged brass and restrained Nordic ornament. Text and item silhouettes take precedence over ornament. The approved concept direction has the minimap at upper right; the old horizontal Skyrim compass is absent when the minimap is active.

The three concept scenes were outdoor exploration, Nordic dungeon combat and an open grid inventory. They are visual references, not exact rendering or feature requirements. Multiple companion portraits and enemy minimap dots in a concept do not prove support for every follower or marker type.

## Gameplay layout

| Area | Intended content | Behavior |
| --- | --- | --- |
| Upper left | Player and follower portrait/status strip. | Player plus at least one supported follower; hide unused slots and use silhouettes when portraits are unavailable. |
| Upper right | Compact local minimap and quest text. | Reserve separate bounds; allow scale/toggle; map uses the selected provider's real content. |
| Bottom center | Player resources and up to eight action slots. | Clear selected/equipped state and native counts; keyboard/controller prompts reflect actual bindings. |
| World center | Combat, aiming and interaction feedback. | Keep unobstructed; normal close aiming can take precedence over elevated framing. |
| Bottom left | Existing notifications where feasible. | A new comprehensive combat/event-log system is deferred. |

Do not duplicate both player resource presentations unnecessarily. Portrait/resource grouping may be adjusted after live readability checks. If the chosen minimap permits circular framing, prefer it; a working provider-supported shape beats a fragile custom rendering fork.

## Inventory layout

Left: equipment slots and a silhouette or inexpensive proven character preview. Right: uniform icon grid with categories, search/sort and scroll. Detail area: selected item name, relevant native stats, weight/value, status and available actions. Footer: current carry weight/capacity, gold and controls.

One grid cell represents an entry/stack display slot. It does not impose physical capacity or item-shape rules. Distinct item instances remain distinguishable. The grid scrolls to contain the real inventory; there is no fake empty-space limit. Modded items use a generic type icon if a suitable icon is unavailable.

Required operations: inspect, equip/unequip, consume/use/read as appropriate, favorite and drop with native quantity/quest restrictions. Container, barter and other non-player-inventory menus remain native/SkyUI and accessible.

## Camera feel

Aim for a held elevated pitch and moderate perspective compression, with configurable distance. Keep character movement intuitive relative to the camera using the selected existing movement route. Allow yaw/rotation, recenter and zoom. Tight rooms lower/shorten the camera; bow/spell precision can enter a closer view. Emergency return is always available.

The overhead view is an aesthetic and navigation mode, not a promise to keep every ceiling invisible. No per-cell roof editing is required for MVP.

## Input and readability

Support keyboard/mouse and controller without pointer emulation as the only controller option. Focus must be obvious, cancellation must consistently back out, and state uses text/icons as well as color. Cover 720p, 1080p and one increased text/scale case. Anchor layouts rather than baking all coordinates into one screenshot resolution.

Pause behavior is explicit: normal inventory pause is allowed; action combat stays continuous while menus are closed. Do not show initiative queues, turn buttons, tactical party orders, artificial ability cooldowns or draggable command paths.

## Presentational scope

Exact concept portraits, live character render, textured parchment map, comprehensive event log and multiple arbitrary companion frameworks are polish/extensions. The MVP must still look like a coherent CRPG interface and deliver the functional grid/minimap/hotbar requirements. A mismatch in artwork is not permission to skip a core function.

## Core HUD candidate — DYL-160

The player panel anchors at bottom center with three engine-fed meters labeled HEALTH, MAGICKA and STAMINA. A read-only native snapshot supplies current values and effective maxima, including temporary modifiers, through the UI's Scaleform bridge. Resource reads run on Skyrim's task queue; Scaleform receives numeric copies at most ten times per second, with loading invalidation and a 500ms freshness limit. Displayed percentages are current/effective maximum; native HUD penalties are not applied a second time. It suppresses duplicate native resource groups only while fresh complete data exists and the current native HUD mode permits the custom panel. Missing/stale data, a missing bridge, disable or reinitialization restores native meters for the current mode. The first runtime candidate had invisible resources. Its callback gate was an unreliable source-level initialization contract; later diagnostics confirmed that using Stage coordinates directly under a translated HUDMenu parent placed the meters off-screen. Panel anchors and scale now convert Stage coordinates through the full parent transform. The initial candidate lacked traces to isolate its cause.

The native crosshair, target/activation prompt, compass and map provider retain their existing authority and placement. The upper-left reserved party panel says “Follower data unavailable”; it supplies no fabricated follower status. The existing upper-right map area remains reserved for its provider. No hotbar or Camera dependency is introduced.

Preferences belong to `SkyCRPG_UI.ini`: version 1, startup scale 85–115%, horizontal offset ±120 and vertical offset ±60 movie-coordinate units, with viewport bounds. Reset by restoring the shipped defaults and restarting Skyrim. This bounded control does not implement the later full layout editor/controller coverage. See [core HUD evidence](evidence/p2/core-hud.md) for source/build checks and the separately open gameplay acceptance.

The owner-approved bounded compass bridge hides the native compass only for the exact provider stack and a visible, in-bounds map in normal gameplay after a 500 ms settle. The settle is not proof of a fresh render; the owner accepted stale-frame risk. Missing, hidden or uncertain map state releases the compass. Set `bReplaceCompass=0` to retain it. DEM K toggles the map and L toggles zoom; these are distinct from F6/F8 and hotbar 1–8. The Survival temperature indicator is inside the hidden compass and is also hidden. The 1080p active-effect overlap remains a DYL-168 layout check; runtime acceptance remains open.
