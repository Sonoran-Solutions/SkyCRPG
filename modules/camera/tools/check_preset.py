#!/usr/bin/env python3
"""Check the camera candidate's SmoothCam preset envelope and intent values."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRESET = ROOT / "config" / "SmoothCamPreset0.json"
TDM_FRAGMENT = ROOT / "config" / "TrueDirectionalMovement-settings.fragment.ini"


def main() -> None:
    preset = json.loads(PRESET.read_text(encoding="utf-8"))
    assert preset["name"] == "SkyCRPG Camera feasibility candidate"
    config = preset["config"]
    expected_states = {
        "standing": (90.0, -180.0),
        "walking": (90.0, -180.0),
        "running": (90.0, -180.0),
        "sprinting": (90.0, -180.0),
        "sneaking": (65.0, -110.0),
        "bowAim": (20.0, 80.0),
    }
    for state, (up, zoom) in expected_states.items():
        offset = config[state]
        assert offset["upOffset"] == up, (state, offset)
        assert offset["zoomOffset"] == zoom, (state, offset)
        assert offset["sideOffset"] >= 0.0, (state, offset)

    # Only fields serialized by SmoothCam are allowed at the preset envelope.
    assert set(preset) == {"name", "config"}
    assert set(config).issubset({
        "standing", "walking", "running", "sprinting", "sneaking", "swimming",
        "bowAim", "sitting", "horseback", "dragon", "vampireLord", "werewolf",
        "vanity", "userDefined", "useArrowPrediction", "drawArrowArc",
        "worldCrosshairType", "enableInterp", "zoomMul", "dialogueMode",
    })
    fragment = TDM_FRAGMENT.read_text(encoding="utf-8")
    for setting in (
        "[DirectionalMovement]\nuAdjustCameraYawDuringMovement = 0",
        "[TargetLock]\nbTargetLockUsePOVSwitchGamepad = 0",
        "[Keys]\nuTargetLockKey = -1",
    ):
        assert setting in fragment, setting
    print(f"PASS: {PRESET.relative_to(ROOT)} is valid JSON with SmoothCam preset envelope and expected candidate offsets")


if __name__ == "__main__":
    main()
