#!/usr/bin/env python3
"""Unit tests for tools/diffcheck/inputs.py and directives.py. No GPU needed."""

import os
import random
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import directives  # noqa: E402
import inputs  # noqa: E402

LAYOUT = {"name": "b", "stride": 16, "leaves": [
    {"off": 0, "scalar": "float"}, {"off": 4, "scalar": "uint"},
    {"off": 8, "scalar": "bool"}, {"off": 10, "scalar": "half"}]}


def make(profile, seed=1, **kw):
    return inputs.fill_buffer(LAYOUT, 400, random.Random(seed), profile, **kw)


class Fill(unittest.TestCase):
    def test_same_seed_same_bytes(self):
        self.assertEqual(make("edge"), make("edge"))
        self.assertNotEqual(make("edge", 1), make("edge", 2))

    def test_bool_is_only_zero_or_one(self):
        for profile in inputs.PROFILES:
            data = make(profile)
            self.assertLessEqual({data[i * 16 + 8] for i in range(400)}, {0, 1}, profile)
            self.assertEqual({data[i * 16 + 9] for i in range(400)}, {0})

    def test_edge_profile_reaches_nan_inf_and_int_min(self):
        data = make("edge")
        floats = [struct.unpack_from("<I", data, i * 16)[0] for i in range(400)]
        self.assertIn(0x7fc00000, floats)
        self.assertIn(0x7f800000, floats)
        self.assertIn(0x80000000, [struct.unpack_from("<I", data, i * 16 + 4)[0] for i in range(400)])

    def test_wide_and_moderate_floats_are_finite(self):
        for profile in ("moderate", "wide"):
            data = make(profile)
            for i in range(400):
                f = struct.unpack_from("<f", data, i * 16)[0]
                self.assertEqual(f, f)
                self.assertNotIn(abs(f), (float("inf"),))

    def test_padding_zero_or_poisoned(self):
        self.assertEqual(make("moderate")[12], 0)
        self.assertEqual(make("moderate", poison_padding=True)[12], inputs.POISON)

    def test_sentinel_fills_every_byte_but_a_bool_leaf(self):
        data = make("edge", sentinel=True)
        self.assertEqual(len(data), 400 * 16)
        bool_at = {i * 16 + 8 for i in range(400)}
        self.assertEqual({b for i, b in enumerate(data) if i not in bool_at}, {0x7f})
        self.assertEqual({data[i] for i in bool_at}, {0, 1})

    def test_sentinel_honours_a_range(self):
        data = make("edge", sentinel=True, bounds=(0, 3))
        self.assertLessEqual({struct.unpack_from("<I", data, i * 16 + 4)[0] for i in range(400)}, {0, 1, 2, 3})

    def test_a_short_edge_run_reaches_every_edge_float(self):
        data = inputs.fill_buffer(LAYOUT, 120, random.Random(7), "edge")
        floats = {struct.unpack_from("<I", data, i * 16)[0] for i in range(120)}
        for want in (0x7fc00000, 0x7f800000, 0xff800000, 0x80000000, 0x7f7fffff,
                     int.from_bytes(struct.pack("<f", 1e-38), "little"), int.from_bytes(struct.pack("<f", -1e38), "little")):
            self.assertIn(want, floats, hex(want))

    def test_the_case_number_shifts_the_walk(self):
        a = inputs.fill_buffer(LAYOUT, 3, random.Random(1), "edge", phase=0)
        b = inputs.fill_buffer(LAYOUT, 3, random.Random(1), "edge", phase=5)
        self.assertNotEqual(a, b)


class Bounds(unittest.TestCase):
    def test_integer_leaves_stay_in_range_and_floats_are_untouched(self):
        for profile in inputs.PROFILES:
            data = make(profile, bounds=(0, 3))
            self.assertLessEqual({struct.unpack_from("<I", data, i * 16 + 4)[0] for i in range(400)}, {0, 1, 2, 3}, profile)
        floats = {struct.unpack_from("<I", make("wide", bounds=(0, 3)), i * 16)[0] for i in range(400)}
        self.assertGreater(len(floats), 100)

    def test_float_bounds_give_integer_valued_floats_and_leave_integers_alone(self):
        for profile in inputs.PROFILES:
            data = make(profile, float_bounds=(1, 3))
            self.assertEqual({struct.unpack_from("<f", data, i * 16)[0] for i in range(400)}, {1.0, 2.0, 3.0}, profile)
            self.assertEqual({struct.unpack_from("<e", data, i * 16 + 10)[0] for i in range(400)} <= {1.0, 2.0, 3.0}, True)
            ints = {struct.unpack_from("<I", data, i * 16 + 4)[0] for i in range(400)}
            self.assertGreater(len(ints), 100, profile)


    def test_float_bounds_apply_to_a_sentinel_case_and_a_half_must_fit(self):
        data = make("wide", sentinel=True, float_bounds=(1, 3))
        self.assertEqual({struct.unpack_from("<f", data, i * 16)[0] for i in range(400)}, {1.0, 2.0, 3.0})
        self.assertEqual({data[i * 16 + 4] for i in range(400)}, {0x7f})
        with self.assertRaises(ValueError):
            make("wide", float_bounds=(0, 70000))


class Directives(unittest.TestCase):
    def test_parse(self):
        d = directives.parse("// RUN: grid=64 seed=2\n// DIFFCHECK: writes-zero\n// DIFFCHECK-RANGE: which 0 3\n"
                             "// DIFFCHECK-EXCLUDE: scratch reads uninitialised memory\nkernel void k() {}\n")
        self.assertEqual(d, {"run": {"grid": 64, "seed": 2}, "writes_zero": True,
                             "exclude": {"scratch": "reads uninitialised memory"},
                             "range": {"which": (0, 3)}, "float_range": {}, "ulp": {}, "texel": {}, "unused": {}, "geometry": None, "position": None, "depth": False})

    def test_float_range(self):
        self.assertEqual(directives.parse("// DIFFCHECK-FLOAT-RANGE: in 0 1\n")["float_range"], {"in": (0, 1)})
        with self.assertRaises(ValueError):
            directives.parse("// DIFFCHECK-FLOAT-RANGE: in 3 1\n")

    def test_exclusion_without_a_reason_is_an_error(self):
        with self.assertRaises(ValueError):
            directives.parse("// DIFFCHECK-EXCLUDE: scratch\n")

    def test_empty_range_is_an_error(self):
        with self.assertRaises(ValueError):
            directives.parse("// DIFFCHECK-RANGE: which 5 2\n")

    def test_a_zero_run_parameter_is_an_error(self):
        with self.assertRaises(ValueError):
            directives.parse("// RUN: grid=0\n")

    def test_unknown_run_key_is_an_error(self):
        with self.assertRaises(ValueError):
            directives.parse("// RUN: gird=64\n")


if __name__ == "__main__":
    unittest.main()
