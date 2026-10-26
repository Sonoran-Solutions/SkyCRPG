# Phase 2 bounded implementation handoff

The owner explicitly accepted Phase 1 GO; PR6 merged as `887f1855cefbd51baa07862398826c16d4eaceba`, also the exact starting main for both branches. DYL-155 was Done, with no administrative discrepancy; both leaf tickets were unblocked. [Startup and contract pins](startup-gate.json).

Camera [PR7](https://github.com/Sonoran-Solutions/SkyCRPG/pull/7) and UI [PR8](https://github.com/Sonoran-Solutions/SkyCRPG/pull/8) remain separate and unmerged. Current exact head SHAs and final both-target CI URLs are recorded in each PR description after the last documentation commit. PR7 needs senior re-review of its shared CI correction; PR8 needs senior review. This is not Phase 2 completion or a public mod release.

## Bounded results

| Check | Result and evidence |
| --- | --- |
| Camera-only exploration, held pitch, yaw/zoom, F7, F6, movement/menu behavior | PASS owner checklist on tested Camera source `8a681390db5b36c2b6fb4fcb056c0abc0e085a35`. [Camera evidence](https://github.com/Sonoran-Solutions/SkyCRPG/blob/phase2/dyl-156-camera/docs/evidence/p2/camera-controls.md). |
| UI-only ordinary-camera resource layout and live update | PASS at 100%; stamina100%→53%, all three meters visible. [Before/after and native evidence](../core-hud.md). Individual health/magicka depletion NOT_RUN. |
| UI menu open/close | PASS owner explicitly confirmed no duplication. |
| Combined camera/HUD, F6 fallback/return, native prompts/menu focus, 115% readability | PASS owner checklist and [capture/logs](../core-hud.md). Exact union artifacts below. |
| CI selector regression | PASS six focused tests, including each module plus shared build/package inputs selecting both. Final-head Windows results live in PR descriptions/checks; no new runtime claim from those rebuilds. |
| Cleanup and preservation | PASS [final audit](final-host-verification.json): 187 original save/co-save hashes and four configuration paths unchanged; 46 original game inventory files unchanged with no additions/removals; no active profile/game/task processes. 26 private character backup files verified, outside Git. |

Initial Camera controller recognition FAIL passed on one unchanged relaunch; cause remains unknown. Initial UI resource-display routes FAIL are retained. Route2 logs proved incorrect parent-relative coordinates; conversion to Stage coordinates corrected visibility. Route1 callback-cause uncertainty remains explicit. Earlier compile/upload failures are historical and retained in module evidence.

## Exact combined candidates and independence

- Camera DLL SHA-256 `aedc9d53e7c7d85719fb600fe8b71129cc9aab1f67f92cffe10ea499f106d024`, source `8a681390db5b36c2b6fb4fcb056c0abc0e085a35`.
- UI DLL SHA-256 `29381c08436486fda835206dceebaac73b1902f7f13af9a3f8d25b070d6fe136`, native source `e49fc30aebb008377e9a010e96757444c0f684c6`.
- UI final HUD SWF SHA-256 `b4552e17f0806848bcbb0a31f09e934e3feffc5a405305ba5707beb6b3215c05`, AS2 source `2d8896c4e8e91189489e983d94beb3444b4a32ea`.
- Temporary 115% INI SHA-256 `4d25b8efb447df95eae207e6a80de12845a13e493740582806cd44e56f6c05b2`; removed after testing. Packaged100% default unchanged. Reset parser tested; no live reload claim.

Camera and UI workers owned disjoint module paths. Shared CMake/dependency/package/CI changes were assigned solely to the orchestrator and are identical in both PRs; those shared files intentionally overlap. Packages have separate DLLs/INIs and namespaced notices; neither requires the other DLL or a shared runtime service. CommonLib is statically linked independently. SmoothCam remains sole normal-third-person transform/collision owner; UI reads engine resources and never writes camera transforms.

[Disposable source union](integration-manifest.json) records the exact merged tree and source-equivalence verification. Its detached worktree was removed after the smoke. It was never committed as a merge or made an authoritative release branch. No original save or config was overwritten; copied test saves and a separate hash-verified private character backup support later tests.

## Pins and remaining gates

Steam Skyrim1.7.104.0, SKSE2.3.1, Address Library13, Ubuntu26.04.1, Proton Experimental11 `1791989105 experimental-11.0-20261014-x86_64`, RTX3080, 1920×1080. SmoothCam1.8.1, TDM2.3.1, MCMHelper1.6.3, SkyUI Community6.11; unchanged DragonEye1.7.5, InfinityUI NG2.1.3, Local Map Upgrade settings fork1.3.4 Common+Line17. CommonLib7.2.0 source `7a60f4de794095d7b0f8928d1b930a52e9a7da83`; vcpkg tool/baseline `ee12231b20c95013c6638d845d04c91559a1d1ff`. Exact provider hashes remain in [dependency lock](../../../compatibility/dependency-lock.md) and module runtime manifests. No upgrades.

Known overhead ranged aiming FAIL → DYL-158; native first-person precision fallback remains usable. Historical loading-screen pitch FAIL is owner-deferred to DYL-159. Comprehensive obstruction/lifecycle, additional controller mappings, malformed-config gameplay route, individual health/magicka depletion, broader modal/layout coverage, other runtimes and performance are NOT_RUN beyond the bounded tests. No new owner scope decision is required for these candidates; senior reviews remain required. DYL-157/158 become eligible after Camera review/merge, DYL-159 after its prerequisites; DYL-161 after UI review/merge. None has started.
