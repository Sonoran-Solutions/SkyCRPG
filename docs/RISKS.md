# Risk register

| Risk | Impact | Mitigation / gate | Owner ticket |
| --- | --- | --- | --- |
| Elevated camera fails in low ceilings and corridors. | Core camera is unusable in ordinary Skyrim content. | P1 real interior proof; collision-preserving close-view fallback and emergency reset. | P1-03, P2-02 |
| Aim or activation no longer matches the player view. | Bows/magic/looting feel broken. | Tested precision view, single camera owner, native range rules. | P2-03 |
| Dialogue/load/special states strand camera or input. | Blocked gameplay or crash. | State precedence, handle invalidation, bounded handoffs and restart checks. | P2-04 |
| Runtime update breaks a dependency or address. | Plugin fails or accesses invalid state. | Pin executable/versions; reject unsupported combinations; no universal support promise. | P1-02, P4-01 |
| Two camera/UI mods overwrite the same owner/files. | Jitter, duplicate widgets or corrupted focus. | Provider contract and package ownership manifest. | P1-05, P4-02 |
| Display grid confuses distinct item instances. | Item loss, duplication or wrong equipment. | Native actions, fresh resolution, generation invalidation and item fixtures. | P3-01 |
| Reusing Tetris inventory silently changes weight rules. | Product no longer matches baseline. | Verify presentation-only route or bounded own view; explicit decision for mechanics. | P1-02, P1-04 |
| Minimap installed but compass hidden while provider fails. | Navigation disappears. | Presence/capability checks and compass restoration. | P3-04 |
| Follower provider lacks a supported data API. | Scope grows into a new follower framework. | Configured external widget or limited native status adapter; one vanilla follower baseline. | P1-04, P3-03 |
| Controller or small-screen UI is an afterthought. | UI MVP unusable on supported controls/displays. | Prove focus feasibility in P1; finish complete paths in P3. | P1-04, P3-06 |
| Source or artwork permission is unclear. | Cannot redistribute chosen implementation. | Separate source license, archive permissions and art provenance; external dependency where appropriate. | P1-02, P4-05 |
| Large inventories or polling hurt responsiveness. | Frame spikes and menu latency. | Bounded updates/virtualized display; measured 500-entry fixture. | P3-02, P4-03 |
| Agents repeatedly run broad suites or leave jobs alive. | Wasted hours and unreliable evidence. | Job ownership, deadlines, one diagnostic retry, targeted validation and explicit cleanup. | AGENTS.md, VALIDATION.md |
| No game host is available to an implementing agent. | Compile success gets mislabeled as playable. | Prepare candidates and reproducible playtest handoff; retain NOT_RUN runtime gates. | P1-01, phase gates |

Critical risks are resolved with evidence, a tested fallback or an explicit blocker. A phase gate cannot be made green by relabeling a core requirement as cosmetic.
