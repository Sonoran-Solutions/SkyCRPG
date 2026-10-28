# Installation and recovery

The concrete file ownership, independent recipes and update/removal boundaries are in [ARCHITECTURE](ARCHITECTURE.md#independent-packages-and-file-ownership). Internal prototypes were tested, but the native ZIPs contain skeleton DLL/INI files only; generated UI patches and provider configs are separate artifacts. [Runtime baseline](compatibility/runtime-matrix.md) pins the tested prerequisites.

**Planning document: no installable SkyCRPG release is available yet.** P4-02/P4-05 must replace this with instructions verified against the actual archives.

## Required published instructions

For Camera and UI separately: supported game executable/storefront, host support, exact prerequisites and download links, install/load-order rules, files owned, default controls, configuration location, and first-run smoke steps.

Provide three recipes: Camera only, UI only, and both. External prerequisites may differ, but neither SkyCRPG module may require the other.

## Safe testing and recovery design

Use a disposable mod profile and copied save during development. Document the emergency camera return/reset, restoring native HUD/compass, and a UI menu fallback. Explain whether disabling a feature is live or requires restart. Native DLL removal is performed with the game closed.

Do not claim universally safe mid-save uninstallation. If a selected provider or SkyCRPG feature introduces persistent save/co-save state, document its cleanup and consequences. Test the supported removal/restart procedure on copied saves before advertising it.

A missing optional minimap must not leave the standard compass permanently hidden. A disabled camera must not change UI-only behavior.

The UI plugin's `[Inventory] bManageItemPreview` (default 1 in `Data/SKSE/Plugins/SkyCRPG_UI.ini`) moves the native 3D inventory preview at runtime only and restores it when the inventory closes or a game loads; it writes no user or game INI. To rule it out while troubleshooting the item preview, set it to 0 with the game closed and restart; see [Controls](CONTROLS.md).

## Camera candidate disable and recovery

In a valid world state, F8 toggles SmoothCam's emergency disable; native Toggle POV returns to the native view. If an interrupted F6 precision session no longer returns, use native Toggle POV to reach third person, then F6 to enable the Camera policy. F7 recenters; it does not restore defaults. These are recovery controls, not a live DLL unload, and their full lifecycle coverage is **NOT_RUN**.

For a candidate disable or settings reset, quit Skyrim completely first. In a disposable mod-manager profile with a copied save, disable the SkyCRPG Camera layer; leave the independently installed UI and shared prerequisites intact. Restore any Camera-owned SmoothCam/TDM overrides only when they still match the values installed by the candidate, using the prior backup or mod-manager layer; preserve and reconcile user edits. For a settings reset, back up and remove or replace only `Data/SKSE/Plugins/SkyCRPG_Camera.ini` with the candidate's default INI while the game is closed. Restart and check movement, camera, native menus and any installed UI on the copied save. Re-enable the Camera layer with the game closed to restore it. The copied-save disable/restart route remains **NOT_RUN**; this procedure does not establish safe mid-save uninstallation or removal of providers.

## Release publication distinction

The four phases prepare and validate internal candidates. Public Nexus/GitHub release publication is a separate action after acceptance and any explicit release instruction.
