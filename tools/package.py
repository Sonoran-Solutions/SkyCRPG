#!/usr/bin/env python3
"""Build the two SkyCRPG install archives from a finished Release build.

Produces one deterministic zip per module (mod-manager layout, rooted at Data/),
a JSON manifest per archive and dist/SHA256SUMS. Fails if the packages share an
install path or if either DLL imports the other module.

    python tools/package.py --build-dir build/windows-x64-release --commit <sha>
"""

import argparse
import hashlib
import json
import struct
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = "0.0.1"
RUNTIME = "SkyrimSE.exe 1.7.104.0 (Steam)"
REQUIRES = ["SKSE64 2.3.1"]

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
    dll = next(build_dir.glob(f"{module_dir}/**/{name}.dll"), None)
    if dll is None:
        sys.exit(f"error: {name}.dll not found under {build_dir / module_dir}")
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--module", choices=["camera", "ui"], help="Package only the independently built target")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    owners: dict[str, str] = {}
    sums = []
    for name, module_dir in PACKAGES.items():
        if args.module and module_dir != f"modules/{args.module}":
            continue
        sources = package_files(name, module_dir, args.build_dir)
        contents = {arc: src.read_bytes() for arc, src in sources.items()}

        for arc in contents:
            key = arc.lower()
            if key in owners:
                sys.exit(f"error: {arc} is installed by both {owners[key]} and {name}")
            owners[key] = name

        imports = pe_imports(contents[f"SKSE/Plugins/{name}.dll"])
        coupled = [d for d in imports if d.lower().startswith("skycrpg")]
        if coupled:
            sys.exit(f"error: {name}.dll imports {coupled}; modules must not depend on each other")

        archive = args.out / f"{name}-{VERSION}-{args.commit[:7]}.zip"
        write_zip(archive, contents)
        archive_hash = sha256(archive.read_bytes())
        manifest = {
            "package": name,
            "version": VERSION,
            "source_commit": args.commit,
            "archive": archive.name,
            "archive_sha256": archive_hash,
            "install_root": "Data/",
            "files": {arc: sha256(data) for arc, data in sorted(contents.items())},
            "dll_imports": sorted(imports),
            "runtime": RUNTIME,
            "requires": REQUIRES + ["Address Library 13", "SkyUI Community 6.11"] + (["SmoothCam 1.8.1", "True Directional Movement 2.3.1", "MCM Helper 1.6.3"] if name == "SkyCRPG_Camera" else []),
            "requires_skycrpg_modules": [],
        }
        (args.out / f"{name}.manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        sums.append(f"{archive_hash}  {archive.name}")
        for arc, data in sorted(contents.items()):
            sums.append(f"{sha256(data)}  {name}/{arc}")
        print(f"{archive.name}  {archive_hash}  imports={','.join(sorted(imports))}")

    (args.out / "SHA256SUMS").write_text("\n".join(sums) + "\n")


if __name__ == "__main__":
    main()
