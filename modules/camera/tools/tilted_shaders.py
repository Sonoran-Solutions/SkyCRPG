#!/usr/bin/env python3
# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""Validate the tilted cut's patched vertex shaders with vkd3d-shader (P5.5-02 wave 2, package "shader", Stage 3.2).

The portable test fixed_spike_shader_policy_test, run with SKYCRPG_SHADER_ARCHIVE (the shipped Skyrim - Shaders.bsa)
and SKYCRPG_TILT_DUMP=DIR, writes the tilted copy of every audited vertex shader (patchVertexShader Cut::tilted) to
DIR, one file per distinct bytecode (l/u/e + the FNV-1a-64 of the original). This script reads each one back through
vkd3d-shader: the container and its checksum must parse, the D3D assembly must show cb13 declared with three rows,
o16.x declared as the clip distance, the three World rows multiplied into one temp, the constant w = 1, the three
plane products and min/max into o16.x; and the SPIR-V translation must decorate a variable BuiltIn ClipDistance.

    tilted_shaders.py DIR            check every *.dxbc in DIR (exit 1 on the first failure)
    tilted_shaders.py DIR --show     also print the inserted block of the first file

The library is found as in frame_shaders.py (SKYCRPG_VKD3D_SHADER or the Steam Linux Runtime's copy). Nothing of the
game is written. Not part of the build.
"""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import frame_shaders as fs  # noqa: E402

CHECKS = [
    r"dcl_constantBuffer cb13\[3\], immediateIndexed",
    r"dcl_output_siv o16\.x, clip_distance",
    r"dp4 (r\d+)\.x, cb2\[\d+\]\.xyzw, (r\d+)\.xyzw\n\s*dp4 \1\.y, cb2\[\d+\]\.xyzw, \2\.xyzw\n\s*dp4 \1\.z, cb2\[\d+\]\.xyzw, \2\.xyzw\n"
    r"\s*mov \1\.w, l\(1\.00000000e\+00\)\n"
    r"\s*dp4 (r\d+)\.x, \1\.xyzw, cb13\[0\]\.xyzw\n\s*dp4 \3\.y, \1\.xyzw, cb13\[1\]\.xyzw\n\s*dp4 \3\.z, \1\.xyzw, cb13\[2\]\.xyzw\n"
    r"\s*min \3\.y, \3\.y, \3\.z\n\s*max o16\.x, \3\.x, \3\.y\n",
]
BUILTIN_CLIP_DISTANCE = (4 << 16 | 71, 11, 3)  # OpDecorate <id> BuiltIn ClipDistance


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    lib, path = fs.library()
    print("vkd3d-shader", lib.vkd3d_shader_get_version(None, None).decode(), path)
    files = sorted(Path(sys.argv[1]).glob("*.dxbc"))
    if not files:
        sys.exit("no *.dxbc files")
    kinds = {"l": 0, "u": 0, "e": 0}
    for f in files:
        data = f.read_bytes()
        if fs.checksum(data) != data[4:20]:
            sys.exit(f"{f.name}: container checksum differs")
        text = fs.compile_(lib, data, fs.SOURCE_DXBC_TPF, fs.TARGET_D3D_ASM, f.name)[0].decode(errors="replace")
        for pattern in CHECKS:
            if not re.search(pattern, text):
                sys.exit(f"{f.name}: no match for {pattern!r}")
        blob = fs.compile_(lib, data, fs.SOURCE_DXBC_TPF, fs.TARGET_SPIRV_BINARY, f.name)[0]
        words = struct.unpack(f"<{len(blob) // 4}I", blob)
        if words[0] != 0x07230203 or not any(words[i] == BUILTIN_CLIP_DISTANCE[0] and words[i + 2:i + 4] == BUILTIN_CLIP_DISTANCE[1:]
                                             for i in range(5, len(words) - 3)):
            sys.exit(f"{f.name}: the SPIR-V has no BuiltIn ClipDistance")
        kinds[f.name[0]] = kinds.get(f.name[0], 0) + 1
    print(f"MATCH {len(files)} tilted shaders (lighting {kinds['l']}, utility {kinds['u']}, effect {kinds['e']}): "
          "D3D assembly checks and SPIR-V ClipDistance")
    if "--show" in sys.argv:
        text = fs.compile_(lib, files[0].read_bytes(), fs.SOURCE_DXBC_TPF, fs.TARGET_D3D_ASM, files[0].name)[0].decode()
        block = re.search(CHECKS[2], text)
        print(files[0].name)
        print(block.group(0).rstrip())


if __name__ == "__main__":
    main()
