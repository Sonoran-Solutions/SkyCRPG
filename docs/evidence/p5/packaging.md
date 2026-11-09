# P5-06 R2 packaging

Owner decision 2026-11-08: Selection is a separate archive; Camera contains an optional, manually applied target-lock overlay. Source base ed9692c; working tree changes pending orchestrator integration. No release or owner acceptance is implied.

## Job record

Owner: P5-06 packaging worker. Focused package test: `python3 -B -I -m unittest discover -s tools/tests -p "test_package*.py" -v`; purpose package policy and synthetic assembly; expected OK, exit 0; timeout 10 minutes; completion process exit, log `/tmp/p5-package-focused.log`. Full Python suite: `python3 -B -I -m unittest discover -s tools/tests -v`; purpose regression; expected OK, exit 0; timeout 10 minutes; completion process exit, log `/tmp/p5-package-all.log`. Both are bounded foreground jobs; timeout terminates their process group, no game processes.

Senior-review fix verification: owner packaging worker; focused and full commands as above, expected OK and exit 0, each with a 10-minute timeout and process-exit completion signal; logs `/tmp/p5-package-review-focused.log` and `/tmp/p5-package-review-all.log`; bounded foreground jobs with timeout cleanup.

## Contents and rationale

Native archives include their own DLL and INI under SKSE/Plugins, CommonLib notices, build inputs and vcpkg notices under Docs, plus a manifest and SHA256SUMS beside the ZIP. Full candidates add project notices; Camera adds its existing SmoothCam and merged baseline TDM settings, UI adds its provenance-checked SWFs, Selection adds no Camera or UI payload. Cursor mode requires both Camera and Selection; neither is required for ordinary UI or Camera use.

The default native run packages Camera and UI and includes Selection only when its DLL is present, logging a skip otherwise; an explicit Selection request requires its DLL.

Camera includes a merge-only MCM settings fragment and README under Docs/SkyCRPG_Camera/overlays/target-lock. The lock staging route merges this fragment into MCM/Config/TrueDirectionalMovement/settings.ini; the staged TDM base TOML contains provider target points and is not a lock overlay. The README lists all 19 values and their purpose, backup, manual merge, separate MO2 mod, precedence and removal. The orientation preservation claim is a hypothesis pending runtime trace. No INI defaults change and nothing under Docs is auto-applied. U and 0.4 seconds remain owner-approved (2026-11-08) test parameters.

Synthetic tests `test_selection_assembly_and_overlay_are_independent`, `test_camera_assembly_preserves_provider_keys_and_independence`, `test_native_packages_keep_independent_payloads_and_checksum_both`, `test_dll_commit_and_import_guard`, and `test_provider_collisions_are_case_insensitive_and_provider_specific` cover disjoint paths, cross-module imports, manifests, hashes and provider collisions. Provider inventory remains mandatory for each full assembly, including Selection.

## Verification

Focused package suite: **PASS**, 17 tests in 0.549s, OK, exit 0. Full suite initial run: **FAIL**, 27 tests in 0.575s, one obsolete assertion expected the former raw Selection copy. After updating that assertion to require the packaging command and dist upload, full suite: **PASS**, 27 tests in 0.605s, OK, exit 0. Logs are the paths above and `/tmp/p5-package-all-rerun.log`; hashes printed by synthetic tests are fixture hashes, not candidate evidence.

Senior-review fix: focused suite initial run **FAIL**, 18 tests, due to an assertion confusing the full expected-path check with the packaged-payload check; after correcting the assertion, focused suite **PASS**, 18 tests in 0.684s, OK, exit 0 (`/tmp/p5-package-review-focused-rerun.log`). Full suite **PASS**, 28 tests in 0.739s, OK, exit 0.

Windows build and CI artifact upload: **NOT_RUN** (MSVC unavailable; orchestrator runs CI). Actual archive hashes: **NOT_RUN**. MO2 installation, update, overlay removal and game routes: **NOT_RUN**. Synthetic tests do not establish runtime behavior.

## Proposed shared-file changes

- INSTALLATION: Selection is a separate archive. Install it alongside Camera for cursor mode. To enable target lock, follow the Camera archive's target-lock overlay README; merge settings, preserve unrelated keys and back up existing values. Remove or restore the overlay on disable.
- RELEASE_CHECKLIST: verify all three native artifacts, full candidates and SHA256SUMS; exercise Camera-only, UI-only, Camera plus Selection and combined profiles, including MO2 overlay apply/removal and conflicting MCM settings.
- STATUS: R2 packaging implemented pending Windows CI, archive hashes, isolated install routes and owner acceptance; portable tests alone do not prove gameplay.
- DECISIONS / ticket: record the owner's 2026-11-08 R2 ruling: separate Selection archive, optional manually applied Camera target-lock overlay.
