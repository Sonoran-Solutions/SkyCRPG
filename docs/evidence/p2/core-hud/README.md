# DYL-160 compact provenance

These source logs and manifests are review metadata only. They contain no external SkyUI source files, exported ActionScript dump, proprietary SWF, game asset, or save.

- [build.json](build.json) — exact HUD-only build command, pinned source/base hashes, Java/FFDec process data and generated artifact hash.
- [jobs.json](jobs.json) — bounded test/build/export commands, owners, limits and completion status.
- [source-tests.log](source-tests.log), [hud-harness.log](hud-harness.log), [settings-parser.log](settings-parser.log) — focused test result summaries.
- [hudmenu-export.log](hudmenu-export.log), [export-verification.json](export-verification.json), [skse-scaleform-timing.json](skse-scaleform-timing.json) — FFDec export result, expected-source checks and SKSE Scaleform registration timing review against tag `v2.3.1`.
- [address-library-guard.json](address-library-guard.json), [address-library-guard-tests.log](address-library-guard-tests.log), [commonlib-public-header-check.json](commonlib-public-header-check.json) — exact prerequisite pin, fail-closed behavior, CommonLib public-header ordering and focused checks.
- [native-source.json](native-source.json), [bridge-atomicity-tests.log](bridge-atomicity-tests.log) — exact native source revision/hashes and readiness-gate regression.
- [native-build-package.json](native-build-package.json) — final native compile/package/upload PASS, diagnostics upload failure, and pending workflow fix verification.
- [native-ci-initial.json](native-ci-initial.json), [include-order-tests.log](include-order-tests.log) — initial C1189 failure, pinned CommonLib header requirement, correction, and focused verification.
- [as2-text-formatting.json](as2-text-formatting.json) — pinned AS2 API/source usage review and dynamic-format regression evidence.
- [SHA256SUMS](SHA256SUMS) — hashes for the files in this provenance folder.

The built `hudmenu.swf` and FFDec-exported `HUDMenu.as` remain outside Git in the session candidate directory named in the parent [core HUD report](../core-hud.md).

Resource snapshot correction: `resource-*` files record the changed candidate and bounded API/source/build checks. `runtime-route-1-*` preserve the initial failed owner route and clean restoration; changed-candidate runtime remains open.

`geometry-*` records the bounded AS2 diagnostic-only follow-up and latest HUD SWF hash. Native source remains `e49fc30`.
