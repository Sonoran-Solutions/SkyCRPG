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
