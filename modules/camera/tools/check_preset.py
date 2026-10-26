#!/usr/bin/env python3
"""Validate our preset against the pinned external SmoothCam source checkout."""

import argparse
import json
import math
import os
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRESET = ROOT / "config" / "SmoothCamPreset0.json"
TDM_FRAGMENT = ROOT / "config" / "TrueDirectionalMovement-settings.fragment.ini"
SMOOTHCAM_COMMIT = "e0afd09aff26e5849dac461252e5bffbd5b69d62"
DEFAULT_SOURCE = Path(os.environ.get(
    "SKYCRPG_SMOOTHCAM_SOURCE", "/tmp/skycrpg-dyl151-audit/SkyrimSE-SmoothCam"
))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def function_body(source: str, signature: str) -> str:
    match = re.search(signature, source)
    require(match is not None, f"SmoothCam source missing function matching {signature!r}")
    start = source.find("{", match.end())
    require(start >= 0, f"SmoothCam function body missing for {signature!r}")
    depth = 0
    for pos in range(start, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:pos]
    raise ValueError(f"Unbalanced SmoothCam function body for {signature!r}")


def member_keys(body: str) -> set[str]:
    return set(re.findall(r"VALUE_FROM_JSON\(obj,\s*(\w+)\)", body))


def serialized_keys(body: str) -> set[str]:
    return set(re.findall(r"CREATE_JSON_VALUE\(obj,\s*(\w+)\)", body))


def source_schema(source_root: Path) -> tuple[set[str], set[str], set[str], dict[str, float | bool]]:
    source_root = source_root.resolve()
    actual_commit = subprocess.check_output(
        ["git", "-C", str(source_root), "rev-parse", "HEAD"], text=True
    ).strip()
    require(actual_commit == SMOOTHCAM_COMMIT,
            f"expected SmoothCam {SMOOTHCAM_COMMIT}, got {actual_commit}")

    config_cpp = (source_root / "SmoothCam/source/config.cpp").read_text(encoding="utf-8")
    config_h = (source_root / "SmoothCam/include/config.h").read_text(encoding="utf-8")
    camera_cpp = (source_root / "SmoothCam/source/camera.cpp").read_text(encoding="utf-8")
    papyrus_cpp = (source_root / "SmoothCam/source/papyrus.cpp").read_text(encoding="utf-8")
    mcm_general = (source_root / "CodeGen/MCM/mcm/include/general.psc").read_text(encoding="utf-8")
    offset_body = function_body(
        config_cpp, r"void\s+Config::from_json\(const json& j,\s*OffsetGroup& obj\)"
    )
    user_body = function_body(
        config_cpp, r"void\s+Config::from_json\(const json& j,\s*UserConfig& obj\)"
    )
    preset_body = function_body(
        config_cpp, r"void\s+Config::from_json\(const json& j,\s*Preset& obj\)"
    )
    offset_to_json = function_body(
        config_cpp, r"void\s+Config::to_json\(json& j,\s*const OffsetGroup& obj\)"
    )
    user_to_json = function_body(
        config_cpp, r"void\s+Config::to_json\(json& j,\s*const UserConfig& obj\)"
    )
    preset_to_json = function_body(
        config_cpp, r"void\s+Config::to_json\(json& j,\s*const Preset& obj\)"
    )

    macro = re.search(
        r"#define\s+VALUE_FROM_JSON\(obj,\s*member\)(.*?)(?=\n\s*\n)",
        config_cpp,
        re.S,
    )
    require(macro is not None, "SmoothCam VALUE_FROM_JSON macro not found")
    require("const auto def = obj." in macro.group(1) and "j.value(#member, def)" in macro.group(1),
            "SmoothCam missing-field fallback no longer preserves the existing member default")
    require(re.search(r"int\s+modToggleKey\s*=\s*-1\s*;", config_h) is not None,
            "SmoothCam emergency toggle is no longer unbound by default")
    require('settingName: "ModEnabledKeyCode"' in mcm_general
            and 'displayName: "Toggle SmoothCam Key"' in mcm_general,
            "SmoothCam MCM emergency toggle binding changed")
    require('displayName: "Disable SmoothCam"' in mcm_general
            and 'desc: "When selected, disables SmoothCam."' in mcm_general,
            "SmoothCam MCM disable control changed")
    require("config->modDisabled = !config->modDisabled;" in camera_cpp,
            "SmoothCam toggle key no longer switches modDisabled")
    require(re.search(r'IMPL_GETTER\("ModEnabled",\s*modDisabled\)', papyrus_cpp) is not None,
            "SmoothCam MCM ModEnabled polarity mapping changed; inspect before relying on disable control")
    require("void Camera::Camera::OnTogglePOV" in camera_cpp
            and "activeCamera->OnTogglePOV(ev);" in camera_cpp,
            "SmoothCam POV action is no longer forwarded to its camera handler")

    offset_keys = member_keys(offset_body)
    user_keys = member_keys(user_body)
    preset_keys = member_keys(preset_body)
    require(preset_keys == {"name", "config"},
            f"unexpected SmoothCam preset envelope: {preset_keys}")
    require(offset_keys == serialized_keys(offset_to_json),
            "SmoothCam OffsetGroup read/write JSON keys diverged")
    require(user_keys == serialized_keys(user_to_json),
            "SmoothCam UserConfig read/write JSON keys diverged")
    require(preset_keys == serialized_keys(preset_to_json),
            "SmoothCam Preset read/write JSON keys diverged")

    # Confirm every candidate nested field is an OffsetGroup field and every
    # configured state is a real UserConfig field. The source's j.value(state,
    # obj.state) path defaults omitted groups to the UserConfig member value.
    user_struct = re.search(r"typedef struct parsedConfig\s*\{(.*?)\}\s*UserConfig;", config_h, re.S)
    require(user_struct is not None, "SmoothCam UserConfig declaration not found")
    state_keys = set(re.findall(r"OffsetGroup\s+(\w+)\s*;", user_struct.group(1)))
    offset_struct = re.search(r"typedef struct offsetGroup\s*\{(.*?)\}\s*OffsetGroup;", config_h, re.S)
    require(offset_struct is not None, "SmoothCam OffsetGroup declaration not found")
    initializers = {}
    for field, value in re.findall(
        r"^\s*(?:bool|float|int)\s+(\w+)\s*=\s*([^;]+);",
        offset_struct.group(1),
        re.M,
    ):
        value = value.strip()
        if value in {"true", "false"}:
            initializers[field] = value == "true"
        else:
            initializers[field] = float(value.rstrip("f"))
    return offset_keys, user_keys, state_keys, initializers


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoothcam-source", type=Path, default=DEFAULT_SOURCE,
                        help="external SmoothCam source checkout at the pinned commit")
    args = parser.parse_args()
    offset_keys, user_keys, state_keys, offset_defaults = source_schema(args.smoothcam_source)

    preset = json.loads(PRESET.read_text(encoding="utf-8"))
    require(set(preset) == {"name", "config"}, "preset envelope must contain exactly name and config")
    require(preset["name"] == "SkyCRPG Camera feasibility candidate", "unexpected preset name")
    config = preset["config"]
    require(set(config).issubset(user_keys), f"unknown UserConfig fields: {set(config) - user_keys}")
    require(set(config).issubset(state_keys), "candidate may only configure source-declared offset groups")

    # The pinned MCM SliderSetting source defaults to [-100, 100]. Keep every
    # supplied numeric offset inside that UI-supported range and finite.
    mcm_controls = (args.smoothcam_source / "CodeGen/MCM/mcm/include/controls.psc").read_text(encoding="utf-8")
    slider = re.search(r"#constexpr_struct SliderSetting\s*\[(.*?)\n\]", mcm_controls, re.S)
    require(slider is not None, "SmoothCam MCM SliderSetting declaration not found")
    minimum = float(re.search(r"float min = (-?[\d.]+)", slider.group(1)).group(1))
    maximum = float(re.search(r"float max = (-?[\d.]+)", slider.group(1)).group(1))
    require((minimum, maximum) == (-100.0, 100.0), "SmoothCam MCM default offset slider range changed")

    for state, offset in config.items():
        require(isinstance(offset, dict), f"{state} offset must be a JSON object")
        require(set(offset).issubset(offset_keys), f"{state} has unknown OffsetGroup fields: {set(offset) - offset_keys}")
        for key, value in offset.items():
            if isinstance(value, (int, float)) and not isinstance(value, bool):
                require(math.isfinite(value) and minimum <= value <= maximum,
                        f"{state}.{key}={value} outside finite MCM slider range [{minimum}, {maximum}]")
        # Preset::from_json constructs a default UserConfig and the nested
        # from_json readers use j.value(key, member) through VALUE_FROM_JSON.
        # Prove a representative omitted field remains its actual C++ member
        # default rather than being synthesized as zero by this packaging path.
        omitted = "combatRangedZoomOffset"
        require(omitted not in offset, f"{state} must not replace inherited {omitted}")
        require(offset_defaults.get(omitted) == 0.0,
                "SmoothCam OffsetGroup default changed; review partial preset behavior")

    tdm = TDM_FRAGMENT.read_text(encoding="utf-8")
    for setting in (
        "[DirectionalMovement]\nuAdjustCameraYawDuringMovement = 0",
        "[Leaning]\nbEnableLeaning = 0\nbEnableLeaningNPC = 0",
        "[Headtracking]\nbHeadtracking = 0\nbHeadtrackSpine = 0\nbCameraHeadtracking = 0",
        "[TargetLock]\nbTargetLockUsePOVSwitchGamepad = 0",
        "bTargetLockUsePOVSwitchKeyboard = 0",
        "[HUD]\nbEnableTargetLockReticle = 0",
        "[Keys]\nuTargetLockKey = -1",
    ):
        require(setting in tdm, f"missing TDM fragment setting: {setting}")
    print(
        f"PASS: SmoothCam {SMOOTHCAM_COMMIT} schema, source-default-preserving partial offsets, "
        f"numeric ranges, and TDM settings fragment"
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError, ValueError, TypeError, KeyError) as exc:
        raise SystemExit(f"FAIL: {exc}") from exc
