#!/usr/bin/env python3
"""Stage original SmoothCam startup configuration and MCM preset deterministically."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


ROOT = Path(__file__).resolve().parents[1]
PRESET = ROOT / "config" / "SmoothCamPreset0.json"
ACTIVE_CONFIG = ROOT / "config" / "SmoothCam.json"
PINNED_SMOOTHCAM_SOURCE = "e0afd09aff26e5849dac461252e5bffbd5b69d62"
INSTALL_PATH = "SKSE/Plugins/SmoothCamPreset0.json"
ACTIVE_INSTALL_PATH = "SKSE/Plugins/SmoothCam.json"
FIXED_TIME = (1980, 1, 1, 0, 0, 0)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="output directory outside the game install")
    parser.add_argument("--commit", required=True, help="SkyCRPG source commit to record in the manifest")
    args = parser.parse_args()
    if re.fullmatch(r"[0-9a-fA-F]{40}", args.commit) is None:
        raise SystemExit("error: --commit must be a full 40-character Git SHA")

    preset_bytes = PRESET.read_bytes()
    active_bytes = ACTIVE_CONFIG.read_bytes()
    parsed = json.loads(preset_bytes)
    if set(parsed) != {"name", "config"}:
        raise SystemExit("error: unexpected SmoothCam preset envelope")
    manifest = {
        "package": "SkyCRPG_Camera_Config",
        "source_commit": args.commit,
        "provider": "SmoothCam 1.8.1 Beta1.8.1",
        "provider_source_commit": PINNED_SMOOTHCAM_SOURCE,
        "install_root": "Data/",
        "third_party_payloads": [],
        "files": {
            ACTIVE_INSTALL_PATH: sha256(active_bytes),
            INSTALL_PATH: sha256(preset_bytes),
        },
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    archive_files = {
        ACTIVE_INSTALL_PATH: active_bytes,
        INSTALL_PATH: preset_bytes,
        "SkyCRPG_Camera_Config.manifest.json": manifest_bytes,
    }

    args.out.mkdir(parents=True, exist_ok=True)
    # Name is content addressed: all inputs affecting the archive, including
    # the source commit embedded in its manifest, are covered.
    temporary_name = "SkyCRPG_Camera_Config.zip"
    archive_path = args.out / temporary_name
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path, content in sorted(archive_files.items()):
            info = zipfile.ZipInfo(path, FIXED_TIME)
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, content)
    archive_digest = sha256(archive_path.read_bytes())
    final_path = args.out / f"SkyCRPG_Camera_Config-{archive_digest[:12]}.zip"
    archive_path.replace(final_path)

    checksum_lines = [f"{archive_digest}  {final_path.name}"]
    checksum_lines.extend(
        f"{sha256(data)}  {final_path.name}/{path}" for path, data in sorted(archive_files.items())
    )
    (args.out / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n", encoding="ascii")
    print(f"{final_path}  {archive_digest}")


if __name__ == "__main__":
    main()
