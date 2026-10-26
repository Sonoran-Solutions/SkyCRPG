"""Source-level safety checks. These do not establish game behavior."""
import importlib.util
import os
from pathlib import Path
import re
import unittest

TOOL = Path(__file__).resolve().parents[1] / "tools/build_scaleform.py"
spec = importlib.util.spec_from_file_location("builder", TOOL)
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class PatchContract(unittest.TestCase):
    def test_imported_panel_and_menu_both_receive_routing(self):
        # Real SWF linkage: menu imports symbol InventoryLists from the external
        # list movie, which registers it to its own InventoryLists class.
        for movie in ("inventorymenu.swf", "skyui/inventorylists.swf"):
            self.assertIn("ItemMenus/InventoryLists.as", builder.TARGETS[movie])

    def test_ambiguous_or_missing_anchor_refused(self):
        for text in ("nothing", "anchor anchor"):
            with self.assertRaises(ValueError):
                builder.replace_once(text, "anchor", "replacement")

    @unittest.skipUnless(os.environ.get("SKYCRPG_SKYUI_SOURCE"), "external pinned SkyUI checkout required")
    def test_native_transaction_and_focus_calls_preserved(self):
        root = Path(os.environ["SKYCRPG_SKYUI_SOURCE"]) / "source/actionscript"
        for name, expected in builder.SOURCES.items():
            with self.subTest(source=name):
                source = root / name
                self.assertEqual(builder.sha(source), expected)
                original = source.read_text()
                candidate = builder.transform(name, original)
                if name.endswith("InventoryLists.as"):
                    self.assertLess(candidate.index("var skycrpgList = this.itemList"),
                                    candidate.index("if(this.categoryList.handleInput"))
                # Any added/deleted engine dispatch, focus or callback registration
                # would make this a different risk class than a presentation spike.
                for pattern in (r"gfx\.io\.GameDelegate\.[^;]+;", r"gfx\.managers\.FocusHandler[^;]+;"):
                    self.assertEqual(re.findall(pattern, original), re.findall(pattern, candidate))

    def test_original_art_has_no_transaction_dispatch(self):
        for path in (builder.ROOT / "scaleform").glob("*.inc"):
            source = path.read_text()
            for forbidden in ("GameDelegate", "ItemSelect", "ItemDrop", "EquipItem", "formId"):
                self.assertNotIn(forbidden, source)

    def test_painting_does_not_change_navigation_focus(self):
        source = (builder.ROOT / "scaleform/grid-methods.as.inc").read_text()
        paint = source.split("public function skycrpgPaintCell", 1)[1].split("private function onItemRollOver", 1)[0]
        self.assertNotIn("skycrpgCategoryFocus", paint)

    def test_hud_uses_native_meters_and_native_interaction_authority(self):
        source = (builder.ROOT / "scaleform/hud-init.as.inc").read_text()
        for native_value in ("lastHealthMeterPercent", "lastMagickaMeterPercent", "lastStaminaMeterPercent"):
            self.assertIn("hud." + native_value, source)
        self.assertIn("native crosshair/rollover remains visible", source)
        self.assertNotIn("SetCrosshairTarget", source)
        self.assertNotIn("Activate", source)
        self.assertIn("hud.Health._visible = modeVisible ? false", source)
        self.assertIn("hud.Magica._visible = modeVisible ? false", source)
        self.assertIn("hud.Stamina._visible = modeVisible ? false", source)
        self.assertIn("skse.plugins.SkyCRPGUI", source)
        self.assertIn("_skycrpgHealthSeen", source)
        self.assertIn("CheckAgainstHudMode", source)
        self.assertIn("this.HudElements.push(skycrpg)", source)
        self.assertIn("skycrpg.onEnterFrame = function()", source)
        self.assertIn("this.SkyCRPGStatus.onEnterFrame = null", source)
        self.assertNotIn("addEventListener", source)
        self.assertNotIn("RolloverText", source)

    @unittest.skipUnless(os.environ.get("SKYCRPG_SKYUI_SOURCE"), "external pinned SkyUI checkout required")
    def test_hud_builder_marks_only_native_meter_callbacks_and_modes(self):
        root = Path(os.environ["SKYCRPG_SKYUI_SOURCE"])
        source = root / "source/actionscript/Vanilla/HUDMenu.as"
        self.assertEqual(builder.sha(source), builder.SOURCES["Vanilla/HUDMenu.as"])
        original = source.read_text()
        candidate = builder.transform("Vanilla/HUDMenu.as", original)
        for flag, setter in (("_skycrpgHealthSeen", "SetHealthMeterPercent"),
                             ("_skycrpgMagickaSeen", "SetMagickaMeterPercent"),
                             ("_skycrpgStaminaSeen", "SetStaminaMeterPercent")):
            method = candidate.index("function " + setter + "(")
            hook = candidate.index("this." + flag + " = true;", method)
            self.assertGreater(hook, method)
        self.assertIn("this.SkyCRPGStatus._visible = skycrpgVisible;", candidate)
        self.assertIn("this._skycrpgMetersSuppressed = skycrpgVisible;", candidate)
        self.assertEqual(builder.sha(source), builder.SOURCES["Vanilla/HUDMenu.as"])

    def test_hud_settings_are_versioned_bounded_and_missing_bridge_safe(self):
        config = (builder.ROOT / "config/SkyCRPG_UI.ini").read_text()
        plugin = (builder.ROOT / "src/plugin.cpp").read_text()
        parser = (builder.ROOT / "src/hud_settings.h").read_text()
        for value in ("iVersion=1", "bEnabled=1", "iScalePercent=100", "iOffsetX=0", "iOffsetY=0"):
            self.assertIn(value, config)
        for bound in ("85, 115", "-120, 120", "-60, 60"):
            self.assertIn(bound, parser)
        self.assertIn("SKSE::GetScaleformInterface()", plugin)
        self.assertIn("RegisterSkyCRPGUI", plugin)
        self.assertIn("skycrpgSettings != undefined", (builder.ROOT / "scaleform/hud-init.as.inc").read_text())
        self.assertIn("native HUD fallback retained", plugin)

    def test_scaleform_builder_can_compile_only_the_changed_hud(self):
        source = TOOL.read_text()
        self.assertIn('choices=sorted(TARGETS)', source)
        self.assertIn('args.target or list(TARGETS)', source)


if __name__ == "__main__":
    unittest.main()
