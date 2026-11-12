#!/usr/bin/env python3
# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""Build, check and validate the frame pass's two shaders (P5.5-02 wave 2, package "shader", fifth pass).

The HLSL sources are modules/camera/shaders/fixed_spike_frame_{vs,ps}.hlsl. They are compiled offline with
vkd3d-shader's HLSL compiler (vs_5_0 / ps_5_0, DXBC) through ctypes, and the bytes are written to
modules/camera/include/fixed_spike_shader_frame_shaders.h together with the FNV-1a-64 of each source file; the
portable test checks that the sources still hash to those values, so a source edit without a rebuild fails CI.

    frame_shaders.py           rebuild the header from the sources
    frame_shaders.py --check   rebuild in memory and compare with the header byte for byte (exit 1 if different)
    frame_shaders.py --validate DIR  also disassemble each blob (D3D assembly) and translate it to SPIR-V; write both to DIR

The library is SKYCRPG_VKD3D_SHADER, or the Steam Linux Runtime's libvkd3d-shader.so.1 found under ~/.local/share/Steam.
It is only read; nothing of the game or of Proton is touched. Not part of the build: CI uses the header as committed.
"""
import ctypes
import glob
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = [("vertex", "fixed_spike_frame_vs.hlsl", "vs_5_0"), ("pixel", "fixed_spike_frame_ps.hlsl", "ps_5_0"),
           ("cap", "fixed_spike_cut_fill_ps.hlsl", "ps_5_0")]  # Stage 3.1: the back-face fill's cap shader
NAMES = {"vertex": "vertexShader", "pixel": "pixelShader", "cap": "capPixelShader"}
HEADER = ROOT / "include/fixed_spike_shader_frame_shaders.h"

# vkd3d_shader.h (API 1.x)
COMPILE_INFO, HLSL_SOURCE_INFO = 0, 6
SOURCE_DXBC_TPF, SOURCE_HLSL = 1, 2
TARGET_SPIRV_BINARY, TARGET_D3D_ASM, TARGET_DXBC_TPF = 1, 3, 5
LOG_WARNING = 2


class Code(ctypes.Structure):
    _fields_ = [("code", ctypes.c_void_p), ("size", ctypes.c_size_t)]


class CompileInfo(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("next", ctypes.c_void_p), ("source", Code), ("source_type", ctypes.c_int),
                ("target_type", ctypes.c_int), ("options", ctypes.c_void_p), ("option_count", ctypes.c_uint),
                ("log_level", ctypes.c_int), ("source_name", ctypes.c_char_p)]


class HlslSourceInfo(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("next", ctypes.c_void_p), ("entry_point", ctypes.c_char_p),
                ("secondary_code", Code), ("profile", ctypes.c_char_p)]


def library():
    named = os.environ.get("SKYCRPG_VKD3D_SHADER")
    found = [named] if named else sorted(glob.glob(os.path.expanduser(
        "~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_*/steamrt*_platform_*/files/lib/x86_64-linux-gnu/libvkd3d-shader.so.1*")))
    if not found:
        sys.exit("no libvkd3d-shader found; set SKYCRPG_VKD3D_SHADER")
    lib = ctypes.CDLL(found[-1])
    lib.vkd3d_shader_compile.argtypes = [ctypes.POINTER(CompileInfo), ctypes.POINTER(Code), ctypes.POINTER(ctypes.c_char_p)]
    lib.vkd3d_shader_compile.restype = ctypes.c_int
    lib.vkd3d_shader_get_version.restype = ctypes.c_char_p
    lib.vkd3d_shader_get_version.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    return lib, found[-1]


def compile_(lib, data, source_type, target_type, name, profile=None):
    buffer = ctypes.create_string_buffer(data, len(data))
    info = CompileInfo(COMPILE_INFO, None, Code(ctypes.cast(buffer, ctypes.c_void_p), len(data)), source_type, target_type,
                       None, 0, LOG_WARNING, name.encode())
    hlsl = None
    if profile:
        hlsl = HlslSourceInfo(HLSL_SOURCE_INFO, None, b"main", Code(None, 0), profile.encode())
        info.next = ctypes.cast(ctypes.pointer(hlsl), ctypes.c_void_p)
    out, messages = Code(), ctypes.c_char_p()
    result = lib.vkd3d_shader_compile(ctypes.byref(info), ctypes.byref(out), ctypes.byref(messages))
    text = messages.value.decode(errors="replace") if messages.value else ""
    if result != 0:
        sys.exit(f"{name}: vkd3d-shader result {result}\n{text}")
    blob = ctypes.string_at(out.code, out.size)
    return blob, text


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


# The DXBC container checksum: MD5's block function over everything after the checksum field, with DXBC's own
# last block (the C++ twin is `checksum` in include/fixed_spike_shader_policy.h).
def _md5_block(state, block):
    import math, struct
    sines = [int(abs(math.sin(i + 1)) * 2**32) & 0xFFFFFFFF for i in range(64)]
    turns = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21]
    m = struct.unpack("<16I", block)
    a, b, c, d = state
    for i in range(64):
        if i < 16: f, g = (b & c) | (~b & d), i
        elif i < 32: f, g = (d & b) | (~d & c), (5 * i + 1) % 16
        elif i < 48: f, g = b ^ c ^ d, (3 * i + 5) % 16
        else: f, g = c ^ (b | (~d & 0xFFFFFFFF)), (7 * i) % 16
        f = (f + a + sines[i] + m[g]) & 0xFFFFFFFF
        a, d, c = d, c, b
        r = turns[(i // 16) * 4 + i % 4]
        b = (b + (((f << r) | (f >> (32 - r))) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return [(x + y) & 0xFFFFFFFF for x, y in zip(state, (a, b, c, d))]


def checksum(blob):
    import struct
    data = blob[20:]
    state = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476]
    whole = len(data) // 64 * 64
    for at in range(0, whole, 64):
        state = _md5_block(state, data[at:at + 64])
    left, bits = data[whole:], len(data) * 8
    last = (bits >> 2) | 1
    if len(left) >= 56:
        state = _md5_block(state, (left + b"\x80").ljust(64, b"\0"))
        tail = struct.pack("<I", bits).ljust(64, b"\0")
    else:
        tail = (struct.pack("<I", bits) + left + b"\x80").ljust(64, b"\0")
    tail = tail[:60] + struct.pack("<I", last)
    state = _md5_block(state, tail)
    return struct.pack("<4I", *state)


# The shipped game shaders are containers of exactly ISGN, OSGN and SHEX (Shader Model 5 code); vkd3d-shader writes the
# same code in a SHDR chunk and adds RDEF and STAT. The container is rebuilt to the shipped form: the input and output
# signatures and the token stream are copied unchanged, the code chunk is tagged SHEX, the two informational chunks
# are dropped, and the checksum is recomputed (vkd3d-shader verifies it again when --validate reads the blob back).
def shipped_form(blob):
    import struct
    count = struct.unpack_from("<I", blob, 28)[0]
    chunks = {}
    for i in range(count):
        at = struct.unpack_from("<I", blob, 32 + 4 * i)[0]
        tag, size = blob[at:at + 4], struct.unpack_from("<I", blob, at + 4)[0]
        chunks[tag] = blob[at + 8:at + 8 + size]
    code = chunks.get(b"SHEX", chunks.get(b"SHDR"))
    if b"ISGN" not in chunks or b"OSGN" not in chunks or code is None or struct.unpack_from("<I", code, 0)[0] >> 16 not in (0, 1) \
            or struct.unpack_from("<I", code, 0)[0] & 0xFF != 0x50:
        sys.exit("unexpected container from vkd3d-shader")
    parts = [(b"ISGN", chunks[b"ISGN"]), (b"OSGN", chunks[b"OSGN"]), (b"SHEX", code)]
    offset = 32 + 4 * len(parts)
    body, offsets = b"", []
    for tag, payload in parts:
        offsets.append(offset + len(body))
        body += tag + struct.pack("<I", len(payload)) + payload
    total = offset + len(body)
    out = bytearray(b"DXBC" + bytes(16) + struct.pack("<III", 1, total, len(parts)) + b"".join(struct.pack("<I", o) for o in offsets) + body)
    out[4:20] = checksum(bytes(out))
    return bytes(out)


def build(lib):
    blobs = []
    for kind, file, profile in SHADERS:
        source = (ROOT / "shaders" / file).read_bytes()
        blob, text = compile_(lib, source, SOURCE_HLSL, TARGET_DXBC_TPF, file, profile)
        if text.strip():
            sys.exit(f"{file}: compiler messages (warnings are not accepted):\n{text}")
        if blob[:4] != b"DXBC" or int.from_bytes(blob[24:28], "little") != len(blob) or checksum(blob) != blob[4:20]:
            sys.exit(f"{file}: not a DXBC container, or its checksum is not the one this tool computes")
        blob = shipped_form(blob)
        blobs.append((kind, file, profile, source, blob))
    return blobs


def header(blobs, version):
    lines = ["// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT",
             "#pragma once",
             "// GENERATED by modules/camera/tools/frame_shaders.py from modules/camera/shaders/*.hlsl; do not edit.",
             f"// Compiler: vkd3d-shader {version} (HLSL -> DXBC, Shader Model 5.0). Shaders written for this project from",
             "// scratch (P5.5-02 wave 2, package \"shader\": the Stage 2 frame pass and the Stage 3.1 cap). The portable tests check the",
             "// container checksum of each blob and that each source file still hashes to the value recorded here.",
             "#include <array>",
             "#include <cstdint>",
             "",
             "namespace skycrpg::camera::fixed::shader::frame_bytes {"]
    for kind, file, profile, source, blob in blobs:
        name = NAMES[kind]
        lines.append(f"// {file}, {profile}: {len(blob)} bytes, FNV-1a-64 of the bytes {fnv(blob):016x}")
        lines.append(f"constexpr std::uint64_t {name}Source = 0x{fnv(source):016x}ull; // FNV-1a-64 of {file} as committed")
        lines.append(f"constexpr std::array<std::uint8_t,{len(blob)}> {name}{{")
        for at in range(0, len(blob), 24):
            lines.append("    " + ",".join(f"0x{b:02x}" for b in blob[at:at + 24]) + ("," if at + 24 < len(blob) else ""))
        lines.append("};")
    lines.append("} // namespace skycrpg::camera::fixed::shader::frame_bytes")
    return "\n".join(lines) + "\n"


def main():
    lib, path = library()
    version = lib.vkd3d_shader_get_version(None, None).decode()
    blobs = build(lib)
    text = header(blobs, version)
    if "--check" in sys.argv:
        same = HEADER.exists() and HEADER.read_text() == text
        print(f"frame shaders {'MATCH' if same else 'DIFFER'}: header vs a fresh build with {path} ({version})")
        sys.exit(0 if same else 1)
    if "--validate" in sys.argv:
        out = Path(sys.argv[sys.argv.index("--validate") + 1])
        out.mkdir(parents=True, exist_ok=True)
        for kind, file, profile, source, blob in blobs:
            asm, notes = compile_(lib, blob, SOURCE_DXBC_TPF, TARGET_D3D_ASM, file)
            spirv, notes2 = compile_(lib, blob, SOURCE_DXBC_TPF, TARGET_SPIRV_BINARY, file)
            (out / (file + ".asm")).write_bytes(asm)
            (out / (file + ".spv")).write_bytes(spirv)
            print(f"{file}: {len(blob)} bytes DXBC; disassembly {len(asm)} bytes; SPIR-V {len(spirv)} bytes; messages: "
                  f"{(notes + notes2).strip() or 'none'}")
        return
    HEADER.write_text(text)
    for kind, file, profile, source, blob in blobs:
        print(f"{file} {profile}: {len(blob)} bytes, source fnv {fnv(source):016x}, bytes fnv {fnv(blob):016x}")
    print(f"wrote {HEADER.relative_to(ROOT.parents[1])} with vkd3d-shader {version}")


if __name__ == "__main__":
    main()
