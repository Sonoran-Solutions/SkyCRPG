#!/usr/bin/env python3
"""Rendered player visibility from runner screenshots and a probe view_sample (P5.5-01, DYL-216).

Final-image technique (visibility-instrument.md, candidate b). At one settled pose the route takes three
frames of the Skyrim window: A with the player as is, B with the player made transparent
(`player.saa 0`), A2 after restoring it (`player.saa 1`). Pixels that change between A and B inside the
player's projected box are player pixels that reached the screen; A against A2 is the noise floor (TAA,
idle animation, flicker) and the proof that the player was restored. A calibration triple taken the same
way at an unobstructed pose with the same rig gives the signal a fully visible player produces.

Geometric clearance (the probe's rays) is diagnostic only; this is the rendered measure. Every in-game use
is NOT_RUN until the oracle checkpoint (session-p55/oracle) has been run and read.

Standard library only. Commands:
  measure   A B A2 --view VIEW.json [--view-after VIEW2.json] --calibration CAL.json -> verdict JSON
  calibrate A B A2 --view VIEW.json -> calibration JSON (or an unconfirmed marker)
  overlay   FRAME --view VIEW.json --out OUT.png -> crop with the measurement box and points drawn
  view      PROBE_SLICE.jsonl TAG -> the view_sample with that tag, as JSON
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import png  # noqa: E402

POINTS = ("head", "chest", "feet", "shoulder_l", "shoulder_r")
VERSION = 1

# Thresholds. Noise limits are set from wave 1 frame pairs taken at the same pose seconds apart (exterior
# wind: box 0.07-0.08, ring 0.08-0.10, frame 0.05-0.06; interiors: ring <= 0.01, frame <= 0.025; dark
# seeded room at threshold 5: box 0.15); see visibility-instrument.md. Signal limits are design starting
# points, not measured with the marker (NOT_RUN); D55-6 belongs to the owner.
PARAMS = {
    "client": (1920, 1080),        # Skyrim.INI iSize W/H the runner deploys (report.json "resolution")
    "change_threshold": 12,        # per-pixel max channel |A-B| counted as changed (8-bit)
    "min_threshold": 3,            # floor of the threshold in a dark box (scaled with luminance below)
    "dark_luminance": 28.0,        # mean box luminance (0..255) below which the box counts as dark
    "box_side": 0.35,              # box margin left/right of the points, in player heights
    "box_top": 0.35,               # above the head point (the head bone sits at the neck)
    "box_bottom": 0.35,            # below the feet point (rendered boots sat 24 px below it in Dragonsreach)
    "point_radius": 0.07,          # point window half-size, in player heights (at least 3 px)
    "ring": 1.0,                   # control ring around the box, in player heights
    "min_height_px": 12,           # head-to-feet height below which the box is too small to measure
    "control_max": 0.20,           # changed fraction allowed in the control ring between A and A2
    "control_max_ab": 0.30,        # between A and B (looser: the player's own shadow may leave the ring)
    "global_max": 0.12,            # changed fraction allowed over the client outside the ring (A-B and A-A2)
    "noise_max_box": 0.25,         # A-A2 changed fraction allowed in the box (else noisy)
    "noise_max_point": 0.40,
    "restore_min_signal": 0.05,    # A-B box signal needed before A2 can prove the player came back
    "restore_ratio": 0.5,          # A2 must differ from B at least this much relative to A
    "cal_min_box": 0.08,           # calibration signal (s - n) needed to confirm the marker, box
    "cal_min_point": 0.40,         # and per point
    "cal_min_points": 3,           # points that must confirm in calibration
    "scale_tolerance": 0.25,       # relative player height difference allowed vs calibration
    "visible": 0.60,               # ratio to calibration for "visible"
    "partly": 0.15,                # ratio to calibration for "partly"; below is "not_visible"
    "pose_tolerance": 2.0,         # view before/after: camera position (units) and yaw/pitch (degrees)
}


class Indeterminate(Exception):
    pass


# ---- geometry -----------------------------------------------------------------------------
def client_origin(image_w: int, image_h: int, client_w: int, client_h: int) -> tuple[int, int]:
    """Top-left of the game's client area inside an xwd window capture.

    The runner captures the decorated window (capture/core.py capture_xwd). On the host the decoration is a
    3-pixel border left, right and bottom and a 29-pixel top (border + title bar): 1926x1112 for a 1920x1080
    client (wave 1 report.json and pixel rows 0-28 / columns 0-2). Side and bottom borders are taken equal.
    """
    extra_w, extra_h = image_w - client_w, image_h - client_h
    if extra_w < 0 or extra_h < 0 or extra_w % 2:
        raise Indeterminate("frame_geometry: %dx%d does not hold a %dx%d client" % (image_w, image_h, client_w, client_h))
    side = extra_w // 2
    top = extra_h - side
    if top < 0:
        raise Indeterminate("frame_geometry: negative title bar")
    return side, top


@dataclass
class Geometry:
    origin: tuple[int, int]
    client: tuple[int, int]

    def pixel(self, nx: float, ny: float) -> tuple[float, float]:
        """Normalised view_sample screen coordinates (0..1, origin top-left, y down) to image pixels."""
        return self.origin[0] + nx * self.client[0], self.origin[1] + ny * self.client[1]

    def inside(self, rect) -> bool:
        x0, y0, x1, y1 = rect
        cx, cy = self.origin
        return x0 >= cx and y0 >= cy and x1 <= cx + self.client[0] and y1 <= cy + self.client[1]


@dataclass
class Layout:
    points: dict                     # name -> (px, py) or None
    box: tuple                       # x0, y0, x1, y1 (half-open, pixels)
    ring: tuple                      # outer rectangle of the control ring
    height: float                    # head-to-feet in pixels
    windows: dict = field(default_factory=dict)   # name -> rect
    off_screen: list = field(default_factory=list)


def layout(view: dict, geometry: Geometry, params=PARAMS) -> Layout:
    if not view or not view.get("available"):
        raise Indeterminate("view_unavailable: %s" % ((view or {}).get("reason") or "no view sample"))
    points, off = {}, []
    for entry in view.get("points") or []:
        name = entry.get("name")
        if name not in POINTS:
            continue
        screen = entry.get("screen")
        if screen is None or entry.get("on_screen") is not True:
            points[name] = None
            if entry.get("on_screen") is False:
                off.append(name)
            continue
        points[name] = geometry.pixel(screen["x"], screen["y"])
    head, feet = points.get("head"), points.get("feet")
    if head is None or feet is None:
        raise Indeterminate("box_off_screen: head or feet point not on screen")
    height = feet[1] - head[1]
    if height < params["min_height_px"]:
        raise Indeterminate("box_too_small: %.1f px from head to feet" % height)
    known = [p for p in points.values() if p is not None]
    xs, ys = [p[0] for p in known], [p[1] for p in known]
    box = (int(math.floor(min(xs) - params["box_side"] * height)), int(math.floor(min(ys) - params["box_top"] * height)),
           int(math.ceil(max(xs) + params["box_side"] * height)) + 1, int(math.ceil(max(ys) + params["box_bottom"] * height)) + 1)
    if not geometry.inside(box):
        raise Indeterminate("box_off_screen: measurement box leaves the client area")
    pad = params["ring"] * height
    cx, cy = geometry.origin
    ring = (max(cx, int(box[0] - pad)), max(cy, int(box[1] - pad)),
            min(cx + geometry.client[0], int(box[2] + pad)), min(cy + geometry.client[1], int(box[3] + pad)))
    r = max(3, int(round(params["point_radius"] * height)))
    windows = {n: (int(round(p[0])) - r, int(round(p[1])) - r, int(round(p[0])) + r + 1, int(round(p[1])) + r + 1)
               for n, p in points.items() if p is not None}
    return Layout(points=points, box=box, ring=ring, height=height, windows=windows, off_screen=off)


# ---- pixel statistics -----------------------------------------------------------------------
def _diff_row(a: bytes, b: bytes):
    return [max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2])) for i in range(0, len(a), 3)]


@dataclass
class RegionStats:
    pixels: int
    changed: int
    mean_diff: float
    luminance: float

    @property
    def fraction(self) -> float:
        return self.changed / self.pixels if self.pixels else 0.0


def region_stats(a: png.Image, b: png.Image, rect, threshold: int, exclude=None, step: int = 1) -> RegionStats:
    """Changed-pixel fraction (max channel |a-b| >= threshold) and mean difference over rect minus exclude.

    Luminance is the mean Rec. 601 luma of a and b over the same pixels.
    """
    x0, y0, x1, y1 = rect
    x0, y0, x1, y1 = max(0, x0), max(0, y0), min(a.width, x1), min(a.height, y1)
    pixels = changed = 0
    total = luma = 0.0
    w = a.width
    for y in range(y0, y1, step):
        row_a = a.rgb[(y * w + x0) * 3:(y * w + x1) * 3]
        row_b = b.rgb[(y * w + x0) * 3:(y * w + x1) * 3]
        diffs = _diff_row(row_a, row_b)
        for k in range(0, x1 - x0, step):
            x = x0 + k
            if exclude and exclude[0] <= x < exclude[2] and exclude[1] <= y < exclude[3]:
                continue
            d = diffs[k]
            pixels += 1
            total += d
            if d >= threshold:
                changed += 1
            i = k * 3
            luma += (0.299 * (row_a[i] + row_b[i]) + 0.587 * (row_a[i + 1] + row_b[i + 1])
                     + 0.114 * (row_a[i + 2] + row_b[i + 2])) / 2
    return RegionStats(pixels, changed, total / pixels if pixels else 0.0, luma / pixels if pixels else 0.0)


# ---- triple analysis ----------------------------------------------------------------------------
def load_frames(paths):
    frames = []
    for name, path in zip(("A", "B", "A2"), paths):
        if path is None or not Path(path).is_file():
            raise Indeterminate("frame_missing: %s" % name)
        try:
            frames.append(png.read(path))
        except png.PngError as exc:
            raise Indeterminate("frame_unreadable: %s: %s" % (name, exc)) from None
    sizes = {(f.width, f.height) for f in frames}
    if len(sizes) != 1:
        raise Indeterminate("frame_size_mismatch: %s" % sorted(sizes))
    return frames


def pose_change(before: dict, after: dict, params=PARAMS):
    """None when the two view samples show the same camera pose, else a reason."""
    if not after:
        return None
    if not after.get("available"):
        return "view_after_unavailable: %s" % after.get("reason")
    cb, ca = before["camera"], after["camera"]
    moved = math.dist([cb["pos"][k] for k in "xyz"], [ca["pos"][k] for k in "xyz"])
    dyaw = abs((cb["yaw_deg"] - ca["yaw_deg"] + 180) % 360 - 180)
    dpitch = abs(cb["pitch_deg"] - ca["pitch_deg"])
    if moved > params["pose_tolerance"] or dyaw > params["pose_tolerance"] or dpitch > params["pose_tolerance"]:
        return "camera_moved: %.1f units, yaw %.2f, pitch %.2f between the view samples" % (moved, dyaw, dpitch)
    return None


def analyse(frames, view: dict, view_after=None, params=PARAMS) -> dict:
    """Signal and noise statistics for one A/B/A2 triple; raises Indeterminate for unusable input."""
    a, b, a2 = frames
    geometry = Geometry(client_origin(a.width, a.height, *params["client"]), params["client"])
    moved = pose_change(view, view_after, params) if view_after is not None else None
    if moved:
        raise Indeterminate(moved)
    lay = layout(view, geometry, params)
    luminance = region_stats(a, b, lay.box, 256).luminance
    dark = luminance < params["dark_luminance"]
    t = change_threshold(luminance, params)
    box_ab, box_aa = region_stats(a, b, lay.box, t), region_stats(a, a2, lay.box, t)
    box_a2b = region_stats(a2, b, lay.box, t)
    ring_ab, ring_aa = region_stats(a, b, lay.ring, t, exclude=lay.box), region_stats(a, a2, lay.ring, t, exclude=lay.box)
    client = (geometry.origin[0], geometry.origin[1], geometry.origin[0] + geometry.client[0], geometry.origin[1] + geometry.client[1])
    glob_ab = region_stats(a, b, client, params["change_threshold"], exclude=lay.ring, step=4)
    glob_aa = region_stats(a, a2, client, params["change_threshold"], exclude=lay.ring, step=4)
    points = {}
    for name in POINTS:
        rect = lay.windows.get(name)
        if rect is None:
            points[name] = None
            continue
        s, n = region_stats(a, b, rect, t), region_stats(a, a2, rect, t)
        points[name] = {"signal": s.fraction, "noise": n.fraction, "contrast": s.mean_diff - n.mean_diff, "rect": rect}
    return {
        "geometry": {"origin": geometry.origin, "client": geometry.client},
        "box": lay.box, "ring": lay.ring, "height_px": lay.height, "off_screen": lay.off_screen,
        "luminance": luminance, "dark": dark, "threshold": t,
        "signal": box_ab.fraction, "noise": box_aa.fraction, "a2_vs_b": box_a2b.fraction,
        "contrast": box_ab.mean_diff - box_aa.mean_diff,
        "control": {"ring_ab": ring_ab.fraction, "ring_aa2": ring_aa.fraction,
                    "global_ab": glob_ab.fraction, "global_aa2": glob_aa.fraction},
        "points": points,
    }


def change_threshold(luminance: float, params=PARAMS) -> int:
    """Per-pixel change threshold: fixed in a lit box, scaled down with luminance in a dark one.

    A player drawn in a dark room is dark too, so the difference it makes is small; a fixed threshold would
    report it as not visible. The threshold falls in proportion to the box luminance below dark_luminance,
    to min_threshold at most.
    """
    if luminance >= params["dark_luminance"]:
        return params["change_threshold"]
    return max(params["min_threshold"], int(round(params["change_threshold"] * luminance / params["dark_luminance"])))


def stability_problem(stats: dict, params=PARAMS):
    c = stats["control"]
    if c["global_ab"] > params["global_max"] or c["global_aa2"] > params["global_max"]:
        return "scene_changed: %.3f / %.3f of the frame changed outside the ring" % (c["global_ab"], c["global_aa2"])
    if c["ring_ab"] > params["control_max_ab"] or c["ring_aa2"] > params["control_max"]:
        return "scene_changed: %.3f / %.3f of the ring around the player changed" % (c["ring_ab"], c["ring_aa2"])
    return None


def restore_state(stats: dict, params=PARAMS):
    """True when frame A2 (after `player.saa 1`) shows the player again, False when it still looks like B,
    None when the player made no difference to begin with (hidden player: restore not provable from frames)."""
    if stats["signal"] < params["restore_min_signal"]:
        return None
    return stats["a2_vs_b"] >= params["restore_ratio"] * stats["signal"]


def calibrate(frames, view: dict, view_after=None, params=PARAMS) -> dict:
    """Calibration record from an unobstructed triple; ``confirmed`` false when the marker did not show."""
    try:
        stats = analyse(frames, view, view_after, params)
    except Indeterminate as exc:
        return {"version": VERSION, "confirmed": False, "reason": str(exc)}
    problems = []
    unstable = stability_problem(stats, params)
    if unstable:
        problems.append(unstable)
    if restore_state(stats, params) is False:
        problems.append("not_restored: A2 looks like B in the box")
    elif stats["noise"] > params["noise_max_box"]:
        problems.append("noisy: A-A2 changed %.3f of the box" % stats["noise"])
    if stats["dark"]:
        problems.append("dark: calibration box luminance %.1f" % stats["luminance"])
    box_signal = stats["signal"] - stats["noise"]
    if box_signal < params["cal_min_box"]:
        problems.append("marker_not_confirmed: box signal %.3f < %.2f" % (box_signal, params["cal_min_box"]))
    point_signal = {}
    for name, p in stats["points"].items():
        point_signal[name] = None if p is None else p["signal"] - p["noise"]
    confirmed_points = [n for n, v in point_signal.items() if v is not None and v >= params["cal_min_point"]]
    if len(confirmed_points) < params["cal_min_points"]:
        problems.append("marker_not_confirmed: %d of 5 points (need %d)" % (len(confirmed_points), params["cal_min_points"]))
    return {"version": VERSION, "confirmed": not problems, "reason": "; ".join(problems) or None,
            "box_signal": box_signal, "box_contrast": stats["contrast"], "point_signal": point_signal,
            "confirmed_points": confirmed_points, "height_px": stats["height_px"], "luminance": stats["luminance"],
            "stats": stats}


def _verdict(ratio, dark, params):
    if ratio >= params["visible"]:
        return "visible"
    if ratio >= params["partly"]:
        return "partly"
    # A weak signal in a dark box can be darkness rather than occlusion: never call it not visible.
    return "indeterminate" if dark else "not_visible"


def measure(frames, view: dict, calibration: dict, view_after=None, params=PARAMS) -> dict:
    """Verdict for the player box and each point. Never raises; unusable input gives indeterminate."""
    result = {"version": VERSION, "tag": (view or {}).get("tag"), "verdict": "indeterminate", "reason": None,
              "ratio": None, "restored": None,
              "points": {n: {"verdict": "indeterminate", "reason": None} for n in POINTS}}
    if not calibration or not calibration.get("confirmed"):
        result["reason"] = "calibration_unconfirmed: %s" % ((calibration or {}).get("reason") or "no calibration")
        for p in result["points"].values():
            p["reason"] = result["reason"]
        return result
    try:
        stats = analyse(frames, view, view_after, params)
    except Indeterminate as exc:
        result["reason"] = str(exc)
        for p in result["points"].values():
            p["reason"] = str(exc)
        return result
    result["stats"] = stats
    result["restored"] = restore_state(stats, params)
    scale = abs(stats["height_px"] - calibration["height_px"]) / calibration["height_px"]
    problem = stability_problem(stats, params)
    if not problem and scale > params["scale_tolerance"]:
        problem = "scale_mismatch: player height %.1f px vs %.1f px in calibration" % (stats["height_px"], calibration["height_px"])
    if not problem and result["restored"] is False:
        problem = "not_restored: A2 looks like B in the box (changed %.3f from B, A changed %.3f)" % (
            stats["a2_vs_b"], stats["signal"])
    if not problem and stats["noise"] > params["noise_max_box"]:
        problem = "noisy: A-A2 changed %.3f of the box" % stats["noise"]
    if problem:
        result["reason"] = problem
        for p in result["points"].values():
            p["reason"] = problem
        return result
    ratio = (stats["signal"] - stats["noise"]) / calibration["box_signal"]
    result.update(ratio=max(0.0, ratio), verdict=_verdict(ratio, stats["dark"], params),
                  reason="dark: box luminance %.1f" % stats["luminance"] if stats["dark"] else None)
    for name in POINTS:
        p, cal = stats["points"][name], calibration["point_signal"].get(name)
        out = result["points"][name]
        if name in stats["off_screen"]:
            out.update(verdict="off_screen", reason="point off screen")
        elif p is None:
            out["reason"] = "point_unavailable"
        elif cal is None or cal < params["cal_min_point"]:
            out["reason"] = "calibration_point_unconfirmed"
        elif p["noise"] > params["noise_max_point"]:
            out["reason"] = "noisy: A-A2 changed %.3f of the window" % p["noise"]
        else:
            r = (p["signal"] - p["noise"]) / cal
            out.update(ratio=max(0.0, r), verdict=_verdict(r, stats["dark"], params),
                       reason="dark" if stats["dark"] else None)
    return result


# ---- view samples ---------------------------------------------------------------------------
def view_from_slice(path, tag: str):
    """Last view_sample with this tag from a probe JSONL slice, or None."""
    found = None
    for line in Path(path).read_text(errors="replace").splitlines():
        if '"view_sample"' not in line:
            continue
        try:
            event = json.loads(line)
        except ValueError:
            continue
        if event.get("ev") == "view_sample" and event.get("tag") == tag:
            found = event
    return found


def draw_overlay(frame: png.Image, lay: Layout, pad: int = 60) -> png.Image:
    """Crop around the ring with the box (green), ring (blue) and point windows (red) outlined."""
    rgb = bytearray(frame.rgb)

    def put(x, y, colour):
        if 0 <= x < frame.width and 0 <= y < frame.height:
            i = (y * frame.width + x) * 3
            rgb[i:i + 3] = bytes(colour)

    def outline(rect, colour):
        x0, y0, x1, y1 = rect
        for x in range(x0, x1):
            put(x, y0, colour)
            put(x, y1 - 1, colour)
        for y in range(y0, y1):
            put(x0, y, colour)
            put(x1 - 1, y, colour)

    outline(lay.ring, (40, 120, 255))
    outline(lay.box, (0, 255, 0))
    for rect in lay.windows.values():
        outline(rect, (255, 0, 0))
    image = png.Image(frame.width, frame.height, bytes(rgb))
    x0, y0, x1, y1 = lay.ring
    return image.crop(x0 - pad, y0 - pad, x1 + pad, y1 + pad)


def _load_json(path):
    return json.loads(Path(path).read_text()) if path else None


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Rendered player visibility from runner screenshots (stdlib only).")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("measure", "calibrate"):
        s = sub.add_parser(name)
        s.add_argument("frame_a")
        s.add_argument("frame_b")
        s.add_argument("frame_a2")
        s.add_argument("--view", required=True, help="view_sample JSON taken at frame A")
        s.add_argument("--view-after", help="view_sample JSON taken after frame A2 (camera must not have moved)")
    sub.choices["measure"].add_argument("--calibration", required=True)
    o = sub.add_parser("overlay")
    o.add_argument("frame")
    o.add_argument("--view", required=True)
    o.add_argument("--out", required=True)
    v = sub.add_parser("view")
    v.add_argument("probe_slice")
    v.add_argument("tag")
    args = parser.parse_args(argv)
    if args.command == "view":
        found = view_from_slice(args.probe_slice, args.tag)
        print(json.dumps(found, indent=2))
        return 0 if found else 1
    if args.command == "overlay":
        frame = png.read(args.frame)
        geometry = Geometry(client_origin(frame.width, frame.height, *PARAMS["client"]), PARAMS["client"])
        png.write(args.out, draw_overlay(frame, layout(_load_json(args.view), geometry)))
        return 0
    view, after = _load_json(args.view), _load_json(args.view_after)
    try:
        frames = load_frames([args.frame_a, args.frame_b, args.frame_a2])
    except Indeterminate as exc:
        frames, failure = None, str(exc)
    if args.command == "calibrate":
        out = calibrate(frames, view, after) if frames else {"version": VERSION, "confirmed": False, "reason": failure}
    else:
        cal = _load_json(args.calibration)
        if frames:
            out = measure(frames, view, cal, after)
        else:
            out = {"version": VERSION, "tag": (view or {}).get("tag"), "verdict": "indeterminate", "reason": failure,
                   "ratio": None, "points": {n: {"verdict": "indeterminate", "reason": failure} for n in POINTS}}
    print(json.dumps(out, indent=2, default=list))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
