"""Regression for PR7's mixed module/shared-change matrix under-selection."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("ci_scope", Path(__file__).parents[1] / "ci_scope.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ScopeTests(unittest.TestCase):
    def test_shared_build_and_packaging_changes_override_either_module(self):
        shared = ["CMakeLists.txt", "CMakePresets.json", "cmake/CommonLib.cmake",
                  "cmake/README.md", "vcpkg.json", "vcpkg-configuration.json",
                  "tools/package.py", ".github/workflows/build.yml",
                  "shared/include/skycrpg/skeleton.h", "LICENSES/CommonLibSSE-NG-COPYING",
                  "tools/ci_scope.py", "tools/tests/test_ci_scope.py", ".github/actions/build/action.yml"]
        for changed_module in ("camera", "ui", "testprobe"):
            for path in shared:
                with self.subTest(module=changed_module, shared=path):
                    self.assertEqual(module.select_modules([{"testprobe": "tools/testprobe/src/plugin.cpp"}.get(changed_module, f"modules/{changed_module}/src/plugin.cpp"), path]), ["camera", "ui", "testprobe"])

    def test_leaf_changes_stay_focused(self):
        self.assertEqual(module.select_modules(["modules/camera/include/policy.h", "docs/CONTROLS.md"]), ["camera"])
        self.assertEqual(module.select_modules(["modules/ui/scaleform/hud-init.as.inc"]), ["ui"])
        self.assertEqual(module.select_modules(["tools/testprobe/src/plugin.cpp", "tools/testprobe/README.md"]), ["testprobe"])

    def test_probe_and_module_changes_build_only_those(self):
        self.assertEqual(module.select_modules(["modules/ui/src/plugin.cpp", "tools/testprobe/src/plugin.cpp"]), ["ui", "testprobe"])

    def test_both_module_changes(self):
        self.assertEqual(module.select_modules(["modules/ui/src/plugin.cpp", "modules/camera/src/plugin.cpp"]), ["camera", "ui"])

    def test_missing_or_unclassified_diff_is_conservative(self):
        for paths in ([], ["docs/STATUS.md"]):
            self.assertEqual(module.select_modules(paths), ["camera", "ui", "testprobe"])

    def test_explicit_dispatch_remains_bounded(self):
        for name in ("camera", "ui", "testprobe"):
            self.assertEqual(module.select_modules([], name), [name])
        self.assertEqual(module.select_modules([], "both"), ["camera", "ui"])
        self.assertEqual(module.select_modules([], "all"), ["camera", "ui", "testprobe"])
        with self.assertRaises(ValueError):
            module.select_modules([], "unknown")

    def test_workflow_uses_tested_selector_and_triggers_its_inputs(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/build.yml").read_text()
        self.assertIn("python tools/ci_scope.py", workflow)
        self.assertIn("python -m unittest discover -s tools/tests -p test_ci_scope.py -v", workflow)
        # Probe jobs run only when selected; the production matrix skips an empty list.
        self.assertIn("needs.scope.outputs.testprobe == 'true'", workflow)
        self.assertIn("needs.scope.outputs.modules != '[]'", workflow)
        self.assertIn("tools/testprobe/tests/", workflow)
        for trigger in ("tools/ci_scope.py", "tools/tests/test_ci_scope.py", "LICENSES/**", "vcpkg-configuration.json", ".github/actions/**"):
            self.assertEqual(workflow.count('"' + trigger + '"'), 2)


if __name__ == "__main__":
    unittest.main()
