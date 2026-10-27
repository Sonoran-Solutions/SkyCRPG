const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('modules/ui/scaleform/follower-panel.as.inc', 'utf8');
class Clip {
  constructor(parent, name) { this._parent = parent; this.name = name; }
  createEmptyMovieClip(name) { return this[name] = new Clip(this, name); }
  createTextField(name) { this[name] = {setNewTextFormat() {}}; }
  lineStyle() {} beginFill() {} moveTo() {} lineTo() {} endFill() {}
}
let sample = {state: 'live', name: 'A very long follower name that should fit', health: 0.5};
let now = 0;
const menu = new Clip(null, 'hud');
menu.CheckAgainstHudMode = () => menu.allowed;
menu.allowed = true;
const context = vm.createContext(Object.assign(menu, {
  skycrpg: {placeAtStage() {}}, skycrpgSettings: {hudScale: 100, hudOffsetX: 0, hudOffsetY: 0},
  Stage: {width: 1920, height: 1080}, TextFormat: function() {},
  getTimer: () => now, skse: {plugins: {SkyCRPGUI: {getFollowerStatus: () => sample}}}
}));
vm.runInContext(source, context);
const panel = menu.SkyCRPGPartyArea;
function frame() { now += 100; panel.onEnterFrame(); }
frame(); assert.equal(panel.availability.text, 'HEALTH 50%');
assert.equal(panel.heading.text.length, 22); assert.ok(panel.portrait); assert.equal(panel._visible, true);
sample = {state: 'unknown', name: ''}; frame(); assert.equal(panel.availability.text, 'Follower data unavailable');
sample = {state: 'unsupported'}; frame(); assert.equal(panel.availability.text, 'Followers untested');
sample = {state: 'dead'}; frame(); assert.equal(panel._visible, false);
sample = {state: 'none'}; frame(); assert.equal(panel._visible, false);
sample = {state: 'live', name: 'Lydia', health: 1}; menu.allowed = false; frame(); assert.equal(panel._visible, false);
menu.allowed = true; panel.onModeChange('All'); assert.equal(panel._visible, true);
console.log('follower panel PASS');
