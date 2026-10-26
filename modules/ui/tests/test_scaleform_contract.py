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


if __name__ == "__main__":
    unittest.main()
