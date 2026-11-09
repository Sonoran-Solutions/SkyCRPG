# R5 controller pass on candidate 8: owner report and log corroboration

**Session:** `owner-p5-r5`, candidate 8 (`53410cd`), combined profile, launched 2026-11-09 about 20:45Z. About 4.9 minutes from deploy to quit, watchdog exit 0, no leftover processes. Restore PASS; the game Plugins folder is empty afterwards.
**Owner report (verbatim):** "everything passed".
**Input device (owner):** “I used an 8bitdo controller”. The console was also opened and closed during the session (refresh sources `0x2`/`0x4`).
**Logs:** `session-p4/owner-p5-r5/provider-logs/` on the test host. Camera log 2009 lines, target-lock frame trace 2048 rows (1808 locked), Selection log 135 lines. The precision trace was off, so native POV transitions are not logged individually.

| Step | Owner | Log corroboration | Record as |
| --- | --- | --- | --- |
| R5.1 R3 short press locks | PASS | Corroborated. Ten observed lock-on transitions (Camera log lines 208, 240, 311, 571, 675, 787, 986, 1076, 1802, 1858); three target handles identified: `0011091F`, `00111506`, `00111505`. | PASS |
| R5.2 R3 short press unlocks | PASS | Corroborated. Each lock ends `locked=0`, and the lease is released only after TDM's unlocked acknowledgement. | PASS |
| R5.3 R3 hold, native POV out and back | PASS | Consistent, not unique. Episodes with the camera ineligible (`eligible=0`) and no menu or load event fit native first person, but F8 would look the same. | PASS (owner), log consistent |
| R5.4 Lock, then R3 hold | PASS | Not independently corroborated. Lines 1619–1622 show the disable lease released and reacquired while unlocked, not a target-lock release. | PASS (owner), not corroborated |
| R5.5 Lock, then F6 and F6 | PASS | Corroborated. A lock (line 675) is followed by entry accepted, claimed, request accepted and returned (lines 743–758). The lease is released after acknowledgement, and the next lock is a separate later episode (line 787). | PASS |
| R5.6 F7 recenter | PASS, reaffirmed: “the F7 recent passed” | **Not corroborated.** No recenter-request entry appears; 30 lifecycle entries were emitted against the 128-entry cap. An eligible, accepted F7 press logs when the recenter is requested, including while target-locked; ineligible or suppressed input may leave no entry. | PASS (owner, reaffirmed after the log gap was raised); log gap recorded |
| R5.7 F8 off and on | PASS | No coverage. Neither the Camera log nor SmoothCam's log records F8 toggles. | PASS (owner), no log coverage |
| R5.8 Lock, Start menu, close | PASS | Corroborated. While locked (line 986), the menu makes the camera ineligible and the lease is reacquired (996–997). Journal close is logged (refresh source `0x40`, line 1000). The lease is released after acknowledgement (1006), and there is no re-lock until line 1076. | PASS |
| R5.9 Free play | PASS | Corroborated. Ten lock-on transitions, one F6 cycle and two loads, with no stranded lease: the last lease is held normally at quit. | PASS |
| R5.10 Quit and restore | PASS | Corroborated. Restore PASS, Plugins folder empty, no leftover processes. | PASS |
