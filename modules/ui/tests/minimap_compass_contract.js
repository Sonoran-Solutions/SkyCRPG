const fs = require('fs');
const assert = require('assert');
const source = fs.readFileSync('modules/ui/scaleform/compass-policy.as.inc', 'utf8');
const init = new Function(source);
let now = 0;
global.getTimer = () => now;
global.Stage = {width: 1000, height: 800};
global._root = {};
let sample = {enabled: true, exact: true, gameplay: true, cellToken: 1};
let blocked = false;
global.skse = {plugins: {SkyCRPGUI: {getMinimapCapability: () => sample, isMenuFocusBlocked: () => blocked}}};
const compass = {_visible: true};
const shout = {_visible: true};
const hud = {_visible: true, _alpha: 100, CompassShoutMeterHolder: {Compass: compass, ShoutMeterInstance: shout},
  CheckAgainstHudMode: () => true,
  createEmptyMovieClip(name) { const clip = {_parent: this, removeMovieClip() { if (this.onUnload) this.onUnload(); }}; this[name] = clip; return clip; }};
hud.Minimap = {_visible: true, _alpha: 100, _parent: hud, getBounds: () => ({xMin: 700, xMax: 900, yMin: 20, yMax: 200})};
function tick(ms) { now += ms; hud.SkyCRPGCompassWatch.onEnterFrame(); }
function acquire() { tick(100); tick(500); assert.strictEqual(compass._visible, false); }
init.call(hud);
tick(100); tick(400); assert.strictEqual(compass._visible, true); tick(100);
assert.strictEqual(compass._visible, false);
compass._visible = true; tick(100); assert.strictEqual(compass._visible, false);
sample = undefined; tick(100); assert.strictEqual(compass._visible, true);
sample = {enabled: true, exact: true, gameplay: true, cellToken: 1}; acquire();
sample.cellToken++; tick(100); assert.strictEqual(compass._visible, true); acquire();
blocked = true; tick(1); assert.strictEqual(compass._visible, true); blocked = false; acquire();
hud.Minimap._visible = false; tick(1); assert.strictEqual(compass._visible, true); hud.Minimap._visible = true; acquire();
const map = hud.Minimap; hud.Minimap = undefined; tick(1); assert.strictEqual(compass._visible, true); hud.Minimap = map; acquire();
const bounds = map.getBounds; map.getBounds = () => ({xMin: 100, xMax: 200, yMin: 20, yMax: 200}); tick(1); assert.strictEqual(compass._visible, true); map.getBounds = bounds; acquire();
const parent = {_visible: false, _alpha: 100, _parent: hud}; map._parent = parent; tick(1); assert.strictEqual(compass._visible, true); map._parent = hud; acquire();
hud.Minimap._alpha = 0; tick(1); assert.strictEqual(compass._visible, true); hud.Minimap._alpha = 100; acquire();
hud.CheckAgainstHudMode = () => false; tick(1); assert.strictEqual(compass._visible, true); hud.CheckAgainstHudMode = () => true; acquire();
for (const key of ['enabled', 'exact', 'gameplay']) { sample[key] = false; tick(100); assert.strictEqual(compass._visible, true); sample[key] = true; acquire(); }
const bridge = skse.plugins.SkyCRPGUI; delete skse.plugins.SkyCRPGUI; tick(1); assert.strictEqual(compass._visible, true); skse.plugins.SkyCRPGUI = bridge; acquire();
init.call(hud); assert.strictEqual(compass._visible, true); acquire(); hud.SkyCRPGCompassWatch.onUnload(); assert.strictEqual(compass._visible, true);
compass._visible = false; acquire(); hud._skycrpgCompassRelease(); assert.strictEqual(compass._visible, false);
acquire(); compass._visible = true; tick(100); hud._skycrpgCompassRelease(); assert.strictEqual(compass._visible, true);
compass._visible = true; acquire(); hud._skycrpgCompassPrior = false; compass._visible = false; hud._skycrpgCompassRelease(); assert.strictEqual(compass._visible, false);
assert.strictEqual(shout._visible, true);
