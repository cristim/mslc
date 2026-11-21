#!/usr/bin/env python3
"""Unit tests for tools/diffcheck/checks.py. No GPU needed."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import checks  # noqa: E402
import compare  # noqa: E402

LEAF = [{"off": 0, "scalar": "float"}]
LAYOUT = {"name": "o", "access": "readwrite", "stride": 4, "leaves": LEAF}


class Perturb(unittest.TestCase):
    def test_every_kind_of_float_becomes_different(self):
        # A NaN and an infinity next to it: flipping a payload bit of a NaN would leave it a NaN,
        # which compares equal to the original, so the control would pass vacuously.
        for pattern in ("0000c07f", "00000000", "0000807f", "0000c0ff"):
            raw = bytes.fromhex(pattern) * 4
            copy, name, offset = checks.perturb({0: LAYOUT}, {0: bytearray(raw)})
            self.assertEqual((name, offset), ("o", 0))
            self.assertTrue(compare.compare({"name": "o", "stride": 4, "leaves": LEAF}, bytearray(raw), copy[0], 0), pattern)

    def test_the_perturbed_value_is_not_apples_either(self):
        # mslc has +inf where Apple has a NaN: flipping inf's low bit would make a NaN and
        # the control would pass by accident.
        inf, nan = bytes.fromhex("0000807f") * 4, bytes.fromhex("0000c07f") * 4
        copy, _, _ = checks.perturb({0: LAYOUT}, {0: bytearray(inf)}, {0: bytearray(nan)})
        self.assertTrue(compare.compare({"name": "o", "stride": 4, "leaves": LEAF}, bytearray(nan), copy[0], 0))
        one = bytes.fromhex("0000803f") * 4
        copy, _, _ = checks.perturb({0: LAYOUT}, {0: bytearray(nan)}, {0: bytearray(one)})
        self.assertTrue(compare.compare({"name": "o", "stride": 4, "leaves": LEAF}, bytearray(one), copy[0], 0))

    def test_integer_and_bool_leaves(self):
        for scalar in ("uint", "bool", "half"):
            layout = {"name": "o", "access": "write", "stride": 4, "leaves": [{"off": 0, "scalar": scalar}]}
            copy, _, _ = checks.perturb({0: layout}, {0: bytearray(8)})
            self.assertTrue(compare.compare(dict(layout), bytearray(8), copy[0], 0), scalar)

    def test_read_only_buffers_are_not_perturbed(self):
        self.assertIsNone(checks.perturb({0: dict(LAYOUT, access="read")}, {0: bytearray(16)}))


class WrittenNonzero(unittest.TestCase):
    def test_untouched_input_is_not_output(self):
        before = {0: bytearray(b"\x01\x00\x00\x00" * 4)}
        self.assertFalse(checks.written_nonzero({0: LAYOUT}, before, before))

    def test_zeroing_everything_is_not_output(self):
        self.assertFalse(checks.written_nonzero({0: LAYOUT}, {0: bytearray(b"\x01" * 16)}, {0: bytearray(16)}))

    def test_a_changed_nonzero_leaf_is_output(self):
        after = bytearray(b"\x01\x00\x00\x00" * 4)
        after[8] = 9
        self.assertTrue(checks.written_nonzero({0: LAYOUT}, {0: bytearray(b"\x01\x00\x00\x00" * 4)}, {0: after}))

    def test_read_only_buffers_do_not_count(self):
        self.assertFalse(checks.written_nonzero({0: dict(LAYOUT, access="read")}, {0: bytearray(16)}, {0: bytearray(b"\x05" * 16)}))


class CrossCheck(unittest.TestCase):
    APPLE = {"buffers": [{"index": 0, "name": "a"}, {"index": 2, "name": "o"}]}

    def test_agreement(self):
        self.assertEqual(checks.cross_check([{"metal_index": 0, "name": "a"}, {"metal_index": 2, "name": "o"}], self.APPLE), [])

    def test_name_index_and_extra_buffer_disagreements(self):
        found = checks.cross_check([{"metal_index": 0, "name": "x"}, {"metal_index": 1, "name": "b"}], self.APPLE)
        self.assertEqual(len(found), 3)
        self.assertIn("buffer(0) is x in mslc and a in Apple", found)
        self.assertTrue(any("has no buffer at that index" in f for f in found))
        self.assertTrue(any("mslc does not bind it" in f for f in found))


class Verdict(unittest.TestCase):
    OK = {"negative_control": True, "diff_count": 0, "diffs": []}

    def test_a_control_that_did_not_fire_is_never_a_match(self):
        self.assertEqual(checks.verdict([dict(self.OK, negative_control=False)], True, False)[0], "control-failed")

    def test_mismatch_zero_output_and_match(self):
        bad = dict(self.OK, diff_count=1, diffs=[{"buffer": "o"}])
        self.assertEqual(checks.verdict([self.OK, bad], True, False)[0], "mismatched")
        self.assertEqual(checks.verdict([self.OK], False, False)[0], "zero-output")
        self.assertEqual(checks.verdict([self.OK], False, True)[0], "matched")
        self.assertEqual(checks.verdict([self.OK], True, False)[0], "matched")


if __name__ == "__main__":
    unittest.main()
