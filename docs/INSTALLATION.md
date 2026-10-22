# Installation and recovery

**Planning document: no installable SkyCRPG release is available yet.** P4-02/P4-05 must replace this with instructions verified against the actual archives.

## Required published instructions

For Camera and UI separately: supported game executable/storefront, host support, exact prerequisites and download links, install/load-order rules, files owned, default controls, configuration location, and first-run smoke steps.

Provide three recipes: Camera only, UI only, and both. External prerequisites may differ, but neither SkyCRPG module may require the other.

## Safe testing and recovery design

Use a disposable mod profile and copied save during development. Document the emergency camera return/reset, restoring native HUD/compass, and a UI menu fallback. Explain whether disabling a feature is live or requires restart. Native DLL removal is performed with the game closed.

Do not claim universally safe mid-save uninstallation. If a selected provider or SkyCRPG feature introduces persistent save/co-save state, document its cleanup and consequences. Test the supported removal/restart procedure on copied saves before advertising it.

A missing optional minimap must not leave the standard compass permanently hidden. A disabled camera must not change UI-only behavior.

## Release publication distinction

The four phases prepare and validate internal candidates. Public Nexus/GitHub release publication is a separate action after acceptance and any explicit release instruction.
