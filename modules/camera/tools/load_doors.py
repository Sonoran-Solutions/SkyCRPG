#!/usr/bin/env python3
# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""Measure the vanilla dungeon load-door models for the frame pass's door void (P5.5-02 wave 2, package "shader", iDoorVoid).

Read-only, standard library only (the mesh archive and NIF readers are tools/open_tops.py's). From Skyrim.esm it reads every
DOOR base and every placed reference of one (interior cells and worldspaces) with its XTEL, if any. A candidate model is a
DOOR model under dungeons\\ used by at least one load door (a reference with XTEL) in an interior cell, minus the trapdoors,
ladders, sewer holes, anchors, hole doors, lift facades and partitions (not a leaf in a doorway). For each candidate the
model's shapes are read from the vanilla mesh archives (node transforms applied) and measured:

    thin      the horizontal model axis with the smaller span (0 local x, 1 local y): the leaf's plane normal
    centre    the middle of the mesh along the thin axis (the plane)        lateral  the middle along the other axis
    halfThick half the span along the thin axis                             halfWidth half the span along the other axis
    floor     the lowest z                                                  height    the z span
    front     the side of the thin axis that the faces whose normal lies within 45 degrees of it point to, by area
              (counter-clockwise winding is the front, as the engine's rasteriser states have FrontCounterClockwise=1)
    keep      how far behind the plane (against `front`) the void must stay away: the reach of those front-facing faces
              behind it, and at least the mesh's half thickness up to 32 (the frame's reveal), so nothing of the door that
              faces the dungeon side is darkened (the mine door's frame is 187 thick but its front faces reach only 44
              behind its middle: its void starts there, not 94 behind it, where Embershard's cliff stub already shows)

and checked against the data: for every interior load door of that model (untilted, enabled), the arrival pose stored in its
partner's XTEL must lie on the `front` side of the plane, placed with the engine's rotation about z (local x -> (cos z,
-sin z), local y -> (sin z, cos z), as include/fixed_spike_shader_door_policy.h places it). A model is kept only if it is a
leaf in a doorway (halfThick <= 128 and at most 0.6 halfWidth, halfWidth >= 48, height >= 128), at least 90 percent of that
face area is on one side (single-sided: the Riften Ratway and ship doors, modelled two-sided, are left out) and at least 90
percent of its arrivals agree. The references whose arrival disagrees (Volunruud's 00020320: the dungeon is behind its
leaf) are listed in the header as wrong-facing, and the runtime never gives them a void. The header also counts the
interior references of each kept model without XTEL (doors that are not load doors; the runtime requires ExtraTeleport).

    load_doors.py            write modules/camera/include/fixed_spike_shader_door_table.h
    load_doors.py --check    compare a fresh table with the committed header (exit 1 if different)
    load_doors.py --list     print every candidate with its measures and why it is kept or left out

SKYCRPG_SKYRIM_DATA overrides the Data folder (open_tops.py's DATA). Nothing of the game is written. Not part of the build.
"""
import math
import struct
import sys
import zlib
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import open_tops as ot  # noqa: E402  (the BSA and NIF readers, FNV-1a-64, DATA)

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include/fixed_spike_shader_door_table.h"
LEFT_OUT = ("trapdoor", "ladder", "sewerhole", "anchor", "holedoor", "facade", "partitions")
MAX_HALF_THICK, THICK_RATIO, MIN_HALF_WIDTH, MIN_HEIGHT, MIN_FRONT, MIN_AGREE, TILT = 128.0, 0.6, 48.0, 128.0, 0.90, 0.90, 0.01
REVEAL = 32.0


# ---- Skyrim.esm: DOOR bases and their references ------------------------------------------
def esm_doors():
    data = (ot.DATA / "Skyrim.esm").read_bytes()
    bases, refs = {}, []

    def body(at, size, flags):
        raw = data[at + 24:at + 24 + size]
        return zlib.decompress(raw[4:]) if flags & 0x40000 else raw

    def walk(pos, end, interior, cell):
        while pos < end:
            tag = data[pos:pos + 4]
            size = struct.unpack_from("<I", data, pos + 4)[0]
            if tag == b"GRUP":
                label, kind = data[pos + 8:pos + 12], struct.unpack_from("<I", data, pos + 12)[0]
                if kind == 0:
                    if label == b"DOOR":
                        walk(pos + 24, pos + size, interior, cell)
                    elif label == b"CELL":
                        walk(pos + 24, pos + size, True, cell)
                    elif label == b"WRLD":
                        walk(pos + 24, pos + size, False, cell)
                elif kind == 6:
                    walk(pos + 24, pos + size, interior, struct.unpack("<I", label)[0])
                else:
                    walk(pos + 24, pos + size, interior, cell)
                pos += size
                continue
            flags, form = struct.unpack_from("<II", data, pos + 8)
            if tag == b"DOOR":
                model = None
                for sub, value in ot.subrecords(body(pos, size, flags)):
                    if sub == b"MODL":
                        model = ot.normalise(value.rstrip(b"\0").decode("latin1"))
                bases[form] = model
            elif tag == b"REFR":
                raw = body(pos, size, flags)
                at = raw.find(b"NAME\x04\x00")
                base = struct.unpack_from("<I", raw, at + 6)[0] if at >= 0 else None
                if base in bases:
                    xtel = place = None
                    for sub, value in ot.subrecords(raw):
                        if sub == b"DATA" and len(value) >= 24:
                            place = struct.unpack_from("<6f", value)
                        elif sub == b"XTEL":
                            xtel = (struct.unpack_from("<I", value)[0], struct.unpack_from("<6f", value, 4))
                    refs.append(dict(form=form, base=base, place=place, xtel=xtel, interior=interior, cell=cell,
                                     off=bool(flags & 0x820)))  # initially disabled 0x800, deleted 0x20
            pos += 24 + size

    walk(24 + struct.unpack_from("<I", data, 4)[0], len(data), False, None)
    return bases, refs


# ---- the meshes ------------------------------------------------------------------------------
def triangles(model, archives):
    key = "meshes\\" + model
    for archive in archives:
        if key in archive.files:
            shapes, nodes = ot.shapes(archive.read(key))
            break
    else:
        return None
    parent = {kid: i for i, (_, _, _, kids) in nodes.items() for kid in kids}
    points, tris = [], []
    for index, _, t, rot, sc, vertices, shape_tris in shapes:
        chain, up = [(t, rot, sc)], parent.get(index)
        while up is not None:
            chain.append(nodes[up][:3])
            up = parent.get(up)
        pts = [tuple(v) for v in vertices]
        for t_, r, s in chain:
            pts = [(t_[0] + s * (r[0] * x + r[1] * y + r[2] * z), t_[1] + s * (r[3] * x + r[4] * y + r[5] * z),
                    t_[2] + s * (r[6] * x + r[7] * y + r[8] * z)) for x, y, z in pts]
        base = len(points)
        points += pts
        tris += [(a + base, b + base, c + base) for a, b, c in shape_tris]
    return points, tris


def measure(model, archives):
    mesh = triangles(model, archives)
    if not mesh or not mesh[0]:
        return None
    points, tris = mesh
    lo = [min(p[i] for p in points) for i in range(3)]
    hi = [max(p[i] for p in points) for i in range(3)]
    thin = 0 if hi[0] - lo[0] < hi[1] - lo[1] else 1
    plus = minus = 0.0
    faces = []  # (sign of the thin component, the triangle's lowest and highest thin coordinate)
    for a, b, c in tris:
        A, B, C = points[a], points[b], points[c]
        u = [B[i] - A[i] for i in range(3)]
        v = [C[i] - A[i] for i in range(3)]
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        length = math.sqrt(sum(x * x for x in n))
        if length < 1e-9:
            continue
        if n[thin] / length > 0.7071:
            plus += length / 2
        elif n[thin] / length < -0.7071:
            minus += length / 2
        else:
            continue
        faces.append((1 if n[thin] > 0 else -1, min(A[thin], B[thin], C[thin]), max(A[thin], B[thin], C[thin])))
    other = 1 - thin
    front = 1 if plus >= minus else -1
    centre = (lo[thin] + hi[thin]) / 2
    reach = max([0.0] + [centre - low if front > 0 else high - centre for sign, low, high in faces if sign == front])
    keep = max(reach, min((hi[thin] - lo[thin]) / 2, REVEAL))
    return dict(thin=thin, centre=centre, lateral=(lo[other] + hi[other]) / 2, keep=keep,
                halfThick=(hi[thin] - lo[thin]) / 2, halfWidth=(hi[other] - lo[other]) / 2, floor=lo[2], height=hi[2] - lo[2],
                front=front, frontShare=max(plus, minus) / (plus + minus) if plus + minus > 0 else 0.0,
                plus=plus, minus=minus)


# The engine's placement of a model axis by the reference's rotation about z (the runtime's doorPlane does the same).
def axis(thin, angle_z):
    c, s = math.cos(angle_z), math.sin(angle_z)
    return (c, -s) if thin == 0 else (s, c)


def audit(bases, refs, archives):
    arrival = {}
    for r in refs:
        if r["xtel"]:
            arrival.setdefault(r["xtel"][0], r["xtel"][1])
    uses = {}
    for r in refs:
        model = bases.get(r["base"])
        if r["interior"] and model:
            uses.setdefault(model, []).append(r)
    rows = []
    for model in sorted(uses):
        loads = [r for r in uses[model] if r["xtel"] and not r["off"]]
        if not model.startswith("dungeons\\") or not loads:
            continue
        row = dict(model=model, loads=len(loads), plain=sum(1 for r in uses[model] if not r["xtel"] and not r["off"]),
                   agree=0, disagree=0, tilted=0, unknown=0, wrong=[])
        if any(word in model for word in LEFT_OUT):
            row["why"] = "not a leaf in a doorway"
            rows.append(row)
            continue
        m = measure(model, archives)
        if m is None:
            row["why"] = "no mesh"
            rows.append(row)
            continue
        row.update(m)
        for r in loads:
            pose = arrival.get(r["form"])
            if not r["place"] or pose is None:
                row["unknown"] += 1
                continue
            x, y, _, ax, ay, az = r["place"]
            if abs(ax) > TILT or abs(ay) > TILT:
                row["tilted"] += 1
                continue
            scale = 1.0  # XSCL is not read: the side test does not depend on the scale
            t, l = axis(m["thin"], az), axis(1 - m["thin"], az)
            px = x + scale * (t[0] * m["centre"] + l[0] * m["lateral"])
            py = y + scale * (t[1] * m["centre"] + l[1] * m["lateral"])
            side = (pose[0] - px) * t[0] + (pose[1] - py) * t[1]
            row["agree" if side * m["front"] > 0 else "disagree"] += 1
            if side * m["front"] <= 0:
                row["wrong"].append(r["form"])
        checked = row["agree"] + row["disagree"]
        if m["halfThick"] > MAX_HALF_THICK or m["halfThick"] > THICK_RATIO * m["halfWidth"] or m["halfWidth"] < MIN_HALF_WIDTH or \
                m["height"] < MIN_HEIGHT:
            row["why"] = "not a leaf in a doorway"
        elif m["frontShare"] < MIN_FRONT:
            row["why"] = "faces on both sides (two-sided leaf)"
        elif not checked or row["agree"] < MIN_AGREE * checked:
            row["why"] = "arrivals lie behind the leaf" if checked else "no arrival to check the side"
        else:
            row["why"] = None
        rows.append(row)
    return rows


def header(rows, candidates):
    kept = sorted((ot.fnv(r["model"]), r) for r in rows if r.get("why", "x") is None)
    lines = ["// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT",
             "#pragma once",
             "// GENERATED by modules/camera/tools/load_doors.py from Skyrim.esm and the vanilla mesh archives; do not edit.",
             f"// {candidates} dungeon load-door models read; these are single-sided leaves whose faces point to the side every",
             "// vanilla arrival lies on (the dungeon side). Model units; key: FNV-1a-64 of the model path as the CK stores it, lower",
             "// case, without \"meshes\\\", backslashes (modelHash). Per model: the interior load doors checked and the interior",
             "// references without XTEL (not load doors: the runtime also requires ExtraTeleport).",
             "#include <array>",
             "#include <cstdint>",
             "",
             "namespace skycrpg::camera::fixed::shader {",
             "struct DoorModel {",
             "    std::uint64_t hash{};",
             "    const char* path{};",
             "    std::uint8_t thin{};   // the leaf's thin axis, the plane's normal: 0 local x, 1 local y",
             "    std::int8_t front{};   // +1 / -1: the side of that axis the leaf's faces point to (the dungeon side)",
             "    float centre{};        // the plane: the middle along the thin axis",
             "    float lateral{};       // the middle along the other horizontal axis",
             "    float keep{};          // how far behind the plane the faces that point to the front side reach",
             "    float halfThick{},halfWidth{},floor{},height{};",
             "};",
             f"constexpr std::array<DoorModel,{len(kept)}> loadDoorModels{{{{"]
    for value, r in kept:
        lines.append(f"    {{0x{value:016x}ull,\"{r['model'].replace(chr(92), chr(92) * 2)}\",{r['thin']},{r['front']},"
                     f"{r['centre']:.2f}f,{r['lateral']:.2f}f,{r['keep']:.2f}f,{r['halfThick']:.2f}f,{r['halfWidth']:.2f}f,{r['floor']:.2f}f,{r['height']:.2f}f}},"
                     f" // load doors: {r['agree']} agree, {r['disagree']} wrong-facing, {r['tilted']} tilted, {r['unknown']} no arrival; {r['plain']} without XTEL; "
                     f"front share {r['frontShare']:.3f}")
    lines.append("}};")
    wrong = sorted((form, r["model"]) for _, r in kept for form in r["wrong"])
    lines.append("// Load doors of these models whose arrival lies behind the leaf (Skyrim.esm form IDs): never given a void.")
    lines.append(f"constexpr std::array<std::uint32_t,{len(wrong)}> wrongFacingDoors{{")
    for i, (form, model) in enumerate(wrong):
        lines.append(f"    0x{form:08x}u{',' if i + 1 < len(wrong) else ''} // {model}")
    lines.append("};")
    lines.append("} // namespace skycrpg::camera::fixed::shader")
    return "\n".join(lines) + "\n"


def main():
    archives = [ot.BSA(str(ot.DATA / name)) for name in ot.ARCHIVES]
    bases, refs = esm_doors()
    rows = audit(bases, refs, archives)
    if "--list" in sys.argv:
        for r in rows:
            state = "KEEP" if r.get("why", "x") is None else "out "
            measures = "" if "thin" not in r else (
                f" thin={'xy'[r['thin']]} front={r['front']:+d} share={r['frontShare']:.3f} centre={r['centre']:.1f} lateral={r['lateral']:.1f} "
                f"keep={r['keep']:.1f} halfThick={r['halfThick']:.1f} halfWidth={r['halfWidth']:.1f} floor={r['floor']:.1f} height={r['height']:.1f}")
            print(f"{state} {r['model']} loads={r['loads']} plain={r['plain']} agree={r['agree']} disagree={r['disagree']} "
                  f"tilted={r['tilted']} unknown={r['unknown']}{measures} {r.get('why') or ''}"
                  f"{' wrong=' + ','.join(f'{w:08X}' for w in r['wrong']) if r['wrong'] else ''}")
        return
    text = header(rows, len(rows))
    kept = sum(1 for r in rows if r.get("why", "x") is None)
    if "--check" in sys.argv:
        same = HEADER.exists() and HEADER.read_text() == text
        print(f"load doors {'MATCH' if same else 'DIFFER'}: {kept} of {len(rows)} dungeon load-door models")
        sys.exit(0 if same else 1)
    HEADER.write_text(text)
    print(f"wrote {HEADER.name}: {kept} of {len(rows)} dungeon load-door models")


if __name__ == "__main__":
    main()
