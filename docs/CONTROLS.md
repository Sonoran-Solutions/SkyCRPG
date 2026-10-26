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
