#!/usr/bin/env python3
"""Inspect one appended spike session. This never declares a P5-04 outcome."""
import argparse
import json
import math
from pathlib import Path


FILTER_MODES = {"filter", "filter_material_alpha", "filter_alpha_property"}
COMBINED_MODES = FILTER_MODES - {"filter"}


def inspect(rows, *, expect_no_discards=False):
    problems, incomplete = [], []
    if not rows or rows[0].get("event") != "session":
        problems.append("missing session header")
    quiet = bool(rows and rows[0].get("quiet_journal"))
    classifier = {}
    classifier_recorded = False
    restores = []
    original = applied = False
    discards = resets = kept = 0
    captures, chains = {}, {}
    scopes, contact_windows = [], []
    selected = None
    guarded = bool(rows and rows[0].get("filter_policy") == "hull_step_guard_v1")
    for row in rows:
        event = row.get("event")
        capture_id = str(row.get("contact_capture_id", row.get("capture_id", 0)))
        capture = captures.setdefault(capture_id, {"label": "unlabeled", "timings": {}, "closed": False, "heights": []})
        if event == "capture":
            capture["label"] = row.get("label", "unlabeled")
        elif event == "camera_height":
            capture["heights"].append(row)
        elif event == "original":
            original = True
        elif event == "apply":
            if not original:
                problems.append("apply without preceding original")
            applied = True
            selected = row.get("reference")
        elif event == "restore":
            restores.append(row)
            if row.get("result") != "PASS":
                problems.append("restoration conflict/failure")
            applied = original = False
        elif event == "discard":
            discards += 1
            if not (row.get("mode") in FILTER_MODES and row.get("allowlist_match") and row.get("smoothcam_caller") and
                    row.get("calling_thread_id") and row.get("calling_thread_id") == row.get("main_thread_id")):
                problems.append("discard without filter/allowlist/SmoothCam/main-thread authority")
            if row.get("mode") in COMBINED_MODES and (not applied or row.get("reference") != selected):
                problems.append("combined discard without fade on the same reference")
            chain = chains.setdefault(row.get("sequence"), {})
            chain["discard"] = chain.get("discard", 0) + 1
        elif event == "recast_step":
            chains.setdefault(row.get("sequence"), {})["started"] = True
        elif event == "recast_gap":
            chain = chains.setdefault(row.get("sequence"), {})
            chain["gaps"] = chain.get("gaps", 0) + 1
            chain["unsafe_gap"] = chain.get("unsafe_gap", False) or not row.get("safe") or not row.get("complete") or not row.get("original_contact_seen")
        elif event == "material_write_scope":
            scopes.append(row)
            if row.get("local_write") is False:
                problems.append("nonlocal material write (local_write=false)")
            if row.get("local_write") and (row.get("original_material") == row.get("installed_material") or not row.get("installed_material")):
                problems.append("local material claim without a distinct clone")
        elif event == "caster_counts":
            contact_windows.append(row)
            classifier_recorded |= "candidate_hits" in row
            ref = row.get("reference")
            if ref and ref != "0x00000000" and "candidate_hits" in row:
                item = classifier.setdefault(ref, {"models": set(), "hits": 0, "candidate_hits": 0, "smoothcam_candidate_hits": 0})
                if row.get("model"):
                    item["models"].add(row["model"])
                for field in ("hits", "candidate_hits", "smoothcam_candidate_hits"):
                    item[field] += row.get(field, 0)
            if row.get("overflow") or row.get("off_thread") or row.get("unresolved"):
                incomplete.append("caster counts include unresolved/off-thread/overflow contacts")
        elif event == "recast_result":
            chains.setdefault(row.get("sequence"), {})["result"] = row
        elif event == "recast_context_reset":
            chains.setdefault(row.get("sequence"), {})["context_reset"] = True
        elif event == "filter_complete":
            chain = chains.setdefault(row.get("sequence"), {})
            chain["complete"] = True
            result = chain.get("result", {})
            if not chain.get("context_reset") or not chain.get("discard") or chain.get("discard") != row.get("steps"):
                problems.append("filter completion without context reset/complete discard journal")
            if guarded and (chain.get("unsafe_gap") or chain.get("gaps") != row.get("steps") or not 1 <= row.get("steps", 0) <= 8 or row.get("gateway_calls") != row.get("steps", 0) + 2):
                problems.append("filter completion without bounded safe gap sweeps")
            if not row.get("returned_hit") and (not result or result.get("hit") is not False):
                problems.append("filter miss without native continuation miss")
            if row.get("kept_behind"):
                if not row.get("returned_hit") or not result.get("hit") or result.get("allowlist_match") or not result.get("contact_metadata_available") or result.get("reference") in (None, "0x00000000") or row.get("returned_reference") != result.get("reference"):
                    problems.append("kept-behind claim without resolved non-allowlisted hit")
                else:
                    chain["kept"] = True
        elif event == "filter_rollback":
            chains.setdefault(row.get("sequence"), {})["rollback"] = True
            incomplete.append("filter chain rolled back")
        elif event == "engine_reset":
            resets += 1
        elif event == "refused":
            problems.append("refused: " + row.get("reason", "unknown"))
        elif event == "inconclusive":
            incomplete.append(row.get("reason", "cap or lost evidence"))
        elif event == "timing":
            capture["label"] = row.get("capture_label", capture["label"])
            capture["timings"][row["metric"]] = row
            capture["closed"] |= row.get("reason") not in (None, "periodic")
            if row.get("inconclusive") or row.get("overwritten", 0) or row.get("missed", 0) or row.get("session_missed", 0):
                incomplete.append("cap, overwritten ring or dropped samples")
    if applied:
        problems.append("apply with no recorded restore (possibly truncated log)")
    if any(not c.get("complete") and not c.get("rollback") for c in chains.values()):
        incomplete.append("unfinished recast/discard chain (possibly truncated log)")
    captures = {key: c for key, c in captures.items() if key != "0"}
    kept = sum(bool(c.get("kept")) for c in chains.values() if not c.get("rollback"))
    committed_discards = sum(c.get("discard", 0) for c in chains.values() if c.get("complete") and not c.get("rollback"))
    if not captures:
        incomplete.append("no capture recorded")
    if quiet:
        committed_discards = sum(c["timings"].get("engine_frame", {}).get("discards", 0) for c in captures.values())
        kept = sum(c["timings"].get("engine_frame", {}).get("kept_behind", 0) for c in captures.values())
    for capture in captures.values():
        capture["height"] = height_metrics(capture.pop("heights"))
        capture["hook_cost"] = hook_cost(capture)
        timing = capture["timings"].get("engine_frame", {})
        if quiet and rows and rows[0].get("mode") in FILTER_MODES:
            required = ("discards", "matched_hits", "completed_chains", "rollbacks", "kept_behind")
            if any(key not in timing for key in required):
                incomplete.append("quiet filter summary missing counters")
            if timing.get("rollbacks"):
                incomplete.append("quiet filter summary contains rollback")
            if timing.get("discards", 0) and not timing.get("completed_chains"):
                problems.append("quiet discards without completed chains")
        capture["anchor"] = anchor(capture)
        if not capture["closed"]:
            incomplete.append("capture without final summary")
        if rows and rows[0].get("mode") in FILTER_MODES:
            timing = capture["timings"].get("engine_frame", {})
            if not expect_no_discards and (not timing.get("matched_hits") or not timing.get("discards")):
                incomplete.append("filter capture had zero matched hits or zero committed discards")
            capture["filter_exercised"] = bool(timing.get("matched_hits") and timing.get("discards"))
    negative_measured = bool(captures) and rows[0].get("mode") == "filter" and all(
        c["closed"] and c["anchor"]["status"] == "PASS" and
        "discards" in c["timings"].get("engine_frame", {}) and
        c["timings"].get("hook_inclusive", {}).get("total", 0) > 0 for c in captures.values())
    negative_discards = max(committed_discards, sum(c["timings"].get("engine_frame", {}).get("discards", 0) for c in captures.values()))
    if expect_no_discards and negative_discards:
        problems.append("negative control committed discards; expected exactly zero")
    if rows and rows[0].get("mode") in FILTER_MODES and not committed_discards and not expect_no_discards:
        incomplete.append("filter run had zero discards; SmoothCam stability is inconclusive")
    for item in classifier.values():
        item["models"] = sorted(item["models"])
        item["would_discard"] = bool(item["smoothcam_candidate_hits"])
    return {"source_commit": rows[0].get("source_commit") if rows else None,
            "mode": rows[0].get("mode") if rows else None, "quiet_journal": quiet,
            "journal_detail": "summary_counters_only" if quiet else "per_chain",
            "restores": restores, "classifier_by_reference": classifier,
            "classifier_telemetry": "PASS: report-only verdict counts" if classifier_recorded else "NOT_RUN",
            "classifier_distinct_would_discard": sum(item["would_discard"] for item in classifier.values()) if classifier_recorded else None,
            "negative_control": ("FAIL" if negative_discards else "PASS" if negative_measured and not problems and not incomplete else "NOT_RUN") if expect_no_discards else "NOT_RUN",
            "journal": "FAIL" if problems else "PASS", "problems": problems,
            "conclusion": "INCONCLUSIVE" if incomplete or problems else "journal_complete_only",
            "inconclusive_reasons": sorted(set(incomplete)), "discard_records": discards, "discards": committed_discards, "resets": resets,
            "kept_hits_behind_beam": kept, "enclosure_preservation": "NOT_RUN: verify returned reference is fixture enclosure" if kept else "INCONCLUSIVE: no kept hit behind beam measured",
            "material_write_scopes": scopes,
            "caster_count_windows": contact_windows,
            "contact_telemetry": "PASS: counters recorded only" if contact_windows else "NOT_RUN",
            "captures": captures, "smoothcam_stability": "NOT_RUN", "visual_behavior": "NOT_RUN", "p5_outcome": "pending"}


def height_metrics(rows, threshold=1.0, max_gap_ms=250):
    """Schmitt-style extrema: count a reversal only after threshold units of travel.

    Height is camera.z-player.z. A steady stretch remains within threshold units
    of its observed minimum/maximum; gaps split stretches and reversal history.
    """
    samples = []
    invalid = False
    for row in rows:
        try:
            t = float(row["t_ms"])
            height = float(row["camera"]["z"]) - float(row["player"]["z"])
            if not math.isfinite(t) or not math.isfinite(height) or (samples and t <= samples[-1][0]):
                raise ValueError("invalid sample")
            samples.append((t, height))
        except (KeyError, ValueError, TypeError):
            invalid = True
    reversals = 0
    longest = 0.0
    direction = 0
    extrema = None
    window = []
    gaps = 0
    previous = None
    for t, h in samples:
        if previous is not None and t - previous > max_gap_ms:
            gaps += 1
            direction, extrema, window = 0, None, []
        if extrema is None:
            extrema = h
        delta = h - extrema
        if direction == 0 and abs(delta) >= threshold:
            direction, extrema = (1 if delta > 0 else -1), h
        elif direction and direction * delta > 0:
            extrema = h
        elif direction and direction * delta <= -threshold:
            reversals += 1
            direction, extrema = -direction, h
        window.append((t, h))
        while max(x[1] for x in window) - min(x[1] for x in window) > threshold:
            window.pop(0)
        longest = max(longest, t - window[0][0])
        previous = t
    heights = [h for _, h in samples]
    return {"status": "FAIL" if invalid else "PASS" if len(samples) >= 2 and not gaps else "NOT_RUN",
            "samples": len(samples), "threshold_units": threshold, "gap_limit_ms": max_gap_ms,
            "gaps": gaps, "range_units": max(heights) - min(heights) if heights else None,
            "minimum_units": min(heights) if heights else None, "maximum_units": max(heights) if heights else None,
            "direction_reversals": reversals, "longest_steady_ms": longest,
            "filter_committed_samples": sum(row.get("filter_committed") is True for row in rows)}


def hook_cost(capture):
    timings = capture["timings"]
    hook, frame = timings.get("hook_inclusive", {}), timings.get("engine_frame", {})
    if hook.get("room_hook_sampled"):
        return {"status": "NOT_RUN", "reason": "mixed caster and 1/64 room-hook samples"}
    total, frames, frame_ms = hook.get("sum_ms"), frame.get("total", 0), frame.get("sum_ms", 0)
    if (total is None or not frames or not frame_ms or not math.isfinite(total) or total < 0 or
            not math.isfinite(frame_ms) or frame_ms <= 0 or hook.get("overwritten", 0) or
            hook.get("missed", 0) or hook.get("inconclusive") or anchor(capture)["status"] != "PASS"):
        return {"status": "NOT_RUN"}
    return {"status": "PASS", "sampled_hook_calls": hook.get("total"),
            "hook_ms_per_frame": total / frames, "share_of_frame_time_percent": 100 * total / frame_ms,
            "scope": "measured inclusive hook CPU time; not GPU cost"}


def anchor(report):
    engine = report["timings"].get("engine_frame", {})
    interval = report["timings"].get("ui_interval", {})
    a, b = engine.get("median_ms", 0), interval.get("median_ms", 0)
    valid = (isinstance(a, (int, float)) and isinstance(b, (int, float)) and
             math.isfinite(a) and math.isfinite(b) and a > 0 and b > 0 and
             engine.get("count", 0) > 0 and engine.get("count") == interval.get("count") and
             engine.get("total", 0) > 0 and engine.get("total") == interval.get("total") and
             all(row.get("overwritten") == 0 and row.get("missed", 0) == 0 and not row.get("inconclusive") and
                 isinstance(row.get("p95_ms"), (int, float)) and math.isfinite(row["p95_ms"]) and row["p95_ms"] > 0
                 for row in (engine, interval)))
    ratio = b / a if valid else None
    return {"status": "PASS" if valid and .9 <= ratio <= 1.1 else "NOT_RUN", "ratio": ratio}


def compare(report, baseline, *, matched=False):
    comparisons = {}
    for key, capture in report["captures"].items():
        label = capture["label"]
        matches = [c for c in baseline["captures"].values() if c["label"] == label]
        entry = {"label": label, "status": "NOT_RUN", "matched": matched,
                 "scene": "matched opaque filter versus transparent filter-material" if matched else
                 "unmatched; baseline camera compressed, scene differs" if baseline.get("mode") == "baseline" else
                 "unmatched or unverified pose/settings"}
        if (label != "unlabeled" and len(matches) == 1 and capture["closed"] and matches[0]["closed"] and
                report["conclusion"] != "INCONCLUSIVE" and baseline["conclusion"] != "INCONCLUSIVE" and
                capture["anchor"]["status"] == matches[0]["anchor"]["status"] == "PASS"):
            delta = 100 * (capture["timings"]["engine_frame"]["p95_ms"] / matches[0]["timings"]["engine_frame"]["p95_ms"] - 1)
            entry.update(p95_difference_ms=capture["timings"]["engine_frame"]["p95_ms"] - matches[0]["timings"]["engine_frame"]["p95_ms"], p95_overhead_percent=delta, within_10_percent_numeric_only=delta <= 10,
                         status="PASS" if matched and delta <= 10 else "FAIL" if matched else "NOT_RUN: unmatched scene; descriptive only")
            if matched and any(c["timings"]["engine_frame"].get("elapsed_ms", 0) < 60000 for c in (capture, matches[0])):
                entry["status"] = "NOT_RUN: capture shorter than 60 seconds"
        comparisons[key] = entry
    return comparisons


def read(path):
    rows = [json.loads(line) for line in Path(path).read_text(encoding="utf-8").splitlines() if line.strip()]
    starts = [i for i, row in enumerate(rows) if row.get("event") == "session"]
    return rows[starts[-1]:] if starts else rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment")
    parser.add_argument("--baseline")
    parser.add_argument("--matched", action="store_true", help="reviewed same pose/settings/uncapped filter versus filter-material pair")
    parser.add_argument("--expect-no-discards", action="store_true", help="explicit no-match negative control")
    args = parser.parse_args()
    report = inspect(read(args.experiment), expect_no_discards=args.expect_no_discards)
    if args.baseline:
        report["comparisons"] = compare(report, inspect(read(args.baseline)), matched=args.matched)
    print(json.dumps(report, indent=2))
    return 1 if report["journal"] == "FAIL" else 2 if report["conclusion"] == "INCONCLUSIVE" else 0


if __name__ == "__main__":
    raise SystemExit(main())
