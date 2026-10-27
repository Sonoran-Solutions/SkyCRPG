# Runtime, dependency and evidence baseline

DYL-154 baseline, updated with DYL-155 evidence on 2026-10-26. **One tested feasibility environment**, with scoped working behaviors and explicit failures below. This is not a finished MVP support certification. Other runtimes, operating systems, Proton builds and untested mod combinations are unsupported by Phase 1 evidence. [Acceptance matrix](acceptance-matrix.md) owns requirement status; [dependency lock](dependency-lock.md) owns exact upstream files, source commits and permissions.

## Host and runtime

| Field | Only tested value |
| --- | --- |
| Game | Steam `SkyrimSE.exe`1.7.104.0, build24914197; base SE + three DLCs + four free CC files, no Anniversary Upgrade; SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` |
| SKSE | 2.3.1 Steam; archive SHA-256 `7bad616ed360823a027f8828801d91e3a4aa2eacd952023af7f3e31adb2ae250`; runtime DLL `d5158f8c61a2257138692940ed27048c40d6ff62676e64791bdc83d4549fb94e` |
| Address Library | 13; installed `versionlib-1-7-104-0.bin`; required by feature providers, not the plain skeletons |
| Host | Ubuntu26.04.1, kernel7.0.0-38, Wayland/XWayland |
| Compatibility layer | Proton Experimental11 `experimental-11.0-20261014-x86_64`; DXVKv3.1.1-97, Steam Linux Runtime4 |
| Graphics | RTX3080 10GB, NVIDIA595.99.02; recorded1920x1080 windowed/default game preset. No720p/scaled-text acceptance inferred. |
| Input | Keyboard/mouse available; controller present and owner-tested for corrected grid/navigation/action route. Hardware model and complete controller/baseline matrix not recorded/verified. |
| Fixture | Copied vanilla1.7.104 Helgen cave/exterior saves; exact source/archive/ESS hashes in [session](../evidence/p1/session-2026-10-26.md#replacement-fixtures). No original personal save modifications. |

| Environment/profile | Status and exact scope |
| --- | --- |
| Baseline, host above | **PASS** rewritten profile real-host deploy/load/undeploy and owner cave/exterior baseline route; full baseline controller coverage **NOT_RUN**. Old heavily modded2021 save **FAIL** to load; replaced fixture separately passed. |
| Camera-only, host above | **PASS** qualified exploration/melee, first-person precision, activation, ten F8 pairs and usable transition; top-down ranged aim/pitch persistence **FAIL**; bounded Nordic obstruction **PASS** in gate run100826; comprehensive obstruction coverage **NOT_RUN**. |
| UI-only with ordinary camera, host above | **PASS** live small grid/native weapon action invariants and final controller correction; map inside/outside/hidden-map compass fallback PASS on recorded earlier UI candidate. Whole-provider absence **PASS** in gate run101713; full instance/modal/layout matrix **NOT_RUN**. |
| Combined, host above | **PASS** startup/all8 DLL loads, clean exit and owner checkpoint075556 with captures; pitch reset **FAIL**. This is a short smoke, not complete combined acceptance. |
| Native Windows, other Linux/Proton builds or GPUs | **NOT_RUN**; no gameplay support claim. Windows CI builds the Windows x64 ABI only. |
| Other Steam versions, GOG, other mod combinations | **NOT_RUN**; no support claim. Never generalize author compatibility lists or silently replace locked packages. |
| VR, consoles, Legendary Edition | **NOT_RUN**, outside current MVP support scope. |

## Exact external packages and inspected source

All archives remain external. Hashes below are archive SHA-256, not DLL hashes; deployed member hashes are in [tested-artifacts.json](../evidence/p1/tested-artifacts.json). Source pins identify inspected source, not proof a downloaded binary was compiled from that source. Full filenames/Nexus file IDs and permission notes remain in [dependency-lock](dependency-lock.md).

| Component | Version / selected payload | Archive SHA-256 | Source or provenance |
| --- | --- | --- | --- |
| Address Library | 13 /1.7.104 database | `be0c7c07fb63fd1c1403690d4797c7a8fdf8799118acceb605f285d7b0ee055f` | Nexus32444 file795954 |
| SmoothCam | 1.8.1 / AE1.7.99+ | `efed5592257aa67d92beb48f9d6f8cacb00c1784491cfb9c748bec9a9ec5d07b` | `e0afd09aff26e5849dac461252e5bffbd5b69d62` |
| TDM | 2.3.1 / camera yaw and lock disabled | `0593379072997689f3d1db2302b0bf060c0ddc116743ab9585feaf328bec0b0f` | `57b913ae17fed857c9e281799174898cd66a3e82` |
| MCM Helper | 1.6.3 / SkyrimSE line | `505af9c27062febd497be09b661d5df82e013ae4a205ffbec99698adf4503f52` | Nexus53000 file795510; no source reuse selected |
| SkyUI Community | 6.11 / Nexus ESP/BSA bases | `4eb0c9fc5241e5c3dd2fd773ee8a89627c924bf4445cf70373d3a4c7c4393ff1` | `3d537a366f1a45c14a7012ea0aa00be18108f073` |
| Dragon's Eye settings fork | 1.7.5 / Common + Line-17 | `27d27d380e6d9e885e4461006ff25e9f79d45d12985c88d77f8cdba47e1c4af8` | `7c3ce18f54bd6c8dff3239db9522cb75d822a31f` |
| Infinity UI NG | 2.1.3 / Common + Line-17 | `c7a3b4d11f485a52f8fd49a2df2a0ffc7fa6cd0a953795518f3861deab2b8c83` | `f20d7e468b16ae8b1bff8a97961bb297e81df020` |
| Local Map Upgrade settings fork | 1.3.4 / Common + Line-17 | `403343518bef794b2b781d2847f3468c3ebb7fd5c5ed628b9f322b9d39a0aed4` | `8a9cf24c30860e4ce5fb49b45ba62a8cc0badcb7` |
| Party Sheet — **not installed/tested** | 3.7 optional inspection only | `f980e34cd2f0a32abe0b5150c0d673017d53399df6a7dd5240c96f53ed21f83d` | Nexus167538 file818098; no verified public actor-data API |

Camera requires SmoothCam/TDM/MCM Helper plus common SKSE/Address Library/SkyUI; UI requires the same common SKSE/Address Library/SkyUI prerequisites and the map trio for the minimap, without Camera providers. CommonLibSSE-NG7.2.0 source`7a60f4de794095d7b0f8928d1b930a52e9a7da83` is a **conditional future build dependency**, not a current mandatory runtime/provider. No TrueHUD, Nemesis, optional map settings frameworks or item-marker add-on is part of the tested subset.

## Source and artifact identity

| Material | Exact source / hash |
| --- | --- |
| Camera PR reviewed head / merge | `1f272ca80e4f207a5a13a97506b44a5fc8ddb1fe` / `a3edd163bb31491427b81eb860f7d440a4379ce8` |
| UI PR reviewed head / merge (P1-05 base) | `2f9d335f4be7f70c5b4116cd5d38f9aa0fbb7340` / `87d91626815dca145c6a876664f1ac7d0ffeafdf` |
| Loaded Camera skeleton source / native ZIP | `cc8a4dcde7b817d392bc66c9d2dc93cfb1beac62` / `e757e6b31afbd89afe9a2c0ebb2d4a103be2414674879aca19cba953e5679a45` |
| Loaded UI skeleton source / native ZIP | `d1f6193681df687860681881de7425a5e00ebbd1` / `1e266bdefb1730095b33f7d19c784d2fbdbad965cc54ec59990c71a84716fcea` |
| Final generated UI patch source | `1be22bbbb0b0519e0b3c876ce4929a395541161d` |
| Final inventorymenu.swf | `5b474c27fd84f1e82022bde4b79184d14b1334bf4f5f8ec8739d8e4b47215f80` |
| Final skyui/inventorylists.swf | `7f0ce7fa3eec3845fc9cfce3776beba7f89cf2e6b9fe21d2851886deca852cd8` |
| Final hudmenu.swf | `c42e7a26bb276bc8ee0a2f877c2248e8bb8bcd73bee9798119ae3ebdddde3e38` |
| Camera resolved profile (166 files) | `1f5535e69c40975f7fd85c7a239bacced75af8b9e95d1a83d199be777327798c` |
| Final UI resolved profile (173 files) | `1e25367e992e029be27738c707a5b6424fe5bbd5e81d1d7ccab944b09fc7af5e` |
| Combined resolved profile (209 files) | `8a31a81a18b196cf51431f570d609f8b0cf0375f967299ac75a22488432ef307` |

Profile digest = SHA-256 of UTF-8 sorted compact JSON **file-hash dictionary**, no newline. It is not an archive hash and must not be confused with earlier external manifest digest formats. [Native inputs](../evidence/p1/native-package-inputs.json) pin each DLL/INI, imports and declared requirements. [Installed file maps](../evidence/p1/tested-artifacts.json) pin every provider/config/SWF winner and prior failed UI candidates. The reviewed source head is not necessarily the source commit embedded in an unchanged loaded skeleton.

## Three classifications and evidence limits

1. **Tested and working:** only the scoped PASS rows above, on these exact artifacts. Owner observations, timestamped captures and runtime logs in [session](../evidence/p1/session-2026-10-26.md), [camera](../evidence/p1/camera-spike.md) and [UI](../evidence/p1/ui-spike.md) are distinct from static/source checks.
2. **Implemented or selected but not verified:** comprehensive obstruction coverage, planned pitch/aim adapter, final native instance lifecycle, Party Sheet integration, comprehensive eight-slot hotbar coverage, automatic compass readiness/replacement, incompatible-provider runtime recovery, full controller/modal/layout and release removal/performance. Several are selected only and **not implemented**.
3. **Known failing or explicitly deferred:** top-down ranged aim FAIL/DYL-158; load pitch FAIL/owner-deferred DYL-159; 19+ scrolling source concern/DYL-164 (gameplay NOT_RUN); visible duplicate compass/U5 incomplete. First camera attempt lost the desktop under systemd-oomd memory pressure; root contributor unresolved, later bounded retry passed. No provider blame or broad stability PASS follows.

Exact-head native Windows CI [Camera40345704901](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40345704901) and [UI40346076477](https://github.com/Sonoran-Solutions/SkyCRPG/actions/runs/40346076477) passed skeleton build/package checks. They do not run the external preset schema check, Scaleform compile/source tests, or gameplay. Local UI compile/source/navigation/bytecode records are separately committed under [ui-spike-provenance](../evidence/p1/ui-spike-provenance/build.json). Neither is a substitute for unrun runtime fixtures.


## DYL-155 targeted gate evidence

The [gate](../evidence/p1/gate.md) reuses the provider-present profiles and adds only two missing proofs on the same host. Camera run20261026T100826 uses the unchanged 166-file Camera artifact above: owner-observed corridor/ceiling/doorway/stair/open-room recovery PASS. UI-only run20261026T101713 omits all 38 map-provider paths, with 134 retained game files plus profile INI; artifact `cdccba7819106c8ce8e065f363d88279ee9b65cbc7ce55f46b2278345fef98ea` ([exact manifest](../evidence/p1/gate-20261026/ui-absence-artifact.json)). Clean startup, native compass/navigation and inventory/Magic/journal usability PASS. Both exit0 with no load errors or leftovers; original configuration and 185 pre-existing save/co-save hashes verified restored/unchanged. Incomplete-provider preflight rejects a missing Minimap.swf before launch; this is not incompatible-binary runtime recovery. No provider/runtime upgrade, native build or generated-asset change occurred. PR5 merge/base is `ea054c78b42ccf051598d34af4f795dd7e18319f`; loaded module/patch source pins remain those above.
