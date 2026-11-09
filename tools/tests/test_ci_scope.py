"""Regression for PR7's mixed module/shared-change matrix under-selection."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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
        for changed_module in ("camera", "ui", "selection", "testprobe"):
            for path in shared:
                with self.subTest(module=changed_module, shared=path):
                    self.assertEqual(module.select_modules([{"testprobe": "tools/testprobe/src/plugin.cpp"}.get(changed_module, f"modules/{changed_module}/src/plugin.cpp"), path]), ["camera", "ui", "selection", "testprobe"])

    def test_leaf_changes_stay_focused(self):
        self.assertEqual(module.select_modules(["modules/camera/include/policy.h", "docs/CONTROLS.md"]), ["camera"])
        self.assertEqual(module.select_modules(["modules/ui/scaleform/hud-init.as.inc"]), ["ui"])
        self.assertEqual(module.select_modules(["tools/testprobe/src/plugin.cpp", "tools/testprobe/README.md"]), ["testprobe"])

    def test_selection_leaf_and_mixed_changes(self):
        self.assertEqual(module.select_modules(["modules/selection/src/plugin.cpp", "docs/evidence/p5/proximity-activation.md"]), ["selection"])
        self.assertEqual(module.select_modules(["modules/selection/include/selection_policy.h", "modules/camera/src/plugin.cpp"]), ["camera", "selection"])
        self.assertEqual(module.select_modules(["modules/selection/config/SkyCRPG_Selection.ini", "tools/testprobe/src/plugin.cpp"]), ["selection", "testprobe"])

    def test_selection_api_rebuilds_camera_consumer(self):
        header = "modules/selection/include/skycrpg/SelectionAPI.h"
        self.assertEqual(module.select_modules([header]), ["camera", "selection"])
        self.assertEqual(module.select_modules([header, "tools/testprobe/src/plugin.cpp"]), ["camera", "selection", "testprobe"])

    def test_probe_and_module_changes_build_only_those(self):
        self.assertEqual(module.select_modules(["modules/ui/src/plugin.cpp", "tools/testprobe/src/plugin.cpp"]), ["ui", "testprobe"])

    def test_both_module_changes(self):
        self.assertEqual(module.select_modules(["modules/ui/src/plugin.cpp", "modules/camera/src/plugin.cpp"]), ["camera", "ui"])

    def test_missing_or_unclassified_diff_is_conservative(self):
        for paths in ([], ["docs/STATUS.md"]):
            self.assertEqual(module.select_modules(paths), ["camera", "ui", "selection", "testprobe"])

    def test_explicit_dispatch_remains_bounded(self):
        for name in ("camera", "ui", "selection", "testprobe"):
            self.assertEqual(module.select_modules([], name), [name])
        self.assertEqual(module.select_modules([], "both"), ["camera", "ui"])
        self.assertEqual(module.select_modules([], "all"), ["camera", "ui", "selection", "testprobe"])
        with self.assertRaises(ValueError):
            module.select_modules([], "unknown")

    def test_selection_uses_own_output_without_changing_production_artifacts(self):
        for requested, production, selection, probe in (
                ("selection", [], "true", "false"),
                ("camera", ["camera"], "false", "false"),
                ("ui", ["ui"], "false", "false"),
                ("both", ["camera", "ui"], "false", "false"),
                ("all", ["camera", "ui"], "true", "true")):
            with self.subTest(requested=requested), tempfile.TemporaryDirectory() as tmp:
                output = Path(tmp) / "output"
                with patch.dict(module.os.environ, {"REQUESTED": requested, "GITHUB_OUTPUT": str(output)}, clear=True), patch("builtins.print"):
                    module.main()
                values = dict(line.split("=", 1) for line in output.read_text().splitlines())
                self.assertEqual(json.loads(values["modules"]), production)
                self.assertEqual(values["selection"], selection)
                self.assertEqual(values["testprobe"], probe)

    def test_workflow_uses_tested_selector_and_triggers_its_inputs(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/build.yml").read_text()
        self.assertIn("python tools/ci_scope.py", workflow)
        self.assertIn("python -m unittest discover -s tools/tests -p test_ci_scope.py -v", workflow)
        # Probe jobs run only when selected; the production matrix skips an empty list.
        self.assertIn("needs.scope.outputs.testprobe == 'true'", workflow)
        self.assertIn("needs.scope.outputs.modules != '[]'", workflow)
        self.assertIn("tools/testprobe/tests/", workflow)
        self.assertIn("needs.scope.outputs.selection == 'true'", workflow)
        self.assertIn("modules/selection/tests/", workflow)
        self.assertIn("--target SkyCRPG_Selection", workflow)
        self.assertIn("-DSKYCRPG_BUILD_SELECTION=ON", workflow)
        self.assertIn("-DSKYCRPG_BUILD_SELECTION=OFF", workflow)
        self.assertIn("name: skycrpg-selection-${{ env.SOURCE_SHA }}", workflow)
        selection_job = workflow.split("  selection:\n", 1)[1].split("  selection-portable:\n", 1)[0]
        self.assertIn("python tools/package.py native --build-dir build/windows-x64-release --commit $env:SOURCE_SHA --module selection", selection_job)
        self.assertIn("path: dist/", selection_job)
        self.assertIn("-DSKYCRPG_BUILD_CAMERA=OFF -DSKYCRPG_BUILD_UI=OFF", selection_job)
        self.assertIn("-DSKYCRPG_BUILD_TEST_PROBE=OFF", selection_job)
        for trigger in ("tools/ci_scope.py", "tools/tests/test_ci_scope.py", "tools/tests/test_package*.py",
                        "LICENSE", "docs/LICENSING.md", "docs/THIRD_PARTY.md", "docs/compatibility/dependency-lock.md", "LICENSES/**", "vcpkg-configuration.json", ".github/actions/**"):
            self.assertEqual(workflow.count('"' + trigger + '"'), 2)


if __name__ == "__main__":
    unittest.main()
