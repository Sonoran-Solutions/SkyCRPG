# Dependency selection record — DYL-151

Updated 2026-10-26. **Recommendation lock plus acquired package hashes; gameplay stack not yet accepted.**
[ADR-001](../decisions/ADR-001-stack.md) is proposed; P1-03/P1-04 provide feature evidence and P1-05 freezes it.

## Evidence levels

| Level | Meaning / current result |
| --- | --- |
| Upstream documentation/source | **PASS (desk inspection only)** for the linked versions/interfaces below; runtime claims remain the authors' claims. |
| Historical pre-install snapshot | DYL-150 snapshot `cd49b3c`: Steam **1.7.104.0**, Ubuntu 26.04.1, Proton Experimental `experimental-11.0-20261014`; **SKSE/Address Library/providers absent**. |
| Current load evidence | Rewritten profile baseline, UI-only providers and camera-only providers passed bounded startup on2026-10-26. See session evidence/logs; startup does not prove gameplay. |
| SkyCRPG feature behavior | UI grid, native equip/unequip, minimap and compass fallback **PASS (owner-observed)** on2026-10-26; corrected controller grid navigation **PASS (owner-observed)** after two failed candidates. Camera owner route recovered after desktop OOM: melee/fallback usable, top-down ranged aiming and load-transition pitch persistence **FAIL**. Combined startup/owner checkpoint **PASS**. See session evidence. |

Executable SHA-256 from [DYL-150](https://github.com/Sonoran-Solutions/SkyCRPG/blob/cd49b3cfc73278350342db4ae7a27c3b5e6a9644/docs/evidence/p1/environment.md):
`846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` (Steam build 24914197).
No native Windows playtest or other storefront/runtime is selected. Controller hardware is available through owner playtesting as of the UI route on2026-10-26.

## Selected candidates and exact upstream files

Nexus file IDs below come from the selected file's manual-download/version-history links, not inferred from
page version headings. The initial audit acquired only SmoothCam. The orchestrator acquired the selected Nexus files on 2026-10-26; the archive hashes and selected runtime paths below supersede that acquisition gap. Runtime compatibility still needs separate evidence.
A source tag is not proof that a Nexus binary was built from that source. Verify manifest, DLL file version,
build line, notices and SHA-256 when acquired; never substitute a newer file silently.

| Component / package ownership | Exact release/file and primary link | Runtime claim / dependencies | Configuration, conflicts and remaining pin |
| --- | --- | --- | --- |
| SKSE — shared external prerequisite | **2.3.1 Steam**, Nexus 30379 file **795992**, 930 KB; [official runtime selection](https://skse.silverlock.org/), [file](https://www.nexusmods.com/skyrimspecialedition/mods/30379?tab=files&file_id=795992) | Steam 1.7.104.0. GOG package is different. | DYL-150 owns acquisition/install/loader logs. Plain skeletons need no Address Library/CommonLib. |
| Address Library — external feature prerequisite | **13**, All in One (1.7.104.0), Nexus 32444 file **795954**, 6.3 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files&file_id=795954) | Lists all runtimes through 1.7.104.0. | Required 1.7 database is `versionlib-1-7-104-0.bin`; format 5 reader needed. Version 12 only reaches 1.7.99. |
| SmoothCam — Camera external | **1.8.1**, Nexus 41252 file **813109**; GitHub **Beta1.8.1 / SmoothCam.zip**, 46,528,681 bytes; [release](https://github.com/mwilsnd/SkyrimSE-SmoothCam/releases/tag/Beta1.8.1) | FOMOD AE **1.7.99+**, SKSE + Address Library; SkyUI for MCM. | Pick `00 Data/AE/SmoothCam.dll`, not AE-Pre1799. Source `e0afd09aff26e5849dac461252e5bffbd5b69d62`. Sole writer, original preset; no second camera mod. Hash verified below. |
| TDM — Camera external | **2.3.1**, Nexus 51614 file **798770**, 5.0 MB; [files](https://www.nexusmods.com/skyrimspecialedition/mods/51614?tab=files&version_history=798770) | 2.3.0 adds 1.7.99+; 2.3.1 fixes gamepad address. SKSE/Address Library; SkyUI + MCM Helper for MCM. | Source inspected at `57b913ae17fed857c9e281799174898cd66a3e82`. Disable movement camera yaw adjustment/target lock/reticle in initial preset. No TrueHUD/Nemesis needed for selected minimal feature subset; disable optional headtracking/leaning requiring behavior work. |
| SkyUI Community — external to both packages; UI patch base | **6.11**, Nexus 12604 file **749043**, 2.6 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/12604?tab=files&version_history=749043), [source release](https://github.com/doodlum/SkyUI-Community/releases/tag/v6.11) | SKSE. File description names 1.5.97, 1.6.1170, GOG 1.6.1179; does not explicitly certify 1.7.104. Inventory Reforged author reports 6.11 with SKSE 2.3.1; infer candidate only. | Tag **v6.11 = `3d537a366f1a45c14a7012ea0aa00be18108f073`**. Page header says 6.9 while release/changelog say 6.11. Confirm installed manifest before patching. Player InventoryMenu winner must be our one patch, no Inventory Reforged/Grid Inventory overlap. |
| MCM Helper — Camera configuration prerequisite | **1.6.3**, Nexus 53000 file **795510**, 7.8 MB; [files](https://www.nexusmods.com/skyrimspecialedition/mods/53000?tab=files&version_history=795510) | Current FOMOD names Steam 1.7.99+ / VR and requires SkyUI 6+. | External; pick Steam line. Needed for TDM MCM, not the two empty DLLs or UI-only package. Source/license archive not acquired; no source reuse proposed. |
| Party Sheet — UI external | **3.7**, Nexus 167538 file **818098**, 19.7 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/167538?tab=files&version_history=818098) | Author names 1.5.97/1.6.640/1.6.1170/**1.7.104**; SKSE + Address Library. | External INI/preset only, one vanilla follower. July source archive is older/reference-only; no verified data API or source license. Portrait pack excluded. |
| Dragon's Eye settings fork — UI external | **1.7.5**, Nexus 189534 file **825711**, 14.8 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/189534?tab=files&version_history=825711), [tag](https://github.com/ApocryphaRealm/DragonsEyeMinimap/tree/v1.7.5) | 1.7.99+ build; SKSE, Address Library, matching Infinity UI/LMU. Source tag **`7c3ce18f54bd6c8dff3239db9522cb75d822a31f`**. | Upper-right anchor/scale/shape; stock art first. `[Compass] bCompassRing=0` for UI-managed native compass recovery. Optional settings framework omitted. Source changelog marks 1.7.5 untested despite broad page test claims. |
| Infinity UI NG — UI external | **2.1.3**, Nexus **192919** file **824500**, 10.7 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/192919?tab=files&version_history=824500) | Choose **Line-17** DLL; SKSE 2.3.x + Address Library 13. | Tag v2.1.3 peels to **`f20d7e468b16ae8b1bff8a97961bb297e81df020`**; later documentation `43daa2e55607c6805a714085634184741575f9ca` reports 1.7.104 verification. Replaces upstream 2.0.3, same `InfinityUI.dll`; never install both. |
| LMU settings fork — UI external | **1.3.4**, Nexus **189625** file **825693**, 14.7 MB; [file](https://www.nexusmods.com/skyrimspecialedition/mods/189625?tab=files&version_history=825693) | Choose **1.7 build**; SKSE, Address Library, Infinity UI NG 1.7 line. | Tag v1.3.4 peels to **`8a9cf24c30860e4ce5fb49b45ba62a8cc0badcb7`**. Changelog marks it untested; 1.3.2 added 1.7 line. Replaces upstream 3.1.0, same `LocalMapUpgrade.dll`; item-marker add-on excluded. |
| CommonLib — conditional build dependency only | alandtse/CommonLibSSE-NG **7.2.0**, **`7a60f4de794095d7b0f8928d1b930a52e9a7da83`**; [release](https://github.com/alandtse/CommonLibSSE-NG/releases/tag/v7.2.0) | 1.7 layouts / format-5 library path, as pinned by chosen map-provider builds. | Not needed by DYL-150's plain skeletons. MSVC Windows x64 runner preferred; CMake/vcpkg pins owned by implementing adapter ticket. Do not substitute CharmedBaryon 3.7.0. |

Hotbar uses original presentation with the pinned native/SkyUI interfaces; **no separate hotbar provider is required**.
The display grid is original layout work over the selected SkyUI entries/actions, not an acquired grid dependency.
See session evidence for the limited Proton owner playtest results; no author statement establishes compatibility.

## Alternative/reference pins

| Candidate | Audited version/source and disposition |
| --- | --- |
| Grid Inventory | `c0d8718b6ea983d8161548192f82d850aa6d0518`; no GitHub release assets. Rejected for native-weight MVP. Source GPL-3.0 + EXCEPTIONS.txt; packaged meshes/icon pak/ESP are outside source grant. |
| Inventory Reforged | 1.5.0 normal release, Nexus 195826 file **824645**, 2.3 MB, 2026-10-23. Reference only; Debug file is a different 1.9 MB diagnostic build. Credited modification permission does not establish a uniform grid or erase SkyUI asset rights. |
| Glasswork Favorites | 1.0.6 regular file **820228**, 854 KB; **1.7 Test file 820229**, 856 KB, Nexus 194052. Reference-only alternative. Source/API/license archive uninspected; don't treat a mention of a mod API as a stable integration contract. |
| Upstream map providers | Infinity UI 2.0.3 (Nexus 74483 file 556134), LMU 3.1.0 (129756 file 571814), upstream DEM source `3ddc378be110830c4d00759a38153a4f97145b25`: reference/older-runtime alternatives only. Not the selected 1.7 trio. |
| Menu frameworks | Minimap/LMU settings are INI-only in initial profile. AMF and SKSE Menu Framework are optional, **not locked/required**. Fork source vendors SMF header 3.13 and detects API exports; do not infer the settings menu is needed for map rendering. |

## Verified SmoothCam archive hashes

Originally inspected outside the repository. The selected AE DLL subsequently passed camera-only startup on2026-10-26; owner route confirms melee/fallback but top-down ranged aiming fails. Source release metadata and downloaded bytes agree.

| Material | SHA-256 |
| --- | --- |
| `SmoothCam.zip` (Beta1.8.1) | `efed5592257aa67d92beb48f9d6f8cacb00c1784491cfb9c748bec9a9ec5d07b` |
| `Release_Package/00 Data/AE/SmoothCam.dll` (1.7.99+) | `b20bf8a276bf76fbe35277dd9b25c64c57d17bfdcc20daa74a5f941c90f563da` |
| `Release_Package/00 Data/AE-Pre1799/SmoothCam.dll` (wrong line for this target) | `d9dde556c522f7803f2be53d5ba0d811201174ed8c034f37235cd5441d9c3304` |
| `Release_Package/Fomod/ModuleConfig.xml` | `30afd94f4e6e2ea7f6c5c255553479bc65866502bf06b68dd03a62bdac7a68a7` |

## Permission/distribution lock

| Material | Permission scope / required action before incorporation |
| --- | --- |
| SmoothCam | External install/configuration. No general project license located in inspected tree/archive; Nexus modification/upload restricted. API header explicitly permits API consumers to copy that header; preserve context and notices, no broad fork grant inferred. |
| TDM | Source COPYING + EXCEPTIONS grants GPL-3.0-or-later with modding/linking exceptions and corresponding-source requirements. Nexus archive modification/upload is restrictive. Leave binary/art external; resolve exact material if ever reusing source. |
| SkyUI patch | Nexus author notes explicitly allow credited skins/patches requiring original. No blanket MIT license inferred from disclaimer or public repository. Do not modify icon files without separate authors' permission or redistribute their packs/fonts. Ship original art and required-dependency patch only. |
| Party Sheet | Source archive described as reference; modification requires permission, no re-upload. Assets have separate credited-use terms, but portrait/icon provenance not audited. External install + original settings/silhouette only; native bounded widget is alternative. |
| DEM/Infinity NG/LMU settings forks | Current LICENSE/NOTICE say GPL-3.0-or-later; keep upstream MIT and other third-party notices. Packaged art/font permissions still need actual archive check. LMU source discusses third-party frame artwork beyond its sparse notices: leave optional frame disabled, no extraction/reuse. |
| CommonLib 7.2.0 | COPYING and EXCEPTIONS.md: GPL-3.0 with modding/linking exceptions, corresponding source obligations. This does not select SkyCRPG's original-work license. Review linked derivative's distribution terms before shipping it. |
| Inventory Reforged / Glasswork | Nexus permissions allow credited modifications; no blanket source/asset grant for all embedded third-party material is inferred. Reference only in recommended route. |

## Install order and ownership

1. DYL-150 verifies vanilla baseline and matched SKSE 2.3.1. Keep its plain independent DLL loads free of providers.
2. Add Address Library 13; verify expected database filename. Add SkyUI 6.11 for provider configuration/UI patch base.
3. Camera-only: TDM 2.3.1, MCM Helper 1.6.3, SmoothCam 1.8.1 correct AE DLL, original Camera configuration.
4. UI-only: Infinity UI NG 2.1.3 → LMU fork 1.3.4 → DEM fork 1.7.5, each 1.7 DLL; Party Sheet 3.7; original UI patch/config. Optional Party Sheet is inspected only and was not deployed.
5. Loose player inventory patch wins over SkyUI only for its owned screen. No Inventory Reforged/Grid Inventory or second HUD/compass owner in the first profile. Respect actual archive path case under Linux staging.
6. Verify required SWF files before enabling the minimap DLL: the stock fork can terminate on incomplete installation. Full provider absence is a different fallback case from corrupt partial installation.
7. Combined profile contains union of dependencies, with disjoint Camera/UI configs and no required cross-module DLL service. Record exact winning files/INI deltas and hashes in spike evidence.

Do not promote this table to tested until actual archives and independent feature proofs are attached. A minimal
DLL load does not establish any of these provider combinations. Remaining checks are listed in REUSE_RESEARCH.

## P1-03/P1-04 acquired packages (2026-10-26)

Archives remain outside Git. Each exact file was downloaded through the authenticated Nexus browser, hashed, and safely extracted outside the game/repository. A version in this table is an acquired package, not a gameplay PASS.

| Archive | SHA-256 |
| --- | --- |
| `Infinity UI 192919 2.1.3 2026-10-22T23-53Z PuQ2UKDH1.zip` | `c7a3b4d11f485a52f8fd49a2df2a0ffc7fa6cd0a953795518f3861deab2b8c83` |
| `Local Map Upgrade 189625 1.3.4 2026-10-25T07-00Z aChSzYJnN.zip` | `403343518bef794b2b781d2847f3468c3ebb7fd5c5ed628b9f322b9d39a0aed4` |
| `True Directional Movement 51614 2.3.1 2026-09-01T17-15Z PuQ2UKIMI.7z` | `0593379072997689f3d1db2302b0bf060c0ddc116743ab9585feaf328bec0b0f` |
| `Skyrim Party Sheet 167538 3.7 2026-10-11T10-24Z VMSnJr6UF.zip` | `f980e34cd2f0a32abe0b5150c0d673017d53399df6a7dd5240c96f53ed21f83d` |
| `Address Library All in One (1.7.104.0) v13 32444 13 2026-08-27T15-29Z Ae46W7Fw2.zip` | `be0c7c07fb63fd1c1403690d4797c7a8fdf8799118acceb605f285d7b0ee055f` |
| `Dragon's Eye Minimap SKSE Menu Framework 189534 1.7.5 2026-10-25T08-00Z Ks18n0khS.zip` | `27d27d380e6d9e885e4461006ff25e9f79d45d12985c88d77f8cdba47e1c4af8` |
| `MCM Helper 53000 1.6.3 2026-08-26T18-07Z kATsMWjPO.7z` | `505af9c27062febd497be09b661d5df82e013ae4a205ffbec99698adf4503f52` |
| `SkyUI-12604-6-11-1778020881.zip` | `4eb0c9fc5241e5c3dd2fd773ee8a89627c924bf4445cf70373d3a4c7c4393ff1` |

Selected payloads: Address Library `SKSE/Plugins/versionlib-1-7-104-0.bin`; SmoothCam `00 Data/AE/SmoothCam.dll`; MCM Helper `SkyrimSE` DLL + ESL-flagged ESP + BSA; each map provider `Common` assets + `Line-17` DLL. Never combine their SE/AE16 and Line-17 DLLs. The map trio's GPL/NOTICE files remain with external staging. Optional Party Sheet was acquired for interface inspection, not installed in the first UI test. No provider assets are committed or redistributed.

SkyUI Nexus 6.11 contains an ESP/BSA; its shipped inventory/HUD SWFs differ from the GitHub source-tree binaries. The UI builder verifies both source and actual Nexus-base hashes and generates the local candidate from the acquired BSA. See ui-spike.md on the DYL-153 branch. Dragon's Eye's page advertises 1.7.6 now, but file 825711 downloaded as the requested **1.7.5**; no version substitution was made.

The merged foundation's SKSE2.3.1 load-only evidence is real, and the rewritten profile tool passed baseline run `20261026T065153` on 2026-10-26. This supersedes the initial reconciliation row that had no loader logs. Feature and controller acceptance remain separate.

## Tested scope and exact local artifacts

Camera-only, UI-only and combined provider load checks passed on Steam1.7.104.0, SKSE2.3.1, Address Library13, Ubuntu26.04.1/Proton `experimental-11.0-20261014-x86_64`. Owner gameplay results and failures are in session evidence; this is not a wider runtime/platform certification. SKSE2.3.1 archive SHA-256 `7bad616ed360823a027f8828801d91e3a4aa2eacd952023af7f3e31adb2ae250`. [Canonical installed-file hashes](../evidence/p1/tested-artifacts.json) distinguish each tested artifact; [native build manifests](../evidence/p1/native-package-inputs.json) pin skeleton DLL source/archive hashes. Party Sheet3.7 was inspected only and never installed. No provider stack was silently upgraded.
