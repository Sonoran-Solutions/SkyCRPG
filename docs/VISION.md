# Product vision

## The experience

SkyCRPG should make Skyrim read and feel like a classic party CRPG: a high camera that makes nearby space legible, a consistent framed interface, visible companion status, a useful action bar, an icon-based inventory, and a local minimap. Combat remains Skyrim's continuous action combat.

The camera and interface are separate products within one repository. A player who likes the ordinary first/third-person view should still be able to use the UI. A player who likes the elevated view should be able to retain their existing interface.

The concept screenshots established a visual direction: charcoal panels, restrained brass/Nordic trim, portraits on the left, action bar below, and a minimap at upper right instead of the horizontal compass. They are illustrations of intent. They do not establish working camera geometry, valid inventory transactions or supported mod combinations.

## What makes this worth building

An outdoor camera preset alone is insufficient. The original [TISS guide](https://www.nexusmods.com/skyrimspecialedition/mods/89358) demonstrates an isometric-style setup but explicitly calls out cave and dungeon limitations. SkyCRPG's camera value is predictable controls, obstruction recovery, usable aiming and lifecycle handling.

The UI value is a coherent, independently usable interface assembled from existing capabilities where possible, with a carefully scoped grid inventory. It is not a reason to rebuild movement, mapping or follower management.

## Design principles

1. Playability wins over maintaining an overhead angle at all costs.
2. Game state remains authoritative; presentation never invents items or capabilities.
3. Integrate a proven component before owning a new subsystem.
4. Missing optional providers degrade a widget, not the entire game.
5. Support claims describe tested executable versions and combinations.
6. Feature completion is demonstrated in Skyrim, not inferred from a successful compile.

## Initial assumptions

The MVP targets desktop Skyrim Special Edition/Anniversary Edition with one pinned executable and compatible SKSE stack. The actual storefront/runtime and Windows or Proton playtest host are selected from the owner's available installation in Phase 1. There is no commitment to every 1.5/1.6/1.7 runtime, GOG, VR or console.

The first inventory is a uniform visual grid with native weight capacity. A Tetris-style storage system would change the product, so it remains a separately recorded choice. Keyboard/mouse and a practical controller route are planned; touchscreen-native control is deferred.

## Future directions

After both MVPs are accepted: broader runtime/provider support, richer portraits, grid container/barter screens, deeper character-sheet styling, configurable event log, extra HUD themes, and improved optional occlusion handling. Click-to-move and party commands need independent feasibility decisions. Turn-based combat has no place in the current plan.
