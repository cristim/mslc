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
        self.assertEqual(compare.ulp_bound_for_source("o[i] = a * b + fma(c, d, e) / d;"), 0)

    def test_correctly_rounded_functions_have_no_bound(self):
        self.assertNotIn("fma", compare.ULP_BOUNDS)

    def test_sqrt_gets_vulkan_inversesqrt_bound(self):
        self.assertEqual(compare.ULP_BOUNDS["sqrt"], 2)
        self.assertEqual(compare.ulp_bound_for_source("o[i] = sqrt(v[i]);"), 2)

    def test_rsqrt_gets_vulkan_inversesqrt_bound(self):
        self.assertEqual(compare.ULP_BOUNDS["rsqrt"], 2)
        self.assertEqual(compare.ulp_bound_for_source("o[i] = rsqrt(v[i]);"), 2)

    def test_bounds_follow_msl_table_8_1(self):
        self.assertEqual([compare.ULP_BOUNDS[n] for n in ("tan", "atan", "atan2", "tanh", "log10")], [6, 5, 6, 5, 4])

    def test_bounds_not_pinned_elsewhere_follow_table_8_1(self):
        names = ("sinpi", "cospi", "tanpi", "sincos", "atanh", "asin", "acos")
        self.assertEqual([compare.ULP_BOUNDS[n] for n in names], [4, 4, 6, 4, 5, 4, 4])

    def test_a_name_that_merely_contains_a_function_does_not_count(self):
        self.assertEqual(compare.ulp_bound_for_source("float mysin(float x); y = mysin(x);"), 0)

    def test_half_divide_gets_the_half_divide_bound(self):
        self.assertEqual(compare.HALF_DIVIDE_ULP, 2)
        self.assertEqual(compare.ulp_bound_for_source("o[i] = v[i] / 3.0h;"), 2)
        self.assertEqual(compare.ulp_bound_for_source("half4 a = x; a /= y;"), 2)

    def test_float_divide_and_half_without_divide_stay_exact(self):
        self.assertEqual(compare.ulp_bound_for_source("o[i] = v[i] / 3.0f;"), 0)
        self.assertEqual(compare.ulp_bound_for_source("half x = v[i] * 3.0h; // a / b"), 0)

    def test_half_divide_bound_covers_apple_and_lavapipe_1870_667(self):
        apple, lvp = struct.pack("<H", 0x334e), struct.pack("<H", 0x3350)
        self.assertFalse(compare.leaf_equal(apple, lvp, "half", 0))
        self.assertTrue(compare.leaf_equal(apple, lvp, "half", compare.HALF_DIVIDE_ULP))

    def test_half_divide_bound_rejects_three_ulp_around_the_correct_result(self):
        good = 0x334f
        b = lambda v: struct.pack("<H", v)
        for off in (-3, 3):
            self.assertFalse(compare.leaf_equal(b(good), b(good + off), "half", compare.HALF_DIVIDE_ULP))
        for off in (-2, 2):
            self.assertTrue(compare.leaf_equal(b(good), b(good + off), "half", compare.HALF_DIVIDE_ULP))

    def test_half_literals_without_leading_digit_or_with_exponent(self):
        self.assertEqual(compare.ulp_bound_for_source("o[i] = v[i] / .5h;"), 2)
        self.assertEqual(compare.ulp_bound_for_source("o[i] = v[i] / 1e3h;"), 2)


class UlpBoundsByRoot(unittest.TestCase):
    ENTRY = "kernel void k(device float *a, device float *b, uint i [[thread_position_in_grid]]) { %s }"

    def by(self, body, before=""):
        return compare.ulp_bounds_by_root(before + self.ENTRY % body)

    def test_sqrt_bound_does_not_leak_onto_an_exactly_rounded_buffer(self):
        src = self.ENTRY % "a[i] = sqrt(b[i]); b[i] = 1.0f;"
        self.assertEqual(compare.ulp_bounds_by_root(src), {"a": 2, "b": 0})
        self.assertEqual(compare.ulp_bound_for_source(src), 2)

    def test_each_buffer_gets_its_own_loosest_bound(self):
        src = "kernel void k(device float *a, device float *b, device float *c, device float *v) { a[0] = rsqrt(v[0]); b[0] = pow(v[0], v[1]); c[0] = sin(v[0]) + a[0]; }"
        self.assertEqual(compare.ulp_bounds_by_root(src), {"a": 2, "b": 16, "c": 4})

    def test_a_local_carries_its_bound_to_the_buffer_it_feeds(self):
        by = self.by("float t = pow(b[0], b[1]); float u = t + 1.0f; a[i] = u;")
        self.assertEqual(by["a"], 16)

    def test_a_subscripted_read_of_a_written_buffer_carries_its_bound(self):
        self.assertEqual(self.by("b[0] = sqrt(2.0f); a[0] = b[0] * 2.0f;")["a"], 2)

    def test_compound_assignment_and_vector_locals(self):
        by = self.by("float2 t = float2(1.0f, 2.0f); t += sqrt(b[i]); a[i] += t[0];")
        self.assertEqual(by["a"], 2)

    def test_a_comma_in_a_call_still_splits(self):
        by = self.by("a[0] = sqrt(2.0f); b[0] = max(3.0f, 4.0f);")
        self.assertEqual((by["a"], by["b"]), (2, 0))

    def test_not_an_entry_point_gives_no_split(self):
        self.assertEqual(compare.ulp_bounds_by_root("a[i] = sqrt(x);"), {})

    def test_condition_or_loop_gives_no_split(self):
        self.assertEqual(self.by("if (sqrt(b[0]) > 1.0f) a[0] = 1.0f;"), {})
        self.assertEqual(self.by("for (int j = 0; j < 4; j++) a[j] = sqrt(b[j]);"), {})
        self.assertEqual(self.by("if (i < 4) { a[i] = sqrt(b[i]); }"), {})

    def test_helper_function_gives_no_split(self):
        self.assertEqual(self.by("a[0] = h(1.0f);", "float h(float x) { float r = sqrt(x); return r; }\n"), {})
        self.assertEqual(self.by("float t; f(1.0f, t); a[0] = t;", "void f(float x, thread float &o) { o = sqrt(x); }\n"), {})
        self.assertEqual(self.by("float t = sqrt(2.0f); st(a, t);", "void st(device float *p, float v) { p[0] = v; }\n"), {})

    def test_aliases_give_no_split(self):
        for body in ("device float *p = a; p[1] = sqrt(2.0f);",
                     "device float &r = a[0]; r = sqrt(2.0f);",
                     "auto p = a; p[1] = sqrt(2.0f);",
                     "auto p(a); p[1] = sqrt(2.0f);",
                     "auto p{a}; p[1] = sqrt(2.0f);",
                     "auto &p = a[0]; p = sqrt(2.0f);",
                     "decltype(a) p = a; p[1] = sqrt(2.0f);",
                     "device float *p[2] = {a, b}; p[0][1] = sqrt(2.0f);",
                     "float t = *(device float *)a; a[0] = sqrt(t);",
                     "device float *p = (device float *)a; p[1] = sqrt(2.0f);",
                     "device float *p = reinterpret_cast<device float *>(a); p[1] = sqrt(2.0f);",
                     "S s = {a}; s.p[1] = sqrt(2.0f);",
                     "ref<float> p = a; p[1] = sqrt(2.0f);"):
            self.assertEqual(self.by(body), {}, body)

    def test_using_and_typedef_give_no_split(self):
        for decl in ("using P = device float *;", "typedef device float *P;"):
            self.assertEqual(self.by("P p = a; p[1] = sqrt(2.0f);", decl + "\n"), {}, decl)

    def test_struct_member_and_cast_give_no_split(self):
        self.assertEqual(self.by("float2 v; v.x = sqrt(b[0]); a[0] = v.x;"), {})
        self.assertEqual(self.by("a[0] = (float)sqrt(b[0]);"), {})
        self.assertEqual(self.by("a[0] = static_cast<float>(sqrt(b[0]));"), {})

    def test_comma_declaration_gives_no_split(self):
        self.assertEqual(self.by("float t = 1.0f, u = sqrt(2.0f); a[0] = u;"), {})

    def test_bare_buffer_name_in_an_expression_gives_no_split(self):
        self.assertEqual(self.by("float t = sqrt(2.0f); a[0] = t; b[0] = foo(a);"), {})


if __name__ == "__main__":
    unittest.main()
