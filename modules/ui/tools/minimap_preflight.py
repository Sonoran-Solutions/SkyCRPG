#!/usr/bin/env python3
"""Read-only loose-file preflight, before deploying selected external map providers.

Point --data at the resolved, extracted Data overlay (after FOMOD choices), not
at an archive or BSA. This does not prove versions, DLL load, map rendering or
compass handoff. No missing provider is ever compensated by hiding the compass.
"""
import argparse
import hashlib
import json
from pathlib import Path

# Minimap attachment path is exact: DEM v1.7.5 MessageListeners.cpp fatal check.
REQUIRED = (
    "SKSE/Plugins/DragonsEyeMinimap.dll",
    "SKSE/Plugins/InfinityUI.dll",
    "SKSE/Plugins/LocalMapUpgrade.dll",
    "Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/Minimap.swf",
    "Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf",
)


def inspect(data):
    files = {}
    for relative in REQUIRED:
        path = data / relative
        files[relative] = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
    return {"status": "PASS_FILE_PRESENCE_ONLY" if all(files.values()) else "BLOCKED",
            "sha256": files, "renderer": "NOT_RUN", "native_compass": "retained",
            "limitations": "Loose-file case-sensitive preflight; resolve BSA/file winners and exact versions separately."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    args = parser.parse_args()
    result = inspect(args.data)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["status"] == "PASS_FILE_PRESENCE_ONLY" else 1)
