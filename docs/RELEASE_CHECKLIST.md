# Phase 4 internal candidate checklist

This is not public-release authorization. Final candidate source, CI run and archive hashes: **pending DYL-171 final-package generation**. Final candidate gameplay is **NOT_RUN**.

| Ticket | Current state | Evidence / remaining work |
| --- | --- | --- |
| [DYL-170](phases/PHASE_4.md) | Scoped runtime **PASS**: 22/22 automated routes on Proton Experimental `experimental-11.0-20261027b-x86_64`, Steam Skyrim SE 1.7.104.0, SKSE 2.3.1, test probe `ac28d3c` (#30), UI `2d39d30` and Camera `1d95915`; the owner approved all 40 review screenshots on 2026-11-01. Camera `1daf2aa` adds the notice-intent fix after the runs (portable tests and Windows CI only). | [UI](evidence/p4/blocker-resolution-ui.md), [Camera](evidence/p4/blocker-resolution-camera.md). Controller play, follower lifecycle, the CC missing-icon fixture and marker travel accuracy remain **NOT_RUN**. Aim FAIL and lock BLOCKED stay accepted MVP limitations, not passes. |
| DYL-171 | In progress; assembly tooling and offline audit; final packages and manager routes NOT_RUN | [Packaging](evidence/p4/packaging.md); produce strict candidates with hashes, provenance and rights notices. |
| DYL-172 | NOT_RUN | Measure frame time, inventory latency and resource stability against recorded budgets. |
| DYL-173 | Final packaged regression **NOT_RUN** | Run final Camera-only, UI-only and combined regression, integrity, update and recovery on exact packages. |
| DYL-174 | Documentation review in progress | [Docs review](evidence/p4/docs-review.md). CommonLib legacy MIT and vcpkg static-library notices ship in both archives; the corresponding-source and third-party redistribution review is a separate owner decision. |
| DYL-175 | Owner release playtest **PENDING** | [Owner route](PLAYTEST.md); record ACCEPT or specific revision request after technical gates. |

Do not claim C1–C5, U1–U6, M1–M3 or Q1–Q3 pass for the final candidate without recorded evidence. Record source, package and dependency hashes, exact host, device and outcome. No unresolved critical crash, item loss, control lock or navigation loss may be silently waived. Public distribution, including rights to third-party assets, requires a later owner decision.
