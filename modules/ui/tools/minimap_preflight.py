#!/usr/bin/env python3
"""Check the resolved loose-file overlay before starting the selected map stack.

This does not resolve BSAs, prove DLL load or demonstrate a working renderer.
"""
import argparse
import hashlib
import json
from pathlib import Path

EXPECTED = {
    "SKSE/Plugins/DragonsEyeMinimap.dll": "918fc9b3f4bf284fb1ae8d028370ebd20908d2750f4706383fe56012bddc5c1a",
    "SKSE/Plugins/InfinityUI.dll": "53ef0932485ee1bb281cc4b2baad1fe91e368b9d656989071747b2f1316a85ed",
    "SKSE/Plugins/LocalMapUpgrade.dll": "f0fb1c9c822133f9e4ece866fc4f6190424dcafac35e040cd0b4cd037340b3d5",
    "Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/Minimap.swf": "dabdfeb3454d0a40d347ad56ab3e252578a9607e9d7d873d8e70183ffc1dd4e5",
    "Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf": "8eebe684e51f914863918cc46ea14dbf572f766ba4c1be3a99747fdc41dd81ef",
}
# Infinity UI and LMU can independently serve other mods.
DEM_FILES = (list(EXPECTED)[0], *list(EXPECTED)[3:])


def inspect(data, enabled=True):
    files = {}
    for relative in EXPECTED:
        path = data / relative
        files[relative] = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
    if not any(files[name] for name in DEM_FILES):
        status = "ABSENT" if enabled else "DISABLED"
    elif not all(files.values()):
        status = "PARTIAL"
    elif any(files[name] != digest for name, digest in EXPECTED.items()):
        status = "MISMATCHED"
    else:
        status = "EXACT" if enabled else "DISABLED"
    return {"status": status, "sha256": files, "renderer": "NOT_RUN",
            "native_compass": "retained", "launch_allowed": status in ("ABSENT", "DISABLED", "EXACT"),
            "limitations": "Resolved loose files only; BSA winners, runtime loading and rendering are not verified."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--disabled", action="store_true", help="replacement disabled in user configuration")
    args = parser.parse_args()
    result = inspect(args.data, enabled=not args.disabled)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["launch_allowed"] else 1)
