# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""The engine calls HUDMenu.SetCompassAngle every frame and writes Compass._visible.

Runs the actual transformed SetCompassAngle with the compass lease under Node,
one call per simulated frame, so a lease that only re-hides between samples
(the flicker seen in run 815386e8) fails here.
"""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_scaleform", ROOT / "tools/build_scaleform.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
NODE = "/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node"

HARNESS = r"""
const assert = require('assert');
const [setAngleBody, lease] = JSON.parse(require('fs').readFileSync(0, 'utf8'));
const SetCompassAngle = new Function('aPlayerAngle', 'aCompassAngle', 'abShowCompass', setAngleBody);
let now = 0;
global.getTimer = () => now;
global.Stage = {width: 1280, height: 720};
global._root = {};
global.Shared = {GlobalFunc: {Lerp: (a, b, c, d, v) => a + (b - a) * (v - c) / (d - c)}};
global.skse = {plugins: {SkyCRPGUI: {
  getMinimapCapability: () => ({enabled: true, exact: true, gameplay: true, cellToken: 1}),
  isMenuFocusBlocked: () => false}}};
const compass = {_visible: true, gotoAndStop() {}};
const hud = {_visible: true, _alpha: 100, CompassShoutMeterHolder: {Compass: compass, ShoutMeterInstance: {_visible: true}},
  CompassRect: {_parent: compass}, CompassZeroX: 0, CompassThreeSixtyX: 0,
  CheckAgainstHudMode: () => true, UpdateCompassMarkers() {},
  createEmptyMovieClip(name) { const clip = {_parent: this}; this[name] = clip; return clip; }};
hud.Minimap = {_visible: true, _alpha: 100, _parent: hud, getBounds: () => ({xMin: 1024.5, xMax: 1285.9, yMin: -0.9, yMax: 217.7})};
new Function(lease).call(hud);
function frame() { now += 16; SetCompassAngle.call(hud, 0, 0, true); hud.SkyCRPGCompassWatch.onEnterFrame(); }
for (let i = 0; i < 60; i++) frame();
assert.strictEqual(compass._visible, false, 'lease acquired after settle');
for (let i = 0; i < 120; i++) { now += 16; SetCompassAngle.call(hud, 0, 0, true);
  assert.strictEqual(compass._visible, false, 'native per-frame write must not show the compass while leased');
  hud.SkyCRPGCompassWatch.onEnterFrame(); }
hud.Minimap._visible = false; frame();
assert.strictEqual(compass._visible, true, 'provider hide releases');
SetCompassAngle.call(hud, 0, 0, true); assert.strictEqual(compass._visible, true, 'native write applies after release');
"""


class CompassFrameContract(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("SKYCRPG_SKYUI_SOURCE") and Path(NODE).is_file(), "pinned SkyUI and Node required")
    def test_lease_holds_through_per_frame_native_write(self):
        source = (Path(os.environ["SKYCRPG_SKYUI_SOURCE"]) / "source/actionscript/Vanilla/HUDMenu.as").read_text()
        hud = builder.transform("Vanilla/HUDMenu.as", source)
        match = re.search(r"function SetCompassAngle\(aPlayerAngle, aCompassAngle, abShowCompass\)\n   \{\n(.*?)\n   \}\n", hud, re.S)
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertLess(body.index("this.CompassRect._parent._visible = abShowCompass;"),
                        body.index("this.CompassRect._parent._visible = false;"))
        lease = (ROOT / "scaleform/compass-policy.as.inc").read_text()
        payload = __import__("json").dumps([body, lease])
        result = subprocess.run([NODE, "-e", HARNESS], input=payload, text=True, capture_output=True, timeout=600)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
