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
                  "tools/package.py", ".github/workflows/build.yml", "tools/tests/test_package.py",
                  "LICENSE", "docs/LICENSING.md", "docs/THIRD_PARTY.md", "docs/compatibility/dependency-lock.md",
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

    def test_vinput_tooling_selects_no_native_target(self):
        # tools/vinput has its own workflow; alone it selects nothing, beside a module it stays focused.
        self.assertEqual(module.select_modules(["tools/vinput/plugin/src/plugin.cpp", "tools/vinput/UPSTREAM.lock"]), [])
        self.assertEqual(module.select_modules(["tools/vinput/README.md", "modules/ui/src/plugin.cpp"]), ["ui"])
        self.assertEqual(module.select_modules(["tools/vinput/README.md", "tools/testprobe/src/plugin.cpp"]), ["testprobe"])
        # A shared change still widens, and an unclassified companion stays conservative.
        self.assertEqual(module.select_modules(["tools/vinput/README.md", "vcpkg.json"]), ["camera", "ui", "testprobe"])
        self.assertEqual(module.select_modules(["tools/vinput/README.md", "docs/STATUS.md"]), ["camera", "ui", "testprobe"])

    def test_vinput_workflow_is_separate_and_pinned(self):
        workflows = Path(__file__).parents[2] / ".github/workflows"
        build = (workflows / "build.yml").read_text()
        vinput = (workflows / "vinput.yml").read_text()
        self.assertNotIn("tools/vinput", build)
        self.assertEqual(vinput.count('"tools/vinput/**"'), 2)
        self.assertEqual(vinput.count('".github/workflows/vinput.yml"'), 2)
        self.assertIn("tools/vinput/UPSTREAM.lock", vinput)
        self.assertIn("call build.bat :arch x64", vinput)
        self.assertIn("runs-on: windows-2022", vinput)
        lock = (Path(__file__).parents[2] / "tools/vinput/UPSTREAM.lock").read_text()
        self.assertRegex(lock, r"(?m)^commit=[0-9a-f]{40}$")

    def test_workflow_uses_tested_selector_and_triggers_its_inputs(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/build.yml").read_text()
        self.assertIn("python tools/ci_scope.py", workflow)
        self.assertIn("python -m unittest discover -s tools/tests -p test_ci_scope.py -v", workflow)
        # Probe jobs run only when selected; the production matrix skips an empty list.
        self.assertIn("needs.scope.outputs.testprobe == 'true'", workflow)
        self.assertIn("needs.scope.outputs.modules != '[]'", workflow)
        self.assertIn("tools/testprobe/tests/", workflow)
        for trigger in ("tools/ci_scope.py", "tools/tests/test_ci_scope.py", "tools/tests/test_package*.py",
                        "LICENSE", "docs/LICENSING.md", "docs/THIRD_PARTY.md", "docs/compatibility/dependency-lock.md", "LICENSES/**", "vcpkg-configuration.json", ".github/actions/**"):
            self.assertEqual(workflow.count('"' + trigger + '"'), 2)


if __name__ == "__main__":
    unittest.main()
