// Executes the actual DYL-160 HUD initialization against native-style HUDMenu stubs.
// It verifies authoritative snapshot gating, visibility ownership, menu modes and bounded anchors.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');

const moduleRoot = process.argv[2];
const skyuiRoot = process.argv[3];
const snippet = fs.readFileSync(path.join(moduleRoot, 'scaleform/hud-init.as.inc'), 'utf8');
const hudSource = fs.readFileSync(path.join(skyuiRoot, 'source/actionscript/Vanilla/HUDMenu.as'), 'utf8');

function extractFunction(name) {
  const start = hudSource.indexOf(`function ${name}(`);
  assert.ok(start >= 0, `missing pinned HUDMenu.${name}`);
  const open = hudSource.indexOf('{', start);
  let depth = 1;
  let end = open + 1;
  for (; depth; end++) {
    if (hudSource[end] === '{') depth++;
    else if (hudSource[end] === '}') depth--;
  }
  return eval(`(${hudSource.slice(start, end)})`);
}

class TextField {
  constructor() { this._text = ''; this.selectable = true; this.newTextFormat = null; this.appliedFormat = null; }
  get text() { return this._text; }
  set text(value) {
    this._text = String(value);
    // AS2 setNewTextFormat controls later text assignment; setTextFormat only
    // changes characters already present in the field.
    this.appliedFormat = this._text.length ? this.newTextFormat : null;
  }
  setTextFormat(format) { this.format = format; if (this._text.length) this.appliedFormat = format; }
  setNewTextFormat(format) { this.newTextFormat = format; }
}

class Clip {
  constructor(parent = null, name = '') { this._parent = parent; this._name = name; this._visible = true; }
  createEmptyMovieClip(name) {
    const clip = new Clip(this, name);
    this[name] = clip;
    return clip;
  }
  createTextField(name) {
    this[name] = new TextField();
    // MovieClip.createTextField is intentionally modeled as void; callers must
    // address the created field by its native timeline member name.
  }
  removeMovieClip() { if (this._parent) delete this._parent[this._name]; }
  lineStyle() {}
  beginFill() {}
  moveTo() {}
  lineTo() {}
  endFill() {}
  clear() {}
}

function makeMenu(width, height, settings) {
  const menu = new Clip();
  menu.geometryLogs = [];
  menu.Health = {All: true, _visible: true};
  menu.Magica = {All: true, _visible: true};
  menu.Stamina = {All: true, _visible: true};
  menu.HudElements = [menu.Health, menu.Magica, menu.Stamina];
  menu.HUDModes = [];
  menu.lastHealthMeterPercent = 0;
  menu.lastMagickaMeterPercent = 0;
  menu.lastStaminaMeterPercent = 0;
  menu.lastHealthPenaltyPercent = 0;
  menu.lastMagickaPenaltyPercent = 0;
  menu.lastStaminaPenaltyPercent = 0;
  menu.HealthPenaltyPercent = menu.MagickaPenaltyPercent = menu.StaminaPenaltyPercent = 0;
  menu.CheckAgainstHudMode = extractFunction('CheckAgainstHudMode');
  menu.ShowElements = extractFunction('ShowElements');
  const context = vm.createContext(Object.assign(menu, {
    Stage: {width, height},
    getTimer: () => menu.now || 0,
    TextFormat: function (...args) { this.args = args; },
    skse: settings === null ? undefined : {plugins: {SkyCRPGUI: settings}, Log: message => menu.geometryLogs.push(message)},
  }));
  vm.runInContext(snippet, context, {filename: 'hud-init.as.inc'});
  return menu;
}

function frame(menu, elapsed = 100) { menu.now = (menu.now || 0) + elapsed; menu.SkyCRPGStatus.onEnterFrame.call(menu.SkyCRPGStatus); }
function settings(scale, offsetX, offsetY, enabled = true) {
  return {hudEnabled: enabled, hudScale: scale, hudOffsetX: offsetX, hudOffsetY: offsetY,
    sample: undefined, calls: 0, getPlayerResources() { this.calls++; return this.sample; }};
}

for (const [width, height] of [[1280, 720], [1920, 1080]]) {
  for (const [scale, x, y] of [[85, -120, -60], [100, 0, 0], [115, 120, 60]]) {
    const menu = makeMenu(width, height, settings(scale, x, y));
    const status = menu.SkyCRPGStatus;
    const party = menu.SkyCRPGPartyArea;
    assert.ok(status && party, 'active settings create status and reserved party area');
    assert.ok(status._x >= 16 && status._x + 420 * scale / 100 <= width - 16);
    assert.ok(status._y >= 16 && status._y + 52 * scale / 100 <= height - 12);
    assert.ok(party._x >= 16 && party._x + 194 * scale / 100 <= width - 16);
    assert.ok(party._y >= 16 && party._y + 49 * scale / 100 <= height - 16);
  }
}

const menu = makeMenu(1280, 720, settings(100, 0, 0));
const originalStatus = menu.SkyCRPGStatus;
assert.equal(menu.geometryLogs.length, 1, 'setup emits one bounded geometry diagnostic');
assert.match(menu.geometryLogs[0], /geometry setup Stage=1280x720 parent_x=.*panel_x=.*modeVisible=true/);
frame(menu);
assert.equal(originalStatus._visible, false, 'never show initialized zero defaults before native callbacks');
assert.equal(menu.Health._visible, true, 'native meter remains visible before live data');
assert.equal(menu.Magica._visible, true);
assert.equal(menu.Stamina._visible, true);

menu.skse.plugins.SkyCRPGUI.sample = {health: 146, healthMax: 200, magicka: 41, magickaMax: 100, stamina: 96, staminaMax: 100};
frame(menu);
assert.equal(originalStatus._visible, true);
assert.equal(originalStatus.meters.meter0.value.text, '73%');
assert.equal(originalStatus.meters.meter1.value.text, '41%');
assert.equal(originalStatus.meters.meter2.value.text, '96%');
assert.equal(originalStatus.meters.meter0.value.appliedFormat.args[0], '$EverywhereFont', 'future meter text inherits the HUD value font');
assert.equal(originalStatus.meters.meter0.value.appliedFormat.args[1], 12, 'future meter text inherits the HUD value size');
assert.equal(originalStatus.meters.meter0.value.appliedFormat.args[2], 0xE8E1D1, 'future meter text inherits the HUD value color');
assert.equal(originalStatus.meters.meter0.bar._y, 18, 'bar sits below its label');
assert.equal(menu.Health._visible, false, 'custom bars replace native duplicates only after live data');
assert.equal(menu.Magica._visible, false);
assert.equal(menu.Stamina._visible, false);
assert.equal(originalStatus._skycrpgDataReady, true);

assert.equal(menu.geometryLogs.length, 2, 'first valid render emits one geometry diagnostic');
assert.match(menu.geometryLogs[1], /geometry first-valid-render/);
const bridge = menu.skse.plugins.SkyCRPGUI;
const calls = bridge.calls;
frame(menu, 1);
assert.equal(bridge.calls, calls, 'frame throttle prevents >10Hz sampling');
bridge.sample.stamina = 50;
frame(menu);
assert.equal(originalStatus.meters.meter2.value.text, '50%', 'live stamina expenditure updates');
bridge.sample.stamina = 100;
bridge.sample.magicka = 30;
frame(menu);
assert.equal(originalStatus.meters.meter2.value.text, '100%', 'stamina regeneration updates without callbacks');
assert.equal(originalStatus.meters.meter1.value.text, '30%', 'magicka expenditure updates');
bridge.sample.magickaMax = 150;
frame(menu);
assert.equal(originalStatus.meters.meter1.value.text, '20%', 'temporary maximum modifier changes denominator');
menu.HealthPenaltyPercent = 20;
frame(menu);
assert.equal(originalStatus.meters.meter0.value.text, '73%', 'effective maximum ratio does not double apply native penalty');
bridge.sample.health = -5;
frame(menu);
assert.equal(originalStatus.meters.meter0.value.text, '0%', 'negative current at death clamps display only');
bridge.sample.health = 210;
frame(menu);
assert.equal(originalStatus.meters.meter0.value.text, '100%', 'current above changed maximum clamps display');

menu.ShowElements('Favor', true);
frame(menu);
assert.equal(originalStatus._visible, false, 'native HUD mode owns overlay visibility');
assert.equal(menu.Health._visible, false, 'native HUD mode still hides native resource bars');
menu.ShowElements('Favor', false);
frame(menu);
assert.equal(originalStatus._visible, true, 'overlay returns with the native All mode');
assert.equal(menu.Health._visible, false);

for (const invalid of [undefined, {}, {health: NaN}, {...bridge.sample, staminaMax: 0}, {...bridge.sample, stamina: Infinity}, {...bridge.sample, health: '10'}]) {
  bridge.sample = invalid;
  frame(menu);
  assert.equal(originalStatus._visible, false, 'missing or invalid snapshot fails closed');
  assert.equal(originalStatus._skycrpgDataReady, false);
  assert.equal(menu.Health._visible, true, 'invalid snapshot restores native mode visibility');
  assert.equal(menu.Magica._visible, true);
  assert.equal(menu.Stamina._visible, true);
}
bridge.sample = {health: 50, healthMax: 100, magicka: 100, magickaMax: 100, stamina: 95, staminaMax: 100};
frame(menu);
assert.equal(menu.Health._visible, false);
assert.equal(menu.geometryLogs.length, 2, 'subsequent samples and fallback do not spam diagnostics');
const staleStatus = menu.SkyCRPGStatus;
const staleParty = menu.SkyCRPGPartyArea;
vm.runInContext(snippet, vm.createContext(Object.assign(menu, {
  Stage: {width: 1280, height: 720}, TextFormat: function () {}, skse: undefined,
})), {filename: 'hud-init-reinit.as.inc'});
assert.equal(staleStatus.onEnterFrame, null, 'teardown clears the owned frame handler');
assert.equal(menu.SkyCRPGStatus, undefined, 'teardown removes old status clip');
assert.equal(menu.SkyCRPGPartyArea, undefined, 'missing bridge leaves no stale party clip');
assert.equal(menu.Health._visible, true, 'teardown restores native meters');
assert.equal(menu.HudElements.length, 3, 'teardown removes stale HudElements references');

const disabled = makeMenu(1280, 720, settings(100, 0, 0, false));
assert.equal(disabled.SkyCRPGStatus, undefined, 'disabled HUD keeps native layout');
const missing = makeMenu(1280, 720, null);
assert.equal(missing.SkyCRPGStatus, undefined, 'missing plugin bridge keeps native layout');

console.log('PASS: authoritative current/effective maximum snapshots, fallback, mode visibility, teardown and 720p/1080p bounds');
