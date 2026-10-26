# Controls contract

The normative ownership/precedence contract is in [ARCHITECTURE](ARCHITECTURE.md#input-and-menu-contract). Proven spike controls: SmoothCam F8/scan66 toggle; native POV precision fallback; controller grid adjacent/row movement and explicit category focus as recorded in [UI evidence](evidence/p1/ui-spike.md). Future reset/recenter/hotbar/full input mappings below remain obligations, not implemented controls.

Bindings are proposed actions until P1/P2 test real conflicts. P2-01 and P2-06 must replace pending bindings with the actual defaults.

| Action | Intended behavior | Binding status |
| --- | --- | --- |
| Camera enable/normal view | Toggle elevated view and release to the safe underlying view. | Configurable key/controller route selected in P2. |
| Emergency camera reset | Recover usable normal view without opening the custom UI. | Must be reachable independently of UI. |
| Rotate/recenter | Yaw/snap as selected; recenter on character. | P2-01. |
| Zoom | Bounded distance adjustment; never disable wall/floor collision. | P2-01. |
| Precision aim | Native attack/aim input and selected closer-view behavior. | P2-03. |
| Hotbar slots | Up to eight favorite bindings; equip/select/use through native behavior. | Native 1–8 where appropriate, plus tested controller path. |
| Open inventory | Open player grid; retain access to other native menus. | Existing inventory action where feasible. |
| Grid navigation | Pointer or keyboard/controller focus; inspect/equip/use/favorite/drop. | P3-06 must document every action. |
| Cancel/back | Close current modal, then menu; never trigger world combat behind it. | Native convention preserved. |
| Minimap toggle/zoom | Selected provider action; restore native navigation when replacement is disabled. | Resolve provider conflicts in P1/P3. |
| UI scale/reset | Restore safe anchors and readable defaults. | P3-06. |

An action bar that merely draws icons is incomplete. Input must be usable and reflect actual selected/equipped state. No new spell cooldown, autoplay rotation, party order or turn-control binding is planned.

## Phase 2 Camera candidate — DYL-156

These mappings describe the new candidate; [Camera evidence](evidence/p2/camera-controls.md) separately records build and gameplay validation. Native movement/combat/activation remain unchanged, with TDM movement-only and SmoothCam as the only normal third-person transform writer.

| Control | Candidate behavior |
| --- | --- |
| F6 (scan 64) | Explicit native first-person fallback from eligible overhead view. Press again to return only when F6 owns that request; it does not reclaim a native POV request. From disabled policy in eligible third person, enable policy without forcing a state. |
| F7 (scan 65) | Recenter yaw toward player heading at the configured rate. |
| F8 (scan 66) | Existing SmoothCam emergency toggle, preserved. It releases to underlying normal third person; SkyCRPG never re-enables the provider itself. |
| Mouse horizontal / controller right-stick horizontal | Native yaw path; vertical input is filtered to hold configured pitch while overhead policy is eligible. |
| Native wheel / controller zoom gesture | Bounded requested zoom; provider collision may move the actual camera closer. Native POV remains available for precision aiming. |
| Controller custom toggle/recenter | Unbound by default. Only explicitly configured unmapped single-button masks are eligible. No native gameplay button or unverified chord is stolen. Mapping/feel require owner validation. |

Camera preferences use version 1 in `SkyCRPG_Camera.ini`: default pitch 55 degrees, bounded 15–75; requested zoom 0.65 with default limits 0.25–0.90. Reset with the shipped INI and a restart. Settings remain outside Skyrim saves. Malformed fields recover to defaults or documented safe suspension; unknown schemas are not overwritten. Persistent writes cover only owned enable/zoom/schema keys.

Native menus, dialogue, loading/death, first person and unsupported special states take precedence. Held controls are discarded through focus changes and require fresh input; no gameplay event is replayed. F8 and map K/L are reserved against custom bindings, and keys already mapped to native gameplay are refused. Full obstruction, targeting and lifecycle recovery remain DYL-157/158/159.
