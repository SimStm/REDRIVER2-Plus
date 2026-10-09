"""Regression checks for the canonical-state comparator (no game assets)."""
import tempfile
import unittest
from pathlib import Path

import e00_compare_state as comparison


class CompareStateTests(unittest.TestCase):
    def test_numbered_fields_keep_their_identity(self):
        self.assertEqual(comparison.parse_fields("d0 7 d1 8 t0count 9 t1count 10"),
                         {"d0": 7, "d1": 8, "t0count": 9, "t1count": 10})

    def test_numbered_damage_and_timers_are_compared(self):
        for key in ("d0", "d5", "t0count", "t1frac", "msg0", "msg1"):
            with self.subTest(key=key):
                self.assertEqual(comparison.compare_record({"car": {key: 3}}, {"car": {key: 4}}),
                                 [("car", key, 3, 4)])

    def test_strict_comparison_detects_missing_car_in_both_directions(self):
        present = {"car:0": {"x": 1}}
        for a, b in ((present, {}), ({}, present)):
            self.assertTrue(comparison.compare_record(a, b, strict=True))

    def test_strict_comparison_detects_missing_field(self):
        self.assertTrue(comparison.compare_record({"car:0": {"d0": 3}}, {"car:0": {}}, strict=True))

    def test_legacy_format_comparison_allows_new_fields(self):
        self.assertFalse(comparison.compare_record({"car:0": {"x": 3}}, {"car:0": {"x": 3, "d0": 4}}))

    def test_display_counter_is_ignored(self):
        self.assertFalse(comparison.compare_record({"tick": {"frame": 1}}, {"tick": {"frame": 100}}, strict=True))

    def test_pool_digest_and_siren_phase_are_compared(self):
        for entity, key in (("effectsstate", "pools"), ("carphase:0", "siren")):
            self.assertTrue(comparison.compare_record({entity: {key: 1}}, {entity: {key: 2}}))

    def test_presentation_mutation_rejects_the_dump(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.txt"
            path.write_text("tick 1 frame 10\npresentation-mutation tick 1 frame 11\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                comparison.parse(path)


if __name__ == "__main__":
    unittest.main()
