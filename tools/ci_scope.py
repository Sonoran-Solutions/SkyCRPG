#!/usr/bin/env python3
"""Select independent native targets (Camera, UI, test probe, occlusion spike), widening shared build/package changes."""
import json
import os
import subprocess
from pathlib import Path

MODULES = ("camera", "ui")  # production plugins, built by the windows-x64 matrix
TARGETS = (*MODULES, "testprobe", "occlusion-spike")  # plus independently built, never-shipped diagnostics
LEAF_PREFIXES = {"camera": "modules/camera/", "ui": "modules/ui/", "testprobe": "tools/testprobe/", "occlusion-spike": "tools/occlusion-spike/"}
SHARED_FILES = frozenset({
    "CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json",
    "tools/package.py", "tools/ci_scope.py", "tools/tests/test_ci_scope.py",
    ".github/workflows/build.yml", "LICENSE", "docs/LICENSING.md", "docs/THIRD_PARTY.md",
    "docs/compatibility/dependency-lock.md",
})
SHARED_PREFIXES = ("shared/", "cmake/", "LICENSES/", ".github/actions/")


def select_modules(paths, requested=""):
    """Targets to build: any of camera, ui, testprobe, occlusion-spike, in that order."""
    if requested:
        if requested not in (*TARGETS, "both", "all"):
            raise ValueError(f"Unknown module selection: {requested}")
        return list(MODULES) if requested == "both" else list(TARGETS) if requested == "all" else [requested]
    paths = tuple(paths)
    # Shared changes take precedence even when a target also changed.
    if any(p in SHARED_FILES or p.startswith(SHARED_PREFIXES) or p.startswith("tools/tests/test_package") for p in paths):
        return list(TARGETS)
    spike_inputs = {"tools/testprobe/src/perf_policy.h", "tools/testprobe/src/json_line.h",
                    "modules/camera/include/third_party/SmoothCamAPI.h"}
    changed = [t for t in TARGETS if any(p.startswith(LEAF_PREFIXES[t]) for p in paths)
               or (t == "occlusion-spike" and any(p in spike_inputs for p in paths))]
    return changed or list(TARGETS)


def main():
    base = os.environ.get("BASE_SHA", "")
    requested = os.environ.get("REQUESTED", "")
    paths = []
    if not requested and base and set(base) != {"0"}:
        paths = subprocess.check_output(
            ["git", "diff", "--name-only", "-z", base, "HEAD"], text=True
        ).rstrip("\0").split("\0")
    targets = select_modules(paths, requested)
    modules = [t for t in targets if t in MODULES]
    with Path(os.environ["GITHUB_OUTPUT"]).open("a") as output:
        output.write("modules=" + json.dumps(modules) + "\n")
        output.write("testprobe=" + ("true" if "testprobe" in targets else "false") + "\n")
        output.write("occlusion_spike=" + ("true" if "occlusion-spike" in targets else "false") + "\n")
    print("Selected native targets: " + ", ".join(targets))


if __name__ == "__main__":
    main()
