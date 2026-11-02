# DYL-175 owner playtest — ACCEPT

Allow about 20 minutes on an isolated profile and copied save. First complete the technical package regression; this route is not a substitute. Record date, host, executable, dependency versions, profile, source `6b4c0fc6a6da4effd9faeafc9a2c59874a612271`, CI `41376680950`, Camera `e3f6d61e2d2075a10d8a31c07ccfbe9c78e324230f5c399cadbc2c3c39c86838`, UI `f508a0e1343e2f15ea825e284f57fafed1108f62eb38acbef4b72582d2d940cb`, and actual device for every segment. Owner result is **OWNER_PLAYTEST_PENDING** until played.

1. **Camera-only, keyboard/mouse (6 minutes):** move outside and into an interior; zoom, recenter F7, F8 recovery, then F6 first-person precision aim and return. Check movement and ordinary inventory. Note overhead aim is a known failure.
2. **UI-only, controller (6 minutes):** with ordinary camera, inspect player inventory, navigate categories and filter, equip/use an item and read its count/weight; inspect hotbar and follower. Check minimap outside/inside and native compass if the optional stack is absent or hidden.
3. **Combined, keyboard/mouse (8 minutes):** travel exterior to interior, fight in real time and use F6 for aiming; inspect inventory readability, hotbar, minimap and native menus. On a copied save and with the game closed, disable one layer and restart to check the other remains usable, then restore and check the reverse if time permits. Record any step not reached as NOT_RUN.

| Area | Device and observation | ACCEPT / revision request / NOT_RUN |
| --- | --- | --- |
| Camera controls and F6 aiming | Not recorded per area | ACCEPT (overall ruling) |
| Inventory readability and actions | Not recorded per area | ACCEPT (overall ruling) |
| Minimap and navigation | Not recorded per area | ACCEPT (overall ruling) |
| Module independence and recovery | Not recorded per area | ACCEPT (overall ruling) |
| Overall | Keyboard/mouse and controller, 2026-11-02 | **ACCEPT** ([record](evidence/p4/acceptance.md)) |

A revision request should identify a reproducible issue; acceptance is the owner's decision. [Limitations](RELEASE_NOTES.md) and [release checklist](RELEASE_CHECKLIST.md).
