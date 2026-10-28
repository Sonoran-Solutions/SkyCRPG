# Phase 4 internal candidate checklist

This is not public-release authorization. Candidate source `<candidate SHA>`, CI `<candidate CI run>`, package hashes `<candidate archive hashes>`; final candidate gameplay is **NOT_RUN**.

| Ticket | Current state | Evidence / remaining work |
| --- | --- | --- |
| [DYL-170](phases/PHASE_4.md) | In progress; portable checks PASS, runtime NOT_RUN | [UI](evidence/p4/blocker-resolution-ui.md), [Camera](evidence/p4/blocker-resolution-camera.md); exercise independent disable and failure routes. Aim FAIL and lock BLOCKED accepted as MVP limits. |
| DYL-171 | In progress; assembly tooling and offline audit; final packages and manager routes NOT_RUN | [Packaging](evidence/p4/packaging.md); produce strict candidates with hashes, provenance and rights notices. |
| DYL-172 | NOT_RUN | Measure frame time, inventory latency and resource stability against recorded budgets. |
| DYL-173 | NOT_RUN | Run final Camera-only, UI-only and combined regression, integrity, update and recovery on exact packages. |
| DYL-174 | Documentation review in progress | [Docs review](evidence/p4/docs-review.md); resolve missing license notices and review corresponding-source draft. |
| DYL-175 | OWNER_PLAYTEST_PENDING | [Owner route](PLAYTEST.md); record ACCEPT or specific revision request after technical gates. |

Do not claim C1–C5, U1–U6, M1–M3 or Q1–Q3 pass for the final candidate without recorded evidence. Record source, package and dependency hashes, exact host, device and outcome. No unresolved critical crash, item loss, control lock or navigation loss may be silently waived. Public distribution, including rights to third-party assets, requires a later owner decision.
