#!/usr/bin/env python3
"""Inspect one appended spike session. This never declares a P5-04 outcome."""
import argparse
import json
import math
from pathlib import Path


def inspect(rows):
    problems, incomplete = [], []
    if not rows or rows[0].get("event") != "session":
        problems.append("missing session header")
    original = applied = False
    discards = resets = kept = 0
    captures, chains = {}, {}
    for row in rows:
        event = row.get("event")
        capture_id = str(row.get("contact_capture_id", row.get("capture_id", 0)))
        capture = captures.setdefault(capture_id, {"label": "unlabeled", "timings": {}, "closed": False})
        if event == "capture":
            capture["label"] = row.get("label", "unlabeled")
        elif event == "original":
            original = True
        elif event == "apply":
            if not original:
                problems.append("apply without preceding original")
            applied = True
        elif event == "restore":
            if row.get("result") != "PASS":
                problems.append("restoration conflict/failure")
            applied = original = False
        elif event == "discard":
            discards += 1
            if not (row.get("mode") == "filter" and row.get("allowlist_match") and row.get("smoothcam_caller") and
                    row.get("calling_thread_id") and row.get("calling_thread_id") == row.get("main_thread_id")):
                problems.append("discard without filter/allowlist/SmoothCam/main-thread authority")
            chain = chains.setdefault(row.get("sequence"), {})
            chain["discard"] = chain.get("discard", 0) + 1
        elif event == "recast_step":
            chains.setdefault(row.get("sequence"), {})["started"] = True
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
    for capture in captures.values():
        capture["anchor"] = anchor(capture)
        if not capture["closed"]:
            incomplete.append("capture without final summary")
        if rows and rows[0].get("mode") == "filter":
            timing = capture["timings"].get("engine_frame", {})
            if not timing.get("matched_hits") or not timing.get("discards"):
                incomplete.append("filter capture had zero matched hits or zero committed discards")
            capture["filter_exercised"] = bool(timing.get("matched_hits") and timing.get("discards"))
    if rows and rows[0].get("mode") == "filter" and not committed_discards:
        incomplete.append("filter run had zero discards; SmoothCam stability is inconclusive")
    return {"journal": "FAIL" if problems else "PASS", "problems": problems,
            "conclusion": "INCONCLUSIVE" if incomplete or problems else "journal_complete_only",
            "inconclusive_reasons": sorted(set(incomplete)), "discard_records": discards, "discards": committed_discards, "resets": resets,
            "kept_hits_behind_beam": kept, "enclosure_preservation": "NOT_RUN: verify returned reference is fixture enclosure" if kept else "INCONCLUSIVE: no kept hit behind beam measured",
            "captures": captures, "smoothcam_stability": "NOT_RUN", "visual_behavior": "NOT_RUN", "p5_outcome": "pending"}


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


def compare(report, baseline):
    comparisons = {}
    for key, capture in report["captures"].items():
        label = capture["label"]
        matches = [c for c in baseline["captures"].values() if c["label"] == label]
        entry = {"label": label, "status": "NOT_RUN"}
        if (label != "unlabeled" and len(matches) == 1 and capture["closed"] and matches[0]["closed"] and
                report["conclusion"] != "INCONCLUSIVE" and baseline["conclusion"] != "INCONCLUSIVE" and
                capture["anchor"]["status"] == matches[0]["anchor"]["status"] == "PASS"):
            delta = 100 * (capture["timings"]["engine_frame"]["p95_ms"] / matches[0]["timings"]["engine_frame"]["p95_ms"] - 1)
            entry.update(p95_overhead_percent=delta, within_10_percent_numeric_only=delta <= 10,
                         status="NOT_RUN: manually verify matched profile, route, duration and captures")
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
    args = parser.parse_args()
    report = inspect(read(args.experiment))
    if args.baseline:
        report["comparisons"] = compare(report, inspect(read(args.baseline)))
    print(json.dumps(report, indent=2))
    return 1 if report["journal"] == "FAIL" else 2 if report["conclusion"] == "INCONCLUSIVE" else 0


if __name__ == "__main__":
    raise SystemExit(main())
