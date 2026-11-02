# DYL-171 / P4-02 — independent internal candidate packaging

2026-10-28. **Tooling PASS; final candidate and runtime acceptance NOT_RUN.**
Base `c7333c7c13284ad9d228e6f15213bc8465bd6e19`, worktree
`phase4/dyl-171-package`. Source for final candidates: `<head SHA>`;
Windows native build: `<CI run>`; final DLLs: `<DLL sha256>`.
The assigned [ticket](https://linear.app/devils17/issue/DYL-171) and
[Phase 4](../../phases/PHASE_4.md) were read before implementation.
No commit, push, branch creation, game launch, live deployment or provider edit
was performed. Artifacts and job records are outside Git at
`/srv/storage/Projects/SkyCRPG/session-p4/logs/package/`.

## Audit before implementation

| Item / criterion | Already covered at base | This lane / remaining gate |
| --- | --- | --- |
| P3 blocker 11: complete independent archives | `tools/package.py` produced DLL, INI and three CommonLib/build notices; [P3 gate](../p3/gate.md#candidate-identity) pins tested native archives and separately built SWFs | Implemented local `assemble` for full Camera/UI archives; final same-commit candidates NOT_RUN |
| Camera configuration | `modules/camera/tools/stage_preset.py` stages SmoothCam active config/preset; committed TDM fragment and `stage-combined-p3/Data` show the install destinations | Added both JSONs and full default TDM settings with the Camera fragment merged; preserves unrelated provider keys. Existing user configs require reconciliation |
| UI movies/provenance | Builder already pins SkyUI source, actual Nexus base SWFs, include hashes and output hashes; [lifecycle evidence](../p3/menu-lifecycle.md#provenance) pins the final movies | Verify full compiled set, hashes, owner and source SHA; builder source-SHA omission supplied as integration patch, not a direct module edit |
| M1/M2 separate ownership | Existing case-insensitive path/import checks; [architecture contract](../../ARCHITECTURE.md#independent-packages-and-file-ownership) | Retained and extended to complete expected payload sets. No mandatory other SkyCRPG module |
| No junk, personal paths or provider collisions | No package-focused checks at base | Strict content lists, path/text checks and pinned-provider path inventory including BSA members; explicit six-rule override list |
| Version, requirements, Q3 reproduction | Native-only deterministic ZIP metadata existed; no demonstrated full assembly reproduction | One package `VERSION=0.1.0-rc.1`, requirements and internal-candidate manifests; `verify-repro` compares all output bytes |
| Fresh Camera/UI/combined install and update/removal (M2; blocker 3 interface) | Prior bounded prototype routes and hash-guarded fixture deployment are evidence, not final packaged installs | Draft procedures below; every final-profile runtime row NOT_RUN, owner/orchestrator owns launches |
| Attribution/distribution part of blocker 11 | Existing CommonLib notices, original license and third-party exclusions | Retained notices; add original LICENSE/licensing/third-party policy to each namespace. Rights review remains DYL-174, BLOCKED for publication; no new rights decision |

## Design and assembly contract

`python3 tools/package.py native --module camera|ui --build-dir <build> --commit <full SHA>`
keeps the original native CI role and original no-subcommand invocation valid.
Native artifacts carry `artifact_kind=native` and `candidate_eligible=false`.
The workflow still builds independently selected targets and preserves existing
artifact names. Package tests and their inputs trigger shared CI selection;
leaf-only and explicit-dispatch behavior remains unchanged. No pins/toolchains
or module sources changed.

`assemble` takes `--module`, `--commit`, `--native-zip`, `--native-manifest`,
`--providers` and `--out`; UI also requires `--swf-dir` and `--swf-owner`.
Use a separate output directory per module. It validates the native ZIP hash,
exact contents/file hashes, imports, and source SHA in both CI manifest and DLL.
UI stores that SHA as a standalone literal; Camera concatenates it into its
startup log literal. Both observed full-SHA formats are checked. Configuration,
license and patch-include hashes come from the requested Git commit, so dirty
working files do not quietly substitute for recorded source inputs.

UI requires the pinned SkyUI commit, original ActionScript hashes, the **Nexus
6.11 BSA** base hashes (GitHub compile-probe bases are refused), exact owner,
complete `COMPILED_NOT_RUN` targets/outputs, valid compiler hash, matching movie
hashes and patch source SHA/include hashes. It copies just three generated movies,
never staging/scripts/compiler logs or absolute paths from `build.json`.
The sanitized provenance records every SWF hash.

Camera places `SmoothCam.json`, `SmoothCamPreset0.json` under `SKSE/Plugins/`
and merged TDM settings under `MCM/Config/TrueDirectionalMovement/settings.ini`.
TDM input/fragment hashes are in the manifest. This contains the provider's
configuration defaults plus declared Camera keys, not its DLL/art/archive.
Third-party config permission is still part of DYL-174 review. On a pre-existing
profile the archive is **not** permission to overwrite the user's complete
settings: merge the declared fragment keys as described below.

Each output ZIP is `<Package>-0.1.0-rc.1-<sha7>.zip`, with members directly under
the mod-manager Data root (`SKSE/`, `Interface/`, `MCM/`, `Docs/`), not nested
inside another `Data` folder. Each accompanying manifest lists archive/file
hashes, runtime/requirements, source commit, overrides, provider inventory hash,
lock hash, provenance and `distribution=internal-candidate`. `SHA256SUMS` hashes
both the archive and manifest. Docs are namespaced by module. No paths are shared.

`--allow-mismatch-for-dry-run` only relaxes native/requested source equality and
missing/different SWF source/include provenance for historical inputs. All
hash/base/owner/content/collision checks stay active. It always adds `DRY-RUN`
to the ZIP name, records mismatches and sets `candidate_eligible=false`, even
when inputs match. **Never use this option to produce candidates.**

`verify-repro` assembles twice in separate temporary directories and compares
the ZIP, manifest and `SHA256SUMS` bytes. On success it retains `repro.json`;
it removes stale success reports before starting, and cleans temporary outputs.
This verifies **assembly from the same prebuilt inputs**, not deterministic
Windows compilation, Java compilation or reproduction across Python/zlib versions.

## Provider collisions and junk checks

The input inventory uses the acquired trees under
`/srv/storage/Projects/SkyCRPG/session-20261026/extracted/`: SkyUI 6.11,
SmoothCam 1.8.1 (AE), TDM 2.3.1, MCM Helper 1.6.3 (SkyrimSE/ESL/BSA/Data),
Address Library 13, Infinity UI NG 2.1.3, LMU 1.3.4 and DEM 1.7.5
(Common + Line-17). Missing/ambiguous/empty provider trees fail.
BSA105 folder/file name tables are inspected without extracting assets.
The inventory digest covers selected loose files and BSA bytes and is recorded
in each manifest. Paths compare case-insensitively; actual archive case remains
unchanged. Optional Party Sheet is not installed or required by these recipes.

Only these entries in `tools/package.py:OVERRIDES` may collide, and only with
the named provider. All other collisions fail.

| Path relative to Data | Provider | Reason | Winning rule |
| --- | --- | --- | --- |
| `Interface/inventorymenu.swf` | SkyUI Community 6.11 | Player inventory patch | SkyCRPG UI loose movie wins over SkyUI BSA |
| `Interface/skyui/inventorylists.swf` | SkyUI Community 6.11 | Imported inventory-list patch | SkyCRPG UI loose movie wins over SkyUI BSA |
| `Interface/hudmenu.swf` | SkyUI Community 6.11 | HUD/compass presentation patch | SkyCRPG UI loose movie wins over SkyUI BSA |
| `SKSE/Plugins/SmoothCam.json` | SmoothCam 1.8.1 | Startup framing keys | Camera keys win; merge existing keys with game closed |
| `SKSE/Plugins/SmoothCamPreset0.json` | SmoothCam 1.8.1 | Original framing preset in slot 0 | Camera slot wins only after backing up/reconciling occupied slot 0 |
| `MCM/Config/TrueDirectionalMovement/settings.ini` | TDM 2.3.1 | Movement-only/no competing camera writer | Camera fragment keys win in merged settings; preserve unrelated/user changes |

The real inventory found all three UI BSA collisions and the TDM settings
collision. SmoothCam's selected archive does not supply the two Camera JSON
paths; those rules document ownership if provider settings already exist.
They do not authorize collision with another camera/HUD provider.

Fail on every unexpected or missing package member, unsafe/traversal/drive path,
case-duplicate, symlink/special/directory ZIP entry, debug/build/cache/editor/OS
junk (`.pdb`, `.ilk`, `.obj`, `.pyc`, build/CMake dirs, `__pycache__`, dotfiles,
backup/swap files). Text must decode as UTF-8 or BOM-marked UTF-16 and is scanned
for `/home/`, `/Users/`, `/root/`, Windows user-home paths, this host's personal
login/home name and explicit username fields. Generated manifest text is checked
too. This is a deterministic pattern/content check, not a universal personal-name
classifier. DLL/SWF contents are validated as their formats/hashes, not decoded
as text. Test probe, game executables, saves and provider binaries cannot match
the expected content list.

## Validation and historical dry inputs

Before every test/assembly job the external runner recorded purpose, exact
command/profile, expected output, 600-second timeout/deadline, owner DYL-171,
PID, log, completion signal and process-group cleanup policy in `<job>.job.json`.
Completed jobs report `process_reaped=true`. No long job remains active.
Initial focused run: 18 tests PASS. Native coverage expansion: 19 PASS.
After Camera literal correction: 20 PASS. Final suite: **21 PASS** (14 package,
7 CI selector). Profile suite: **22 PASS** on temporary fixtures/mocked launches.

Exact final focused commands (cwd is this worktree):

```sh
timeout 600 python3 -B -I -m unittest discover -s tools/tests -v
timeout 600 python3 -B -I -m unittest discover -s tests -p test_profile_tool.py -v
git apply --check /srv/storage/Projects/SkyCRPG/session-p4/integration/package.patch
```

Logs: `acceptance-focused.log`, `profile.log`, their job JSONs, and the dry/repro
jobs listed below. `-B` keeps Python bytecode out of the worktree; `-I` isolates
imports. Synthetic fixtures include small PE/ZIP/SWF/BSA inputs, tampered hashes,
wrong source/base/owner, duplicate/symlink members, unknown collisions, privacy
failures and a deliberately nondeterministic ZIP writer that must fail repro.
Portable tests do not prove engine behavior.

**UI real inputs:** latest native ZIP with accompanying manifest found under
`session-p3` (mtime inspection) was
`dist-167b/SkyCRPG_UI-0.0.1-9a8692e.zip`, SHA-256
`79b79809fcc744b58792192551114c908a4d436c8fff7381f90f22e81029a91c`,
source `9a8692e4b5592edd26ecf8e82733a067862f3b13`.
Use the later final movie set
`session-p3/logs/dyl-167/swf-merged-main`, owner `DYL-167`; the earlier
`swf-p3-combined` is superseded. It has matching include hashes for the base
but no SkyCRPG source commit. UI dry mismatches: native SHA differs from
requested base, SWF source commit missing. UI DLL SHA-256:
`c3ef6c3a1fef2e0df1d70a6c2ad864252b6ed1df90ac9cec3a5202819c71ed53`.

**Camera supplemental real inputs:** Phase 3 Camera ZIP copies have no adjacent
Camera CI manifest, so no manifest was invented for them. Instead the matched
older native pair `session-p2-20261026/dist-camera/`
`SkyCRPG_Camera-0.0.1-8a68139.zip` and `SkyCRPG_Camera.manifest.json` was used;
archive SHA-256 `50a23e3b768ff0d5a949b88b0f97f25787f64b2155d42da675338773e7309e76`,
source `8a681390db5b36c2b6fb4fcb056c0abc0e085a35`, DLL
`aedc9d53e7c7d85719fb600fe8b71129cc9aab1f67f92cffe10ea499f106d024`.
It is an assembly probe, not the game-tested `28aae27` or a final Camera DLL.
Camera mismatch: native SHA differs from requested base.

The first Camera dry/repro commands **FAIL**ed because the initial validator
incorrectly required a standalone null-terminated commit string. Read-only
binary/source inspection found Camera's concatenated log literal; the validator
was corrected and exact-boundary regression coverage added. The changed-code
recheck and subsequent repro **PASS**ed. Failed job records are retained as
`dry-camera.job.json` and `repro-camera.job.json`; no hash check was bypassed.

Both successful assemblies requested source `c7333c7...`; all final candidates
must instead use `<head SHA>` consistently and omit the mismatch flag.
No Windows DLL or SWF was built by this lane. Linux Python 3.14 executed these
checks; gameplay host/runtime evidence is inherited from the P3 gate, not
re-attributed to these packages.

Successful real-input commands (each was run through the recorded job runner):

```sh
timeout 600 python3 -B -I tools/package.py assemble --module ui --commit c7333c7c13284ad9d228e6f15213bc8465bd6e19 --native-zip /srv/storage/Projects/SkyCRPG/session-p3/dist-167b/SkyCRPG_UI-0.0.1-9a8692e.zip --native-manifest /srv/storage/Projects/SkyCRPG/session-p3/dist-167b/SkyCRPG_UI.manifest.json --swf-dir /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/swf-merged-main --swf-owner DYL-167 --providers /srv/storage/Projects/SkyCRPG/session-20261026/extracted --allow-mismatch-for-dry-run --out /srv/storage/Projects/SkyCRPG/session-p4/logs/package/dry-ui
timeout 600 python3 -B -I tools/package.py verify-repro --module ui --commit c7333c7c13284ad9d228e6f15213bc8465bd6e19 --native-zip /srv/storage/Projects/SkyCRPG/session-p3/dist-167b/SkyCRPG_UI-0.0.1-9a8692e.zip --native-manifest /srv/storage/Projects/SkyCRPG/session-p3/dist-167b/SkyCRPG_UI.manifest.json --swf-dir /srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-167/swf-merged-main --swf-owner DYL-167 --providers /srv/storage/Projects/SkyCRPG/session-20261026/extracted --allow-mismatch-for-dry-run --out /srv/storage/Projects/SkyCRPG/session-p4/logs/package/repro-ui
timeout 600 python3 -B -I tools/package.py assemble --module camera --commit c7333c7c13284ad9d228e6f15213bc8465bd6e19 --native-zip /srv/storage/Projects/SkyCRPG/session-p2-20261026/dist-camera/SkyCRPG_Camera-0.0.1-8a68139.zip --native-manifest /srv/storage/Projects/SkyCRPG/session-p2-20261026/dist-camera/SkyCRPG_Camera.manifest.json --providers /srv/storage/Projects/SkyCRPG/session-20261026/extracted --allow-mismatch-for-dry-run --out /srv/storage/Projects/SkyCRPG/session-p4/logs/package/dry-camera
timeout 600 python3 -B -I tools/package.py verify-repro --module camera --commit c7333c7c13284ad9d228e6f15213bc8465bd6e19 --native-zip /srv/storage/Projects/SkyCRPG/session-p2-20261026/dist-camera/SkyCRPG_Camera-0.0.1-8a68139.zip --native-manifest /srv/storage/Projects/SkyCRPG/session-p2-20261026/dist-camera/SkyCRPG_Camera.manifest.json --providers /srv/storage/Projects/SkyCRPG/session-20261026/extracted --allow-mismatch-for-dry-run --out /srv/storage/Projects/SkyCRPG/session-p4/logs/package/repro-camera
```

Archive listing and SHA-256 (full per-file hashes and manifest hashes are in each output directory):

`/srv/storage/Projects/SkyCRPG/session-p4/logs/package/dry-ui/SkyCRPG_UI-0.1.0-rc.1-c7333c7-DRY-RUN.zip`

SHA-256: `1fa7a7f58bb2327f384a22d4bb92139f63bbe46201e0a2aacd74da201db9928f`.

```text
   35562  Docs/SkyCRPG_UI/CommonLibSSE-NG-COPYING
    2095  Docs/SkyCRPG_UI/CommonLibSSE-NG-EXCEPTIONS.md
    1066  Docs/SkyCRPG_UI/LICENSE
    2015  Docs/SkyCRPG_UI/LICENSING.md
    1326  Docs/SkyCRPG_UI/THIRD_PARTY.md
    2328  Docs/SkyCRPG_UI/build-inputs.md
  378479  Interface/hudmenu.swf
   77181  Interface/inventorymenu.swf
   60420  Interface/skyui/inventorylists.swf
  962048  SKSE/Plugins/SkyCRPG_UI.dll
    1114  SKSE/Plugins/SkyCRPG_UI.ini
```

Repro **PASS**: archive, manifest and `SHA256SUMS` were byte-identical; `dry_run=true`.

`/srv/storage/Projects/SkyCRPG/session-p4/logs/package/dry-camera/SkyCRPG_Camera-0.1.0-rc.1-c7333c7-DRY-RUN.zip`

SHA-256: `de57a66d7db63830c501a665c8828ae8333bbf93537a41bf193168da824c3f64`.

```text
   35562  Docs/SkyCRPG_Camera/CommonLibSSE-NG-COPYING
    2095  Docs/SkyCRPG_Camera/CommonLibSSE-NG-EXCEPTIONS.md
    1066  Docs/SkyCRPG_Camera/LICENSE
    2015  Docs/SkyCRPG_Camera/LICENSING.md
    1326  Docs/SkyCRPG_Camera/THIRD_PARTY.md
    2091  Docs/SkyCRPG_Camera/build-inputs.md
    2648  MCM/Config/TrueDirectionalMovement/settings.ini
  841728  SKSE/Plugins/SkyCRPG_Camera.dll
     667  SKSE/Plugins/SkyCRPG_Camera.ini
     586  SKSE/Plugins/SmoothCam.json
     718  SKSE/Plugins/SmoothCamPreset0.json
```

Repro **PASS**: archive, manifest and `SHA256SUMS` were byte-identical; `dry_run=true`.

Reused SWF hashes (no new SWF compilation):

| Movie | SHA-256 |
| --- | --- |
| `hudmenu.swf` | `54937d9d48d9a5de4c757ca2ce874fff4a41ed72b39048ad2eabd98292a31628` |
| `inventorymenu.swf` | `2302b306cd3beb6e69a1962a76030f57f1c85a2fcea43ec9938c818e8fb1b3ba` |
| `skyui/inventorylists.swf` | `34e32f04521a28acf16b00e263a479ca233a0814e986a8e0d91bcc46b4024518` |

## Draft fresh installation, update and removal

These are DYL-174 handoff instructions, **not yet verified in MO2/Vortex or in
game**. Use the final eligible archives and sidecars, never the dry ZIPs above.
In MO2 install each ZIP as a separate mod with its contents at Data root; in
Vortex install each as a separate mod and set the specified file-conflict rules.
UI's three loose movies win over SkyUI. Camera's configuration layer wins only
after reconciling existing settings. Provider ESP/ESL load order follows those
providers' instructions; neither SkyCRPG DLL requires a SkyCRPG ESP.

For every recipe, close Skyrim first, make a new isolated mod-manager profile,
use a copied disposable save, preserve baseline settings/winners and record
package/manifest checksums. Check `candidate_eligible=true`, matching full source
commits, the exact selected runtime/versions/build lines and complete provider
files before launch. Install SKSE 2.3.1's loader/runtime in the game root through
its own external instructions and scripts under Data; do not install root SKSE
binaries from a SkyCRPG ZIP. Address Library 13 must supply
`SKSE/Plugins/versionlib-1-7-104-0.bin`. Do not mix SE/AE16 and Line-17 map DLLs.

1. **Fresh Camera-only:** install shared SKSE/Address Library/SkyUI once, MCM
   Helper 1.6.3, TDM 2.3.1 and SmoothCam 1.8.1 (AE 1.7.99+ DLL), then only the
   Camera candidate. Verify its DLL/INI, active JSON, preset slot 0 and TDM MCM
   settings destinations. For an existing SmoothCam JSON or occupied preset
   slot, back it up and merge only Camera-owned keys/slot; keep unrelated keys.
   Merge the committed TDM fragment's keys into the actual current settings
   while preserving other values. The premerged archive settings are for clean
   pinned defaults; do not simply replace a user's existing full settings.
   Confirm movement-only TDM, yaw adjustment and lock activation/POV switching
   disabled, F8 scan66 available. Do not install SkyCRPG UI or its movies/map trio.
   Launch/restart only in the orchestrator's scheduled route and verify ordinary
   SkyUI/native HUD, Camera controls and first-person fallback.
2. **Fresh UI-only:** install shared prerequisites once and the pinned Infinity
   UI NG → LMU → DEM Common + Line-17 stack externally, then only the UI
   candidate. Ensure its three movies win over SkyUI and no other inventory/HUD
   replacer wins those paths. Reconcile DEM settings in its external config:
   upper-right `uAnchor=1`, `fScale=0.5`, `bShowOnGameStart=1`, and
   `[Compass] bCompassRing=0`; LMU `bLocalMapBorder=0`, no optional border.
   The audited `stage-ui-p3` differed from acquired DEM defaults only in
   `bCompassRing=0`; LMU matched provider defaults. These external provider INIs
   are not added to the SkyCRPG archive. Verify all provider SWFs/art before
   enabling the DLLs; partial installation can terminate inside the provider.
   Do not install Camera/SmoothCam/TDM/MCM Helper merely for UI. Verify normal
   Skyrim camera, grid, HUD, hotbar, follower and usable navigation after restart.
3. **Fresh combined:** install the union of the above prerequisites once and
   keep the two SkyCRPG archives as separate mod layers. Their file ownership
   must remain disjoint. Apply each layer's explicit provider override/merge
   rules, then verify both features and independently toggle/remove one layer
   at a time in the bounded route below. A second camera/HUD/compass owner is
   outside this profile, not an automatic winning-file choice.

**Update Camera-only:** close the game; preserve the old archive, manifest,
source/package hashes and current INI/provider settings. Verify the new archive
before replacing its layer. Compare old installed versus current keys; update
only unchanged owned keys, reconcile later user changes and keep unrelated
SmoothCam/TDM settings. Reset only the Camera INI to shipped defaults if desired,
with a backup. Restart the copied save and check controls/F6/F8/ordinary UI.

**Update UI-only:** close the game; preserve the prior archive/manifest/INI and
movie winners; verify the new native and movie provenance belongs to one source
commit. Replace the UI layer and all three movies together, reconcile INI edits,
then restart and check inventory/native menus, HUD, compass and ordinary camera.
`[HUD] bEnabled=0` alone is not removal of the inventory movies or DLL. Do not
update external providers or restore map settings by deleting provider files.

**Update combined:** apply those two updates independently, preserving each
layer and one shared external prerequisite set. Use the intended common source
commit for this phase's final pair. Inspect file winners and restart; test both
features plus removal of each layer without disabling the other.

**Remove/disable Camera-only:** quit fully, disable the Camera mod layer,
including its DLL/INI, active JSON/preset and TDM merged override. Reveal the
previous mod-manager layer or restore only owned values still equal to the
installed overrides; keep and reconcile edited keys. The native/provider camera
configuration returns according to those restored layers; if SmoothCam remains,
it still owns ordinary third person with its prior settings. TDM returns to its
prior settings. UI, if present, remains installed. F8 is a provider emergency
toggle and F6 a precision fallback, neither a DLL unload. Restart on the copied
save; check normal camera, movement, menus and any UI.

**Remove/disable UI-only:** quit fully; disable the entire UI layer (DLL, INI,
all three movies and its namespaced Docs). Reveal SkyUI's original movie winners
and retain external minimap/prerequisites. Restore any manually applied map
config deltas only if unchanged since installation. On restart native/SkyUI
inventory and HUD return, with native compass ownership according to the
remaining provider configuration; check compass/navigation explicitly. Camera,
if present, remains installed. Do not remove an external provider as if it were
owned by UI; any provider removal needs its own complete profile and restart.

**Remove combined:** perform each layer's procedure independently, retain
shared prerequisites while either layer still uses them, then verify native
menus/navigation/movement on the copied save. Compare original settings/backups
and retain conflicts for reconciliation. None of these procedures certifies
universal mid-save uninstall safety or a live DLL unload.

## Acceptance ledger and runtime route (all future game rows NOT_RUN)

| Criterion | Status / evidence |
| --- | --- |
| Exact expected contents; no junk/personal text/unapproved collision/module coupling | PASS for synthetic checks and historical dry outputs; final same-commit artifact inspection NOT_RUN |
| Archive/manifest/requirements/version/hash output | PASS for both dry outputs; source mismatches are explicitly recorded, not candidate passes |
| Same-input assembly reproduction | PASS for synthetic UI and real historical UI/Camera archive, manifest and checksum bytes; native/SWF rebuild reproducibility NOT_RUN |
| Fresh Camera-only, UI-only, combined installs using written instructions | NOT_RUN; final native builds/assets required |
| Update/independent disable/removal/restart | NOT_RUN; draft is not owner acceptance |
| Final DYL-171 ticket acceptance | NOT_RUN; runtime rows and integration work remain |
| Public distribution/third-party rights clearance | BLOCKED pending DYL-174; no publication requested or authorized |

The orchestrator must register each game job before launching: purpose, exact
profile/command, one PID/run handle, logs, 20-minute route deadline, expected
completion and cleanup. Use copied saves and fresh profiles; no launch is
performed by this lane. Record Steam executable hash/runtime, SKSE, dependency
archive/DLL versions/hashes, CommonLib 7.2.0, actual host, source `<head SHA>`,
`<CI run>`, package/DLL/SWF hashes and final resolved file winners with results.
State which device the owner actually used. On this Proton host turn the
controller off for keyboard/mouse, then do a separate controller pass.

| Route (repeat input steps separately on keyboard/mouse and controller) | Status |
| --- | --- |
| Fresh Camera-only: install through selected manager, confirm UI DLL/movies absent; load copied save, walk/zoom/recenter, use F6/native POV for aim, F8 disable/return, native inventory and movement usable | NOT_RUN, both device passes |
| Fresh UI-only: confirm Camera/providers absent; inspect/equip/consume/favorite/drop quantity then cancel, record numeric count/weight, use hotbar/native Favorites, recruit/dismiss follower; ordinary camera works | NOT_RUN, both device passes |
| UI navigation: exterior/interior/load, minimap replaces compass; provider hidden/whole-stack absent restores compass. Refuse corrupt partial-provider profile before launch | NOT_RUN, both device passes |
| Fresh combined: union of two separate layers, both controls/features, dialogue and cell/load transitions; record exactly one provider owner/file winner | NOT_RUN, both device passes |
| Update Camera, UI, then combined from a retained prior candidate, with an edited INI/unrelated provider key; edits preserved or reported and no cross-module change | NOT_RUN |
| Combined minus Camera, restart: UI/grid/HUD/map remain usable; restored provider camera and ordinary movement; prior SmoothCam/TDM keys preserved | NOT_RUN, both device passes |
| Combined minus UI, restart: Camera remains usable, SkyUI/native inventory/HUD/compass restored; no orphan UI movie winner or navigation loss | NOT_RUN, both device passes |
| Remove both, restart copied save: ordinary camera/UI/navigation/input; compare owned-key restoration, original configs/saves and clean exit | NOT_RUN, both device passes |
| Explicit Camera fragment difference: `bHeadtrackSpine=0` versus historical combined stage's 1; check headtracking/movement with fragment applied | NOT_RUN |

## Integration handoff, limitations and owner decisions

`session-p4/integration/package.patch` (against `c7333c7`) adds only SWF builder
`source_commit` provenance plus a refusal for dirty/untracked includes/builder.
`package.md` explains it. `git apply --check` and patched-builder AST syntax
inspection PASS (`integration-check.log`; no builder/compiler executed). No module file was edited;
no engine API was introduced, so there is no unverified CommonLib method call.
The patch's actual Java/SWF build is NOT_RUN. Apply/commit it with all DYL-170
changes, build both native targets at that same commit, rebuild all three UI
movies from clean includes, then run strict assemble/repro without mismatch
flags. The orchestrator owns commits/CI/gameplay; this lane leaves a working diff.

No new product/scope/provider decision is needed for the tooling. DYL-174 must
resolve rights/notices for linked CommonLib, generated SkyUI derivative movies
and included TDM configuration before distribution; retain corresponding-source
and font/art exclusions. The owner must still supply runtime/manager/device
results and final playtest acceptance. The headtracking config discrepancy is
an engineering finding for runtime review, not an owner waiver or hidden change
to the committed fragment. Do not mark DYL-171 Done from this offline evidence.

Post-MVP backlog candidates: automated compare-and-restore mod-manager settings
migration; cross-host native/SWF build reproducibility. No enhancement was
implemented in the runtime modules.

## Orchestrator review (2026-10-28)

- The SWF provenance request (`session-p4/integration/package.patch`) is applied on `phase4/dyl-170-ui`, which owns `build_scaleform.py`. One change from the request: a dirty Scaleform/builder tree no longer aborts the build, because workers cannot commit and still need local compile checks. `build.json` now records `source_dirty`, and `assemble` refuses any SWF set whose `source_dirty` is not `false` (dry runs record a mismatch instead). Tests cover `source_dirty` true and missing.
- The UI manifest now lists the minimap trio under `optional`, not `requires`. A missing provider leaves the compass in place (DYL-166 evidence), so it must not read as a hard dependency.
- Rights: the Camera archive ships a merged copy of TDM's `MCM/Config/TrueDirectionalMovement/settings.ini` and SmoothCam config files. Like the SkyUI-derived SWFs, these wait on the rights research and the DYL-174 decision. The archives stay `internal-candidate`.
- Reran `tools/tests` (21 tests) and `tests/test_profile_tool.py` (22 tests): PASS.
