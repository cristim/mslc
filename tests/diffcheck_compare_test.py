#!/usr/bin/env python3
"""Unit tests for tools/diffcheck/compare.py. No GPU needed."""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import compare  # noqa: E402


def f32(x):
    return struct.pack("<f", x)


def bits(v):
    return struct.pack("<I", v)


NAN = bits(0x7fc00000)
OTHER_NAN = bits(0x7fc12345)


class LeafEqual(unittest.TestCase):
    def test_exact_float_differs_by_one_ulp(self):
        self.assertFalse(compare.leaf_equal(bits(0x3f800000), bits(0x3f800001), "float"))

    def test_one_ulp_passes_a_bound_of_one(self):
        self.assertTrue(compare.leaf_equal(bits(0x3f800000), bits(0x3f800001), "float", ulp=1))
        self.assertFalse(compare.leaf_equal(bits(0x3f800000), bits(0x3f800003), "float", ulp=2))

    def test_nan_never_passes_a_tolerance(self):
        self.assertFalse(compare.leaf_equal(NAN, f32(1.0), "float", ulp=1 << 30))
        self.assertFalse(compare.leaf_equal(f32(1.0), NAN, "float", ulp=1 << 30))
        self.assertFalse(compare.leaf_equal(NAN, bits(0x7f800000), "float", ulp=1 << 30))

    def test_two_nans_match_whatever_the_payload(self):
        self.assertTrue(compare.leaf_equal(NAN, OTHER_NAN, "float"))

    def test_infinities_must_be_the_same(self):
        self.assertTrue(compare.leaf_equal(bits(0x7f800000), bits(0x7f800000), "float"))
        self.assertFalse(compare.leaf_equal(bits(0x7f800000), bits(0xff800000), "float", ulp=1 << 30))

    def test_infinity_is_not_within_a_bound_of_the_largest_finite_value(self):
        self.assertFalse(compare.leaf_equal(bits(0x7f800000), bits(0x7f7fffff), "float", ulp=4))
        self.assertFalse(compare.leaf_equal(bytes([0xff, 0x7b]), bytes([0x00, 0x7c]), "half", ulp=4))

    def test_signed_zero_is_exact_only_without_a_bound(self):
        self.assertFalse(compare.leaf_equal(f32(0.0), f32(-0.0), "float"))
        self.assertTrue(compare.leaf_equal(f32(0.0), f32(-0.0), "float", ulp=1))

    def test_half(self):
        one, next_up = struct.pack("<e", 1.0), bytes([0x01, 0x3c])
        self.assertFalse(compare.leaf_equal(one, next_up, "half"))
        self.assertTrue(compare.leaf_equal(one, next_up, "half", ulp=1))
        self.assertFalse(compare.leaf_equal(bytes([0x00, 0x7e]), one, "half", ulp=99))

    def test_integers_and_bool_are_bit_exact_even_with_a_bound(self):
        self.assertFalse(compare.leaf_equal(bits(1), bits(2), "uint", ulp=100))
        self.assertFalse(compare.leaf_equal(b"\x00", b"\x01", "bool", ulp=100))
        self.assertTrue(compare.leaf_equal(bits(0x80000000), bits(0x80000000), "int"))


class Compare(unittest.TestCase):
    LAYOUT = {"name": "out", "stride": 8,
              "leaves": [{"off": 0, "scalar": "float"}, {"off": 4, "scalar": "uint"}]}

    def test_padding_is_not_compared(self):
        layout = {"name": "p", "stride": 8, "leaves": [{"off": 0, "scalar": "uint"}]}
        a = bits(7) + b"\x01\x02\x03\x04"
        b = bits(7) + b"\x09\x09\x09\x09"
        self.assertEqual(compare.compare(layout, a, b), [])

    def test_reports_element_offset_and_both_values(self):
        a = f32(1.0) + bits(5) + f32(2.0) + bits(6)
        b = f32(1.0) + bits(5) + f32(2.0) + bits(9)
        (diff,) = compare.compare(self.LAYOUT, a, b)
        self.assertEqual((diff.element, diff.offset, diff.scalar), (1, 12, "uint"))
        self.assertEqual(diff.to_json()["apple"], 6)
        self.assertEqual(diff.to_json()["mslc"], 9)

    def test_signed_leaves_are_reported_signed(self):
        layout = {"name": "s", "stride": 4, "leaves": [{"off": 0, "scalar": "int"}]}
        (diff,) = compare.compare(layout, bits(0xfffffffe), bits(1))
        self.assertEqual(diff.to_json()["apple"], -2)

    def test_length_mismatch_is_an_error(self):
        with self.assertRaises(ValueError):
            compare.compare(self.LAYOUT, bytes(8), bytes(16))

    def test_limit(self):
        a, b = bytes(8 * 40), bytes([1]) * (8 * 40)
        self.assertEqual(len(compare.compare(self.LAYOUT, a, b, limit=5)), 5)


class UlpBound(unittest.TestCase):
    def test_source_calling_pow_gets_the_pow_bound(self):
        self.assertEqual(compare.ulp_bound_for_source("o[i] = pow(a, b);"), compare.ULP_BOUNDS["pow"])

    def test_namespaced_call_counts(self):
        self.assertEqual(compare.ulp_bound_for_source("x = metal::precise::sin(y);"), compare.ULP_BOUNDS["sin"])

    def test_a_comment_or_string_is_not_a_call(self):
        self.assertEqual(compare.ulp_bound_for_source('// sin( here\nconst char* s = "pow(";\n/* exp( */ x = a + b;'), 0)

    def test_exact_only_source_has_no_bound(self):
        self.assertEqual(compare.ulp_bound_for_source("o[i] = a * b + sqrt(c) / d;"), 0)

    def test_correctly_rounded_functions_have_no_bound(self):
        for name in ("sqrt", "rsqrt", "fma"):
            self.assertNotIn(name, compare.ULP_BOUNDS)

    def test_bounds_follow_msl_table_8_1(self):
        self.assertEqual([compare.ULP_BOUNDS[n] for n in ("tan", "atan", "atan2", "tanh", "log10")], [6, 5, 6, 5, 4])

    def test_bounds_not_pinned_elsewhere_follow_table_8_1(self):
        names = ("sinpi", "cospi", "tanpi", "sincos", "atanh", "asin", "acos")
        self.assertEqual([compare.ULP_BOUNDS[n] for n in names], [4, 4, 6, 4, 5, 4, 4])

    def test_a_name_that_merely_contains_a_function_does_not_count(self):
        self.assertEqual(compare.ulp_bound_for_source("float mysin(float x); y = mysin(x);"), 0)


if __name__ == "__main__":
    unittest.main()
