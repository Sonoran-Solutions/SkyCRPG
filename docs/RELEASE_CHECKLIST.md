# MVP release-candidate checklist

This checklist is open. It does not mean the mod has shipped.

## Technical candidate

- [ ] C1–C5 camera criteria pass on the pinned support baseline.
- [ ] U1–U6 UI criteria pass.
- [ ] M1–M3 independent installation and supported-stack criteria pass.
- [ ] Q1–Q3 integrity/performance/evidence criteria pass.
- [ ] Camera-only, UI-only and combined package installs tested.
- [ ] Source commit, artifact hashes and dependency lock recorded.
- [ ] No unresolved critical crash, item-loss/duplication, control-lock or navigation-loss defect.
- [ ] Unsupported states/versions have documented safe fallback or explicit incompatibility.
- [ ] Known limitations distinguish intended scope from defects.

## Documentation and distribution readiness

- [ ] Reproducible build/package steps.
- [ ] Verified install, update, configure, disable and recovery steps.
- [ ] Controls and supported input/display modes.
- [ ] Provider versions, source/asset attribution and license obligations.
- [ ] Real screenshots labeled as game evidence; concept art labeled as concept.
- [ ] Internal candidate links and short owner playtest script.

## Owner playtest

Owner route: install a module alone; check controls and ordinary menus; install the pair; travel from exterior to Nordic interior; fight with melee/ranged/magic; loot and equip using the grid; use minimap and follower status; converse, save/reload and exercise camera/UI disable.

Record candidate/hash, host, date, observations and **ACCEPT**, **REVISE** with concrete defects, or **OWNER_PLAYTEST_PENDING**. Prepare the complete reviewable candidate before asking for this check.

## Public release

Publishing a public release is not authorized by the planning task and is not necessary to create the project/tickets. After an explicit release instruction, publish only the accepted artifacts with accurate limitations and required notices.
