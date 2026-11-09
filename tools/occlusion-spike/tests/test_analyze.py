"""Synthetic journal checks do not establish runtime behavior."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("spike_analyze", Path(__file__).parents[1] / "analyze.py")
analyze = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyze)


class JournalTests(unittest.TestCase):
    def test_fade_needs_original_and_restore(self):
        for tail in ([{"event": "apply"}], [{"event": "original"}, {"event": "apply"}],
                     [{"event": "original"}, {"event": "apply"}, {"event": "restore", "result": "FAIL"}]):
            self.assertEqual(analyze.inspect([{"event": "session"}, *tail])["journal"], "FAIL")
        self.assertEqual(analyze.inspect([{"event": "session"}, {"event": "original"},
                                         {"event": "apply"}, {"event": "restore", "result": "PASS"}])["journal"], "PASS")

    def test_classifier_is_not_filter_authority(self):
        bad = {"event": "discard", "mode": "filter", "smoothcam_caller": True, "overhead_candidate": True}
        self.assertEqual(analyze.inspect([{"event": "session"}, bad])["journal"], "FAIL")
        good = {**bad, "allowlist_match": True, "calling_thread_id": 42, "main_thread_id": 42}
        self.assertEqual(analyze.inspect([{"event": "session"}, good])["journal"], "PASS")

    def test_anchor_requires_matching_counts_and_cadence(self):
        frame = {"count": 100, "total": 100, "median_ms": 16, "p95_ms": 17, "overwritten": 0}
        report = {"timings": {"engine_frame": frame, "ui_interval": dict(frame)}}
        self.assertEqual(analyze.anchor(report)["status"], "PASS")
        for change in ({"count": 99}, {"median_ms": 20}, {"overwritten": 1}, {"p95_ms": float("inf")}, {"total": 0}):
            report["timings"]["ui_interval"] = {**frame, **change}
            self.assertEqual(analyze.anchor(report)["status"], "NOT_RUN")

    def test_refusal_is_not_a_runtime_pass(self):
        report = analyze.inspect([{"event": "session"}, {"event": "refused", "reason": "abi"}])
        self.assertEqual(report["journal"], "FAIL")
        self.assertEqual(report["p5_outcome"], "pending")
        self.assertEqual(report["visual_behavior"], "NOT_RUN")

    def test_zero_filter_hits_is_inconclusive(self):
        report = analyze.inspect([{"event": "session", "mode": "filter"}])
        self.assertEqual(report["conclusion"], "INCONCLUSIVE")
        self.assertEqual(report["smoothcam_stability"], "NOT_RUN")

    def test_zero_matches_in_one_capture_invalidates_filter_run(self):
        rows = [{"event": "session", "mode": "filter"},
                {"event": "timing", "capture_id": 1, "metric": "engine_frame", "reason": "stop",
                 "matched_hits": 0, "discards": 0}]
        report = analyze.inspect(rows)
        self.assertFalse(report["captures"]["1"]["filter_exercised"])
        self.assertEqual(report["conclusion"], "INCONCLUSIVE")

    def test_precapture_periodic_rows_do_not_create_a_capture(self):
        report = analyze.inspect([{"event": "session"}, {"event": "timing", "capture_id": 0,
                                  "metric": "engine_frame", "reason": "periodic"}])
        self.assertEqual(report["captures"], {})

    def test_caps_and_lost_evidence_are_inconclusive(self):
        for row in ({"event": "inconclusive", "reason": "hit_queue_or_observe_record_cap"},
                    {"event": "timing", "metric": "engine_frame", "overwritten": 1},
                    {"event": "timing", "metric": "engine_frame", "missed": 1},
                    {"event": "recast_step", "sequence": 123}):
            self.assertEqual(analyze.inspect([{"event": "session"}, row])["conclusion"], "INCONCLUSIVE")

    def test_standing_and_turning_are_separate(self):
        rows = [{"event": "session"}]
        for capture, label, p95 in ((1, "standing", 17), (2, "turning", 20)):
            for metric in ("engine_frame", "ui_interval"):
                rows.append({"event": "timing", "capture_id": capture, "capture_label": label, "metric": metric,
                             "count": 100, "total": 100, "median_ms": 16, "p95_ms": p95, "overwritten": 0,
                             "reason": "main_thread_request"})
        report = analyze.inspect(rows)
        self.assertEqual(report["captures"]["1"]["label"], "standing")
        self.assertEqual(report["captures"]["2"]["timings"]["engine_frame"]["p95_ms"], 20)
        comparison = analyze.compare(report, report)
        self.assertEqual(len(comparison), 2)
        self.assertEqual(comparison["1"]["p95_overhead_percent"], 0)
        self.assertEqual(comparison["2"]["p95_overhead_percent"], 0)

    def test_recast_kept_hit_requires_real_result_and_context_reset(self):
        rows = [{"event": "session", "mode": "filter"},
                {"event": "recast_step", "sequence": 1},
                {"event": "recast_result", "sequence": 1, "hit": True, "allowlist_match": False, "contact_metadata_available": True, "reference": "0x12345678"},
                {"event": "recast_context_reset", "sequence": 1},
                {"event": "discard", "sequence": 1, "mode": "filter", "smoothcam_caller": True,
                 "allowlist_match": True, "calling_thread_id": 42, "main_thread_id": 42},
                {"event": "filter_complete", "sequence": 1, "returned_hit": True, "kept_behind": True, "steps": 1, "returned_reference": "0x12345678"}]
        report = analyze.inspect(rows)
        self.assertEqual(report["journal"], "PASS")
        self.assertEqual(report["kept_hits_behind_beam"], 1)
        rows[2]["hit"] = False
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
        self.assertEqual(analyze.inspect(rows[:2])["conclusion"], "INCONCLUSIVE")

    def test_filter_rollback_is_never_complete_evidence(self):
        rows = [{"event": "session"}, {"event": "recast_step", "sequence": 1},
                {"event": "filter_rollback", "sequence": 1}]
        self.assertEqual(analyze.inspect(rows)["conclusion"], "INCONCLUSIVE")

    def test_capture_without_stop_is_inconclusive(self):
        self.assertEqual(analyze.inspect([{"event": "session"}, {"event": "capture", "capture_id": 1,
                                          "label": "standing"}])["conclusion"], "INCONCLUSIVE")

    def guarded_rows(self, mode="filter"):
        return [{"event": "session", "mode": mode, "filter_policy": "hull_step_guard_v1"},
                {"event": "original"}, {"event": "apply", "reference": "0x000E7A7B"},
                {"event": "recast_step", "sequence": 1},
                {"event": "recast_gap", "sequence": 1, "safe": True, "complete": True, "original_contact_seen": True},
                {"event": "recast_result", "sequence": 1, "hit": False},
                {"event": "recast_context_reset", "sequence": 1},
                {"event": "discard", "sequence": 1, "mode": mode, "reference": "0x000E7A7B",
                 "allowlist_match": True, "smoothcam_caller": True, "calling_thread_id": 1, "main_thread_id": 1},
                {"event": "filter_complete", "sequence": 1, "returned_hit": False, "steps": 1, "gateway_calls": 3},
                {"event": "restore", "result": "PASS"}]

    def test_combined_requires_same_reference_and_active_fade(self):
        for mode in analyze.COMBINED_MODES:
            rows = self.guarded_rows(mode)
            self.assertEqual(analyze.inspect(rows)["journal"], "PASS")
            rows[7]["reference"] = "0x12345678"
            self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
            rows = self.guarded_rows(mode)
            del rows[2]
            self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
            self.assertEqual(analyze.inspect([{"event": "session", "mode": mode}])["conclusion"], "INCONCLUSIVE")

    def test_guarded_filter_cannot_commit_unsafe_or_missing_gap(self):
        for change in ({"safe": False}, {"complete": False}, {"original_contact_seen": False}):
            rows = self.guarded_rows(); rows[4].update(change)
            self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
        rows = self.guarded_rows(); del rows[4]
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
        rows = self.guarded_rows(); rows[8]["gateway_calls"] = 11
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
        rows = self.guarded_rows(); del rows[5]
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")

    def test_local_material_claim_needs_distinct_allocation(self):
        rows = [{"event": "session"}, {"event": "material_write_scope", "local_write": True,
                "original_material": 123, "installed_material": 456}]
        self.assertEqual(analyze.inspect(rows)["journal"], "PASS")
        rows[1]["installed_material"] = 123
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")
        rows[1]["local_write"] = False
        self.assertEqual(analyze.inspect(rows)["journal"], "FAIL")

    def test_culled_zero_counts_are_recorded_without_visual_claim(self):
        rows = [{"event": "session", "mode": "app_culled"},
                {"event": "caster_counts", "reference": "0x000E7A7B", "hits": 0, "calls": 50, "fade_applied": True}]
        report = analyze.inspect(rows)
        self.assertEqual(report["caster_count_windows"][0]["hits"], 0)
        self.assertEqual(report["visual_behavior"], "NOT_RUN")
        for key in ("overflow", "off_thread", "unresolved"):
            rows[1][key] = 1
            self.assertEqual(analyze.inspect(rows)["conclusion"], "INCONCLUSIVE")


class FinalEvidenceTests(unittest.TestCase):
    def height_rows(self, values):
        return [{"t_ms": i*100, "camera": {"z": h+50}, "player": {"z": 50},
                 "filter_committed": i % 2 == 0} for i, h in enumerate(values)]

    def test_height_reversals_threshold_and_steady(self):
        r = analyze.height_metrics(self.height_rows([10, 10.2, 10, 10.3, 10]))
        self.assertEqual(r["direction_reversals"], 0)
        self.assertEqual(r["longest_steady_ms"], 400)
        r = analyze.height_metrics(self.height_rows([10, 12, 14, 12, 10, 12, 14]))
        self.assertEqual(r["direction_reversals"], 2)
        self.assertEqual(r["range_units"], 4)
        self.assertEqual(r["filter_committed_samples"], 4)
        r = analyze.height_metrics(self.height_rows([1, 2, 3, 4]))
        self.assertEqual(r["direction_reversals"], 0)

    def test_height_missing_invalid_and_gaps(self):
        self.assertEqual(analyze.height_metrics([])["status"], "NOT_RUN")
        rows = self.height_rows([1, 1, 1]); rows[2]["t_ms"] = 1000
        r = analyze.height_metrics(rows)
        self.assertEqual(r["gaps"], 1)
        self.assertEqual(r["longest_steady_ms"], 100)
        rows[2]["camera"]["z"] = float("nan")
        self.assertEqual(analyze.height_metrics(rows)["status"], "FAIL")

    def quiet_rows(self, discards=10):
        rows = [{"event": "session", "mode": "filter", "quiet_journal": True}]
        for metric in ("engine_frame", "ui_interval", "hook_inclusive"):
            rows.append({"event": "timing", "capture_id": 1, "capture_label": "standing",
                         "metric": metric, "reason": "stop", "discards": discards, "matched_hits": discards,
                         "completed_chains": discards, "rollbacks": 0, "kept_behind": discards,
                         "count": 100, "total": 100, "overwritten": 0, "median_ms": 10, "p95_ms": 12,
                         "elapsed_ms": 60010, "sum_ms": 1000 if metric != "hook_inclusive" else 20})
        return rows

    def test_quiet_summary_counts_and_no_double_count(self):
        rows = self.quiet_rows()
        rows.insert(1, {**rows[1], "reason": "periodic", "discards": 5})
        r = analyze.inspect(rows)
        self.assertEqual(r["discards"], 10)
        self.assertEqual(r["discard_records"], 0)
        self.assertEqual(r["journal_detail"], "summary_counters_only")
        cost = r["captures"]["1"]["hook_cost"]
        self.assertEqual(cost["hook_ms_per_frame"], .2)
        self.assertEqual(cost["share_of_frame_time_percent"], 2)
        rows[2].pop("completed_chains")
        self.assertEqual(analyze.inspect(rows)["conclusion"], "INCONCLUSIVE")

    def test_negative_control_zero_is_expected_and_nonzero_fails(self):
        r = analyze.inspect(self.quiet_rows(0), expect_no_discards=True)
        self.assertEqual(r["negative_control"], "PASS")
        self.assertEqual(analyze.inspect(self.quiet_rows(), expect_no_discards=True)["negative_control"], "FAIL")
        self.assertEqual(analyze.inspect(self.quiet_rows(0))["conclusion"], "INCONCLUSIVE")
        rows = self.quiet_rows(); rows[0]["quiet_journal"] = False
        self.assertEqual(analyze.inspect(rows, expect_no_discards=True)["negative_control"], "FAIL")

    def test_classifier_distinct_models_and_mixed_verdict(self):
        rows = self.quiet_rows()
        for candidates in (0, 3, 2):
            rows.append({"event": "caster_counts", "reference": "0x00000042", "hits": 5,
                         "candidate_hits": candidates, "smoothcam_candidate_hits": candidates,
                         "model": "ceiling.nif"})
        r = analyze.inspect(rows)
        self.assertEqual(r["classifier_distinct_would_discard"], 1)
        self.assertEqual(r["classifier_by_reference"]["0x00000042"]["smoothcam_candidate_hits"], 5)
        self.assertEqual(r["classifier_by_reference"]["0x00000042"]["models"], ["ceiling.nif"])

    def test_miss_only_classifier_window_reports_numeric_zero(self):
        rows = self.quiet_rows(0) + [{"event": "caster_counts", "reference": "0x00000000",
                                    "hits": 0, "candidate_hits": 0, "calls": 50, "misses": 50}]
        r = analyze.inspect(rows, expect_no_discards=True)
        self.assertEqual(r["classifier_distinct_would_discard"], 0)
        self.assertEqual(r["classifier_by_reference"], {})

    def test_matched_fading_delta_and_unmatched_baseline(self):
        base = analyze.inspect(self.quiet_rows())
        rows = self.quiet_rows(); rows[1]["p95_ms"] = 13.5
        experiment = analyze.inspect(rows)
        pair = analyze.compare(experiment, base, matched=True)["1"]
        self.assertEqual(pair["status"], "FAIL")
        self.assertEqual(pair["p95_difference_ms"], 1.5)
        self.assertEqual(pair["p95_overhead_percent"], 12.5)
        self.assertFalse(analyze.compare(experiment, base)["1"]["matched"])
        rows[1]["elapsed_ms"] = 59000
        self.assertTrue(analyze.compare(analyze.inspect(rows), base, matched=True)["1"]["status"].startswith("NOT_RUN"))


if __name__ == "__main__":
    unittest.main()
