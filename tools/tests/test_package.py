"""Synthetic inputs verify package integrity and deterministic assembly, never gameplay."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import runpy
import struct
import subprocess
import tempfile
import warnings
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location("package", Path(__file__).parents[1] / "package.py")
pkg = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pkg)
BUILDER = runpy.run_path(str(pkg.ROOT / "modules/ui/tools/build_scaleform.py"))
COMMIT = subprocess.check_output(["git", "-C", str(pkg.ROOT), "rev-parse", "HEAD"], text=True).strip()


def pe(commit=COMMIT, imported=None):
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x86, 1)
    struct.pack_into("<H", data, 0x94, 240)
    struct.pack_into("<H", data, 0x98, 0x20b)
    struct.pack_into("<IIII", data, 0x98 + 240 + 8, 512, 0x1000, 512, 512)
    if imported:
        struct.pack_into("<I", data, 0x98 + 120, 0x1000)
        struct.pack_into("<I", data, 512 + 12, 0x1050)
        data[592:592 + len(imported) + 1] = imported.encode() + b"\0"
    data[800:841] = commit.encode() + b"\0"
    return bytes(data)


def bsa():
    folder = b"interface\0"
    filename = b"inventorymenu.swf\0"
    header = struct.pack("<9I", 0x00415342, 105, 36, 3, 1, 1, len(folder), len(filename), 0)
    record = struct.pack("<QIIQ", 0, 1, 0, 60)
    return header + record + bytes([len(folder)]) + folder + bytes(16) + filename


def vcpkg_fixture(build):
    build.mkdir(parents=True, exist_ok=True)
    (build / "CMakeCache.txt").write_text("VCPKG_TARGET_TRIPLET:STRING=x64-windows-static\n")
    for port in pkg.VCPKG_PORTS:
        notice = build / "vcpkg_installed" / pkg.VCPKG_TRIPLET / "share" / port / "copyright"
        notice.parent.mkdir(parents=True, exist_ok=True)
        notice.write_text(f"{port} notice\n")


class PackageTests(unittest.TestCase):
    def test_legacy_notice_in_both_packages(self):
        for name, module in pkg.PACKAGES.items():
            with tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                vcpkg_fixture(build)
                dll = build / module / (name + ".dll")
                dll.parent.mkdir(parents=True)
                dll.write_bytes(b"fixture")
                path = f"Docs/{name}/CommonLibSSE-legacy-MIT"
                self.assertIn(path, pkg.expected_paths(name, full=True))
                self.assertEqual(pkg.package_files(name, module, build)[path].read_bytes(),
                                 (pkg.ROOT / "LICENSES/CommonLibSSE-legacy-MIT").read_bytes())

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.providers = self.root / "providers"
        self.locations = {}
        for provider, (pattern, roots) in pkg.PROVIDERS.items():
            name = pattern.replace("*", "")
            directory = self.providers / name
            self.locations[provider] = directory
            for rel in roots:
                target = directory / rel / "Scripts/provider.pex"
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"external")
        skyui = self.locations["SkyUI Community 6.11"] / "SkyUI_SE.bsa"
        skyui.write_bytes(bsa())
        tdm = self.locations["True Directional Movement 2.3.1"] / "MCM/Config/TrueDirectionalMovement/settings.ini"
        tdm.parent.mkdir(parents=True)
        tdm.write_text("[DirectionalMovement]\nuAdjustCameraYawDuringMovement = 1\nfUnrelated = 42\n")
        self.swfs = self.root / "swf"
        paths = subprocess.check_output(["git", "-C", str(pkg.ROOT), "ls-tree", "--name-only", COMMIT,
                                         "modules/ui/scaleform/"], text=True).splitlines()
        self.proof = {"skyui_commit": BUILDER["PIN"], "status": "COMPILED_NOT_RUN",
                      "source_commit": COMMIT, "source_dirty": False, "owner": "DYL-171", "source_sha256": BUILDER["SOURCES"],
                      "base": "Nexus6.11 acquired BSA", "inputs": BUILDER["NEXUS_BASE_SWFS"],
                      "patch_sha256": {Path(p).name: pkg.sha256(pkg.git_bytes(COMMIT, p)) for p in paths if p.endswith(".inc")},
                      "targets": list(pkg.SWF_PATHS), "outputs": {}, "ffdec_cli_sha256": "a" * 64}
        for swf in pkg.SWF_PATHS:
            target = self.swfs / "Data/Interface" / swf
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"FWS\x09" + bytes(24))
            self.proof["outputs"][swf] = pkg.sha256(target.read_bytes())
        self.write_proof()

    def write_proof(self):
        (self.swfs / "build.json").write_bytes(pkg.json_bytes(self.proof))

    def args(self, module="ui"):
        name = "SkyCRPG_UI" if module == "ui" else "SkyCRPG_Camera"
        files = {p: b"notice\n" for p in pkg.native_paths(name)}
        files[f"SKSE/Plugins/{name}.dll"] = pe()
        files[f"SKSE/Plugins/{name}.ini"] = b"[Debug]\niLogLevel=1\n"
        archive = self.root / f"{name}.zip"
        pkg.write_zip(archive, files)
        manifest = {"package": name, "source_commit": COMMIT, "install_root": "Data/",
                    "archive": archive.name, "archive_sha256": pkg.sha256(archive.read_bytes()),
                    "files": pkg.hashes(files), "dll_imports": []}
        manifest_path = self.root / f"{name}.manifest.json"
        manifest_path.write_bytes(pkg.json_bytes(manifest))
        return argparse.Namespace(commit=COMMIT, module=module, native_zip=archive, native_manifest=manifest_path,
                                  providers=self.providers, swf_dir=self.swfs, swf_owner="DYL-171",
                                  out=self.root / module, allow_mismatch_for_dry_run=False)

    def test_ui_assembly_manifest_and_data_relative_paths(self):
        args = self.args()
        manifest = pkg.assemble(args)
        self.assertEqual(manifest["version"], "0.1.0-rc.1")
        self.assertTrue(manifest["candidate_eligible"])
        self.assertEqual(manifest["distribution"], "internal-candidate")
        self.assertEqual(manifest["requires_skycrpg_modules"], [])
        self.assertEqual(manifest["requires"], ["SKSE64 2.3.1", "Address Library 13", "SkyUI Community 6.11"])
        self.assertEqual(manifest["optional"], ["Infinity UI NG 2.1.3 (Common + Line-17)",
                         "Local Map Upgrade settings fork 1.3.4 (Common + Line-17)",
                         "Dragon's Eye Minimap settings fork 1.7.5 (Common + Line-17)"])
        self.assertEqual(set(manifest["files"]), pkg.expected_paths("SkyCRPG_UI", True))
        self.assertEqual(manifest["overrides"]["Interface/inventorymenu.swf"]["provider"], "SkyUI Community 6.11")
        self.assertEqual(manifest["asset_provenance"]["outputs"], self.proof["outputs"])
        self.assertNotIn(str(self.root), json.dumps(manifest))
        with zipfile.ZipFile(args.out / manifest["archive"]) as z:
            self.assertFalse(any(p.startswith("Data/") for p in z.namelist()))
        sums = (args.out / "SHA256SUMS").read_text()
        self.assertIn(manifest["archive_sha256"], sums)
        self.assertIn(pkg.sha256((args.out / "SkyCRPG_UI.manifest.json").read_bytes()), sums)

    def test_camera_assembly_preserves_provider_keys_and_independence(self):
        args = self.args("camera")
        manifest = pkg.assemble(args)
        with zipfile.ZipFile(args.out / manifest["archive"]) as z:
            data = z.read("MCM/Config/TrueDirectionalMovement/settings.ini")
            self.assertIn(b"fUnrelated = 42", data)
            self.assertIn(b"uAdjustCameraYawDuringMovement = 0", data)
        self.assertEqual(manifest["requires"], ["SKSE64 2.3.1", "Address Library 13", "SkyUI Community 6.11",
                         "SmoothCam 1.8.1", "True Directional Movement 2.3.1", "MCM Helper 1.6.3"])
        pkg.check_independence({"camera": manifest["files"], "ui": pkg.expected_paths("SkyCRPG_UI", True)})
        with self.assertRaisesRegex(ValueError, "shared"):
            pkg.check_independence({"camera": ["a.ini"], "ui": ["A.INI"]})

    def test_native_archive_and_file_integrity(self):
        args = self.args()
        args.native_zip.write_bytes(args.native_zip.read_bytes() + b"tamper")
        with self.assertRaisesRegex(ValueError, "archive hash mismatch"):
            pkg.assemble(args)
        args = self.args()
        m = json.loads(args.native_manifest.read_bytes())
        m["files"]["SKSE/Plugins/SkyCRPG_UI.ini"] = "b" * 64
        args.native_manifest.write_bytes(pkg.json_bytes(m))
        with self.assertRaisesRegex(ValueError, "file hashes"):
            pkg.assemble(args)

    def test_camera_concatenated_commit_literal_and_exact_boundaries(self):
        camera = pe().replace(COMMIT.encode() + b"\0", b"source " + COMMIT.encode() + b"; startup")
        self.assertTrue(pkg.dll_has_commit(camera, COMMIT))
        self.assertFalse(pkg.dll_has_commit(camera, "a" * 40))
        self.assertFalse(pkg.dll_has_commit(b"source " + COMMIT.encode() + b"0;", COMMIT))
        self.assertTrue(pkg.dll_has_commit(pe(), COMMIT))

    def test_dll_commit_and_import_guard(self):
        args = self.args()
        with zipfile.ZipFile(args.native_zip) as z:
            files = {p: z.read(p) for p in z.namelist()}
        files["SKSE/Plugins/SkyCRPG_UI.dll"] = pe("b" * 40)
        pkg.write_zip(args.native_zip, files)
        m = json.loads(args.native_manifest.read_bytes())
        m.update(archive_sha256=pkg.sha256(args.native_zip.read_bytes()), files=pkg.hashes(files))
        args.native_manifest.write_bytes(pkg.json_bytes(m))
        with self.assertRaisesRegex(ValueError, "DLL does not contain"):
            pkg.assemble(args)
        files["SKSE/Plugins/SkyCRPG_UI.dll"] = pe(imported="SkyCRPG_Camera.dll")
        with self.assertRaisesRegex(ValueError, "cross-module"):
            pkg.check_contents("SkyCRPG_UI", files)

    def test_swf_provenance_failures(self):
        for key, value in (("skyui_commit", "b" * 40), ("inputs", {}), ("source_sha256", {}),
                           ("owner", "wrong"), ("status", "PREPARED"), ("targets", []),
                           ("outputs", {}), ("patch_sha256", {}), ("source_commit", None),
                           ("source_dirty", True), ("source_dirty", None)):
            with self.subTest(key=key):
                original = copy.deepcopy(self.proof)
                self.proof[key] = value
                self.write_proof()
                with self.assertRaises(ValueError):
                    pkg.assemble(self.args())
                self.proof = original
                self.write_proof()
        path = self.swfs / "Data/Interface/hudmenu.swf"
        path.write_bytes(b"FWS" + b"tampered movie")
        with self.assertRaisesRegex(ValueError, "SWF hash mismatch"):
            pkg.assemble(self.args())

    def test_dry_run_cannot_masquerade_as_candidate_or_bypass_hashes(self):
        args = self.args()
        self.proof.pop("source_commit")
        self.write_proof()
        args.allow_mismatch_for_dry_run = True
        m = pkg.assemble(args)
        self.assertFalse(m["candidate_eligible"])
        self.assertTrue(m["dry_run"])
        self.assertTrue(m["mismatches"])
        self.assertIn("DRY-RUN", m["archive"])
        self.proof["inputs"] = {}
        self.write_proof()
        with self.assertRaises(ValueError):
            pkg.assemble(args)

    def test_provider_collisions_are_case_insensitive_and_provider_specific(self):
        allowed = {"Interface/inventorymenu.swf": b""}
        self.assertTrue(pkg.collisions(allowed, {"interface/inventorymenu.swf": {"SkyUI Community 6.11"}}))
        with self.assertRaises(ValueError):
            pkg.collisions(allowed, {"interface/inventorymenu.swf": {"Other HUD"}})
        with self.assertRaises(ValueError):
            pkg.collisions({"SKSE/Plugins/SkyCRPG_UI.ini": b""}, {"skse/plugins/skycrpg_ui.ini": {"SkyUI Community 6.11"}})
        for rule in pkg.OVERRIDES.values():
            self.assertTrue(all(rule.get(key) for key in ("provider", "reason", "load_order")))

    def test_bsa_inventory_and_missing_providers(self):
        inventory, digest, _ = pkg.provider_inventory(self.providers)
        self.assertIn("SkyUI Community 6.11", inventory["interface/inventorymenu.swf"])
        self.assertEqual(len(digest), 64)
        with self.assertRaises(ValueError):
            pkg.provider_inventory(self.root / "missing")

    def test_junk_paths_unexpected_files_and_personal_text(self):
        args = self.args()
        with zipfile.ZipFile(args.native_zip) as z:
            base = {p: z.read(p) for p in z.namelist()}
        for bad in ("x.pdb", "x.ilk", "x.obj", "build/x", "CMakeFiles/x", "__pycache__/x", ".DS_Store", "notes.txt",
                    "../x", "/tmp/x", "C:/Users/test/x", "x\\y"):
            with self.subTest(path=bad), self.assertRaises(ValueError):
                pkg.check_contents("SkyCRPG_UI", {**base, bad: b"junk"})
        for text in (b"/home/private/settings", b"C:\\Users\\private\\settings", b"/Users/private/settings",
                     b"username=private", "/home/private".encode("utf-16")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                pkg.check_contents("SkyCRPG_UI", {**base, "SKSE/Plugins/SkyCRPG_UI.ini": text})
        with patch.object(pkg.getpass, "getuser", return_value="privateperson"):
            with self.assertRaises(ValueError):
                pkg.check_contents("SkyCRPG_UI", {**base, "SKSE/Plugins/SkyCRPG_UI.ini": b"author privateperson"})

    def test_generated_manifest_rejects_personal_owner(self):
        args = self.args()
        args.swf_owner = "privateperson"
        self.proof["owner"] = "privateperson"
        self.write_proof()
        with patch.object(pkg.getpass, "getuser", return_value="privateperson"):
            with self.assertRaisesRegex(ValueError, "personal user name"):
                pkg.assemble(args)

    def test_duplicate_and_symlink_zip_entries_rejected(self):
        for symlink in (False, True):
            args = self.args()
            with warnings.catch_warnings(), zipfile.ZipFile(args.native_zip, "a") as z:
                warnings.simplefilter("ignore", UserWarning)
                info = zipfile.ZipInfo("link" if symlink else "SKSE/Plugins/SkyCRPG_UI.ini")
                info.external_attr = (0o120777 if symlink else 0o100644) << 16
                z.writestr(info, b"somewhere")
            m = json.loads(args.native_manifest.read_bytes())
            m["archive_sha256"] = pkg.sha256(args.native_zip.read_bytes())
            args.native_manifest.write_bytes(pkg.json_bytes(m))
            with self.assertRaises(ValueError):
                pkg.assemble(args)

    def test_native_packages_keep_independent_payloads_and_checksum_both(self):
        build = self.root / "build"
        vcpkg_fixture(build)
        for name, module in pkg.PACKAGES.items():
            path = build / module / "Release" / f"{name}.dll"
            path.parent.mkdir(parents=True)
            path.write_bytes(pe())
        args = argparse.Namespace(build_dir=build, module=None, commit=COMMIT, out=self.root / "native")
        original_git_bytes = pkg.git_bytes
        def fixture_git_bytes(commit, path):
            if path == "LICENSES/CommonLibSSE-legacy-MIT":
                return (pkg.ROOT / path).read_bytes()
            return original_git_bytes(commit, path)
        with patch.object(pkg, "git_bytes", side_effect=fixture_git_bytes):
            pkg.native(args)
        sums = (args.out / "SHA256SUMS").read_text()
        for name in pkg.PACKAGES:
            manifest = json.loads((args.out / f"{name}.manifest.json").read_bytes())
            self.assertFalse(manifest["candidate_eligible"])
            self.assertEqual(manifest["artifact_kind"], "native")
            self.assertEqual(set(manifest["files"]), pkg.native_paths(name))
            self.assertIn(manifest["archive"], sums)
            self.assertIn(manifest["archive_sha256"], sums)
            with zipfile.ZipFile(args.out / manifest["archive"]) as archive:
                for port in pkg.VCPKG_PORTS:
                    self.assertEqual(archive.read(f"Docs/{name}/third-party/{port}-copyright"),
                                     f"{port} notice\n".encode())
        duplicate = build / "modules/ui/Other/SkyCRPG_UI.dll"
        duplicate.parent.mkdir()
        duplicate.write_bytes(pe())
        args.module = "ui"
        with self.assertRaisesRegex(ValueError, "expected one"):
            pkg.native(args)

    def test_missing_vcpkg_notice_and_wrong_triplet_fail(self):
        build = self.root / "build"
        vcpkg_fixture(build)
        dll = build / "modules/ui/Release/SkyCRPG_UI.dll"
        dll.parent.mkdir(parents=True)
        dll.write_bytes(pe())
        args = argparse.Namespace(build_dir=build, module="ui", commit=COMMIT, out=self.root / "native")
        notice = build / "vcpkg_installed" / pkg.VCPKG_TRIPLET / "share/fmt/copyright"
        notice.unlink()
        with self.assertRaisesRegex(ValueError, "missing vcpkg copyright notice for fmt"):
            pkg.native(args)
        (build / "CMakeCache.txt").write_text("VCPKG_TARGET_TRIPLET:STRING=x64-windows\n")
        with self.assertRaisesRegex(ValueError, "expected VCPKG_TARGET_TRIPLET=x64-windows-static"):
            pkg.native(args)

    def test_verify_repro_compares_all_output_bytes_and_cleans_temporary_dirs(self):
        args = self.args()
        self.assertEqual(pkg.verify_repro(args)["result"], "PASS")
        self.assertEqual([p.name for p in args.out.iterdir()], ["repro.json"])
        original = pkg.write_zip
        count = 0
        def unstable(path, files):
            nonlocal count
            original(path, files)
            count += 1
            with path.open("ab") as stream:
                stream.write(bytes([count]))
        with patch.object(pkg, "write_zip", side_effect=unstable):
            with self.assertRaisesRegex(ValueError, "not byte-identical"):
                pkg.verify_repro(args)
        self.assertFalse((args.out / "repro.json").exists())


if __name__ == "__main__":
    unittest.main()
