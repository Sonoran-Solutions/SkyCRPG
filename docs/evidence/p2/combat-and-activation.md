# DYL-158 — combat and activation evidence

**Status: incomplete.** This change adds a focused policy for retaining the existing F6 manual native-first-person session only while its provider, world, and ordinary camera state remain valid. It is a bounded interruption guard; it does not implement automatic aim detection, elevated ranged aiming, or target lock. The runtime integration must call the guard on input events before evaluating F6 and preserve the existing cancellation on native Toggle POV and load events. F6's deliberate native-first-person return must continue to work when the elevated preference is disabled. The guard does not itself change any camera state, invoke native actions, or write a transform. An unsafe transition that starts and ends with no input observed by the sink cannot be detected by this guard; lifecycle handoff remains DYL-159 work.

The existing Camera F6 behavior is a manually selected native first-person precision fallback; it does not change projectile physics, crosshair placement, activation range, line of sight, attack, cast, or activation behavior. TDM target lock is disabled by the existing exclusive lease; lock-on remains unsupported in this configuration. The Phase 1 overhead ranged result remains **FAIL**: projectiles aimed at the player's feet. Native first-person precision was previously observed in a bounded Phase 1 route, but that does not establish the new candidate's combat acceptance. An automatic weapon- or spell-triggered switch and true overhead aiming require a separately verified state signal and camera ownership design; neither is asserted here.

| Check | Result | Scope |
| --- | --- | --- |
| Precision ownership and precedence | **PASS** | Portable focused C++ test checks menu deferral, observed special/world/provider interruption, no spontaneous reacquisition, and first-person/precision priority over obstruction. [Job](combat-and-activation/precision-test-job.json), [log](combat-and-activation/precision-test.log). This tests a predicate, not native event hooks. |
| Event-sink integration and Windows target build | **NOT_RUN** | Integration owner must wire and verify the exact candidate. |
| Melee, bow, targeted/concentration magic, crossbow | **NOT_RUN** | Actual hit direction and interruption behavior need a bounded Camera-only runtime route. |
| Doors, loot, small-object activation and native range | **NOT_RUN** | Requires actual gameplay and before/after evidence. |
| Target lock | **BLOCKED** | TDM competing camera ownership is disabled. No verified exclusive handoff exists. |
| UI/menu focus and combined regression | **NOT_RUN** | Predicate test does not establish UI input or gameplay behavior. |
| Camera-only runtime candidate, generated artifact identities and preservation | **NOT_RUN** | No game was launched or deployed for this worker. Record exact source, binary hashes, pinned providers, captures, logs and restoration after a candidate route. |

The integration owner should retain separate Camera/UI DLLs and configuration and record proposed controls changes: F6 selects a native first-person precision view for aiming or small-object interaction and returns only a valid session owned by the adapter; native POV remains available. No automatic transition, lock-on, or corrected overhead aiming is implied. Do not advance the ticket to review-ready on this predicate alone.
