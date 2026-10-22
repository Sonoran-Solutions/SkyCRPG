# Existing mods and reuse research

Research checked 2026-10-22. These are upstream claims and candidate roles; none has been tested inside SkyCRPG. Exact versions, files and code/asset permissions must be rechecked when selected in P1-02.

## Candidate matrix

| Candidate | Evidence and role | Planning disposition |
| --- | --- | --- |
| [TISS](https://www.nexusmods.com/skyrimspecialedition/mods/89358) | Author's isometric-style tweak guide for 1.6.640; explicitly identifies cave/dungeon limitations. | Useful precedent and test inspiration. Not a finished camera module to rebrand. |
| [True Directional Movement](https://www.nexusmods.com/skyrimspecialedition/mods/51614) / [source](https://github.com/ersh1/TrueDirectionalMovement) | Directional movement, target lock and projectile aim support; author documents keyboard/controller and SmoothCam compatibility. | Preferred movement/targeting candidate. Validate our camera handoff and exact runtime. |
| [SmoothCam](https://www.nexusmods.com/skyrimspecialedition/mods/41252) / [source](https://github.com/mwilsnd/SkyrimSE-SmoothCam) | Configurable third-person camera project. | Test as preset/API foundation. Do not assume it supplies the complete elevated-camera behavior. |
| [Grid Inventory](https://github.com/skypia0147-dev/grid-inventory) / [English feature README](https://github.com/skypia0147-dev/grid-inventory/blob/main/README_EN.md) | Existing Tetris inventory plugin; its feature README says normal carry-weight limits are effectively retired. | Strong reuse/reference candidate, but the default gameplay does not meet our native-weight display-grid assumption. Require a verified presentation-only route before adoption. |
| [Inventory Reforged for SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/195826) | Equipment/character panel, item details and follower equipment workflows; author page permits credited reuse. | Possible character/equipment presentation reuse. It does not establish our uniform-grid requirement. |
| [Skyrim Party Sheet](https://www.nexusmods.com/skyrimspecialedition/mods/167538) | Existing player/follower HUD. Its page requires permission to modify files; source availability alone is not permission. | Prefer external configured integration; inspect any data/API route. A small own status adapter is a bounded fallback. |
| [Dragon's Eye Minimap – SKSE Menu Framework fork](https://www.nexusmods.com/skyrimspecialedition/mods/189534) | Configurable minimap corner, size and shape; documents 1.5.97, 1.6.1170 and 1.7 builds, with 1.7.104 tested by its author. | Preferred minimap candidate, pending our in-game proof and matched dependencies. |
| [Dragon's Eye upstream source](https://github.com/alexsylex/DragonsEyeMinimap) | Public source carrying an MIT license. | Reference and possible permissive upstream route; distinguish it from the later fork. |
| [Local Map Upgrade](https://www.nexusmods.com/skyrimspecialedition/mods/129756) / [source](https://github.com/alexsylex/LocalMapUpgrade) | Local-map component used by Dragon's Eye. | Select the matching version as part of the minimap stack, not as a standalone HUD minimap. |

## Important decisions this research changes

**Do not write another minimap.** The Dragon's Eye fork documents an existing customization path through its winning Minimap.swf and MinimapArt.swf. Select a compatible release and integrate positioning/style. It requires SKSE, Address Library, Infinity UI and Local Map Upgrade, with matching 1.7 builds on that runtime. The native compass handoff remains our integration responsibility; a provider compass ring is a different widget. [Author documentation](https://www.nexusmods.com/skyrimspecialedition/mods/189534).

**Do not assume that “grid inventory” means unchanged game rules.** Grid Inventory is a real storage-mechanics overhaul. Its main README currently describes 1.5–1.7 support, while the English feature README still lists 1.5–1.6; inspect the chosen archive/source revision instead of treating either sentence as proof of our supported matrix. It is GPL-3.0 with a linking/modding exceptions file; inspect both before source reuse. [Main README](https://github.com/skypia0147-dev/grid-inventory), [feature README](https://github.com/skypia0147-dev/grid-inventory/blob/main/README_EN.md), [exceptions](https://github.com/skypia0147-dev/grid-inventory/blob/main/EXCEPTIONS.txt).

**Do not assume a public source download means unrestricted reuse.** Party Sheet's modification permission is restrictive. Prefer external dependencies/configuration, obtain the relevant permission, or implement the small missing boundary independently. Do not automatically import portrait art.

**Pin current runtime support.** [SKSE's official page](https://skse.silverlock.org/) lists Steam build 2.3.1 for game 1.7.104 as of this research. Choosing a CommonLib fork, Address Library release or “AE” label does not itself prove compatibility. The [CommonLibSSE-NG documentation](https://github.com/CharmedBaryon/CommonLibSSE-NG) explains multi-runtime abstractions; selected hooks and providers still need verification.

## Reuse order

1. Configuration/preset of an independently installed mod.
2. Small adapter using a documented, tested interface.
3. Source contribution or fork when licensing, asset rights and maintenance cost are understood.
4. Original implementation of the narrow missing capability.

Do not spend weeks designing a general framework if an existing configured widget meets the acceptance criteria. Equally, do not force a dependency that changes the intended inventory mechanics just because its name matches the feature.

## Selection record

For each chosen component, P1-02 records: upstream URL; release/file version; commit if source is used; supported runtime claims; actual tested runtime/host; transitive dependencies; relevant APIs; conflicts; source license; asset permissions; distribution plan; fallback and owning ticket.

The minimap fork's page contains older MIT language alongside newer GPL-3.0-or-later changelog/current-license text. Verify the exact selected release's license files; do not generalize from the upstream MIT project or repeat its explanation of other libraries' licenses as an independent legal finding.

No third-party source, binary or artwork is copied by this planning commit.
