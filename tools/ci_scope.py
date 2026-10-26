#!/usr/bin/env python3
"""Select independent native targets, widening shared build/package changes."""
import json
import os
import subprocess
from pathlib import Path

MODULES = ("camera", "ui")
SHARED_FILES = frozenset({
    "CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json",
    "tools/package.py", "tools/ci_scope.py", "tools/tests/test_ci_scope.py",
    ".github/workflows/build.yml",
})
SHARED_PREFIXES = ("shared/", "cmake/", "LICENSES/", ".github/actions/")


def select_modules(paths, requested=""):
    if requested:
        if requested not in (*MODULES, "both"):
            raise ValueError(f"Unknown module selection: {requested}")
        return list(MODULES) if requested == "both" else [requested]
    paths = tuple(paths)
    # Shared changes take precedence even when a module also changed.
    if any(p in SHARED_FILES or p.startswith(SHARED_PREFIXES) for p in paths):
        return list(MODULES)
    changed = [m for m in MODULES if any(p.startswith(f"modules/{m}/") for p in paths)]
    return changed or list(MODULES)


def main():
    base = os.environ.get("BASE_SHA", "")
    requested = os.environ.get("REQUESTED", "")
    paths = []
    if not requested and base and set(base) != {"0"}:
        paths = subprocess.check_output(
            ["git", "diff", "--name-only", "-z", base, "HEAD"], text=True
        ).rstrip("\0").split("\0")
    modules = select_modules(paths, requested)
    with Path(os.environ["GITHUB_OUTPUT"]).open("a") as output:
        output.write("modules=" + json.dumps(modules) + "\n")
    print("Selected native targets: " + ", ".join(modules))


if __name__ == "__main__":
    main()
