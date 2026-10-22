# Dependency selection record

Status: candidates only; no implementation lock exists yet.

P1-02 and ADR-001 populate this table from chosen releases and P1 game evidence.

| Component | Selected version/file/commit | Role | Runtime/host tested | Permission/distribution | Fallback |
| --- | --- | --- | --- | --- | --- |
| SKSE / Address Library | Pending | Game/plugin runtime | NOT_RUN | External prerequisites | Disable unsupported native plugin. |
| CommonLib fork, if used | Pending | Build/runtime abstraction | NOT_RUN | Inspect exact license and commit | Select proven supported line. |
| Camera owner / TDM | Pending | Framing / movement | NOT_RUN | External or permitted adapter | Safe underlying camera. |
| UI renderer / HUD provider | Pending | Presentation | NOT_RUN | Pending | Native UI. |
| Player inventory route | Pending | Grid and actions | NOT_RUN | Pending | Native/SkyUI player menu. |
| Follower status provider | Pending | Portrait/status data | NOT_RUN | Pending | Limited native adapter/silhouette. |
| Hotbar provider/route | Pending | Native favorites display/actions | NOT_RUN | Pending | Native favorites. |
| Minimap / Infinity UI / Local Map Upgrade | Pending | Local navigation | NOT_RUN | External provider/configuration first | Restore standard compass. |

Retain upstream URLs, release-file identity, source commit where relevant, transitive requirements, conflicts, installation order and exact configuration changes. Do not treat a mod's latest release as a reproducible lock.

The candidate list is in [REUSE_RESEARCH.md](../REUSE_RESEARCH.md). Update this file and the actual build/package manifests together when a dependency changes.
