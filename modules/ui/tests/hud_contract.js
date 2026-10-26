// Executes the actual DYL-160 HUD initialization against native-style HUDMenu stubs.
// It verifies live callback gating, visibility ownership, menu modes and bounded anchors.
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

function extractObservedMeterSetter(name, observedFlag) {
  const start = hudSource.indexOf(`function ${name}(`);
  assert.ok(start >= 0, `missing native meter callback ${name}`);
  const open = hudSource.indexOf('{', start);
  let depth = 1;
  let end = open + 1;
  for (; depth; end++) {
    if (hudSource[end] === '{') depth++;
    else if (hudSource[end] === '}') depth--;
  }
  const method = hudSource.slice(start, end);
  const instrumented = method.replace('{', `{\nthis.${observedFlag} = true;`);
  assert.notEqual(instrumented, method, `callback receipt hook inserted for ${name}`);
  return eval(`(${instrumented})`);
}

class TextField {
  constructor() { this.text = ''; this.selectable = true; }
  setTextFormat(format) { this.format = format; }
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
  menu._skycrpgHealthSeen = false;
  menu._skycrpgMagickaSeen = false;
  menu._skycrpgStaminaSeen = false;
  menu.HealthMeterLeft = {SetPercent() {}, SetTargetPercent() {}};
  menu.MagickaMeter = {SetPercent() {}, SetTargetPercent() {}};
  menu.StaminaMeter = {SetPercent() {}, SetTargetPercent() {}};
  menu.HealthMeterAnim = menu.MagickaMeterAnim = menu.StaminaMeterAnim = {};
  menu.RunMeterAnim = function () {};
  menu.SetHealthMeterPercent = extractObservedMeterSetter('SetHealthMeterPercent', '_skycrpgHealthSeen');
  menu.SetMagickaMeterPercent = extractObservedMeterSetter('SetMagickaMeterPercent', '_skycrpgMagickaSeen');
  menu.SetStaminaMeterPercent = extractObservedMeterSetter('SetStaminaMeterPercent', '_skycrpgStaminaSeen');
  menu.CheckAgainstHudMode = extractFunction('CheckAgainstHudMode');
  menu.ShowElements = extractFunction('ShowElements');
  const context = vm.createContext(Object.assign(menu, {
    Stage: {width, height},
    TextFormat: function (...args) { this.args = args; },
    skse: settings === null ? undefined : {plugins: {SkyCRPGUI: settings}},
  }));
  vm.runInContext(snippet, context, {filename: 'hud-init.as.inc'});
  return menu;
}

function frame(menu) { menu.SkyCRPGStatus.onEnterFrame.call(menu.SkyCRPGStatus); }
function settings(scale, offsetX, offsetY, enabled = true) {
  return {hudEnabled: enabled, hudScale: scale, hudOffsetX: offsetX, hudOffsetY: offsetY};
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
frame(menu);
assert.equal(originalStatus._visible, false, 'never show initialized zero defaults before native callbacks');
assert.equal(menu.Health._visible, true, 'native meter remains visible before live data');
assert.equal(menu.Magica._visible, true);
assert.equal(menu.Stamina._visible, true);

menu.SetHealthMeterPercent(73, false);
menu.SetMagickaMeterPercent(41, false);
menu.SetStaminaMeterPercent(96, false);
frame(menu);
assert.equal(originalStatus._visible, true);
assert.equal(originalStatus.meters.meter0.value.text, '73%');
assert.equal(originalStatus.meters.meter1.value.text, '41%');
assert.equal(originalStatus.meters.meter2.value.text, '96%');
assert.equal(originalStatus.meters.meter0.bar._y, 18, 'bar sits below its label');
assert.equal(menu.Health._visible, false, 'custom bars replace native duplicates only after live data');
assert.equal(menu.Magica._visible, false);
assert.equal(menu.Stamina._visible, false);
assert.equal(originalStatus._skycrpgDataReady, true);

menu.HealthPenaltyPercent = 20;
menu.MagickaPenaltyPercent = 25;
menu.StaminaPenaltyPercent = 50;
frame(menu);
assert.equal(originalStatus.meters.meter0.value.text, '58%', 'display tracks native effective health meter');
assert.equal(originalStatus.meters.meter1.value.text, '31%');
assert.equal(originalStatus.meters.meter2.value.text, '48%');
menu.HealthPenaltyPercent = menu.MagickaPenaltyPercent = menu.StaminaPenaltyPercent = 0;

menu.ShowElements('Favor', true);
frame(menu);
assert.equal(originalStatus._visible, false, 'native HUD mode owns overlay visibility');
assert.equal(menu.Health._visible, false, 'native HUD mode still hides native resource bars');
menu.ShowElements('Favor', false);
frame(menu);
assert.equal(originalStatus._visible, true, 'overlay returns with the native All mode');
assert.equal(menu.Health._visible, false);

menu.lastStaminaMeterPercent = 101;
frame(menu);
assert.equal(originalStatus._visible, false, 'invalid data hides the custom replacement');
assert.equal(originalStatus._skycrpgDataReady, false);
assert.equal(menu.Health._visible, true, 'invalid data restores native mode visibility');
assert.equal(menu.Magica._visible, true);
assert.equal(menu.Stamina._visible, true);

menu.lastStaminaMeterPercent = 95;
frame(menu);
assert.equal(menu.Health._visible, false);
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

console.log('PASS: live native meter data, fallback, mode visibility, teardown and 720p/1080p bounds');
