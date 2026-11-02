# DYL-175 owner playtest — pending

Allow about 20 minutes on an isolated profile and copied save. First complete the technical package regression; this route is not a substitute. Record date, host, executable, dependency versions, profile, source `<candidate SHA>`, CI `<candidate CI run>`, Camera `<candidate archive hash>`, UI `<candidate archive hash>`, and actual device for every segment. Owner result is **OWNER_PLAYTEST_PENDING** until played.

1. **Camera-only, keyboard/mouse (6 minutes):** move outside and into an interior; zoom, recenter F7, F8 recovery, then F6 first-person precision aim and return. Check movement and ordinary inventory. Note overhead aim is a known failure.
2. **UI-only, controller (6 minutes):** with ordinary camera, inspect player inventory, navigate categories and filter, equip/use an item and read its count/weight; inspect hotbar and follower. Check minimap outside/inside and native compass if the optional stack is absent or hidden.
3. **Combined, keyboard/mouse (8 minutes):** travel exterior to interior, fight in real time and use F6 for aiming; inspect inventory readability, hotbar, minimap and native menus. On a copied save and with the game closed, disable one layer and restart to check the other remains usable, then restore and check the reverse if time permits. Record any step not reached as NOT_RUN.

| Area | Device and observation | ACCEPT / revision request / NOT_RUN |
| --- | --- | --- |
| Camera controls and F6 aiming | | NOT_RUN |
| Inventory readability and actions | | NOT_RUN |
| Minimap and navigation | | NOT_RUN |
| Module independence and recovery | | NOT_RUN |
| Overall | | OWNER_PLAYTEST_PENDING |

A revision request should identify a reproducible issue; acceptance is the owner's decision. [Limitations](RELEASE_NOTES.md) and [release checklist](RELEASE_CHECKLIST.md).
