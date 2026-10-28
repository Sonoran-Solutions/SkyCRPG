#!/usr/bin/env python3
"""Build native CI payloads and assemble independent internal candidates.

ZIP members are relative to Data/ (no enclosing Data folder). Native mode is
backward compatible with the original --build-dir invocation. Assemble and
verify-repro consume a CI ZIP/manifest and read-only extracted providers; UI
also requires a complete provenance-bound build_scaleform.py output.
"""

import argparse
import hashlib
import json
import getpass
import re
import subprocess
import tempfile
import configparser
import io
import struct
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = "0.1.0-rc.1"
RUNTIME = "SkyrimSE.exe 1.7.104.0 (Steam)"

PACKAGES = {
    "SkyCRPG_Camera": "modules/camera",
    "SkyCRPG_UI": "modules/ui",
}

FIXED_TIME = (1980, 1, 1, 0, 0, 0)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pe_imports(data: bytes) -> list[str]:
    """Return the DLL names in a PE32+ import directory."""
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x20B:
        raise ValueError("not PE32+ (x64)")
    import_rva = struct.unpack_from("<I", data, opt + 120)[0]
    table = opt + opt_size

    def offset(rva: int) -> int:
        for i in range(sections):
            s = table + i * 40
            vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, s + 8)
            if vaddr <= rva < vaddr + max(vsize, rsize):
                return rva - vaddr + raddr
        raise ValueError(f"RVA {rva:#x} outside sections")

    names, pos = [], offset(import_rva) if import_rva else None
    while pos is not None:
        name_rva = struct.unpack_from("<I", data, pos + 12)[0]
        if name_rva == 0:
            break
        start = offset(name_rva)
        names.append(data[start:data.index(b"\0", start)].decode("ascii"))
        pos += 20
    return names


def package_files(name: str, module_dir: str, build_dir: Path) -> dict[str, Path]:
    dlls = sorted(build_dir.glob(f"{module_dir}/**/{name}.dll"))
    require(len(dlls) == 1, f"expected one {name}.dll under {build_dir / module_dir}, found {len(dlls)}")
    dll = dlls[0]
    return {
        f"SKSE/Plugins/{name}.dll": dll,
        f"SKSE/Plugins/{name}.ini": ROOT / module_dir / "config" / f"{name}.ini",
        f"Docs/{name}/CommonLibSSE-NG-COPYING": ROOT / "LICENSES/CommonLibSSE-NG-COPYING",
        f"Docs/{name}/CommonLibSSE-NG-EXCEPTIONS.md": ROOT / "LICENSES/CommonLibSSE-NG-EXCEPTIONS.md",
        f"Docs/{name}/build-inputs.md": ROOT / "cmake/README.md",
    }


def write_zip(path: Path, files: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for arcname in sorted(files):
            info = zipfile.ZipInfo(arcname, FIXED_TIME)
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, files[arcname])


# Deliberate file winners only. Providers themselves remain external installs.
OVERRIDES = {
    **{f"interface/{p}": {
        "provider": "SkyUI Community 6.11",
        "reason": "Dependency-requiring player inventory/HUD patch",
        "load_order": "SkyCRPG UI loose movies must win over SkyUI's BSA",
    } for p in ("inventorymenu.swf", "skyui/inventorylists.swf", "hudmenu.swf")},
    "skse/plugins/smoothcampreset0.json": {
        "provider": "SmoothCam 1.8.1", "reason": "Original Camera framing preset in slot 0",
        "load_order": "Camera wins slot 0; back up occupied slot and reconcile user edits",
    },
    "skse/plugins/smoothcam.json": {
        "provider": "SmoothCam 1.8.1", "reason": "Original Camera startup framing keys",
        "load_order": "Camera wins declared keys; merge existing settings with game closed",
    },
    "mcm/config/truedirectionalmovement/settings.ini": {
        "provider": "True Directional Movement 2.3.1", "reason": "Movement-only Camera ownership settings",
        "load_order": "Camera merged settings win; preserve unrelated and later user keys",
    },
}
PROVIDERS = {
    "SkyUI Community 6.11": ("SkyUI-12604-6-11-*.zip", (".",)),
    "SmoothCam 1.8.1": ("SmoothCam-Beta1.8.1.zip", ("Release_Package/00 Data",)),
    "True Directional Movement 2.3.1": ("True Directional Movement *2.3.1*.7z", (".",)),
    "MCM Helper 1.6.3": ("MCM Helper *1.6.3*.7z", ("SkyrimSE", "ESL", "BSA", "Data")),
    "Address Library 13": ("Address Library *v13*.zip", (".",)),
    "Infinity UI NG 2.1.3": ("Infinity UI *2.1.3*.zip", ("Common", "Line-17")),
    "Local Map Upgrade settings fork 1.3.4": ("Local Map Upgrade *1.3.4*.zip", ("Common", "Line-17")),
    "Dragon's Eye Minimap settings fork 1.7.5": ("Dragon's Eye Minimap *1.7.5*.zip", ("Common", "Line-17")),
}
COMMON_REQUIREMENTS = ["SKSE64 2.3.1", "Address Library 13", "SkyUI Community 6.11"]
CAMERA_REQUIREMENTS = ["SmoothCam 1.8.1", "True Directional Movement 2.3.1", "MCM Helper 1.6.3"]
UI_OPTIONAL = ["Infinity UI NG 2.1.3 (Common + Line-17)",
                   "Local Map Upgrade settings fork 1.3.4 (Common + Line-17)",
                   "Dragon's Eye Minimap settings fork 1.7.5 (Common + Line-17)"]
CAMERA_CONFIGS = {
    "SKSE/Plugins/SmoothCam.json": "SmoothCam.json",
    "SKSE/Plugins/SmoothCamPreset0.json": "SmoothCamPreset0.json",
    "MCM/Config/TrueDirectionalMovement/settings.ini": "TrueDirectionalMovement-settings.fragment.ini",
}
SWF_PATHS = ("inventorymenu.swf", "skyui/inventorylists.swf", "hudmenu.swf")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git_bytes(commit, path):
    return subprocess.check_output(["git", "-C", str(ROOT), "show", f"{commit}:{path}"])


def commit_sha(value):
    require(re.fullmatch(r"[0-9a-f]{40}", value) is not None, "commit must be a full lowercase Git SHA")
    return value


def safe_path(path):
    require(bool(path) and "\\" not in path and ":" not in path and "\0" not in path and
            all(p not in ("", ".", "..") for p in path.split("/")), f"unsafe install path: {path!r}")
    return path.lower()


def hashes(files):
    return {p: sha256(data) for p, data in sorted(files.items())}


def native_paths(name):
    return {f"SKSE/Plugins/{name}.dll", f"SKSE/Plugins/{name}.ini",
            *[f"Docs/{name}/{p}" for p in ("CommonLibSSE-NG-COPYING", "CommonLibSSE-NG-EXCEPTIONS.md", "build-inputs.md")]}


def expected_paths(name, full=False):
    paths = native_paths(name)
    if full:
        paths |= ({f"Interface/{p}" for p in SWF_PATHS} if name == "SkyCRPG_UI" else set(CAMERA_CONFIGS))
        paths |= {f"Docs/{name}/{p}" for p in ("LICENSE", "LICENSING.md", "THIRD_PARTY.md")}
    return paths


def check_independence(packages):
    owners = {}
    for name, files in packages.items():
        for path in files:
            key = safe_path(path)
            require(key not in owners, f"install path {path} shared by {owners.get(key)} and {name}")
            owners[key] = name


def check_text(path, data):
    # Text payloads must be decodable; do not let a binary hide in an INI.
    text = data.decode("utf-8-sig") if not data.startswith((b"\xff\xfe", b"\xfe\xff")) else data.decode("utf-16")
    require(not re.search(r"(?i)(/(?:home|users)/|[a-z]:[\\/]+users[\\/]+|/root/)", text),
            f"personal home path in {path}")
    users = {getpass.getuser(), Path.home().name}
    users -= {"root", "runner", "runneradmin"}
    require(not any(re.search(r"(?i)(?<![\w])" + re.escape(user) + r"(?![\w])", text)
                    for user in users if user), f"personal user name in {path}")
    require(not re.search(r"(?im)^\s*(?:username|user_name|author_user)\s*[:=]", text),
            f"personal user field in {path}")


def check_contents(name, files, full=False):
    seen = set()
    for path, data in files.items():
        key = safe_path(path)
        require(key not in seen, f"duplicate install path: {path}")
        seen.add(key)
        parts = key.split("/")
        require(not any(p.startswith((".", "~")) or p in
                        ("build", "cmake", "cmakefiles", "__pycache__", "node_modules") for p in parts)
                and not key.endswith((".pdb", ".ilk", ".obj", ".pyc", ".swp", ".bak")), f"junk path: {path}")
        if not key.endswith((".dll", ".swf")):
            check_text(path, data)
    require(set(files) == expected_paths(name, full),
            f"unexpected/missing contents: {sorted(set(files) ^ expected_paths(name, full))}")
    imports = pe_imports(files[f"SKSE/Plugins/{name}.dll"])
    require(not any(d.lower().startswith("skycrpg") for d in imports), "cross-module DLL import")
    return sorted(imports)


def dll_has_commit(data, commit):
    # UI ModuleInfo stores a standalone string; Camera concatenates the macro
    # into its startup log literal. Both forms are used by the merged sources.
    encoded = commit.encode("ascii")
    return encoded + b"\0" in data or b"source " + encoded + b";" in data


def read_native(archive, manifest_path, name, commit, mismatches, dry_run):
    manifest = json.loads(manifest_path.read_bytes())
    require(manifest.get("package") == name and manifest.get("install_root") == "Data/", "wrong native package/root")
    require(manifest.get("archive") == archive.name, "native archive filename differs from manifest")
    require(sha256(archive.read_bytes()) == manifest.get("archive_sha256"), "native archive hash mismatch")
    source = commit_sha(manifest["source_commit"])
    if source != commit:
        require(dry_run, "native source commit differs from --commit")
        mismatches.append(f"native source commit {source} differs from requested {commit}")
    with zipfile.ZipFile(archive) as z:
        files = {}
        for info in z.infolist():
            safe_path(info.filename)
            require(not info.is_dir() and ((info.external_attr >> 16) & 0o170000) in (0, 0o100000),
                    "native archive contains a directory, symlink or special file")
            require(info.filename not in files, "duplicate native ZIP member")
            files[info.filename] = z.read(info)
    imports = check_contents(name, files)
    require(hashes(files) == manifest.get("files"), "native file hashes differ from manifest")
    dll = files[f"SKSE/Plugins/{name}.dll"]
    require(dll_has_commit(dll, source), "DLL does not contain its manifest source commit")
    require(imports == sorted(manifest.get("dll_imports", [])), "DLL imports differ from manifest")
    return files, manifest


def bsa_paths(path):
    """Read only the Skyrim SE BSA105 name tables, without extracting assets."""
    data = path.read_bytes()
    magic, version, offset, flags, folders, count, _, _, _ = struct.unpack_from("<9I", data)
    require(magic == 0x00415342 and version == 105 and flags & 3 == 3, "unsupported BSA name tables")
    pos = offset + folders * 24
    folder_names = []
    for i in range(folders):
        nfiles = struct.unpack_from("<I", data, offset + i * 24 + 8)[0]
        size = data[pos]
        folder = data[pos + 1:pos + size].decode("utf-8").replace("\\", "/")
        folder_names.extend([folder] * nfiles)
        pos += 1 + size + nfiles * 16
    require(len(folder_names) == count, "BSA file count mismatch")
    result = []
    for folder in folder_names:
        end = data.index(b"\0", pos)
        filename = data[pos:end].decode("utf-8")
        result.append(folder + "/" + filename)
        pos = end + 1
    return result


def provider_inventory(extracted):
    inventory, inputs, locations = {}, {}, {}
    for provider, (pattern, roots) in PROVIDERS.items():
        matches = sorted(extracted.glob(pattern))
        require(len(matches) == 1, f"expected one extracted {provider} archive, found {len(matches)}")
        locations[provider] = matches[0]
        listed = 0
        for relative in roots:
            root = matches[0] / relative
            require(root.is_dir(), f"missing provider install tree: {provider}/{relative}")
            for path in sorted(root.rglob("*")):
                require(not path.is_symlink(), "provider symlink refused")
                if not path.is_file():
                    continue
                rel = path.relative_to(root).as_posix()
                # SmoothCam FOMOD places its selected DLL under AE/. Configs at
                # Data root retain their paths. Never select AE-Pre1799 or SSE.
                if provider == "SmoothCam 1.8.1" and rel.startswith(("AE/", "AE-Pre1799/", "SSE/")):
                    if not rel.startswith("AE/"):
                        continue
                    rel = "SKSE/Plugins/" + Path(rel).name
                key = safe_path(rel)
                inventory.setdefault(key, set()).add(provider)
                inputs[f"{provider}/{relative}/{rel}"] = sha256(path.read_bytes())
                listed += 1
                if path.suffix.lower() == ".bsa":
                    for contained in bsa_paths(path):
                        inventory.setdefault(safe_path(contained), set()).add(provider)
        require(listed > 0, f"empty provider inventory: {provider}")
    return inventory, hashes_json(inputs), locations


def hashes_json(value):
    return sha256(json_bytes(value))


def collisions(files, inventory):
    found = {}
    for path in files:
        key = safe_path(path)
        for provider in sorted(inventory.get(key, ())):
            allow = OVERRIDES.get(key)
            require(allow is not None and allow["provider"] == provider, f"unapproved provider collision: {path} / {provider}")
            found[path] = allow
    return found


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def swf_files(directory, commit, owner, mismatches, dry_run):
    # Reuse the builder's exact pins without running its compiler or main().
    import runpy
    builder = runpy.run_path(str(ROOT / "modules/ui/tools/build_scaleform.py"))
    manifest = json.loads((directory / "build.json").read_bytes())
    require(manifest.get("skyui_commit") == builder["PIN"], "SWF SkyUI source pin mismatch")
    require(manifest.get("source_sha256") == builder["SOURCES"], "SWF base ActionScript hashes mismatch")
    require(manifest.get("base") == "Nexus6.11 acquired BSA" and manifest.get("inputs") == builder["NEXUS_BASE_SWFS"],
            "SWF base hashes must match the acquired Nexus 6.11 BSA")
    require(manifest.get("owner") == owner and bool(re.fullmatch(r"[A-Za-z0-9_.-]+", owner)), "SWF evidence owner mismatch/invalid")
    require(manifest.get("status") == "COMPILED_NOT_RUN" and set(manifest.get("targets", [])) == set(SWF_PATHS),
            "SWF build is not a complete compiled set")
    require(set(manifest.get("outputs", {})) == set(SWF_PATHS), "SWF outputs incomplete")
    require(re.fullmatch(r"[0-9a-f]{64}", manifest.get("ffdec_cli_sha256", "")) is not None,
            "SWF compiler hash missing/invalid")
    source = manifest.get("source_commit")
    if manifest.get("source_dirty") is not False:
        require(dry_run, "SWF build.json does not record a clean committed source tree")
        mismatches.append("SWF build source tree was dirty or unrecorded")
    if source != commit:
        require(dry_run, "SWF patch source commit missing or differs from --commit; rebuild with provenance patch")
        mismatches.append(f"SWF source commit {source!r} differs from requested {commit}")
    paths = subprocess.check_output(["git", "-C", str(ROOT), "ls-tree", "--name-only", commit,
                                     "modules/ui/scaleform/"], text=True).splitlines()
    expected = {Path(p).name: sha256(git_bytes(commit, p)) for p in paths if p.endswith(".inc")}
    require(bool(expected), "no patch includes at requested commit")
    if manifest.get("patch_sha256") != expected:
        require(dry_run, "SWF patch include hashes differ from --commit")
        mismatches.append("SWF patch include hashes differ from requested commit")
    files = {}
    for path in SWF_PATHS:
        source_path = directory / "Data/Interface" / path
        require(not source_path.is_symlink(), "SWF symlink refused")
        data = source_path.read_bytes()
        require(len(data) > 8 and data[:3] in (b"FWS", b"CWS", b"ZWS"), f"invalid SWF: {path}")
        require(sha256(data) == manifest["outputs"][path], f"SWF hash mismatch: {path}")
        files[f"Interface/{path}"] = data
    # Do not copy absolute compiler paths, logs, PIDs or clocks from build.json.
    proof = {k: manifest.get(k) for k in ("skyui_commit", "source_commit", "owner", "source_sha256",
                                         "patch_sha256", "inputs", "outputs", "ffdec_cli_sha256")}
    return files, proof


def merge_ini(base, fragment):
    def parse(data):
        config = configparser.ConfigParser(interpolation=None)
        config.optionxform = str
        config.read_string(data.decode("utf-8-sig"))
        return config
    result, overrides = parse(base), parse(fragment)
    for section in overrides.sections():
        if not result.has_section(section):
            result.add_section(section)
        for key, value in overrides.items(section):
            result.set(section, key, value)
    output = io.StringIO()
    result.write(output)
    return output.getvalue().encode("utf-8")


def emit(out, name, commit, files, metadata, dry_run=False):
    imports = check_contents(name, files, metadata["artifact_kind"] == "assembled")
    check_independence({n: expected_paths(n, full=True) for n in PACKAGES})
    out.mkdir(parents=True, exist_ok=True)
    suffix = "-DRY-RUN" if dry_run else ""
    archive = out / f"{name}-{VERSION}-{commit[:7]}{suffix}.zip"
    write_zip(archive, files)
    manifest = {"package": name, "version": VERSION, "source_commit": commit,
                "archive": archive.name, "archive_sha256": sha256(archive.read_bytes()),
                "install_root": "Data/", "files": hashes(files), "dll_imports": imports,
                "runtime": RUNTIME, "requires": COMMON_REQUIREMENTS +
                (CAMERA_REQUIREMENTS if name == "SkyCRPG_Camera" else []),
                # Minimap providers are optional: without them the compass stays.
                "optional": [] if name == "SkyCRPG_Camera" else UI_OPTIONAL,
                "requires_skycrpg_modules": [], "distribution": "internal-candidate",
                "candidate_eligible": not dry_run, **metadata}
    manifest_path = out / f"{name}.manifest.json"
    manifest_data = json_bytes(manifest)
    check_text(manifest_path.name, manifest_data)
    manifest_path.write_bytes(manifest_data)
    sums = [f"{sha256(p.read_bytes())}  {p.name}" for p in (archive, manifest_path)]
    return archive, manifest, sums


def assemble(args):
    commit = commit_sha(args.commit)
    name = "SkyCRPG_Camera" if args.module == "camera" else "SkyCRPG_UI"
    mismatches = []
    files, native = read_native(args.native_zip, args.native_manifest, name, commit, mismatches, args.allow_mismatch_for_dry_run)
    inventory, provider_hash, locations = provider_inventory(args.providers)
    proof = {}
    if args.module == "ui":
        require(args.swf_dir is not None and args.swf_owner is not None, "UI requires --swf-dir and --swf-owner")
        swfs, proof = swf_files(args.swf_dir, commit, args.swf_owner, mismatches, args.allow_mismatch_for_dry_run)
        files.update(swfs)
    else:
        for destination, source in CAMERA_CONFIGS.items():
            data = git_bytes(commit, f"modules/camera/config/{source}")
            if destination.startswith("MCM/"):
                base = (locations["True Directional Movement 2.3.1"] / destination).read_bytes()
                proof = {"tdm_base_sha256": sha256(base), "tdm_fragment_sha256": sha256(data),
                         "merge_destination": destination, "merge_policy": "preserve unrelated keys; compare and restore on removal"}
                data = merge_ini(base, data)
            files[destination] = data
    for source in ("LICENSE", "docs/LICENSING.md", "docs/THIRD_PARTY.md"):
        files[f"Docs/{name}/{Path(source).name}"] = git_bytes(commit, source)
    overrides = collisions(files, inventory)
    metadata = {"artifact_kind": "assembled", "native_archive_sha256": native["archive_sha256"],
                "native_source_commit": native["source_commit"], "provider_inventory_sha256": provider_hash,
                "overrides": overrides, "asset_provenance": proof, "dry_run": args.allow_mismatch_for_dry_run,
                "mismatches": mismatches, "dependency_lock_sha256": sha256(git_bytes(commit, "docs/compatibility/dependency-lock.md")),
                "rights_review": "pending DYL-174; internal use only, no publication authorization"}
    archive, manifest, sums = emit(args.out, name, commit, files, metadata, args.allow_mismatch_for_dry_run)
    (args.out / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="ascii")
    print(f"{archive.name}  {manifest['archive_sha256']}  candidate_eligible={manifest['candidate_eligible']}")
    return manifest


def verify_repro(args):
    # Revalidate inputs on each pass; compare every emitted byte in independent
    # directories, including the provenance manifest and checksums.
    import copy
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "repro.json").unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="repro-", dir=args.out) as temporary:
        dirs = [Path(temporary) / part for part in ("first", "second")]
        for directory in dirs:
            run = copy.copy(args)
            run.out = directory
            assemble(run)
        first, second = [{p.name: p.read_bytes() for p in d.iterdir()} for d in dirs]
        require(first == second, "FAIL: assembled outputs are not byte-identical")
        report = {"result": "PASS", "scope": "assembly from identical native/asset inputs; not DLL/SWF recompilation",
                  "dry_run": args.allow_mismatch_for_dry_run, "outputs": hashes(first)}
        (args.out / "repro.json").write_bytes(json_bytes(report))
        print("PASS: byte-identical archives, manifests and SHA256SUMS")
    return report


def native(args):
    commit = commit_sha(args.commit)
    sums, packages = [], {}
    for name, module_dir in PACKAGES.items():
        if args.module and module_dir != f"modules/{args.module}":
            continue
        files = {arc: path.read_bytes() for arc, path in package_files(name, module_dir, args.build_dir).items()}
        # Use committed inputs, not potentially dirty configuration/notices.
        for arc, path in package_files(name, module_dir, args.build_dir).items():
            if path.is_relative_to(ROOT) and not arc.endswith(".dll"):
                files[arc] = git_bytes(commit, path.relative_to(ROOT).as_posix())
        require(dll_has_commit(files[f"SKSE/Plugins/{name}.dll"], commit), "DLL source commit differs from --commit")
        packages[name] = files
        check_independence(packages)
        archive, manifest, lines = emit(args.out, name, commit, files,
                                        {"artifact_kind": "native", "candidate_eligible": False})
        sums.extend(lines)
        print(f"{archive.name}  {manifest['archive_sha256']}  native-only")
    (args.out / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description="Build native payloads or assemble independent internal candidates")
    parser.add_argument("mode", nargs="?", choices=("native", "assemble", "verify-repro"), default="native")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--module", choices=("camera", "ui"))
    parser.add_argument("--native-zip", type=Path)
    parser.add_argument("--native-manifest", type=Path)
    parser.add_argument("--providers", type=Path, help="Pinned extracted provider archives (read-only)")
    parser.add_argument("--swf-dir", type=Path)
    parser.add_argument("--swf-owner", help="Expected build.json evidence owner (exact match)")
    parser.add_argument("--allow-mismatch-for-dry-run", action="store_true",
                        help="NON-CANDIDATE historical provenance probe; never bypasses hashes/pins/content checks")
    args = parser.parse_args()
    try:
        if args.mode == "native":
            require(args.build_dir is not None, "native mode requires --build-dir")
            require(not args.allow_mismatch_for_dry_run, "dry-run mismatch flag is assembly-only")
            native(args)
        else:
            require(all((args.module, args.native_zip, args.native_manifest, args.providers)),
                    "assembly requires --module, --native-zip, --native-manifest and --providers")
            (verify_repro if args.mode == "verify-repro" else assemble)(args)
    except (ValueError, OSError, KeyError, struct.error, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
