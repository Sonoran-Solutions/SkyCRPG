"""Fixture checks for tools/profile.py: filesystem safety, recovery and launch results.

Runs against a temporary fake Steam/game tree and fake launch processes; never
touches a real Skyrim install. `python3 -m unittest discover -s tests -v`
"""

import hashlib
import importlib.util
import io
import json
import os
import tempfile
import unittest
import zipfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "profile.py"
GOOD_SKSE_LOG = (
    "SKSE64 runtime: initialize (version = 2.3.1 01070680 01DD64DB16681E69, os = 6.2 (9200))\n"
    "checking plugin SkyCRPG_Camera.dll\n"
    "plugin SkyCRPG_Camera.dll (00000001 SkyCRPG Camera 00000010) loaded correctly (handle 1)\n"
    "init complete\n"
    "dispatch message (8) to plugin listeners\n"
)
GOOD_MODULE_LOG = "SkyCRPG Camera 0.0.1 loaded\nruntime check: PASS pinned 1.7.104\n"


def snapshot(root: Path) -> dict[str, bytes | None]:
    """Every file (with content) and directory under root."""
    out = {}
    for path in sorted(root.rglob("*")):
        out[str(path.relative_to(root))] = path.read_bytes() if path.is_file() else None
    return out


class ProfileToolTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.sandbox = self.root / "sandbox"  # everything the tool may see lives here
        steam = self.sandbox / "steam"
        os.environ["STEAM_ROOT"] = str(steam)
        os.environ["SKYCRPG_PROFILE_ROOT"] = str(self.sandbox / "profiles")
        for var in ("SKYRIM_DIR", "SKYRIM_COMPATDATA", "PROTON_DIR", "STEAM_RUNTIME_DIR"):
            os.environ.pop(var, None)
        spec = importlib.util.spec_from_file_location("profile_tool", TOOL)
        self.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.tool)
        self.tool.POLL_SECONDS = 0.1

        game = self.tool.GAME
        (game / "Data").mkdir(parents=True)
        (game / "Data" / "Skyrim.esm").write_bytes(b"esm")
        (game / "SkyrimSE.exe").write_bytes(b"fake exe")
        self.tool.EXPECTED_EXE_SHA256 = hashlib.sha256(b"fake exe").hexdigest()
        self.tool.OWNER_SAVES.mkdir(parents=True)
        (self.tool.OWNER_SAVES / "owner.ess").write_bytes(b"owner save")

        self.skse = self.sandbox / "skse"
        (self.skse / "Data" / "Scripts").mkdir(parents=True)
        (self.skse / "skse64_loader.exe").write_bytes(b"loader")
        (self.skse / "skse64_1_7_104.dll").write_bytes(b"runtime dll")
        (self.skse / "Data" / "Scripts" / "skse.pex").write_bytes(b"pex")
        (self.skse / "skse64_readme.txt").write_text("readme")

        self.dist = self.sandbox / "dist"
        self.dist.mkdir()
        self.make_package("SkyCRPG_Camera", {"SKSE/Plugins/SkyCRPG_Camera.dll": b"camera", "SKSE/Plugins/SkyCRPG_Camera.ini": b"[Debug]"})
        self.make_package("SkyCRPG_UI", {"SKSE/Plugins/SkyCRPG_UI.dll": b"ui", "SKSE/Plugins/SkyCRPG_UI.ini": b"[Debug]"})
        self.assertEqual(self.run_tool("init", "combined", "--save", "owner.ess"), 0)
        self.assertEqual(self.run_tool("init", "camera-only"), 0)

    def tearDown(self):
        self.tmp.cleanup()

    def make_package(self, name: str, files: dict[str, bytes]) -> None:
        for old in self.dist.glob(f"{name}-*.zip"):
            old.unlink()
        with zipfile.ZipFile(self.dist / f"{name}-0.0.1-test.zip", "w") as z:
            for arc, data in files.items():
                z.writestr(arc, data)

    def run_tool(self, *argv: str) -> int:
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = self.tool.main(list(argv))
        self.output = out.getvalue() + err.getvalue()
        return code

    def deploy(self, profile: str = "combined") -> int:
        return self.run_tool("deploy", profile, "--skse", str(self.skse), "--dist", str(self.dist))

    # --- archive path validation --------------------------------------------------------

    def assert_refused_without_writes(self, expected: str) -> None:
        before = snapshot(self.root)
        self.assertEqual(self.deploy(), 1)
        self.assertIn(expected, self.output)
        self.assertEqual(snapshot(self.root), before, "a refused deploy must not write anything")

    def test_traversal_entry_refused_with_zero_writes(self):
        self.make_package("SkyCRPG_Camera", {"../../escaped-outside-game.txt": b"x", "SKSE/Plugins/SkyCRPG_Camera.dll": b"camera"})
        self.assert_refused_without_writes("traversal")

    def test_absolute_and_drive_entries_refused(self):
        for bad in (str(self.root / "abs.txt"), "C:/Windows/evil.dll"):
            with self.subTest(entry=bad):
                self.make_package("SkyCRPG_Camera", {bad: b"x"})
                self.assert_refused_without_writes("absolute")

    def test_backslash_entry_refused(self):
        self.make_package("SkyCRPG_Camera", {"..\\..\\escaped.txt": b"x"})
        self.assert_refused_without_writes("invalid path")

    def test_symlinked_data_directory_cannot_redirect_writes(self):
        outside = self.root / "outside"
        outside.mkdir()
        (self.tool.GAME / "Data" / "SKSE").symlink_to(outside, target_is_directory=True)
        self.assert_refused_without_writes("resolves outside")

    def test_colliding_destinations_refused(self):
        self.make_package("SkyCRPG_UI", {"skse/plugins/skycrpg_camera.dll": b"ui"})
        self.assert_refused_without_writes("claimed by both")

    def test_symlink_in_skse_dir_refused(self):
        (self.skse / "Data" / "Scripts" / "link.pex").symlink_to(self.root / "secret")
        self.assert_refused_without_writes("symlink")

    def test_existing_different_file_refused(self):
        (self.tool.GAME / "skse64_loader.exe").write_bytes(b"someone else's loader")
        self.assert_refused_without_writes("refusing to overwrite")

    # --- round trip and SkyrimCustom.ini ownership --------------------------------------

    def test_deploy_undeploy_round_trip_restores_game_tree(self):
        before = snapshot(self.tool.GAME)
        self.assertEqual(self.deploy(), 0)
        self.assertTrue((self.tool.GAME / "Data" / "SKSE" / "Plugins" / "SkyCRPG_UI.dll").is_file())
        self.assertTrue(self.tool.CUSTOM_INI.is_file())
        self.assertEqual(self.run_tool("undeploy"), 0)
        self.assertEqual(snapshot(self.tool.GAME), before)
        self.assertFalse(self.tool.CUSTOM_INI.exists())
        self.assertEqual((self.tool.OWNER_SAVES / "owner.ess").read_bytes(), b"owner save")
        self.assertFalse(self.tool.ACTIVE.exists())

    def test_preexisting_identical_file_is_not_removed(self):
        (self.tool.GAME / "skse64_loader.exe").write_bytes(b"loader")
        self.assertEqual(self.deploy(), 0)
        self.assertEqual(self.run_tool("undeploy"), 0)
        self.assertEqual((self.tool.GAME / "skse64_loader.exe").read_bytes(), b"loader")

    def test_edited_custom_ini_is_preserved_and_flagged(self):
        self.assertEqual(self.deploy(), 0)
        edited = self.tool.CUSTOM_INI.read_bytes() + b"[Display]\r\nfGamma=1.7\r\n"
        self.tool.CUSTOM_INI.write_bytes(edited)
        self.assertEqual(self.run_tool("undeploy"), 3)
        self.assertEqual(self.tool.CUSTOM_INI.read_bytes(), edited)
        self.assertFalse((self.tool.GAME / "skse64_loader.exe").exists(), "unchanged files are still removed")
        self.assertEqual(json.loads(self.tool.ACTIVE.read_text())["state"], "needs_reconciliation")
        self.assertEqual(self.deploy(), 1, "a profile awaiting reconciliation blocks the next deploy")
        self.assertEqual(self.run_tool("undeploy", "--release-kept"), 0)
        self.assertEqual(self.tool.CUSTOM_INI.read_bytes(), edited)
        self.assertFalse(self.tool.ACTIVE.exists())

    def test_changed_deployed_file_is_kept(self):
        self.assertEqual(self.deploy(), 0)
        dll = self.tool.GAME / "Data" / "SKSE" / "Plugins" / "SkyCRPG_Camera.dll"
        dll.write_bytes(b"replaced by user")
        self.assertEqual(self.run_tool("undeploy"), 3)
        self.assertEqual(dll.read_bytes(), b"replaced by user")

    # --- interrupted deploy ------------------------------------------------------------

    def interrupted_deploy(self, fail_on_call: int, after_link: bool) -> None:
        real_link, calls = os.link, {"n": 0}

        def flaky_link(src, dst):
            calls["n"] += 1
            if calls["n"] == fail_on_call:
                if after_link:
                    real_link(src, dst)
                raise KeyboardInterrupt  # simulated kill
            return real_link(src, dst)

        self.tool.os.link = flaky_link
        try:
            with self.assertRaises(KeyboardInterrupt), redirect_stdout(io.StringIO()):
                self.deploy()
        finally:
            self.tool.os.link = real_link

    def test_interruption_at_each_write_stage_is_recoverable(self):
        before = snapshot(self.tool.GAME)
        for fail_on_call in (1, 3):
            for after_link in (False, True):
                with self.subTest(file=fail_on_call, after_link=after_link):
                    self.interrupted_deploy(fail_on_call, after_link)
                    self.assertEqual(self.run_tool("undeploy"), 0, self.output)
                    self.assertEqual(snapshot(self.tool.GAME), before)
                    self.assertFalse(self.tool.ACTIVE.exists())

    def test_interruption_before_temp_cleanup_is_recoverable(self):
        before = snapshot(self.tool.GAME)
        real_unlink, state = Path.unlink, {"done": False}

        def flaky_unlink(path, *a, **k):
            if not state["done"] and path.name.endswith(self.tool.TEMP_SUFFIX):
                state["done"] = True
                raise KeyboardInterrupt
            return real_unlink(path, *a, **k)

        Path.unlink = flaky_unlink
        try:
            with self.assertRaises(KeyboardInterrupt), redirect_stdout(io.StringIO()):
                self.deploy()
        finally:
            Path.unlink = real_unlink
        self.assertEqual(self.run_tool("undeploy"), 0, self.output)
        self.assertEqual(snapshot(self.tool.GAME), before)

    # --- launch results ----------------------------------------------------------------

    def launch_with(self, script: str, *args: str) -> tuple[int, dict]:
        self.tool.save_active({"profile": "camera-only", "state": "deployed", "files": [], "dirs": [], "kept": []})
        logs = self.tool.SKSE_LOGS
        logs.mkdir(parents=True, exist_ok=True)
        (self.sandbox / "good_skse.log").write_text(GOOD_SKSE_LOG)
        (self.sandbox / "good_module.log").write_text(GOOD_MODULE_LOG)
        env = f'L="{logs}"; S="{self.sandbox}"; '
        self.tool.launch_command = lambda: ["bash", "-c", env + script]
        code = self.run_tool("launch", *args)
        run = json.loads(sorted((self.tool.PROFILE_ROOT / "camera-only" / "runs").glob("*/run.json"))[-1].read_text())
        self.assertEqual(run["leftover_pids"], [])
        self.assertEqual(self.tool.skyrim_pids(), [])
        return code, run

    WRITE_GOOD = 'cp "$S/good_skse.log" "$L/skse64.log"; cp "$S/good_module.log" "$L/SkyCRPG_Camera.log"; '

    def test_verified_load_succeeds(self):
        code, run = self.launch_with(self.WRITE_GOOD + "exec -a SkyrimSE.exe sleep 30", "--timeout", "10", "--settle", "0")
        self.assertEqual((code, run["result"], run["success"]), (0, "LOADED", True))

    def test_crash_during_settle_fails(self):
        script = self.WRITE_GOOD + "exec -a SkyrimSE.exe bash -c 'sleep 1; exit 1'"
        code, run = self.launch_with(script, "--timeout", "10", "--settle", "2")
        self.assertEqual(run["load_problems"], [], "positive logs must not mask a later crash")
        self.assertEqual((code, run["result"], run["success"], run["exit_code"]), (1, "EXITED", False, 1))

    def test_early_exit_fails(self):
        code, run = self.launch_with("exit 1", "--timeout", "10")
        self.assertEqual((code, run["result"], run["exit_code"]), (1, "EXITED", 1))

    def test_timeout_fails(self):
        code, run = self.launch_with("exec -a SkyrimSE.exe sleep 30", "--timeout", "2")
        self.assertEqual((code, run["result"]), (1, "TIMEOUT"))

    def test_rejected_plugin_fails_even_with_fresh_logs(self):
        rejected = GOOD_SKSE_LOG.replace("loaded correctly (handle 1)", "disabled, fatal error occurred while loading plugin")
        script = f"printf '%s' '{rejected}' > \"$L/skse64.log\"; cp \"$S/good_module.log\" \"$L/SkyCRPG_Camera.log\"; exec -a SkyrimSE.exe sleep 30"
        code, run = self.launch_with(script, "--timeout", "3", "--settle", "0")
        self.assertEqual((code, run["result"]), (1, "LOAD_FAILED"))
        self.assertTrue(any("loaded correctly" in p for p in run["load_problems"]))

    def test_stale_logs_from_earlier_run_are_ignored(self):
        logs = self.tool.SKSE_LOGS
        logs.mkdir(parents=True, exist_ok=True)
        for name, text in (("skse64.log", GOOD_SKSE_LOG), ("SkyCRPG_Camera.log", GOOD_MODULE_LOG)):
            (logs / name).write_text(text)
            os.utime(logs / name, (1, 1))
        code, run = self.launch_with("exec -a SkyrimSE.exe sleep 30", "--timeout", "2")
        self.assertEqual((code, run["result"]), (1, "TIMEOUT"))

    def test_interactive_quit_after_load_succeeds(self):
        code, run = self.launch_with(self.WRITE_GOOD + "exec -a SkyrimSE.exe sleep 1", "--interactive", "--timeout", "10")
        self.assertEqual((code, run["result"]), (0, "QUIT"))

    def test_interactive_crash_after_load_fails(self):
        script = self.WRITE_GOOD + "exec -a SkyrimSE.exe bash -c 'sleep 1; exit 1'"
        code, run = self.launch_with(script, "--interactive", "--timeout", "10")
        self.assertEqual(run["load_problems"], [], "positive logs must not mask a later crash")
        self.assertEqual((code, run["result"], run["success"], run["exit_code"]), (1, "EXITED", False, 1))

    def test_interactive_exit_without_load_fails(self):
        code, run = self.launch_with("exit 0", "--interactive", "--timeout", "10")
        self.assertEqual((code, run["result"]), (1, "EXITED"))


if __name__ == "__main__":
    unittest.main()
