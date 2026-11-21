#!/usr/bin/env python3
"""Unit tests for the graphics-mode helpers of tools/diffcheck (directives, spirvio, graphics,
compare's epsilon). No GPU needed."""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import checks  # noqa: E402
import compare  # noqa: E402
import directives  # noqa: E402
import graphics  # noqa: E402
import outputs  # noqa: E402
import spirvio  # noqa: E402

FLOAT = lambda v: struct.pack("<f", v)  # noqa: E731

DISASM = """
OpEntryPoint Fragment %20 "fs" %1 %2 %3 %4
OpEntryPoint Fragment %21 "other" %5
OpDecorate %1 BuiltIn FragCoord
OpDecorate %2 Location 0
OpDecorate %3 Location 1
OpDecorate %3 Flat
OpDecorate %4 Location 0
OpDecorate %5 Location 3
%10 = OpTypeFloat 32
%11 = OpTypeVector %10 4
%12 = OpTypeInt 32 0
%13 = OpTypeArray %11 %12
%14 = OpTypePointer Input %11
%15 = OpTypePointer Input %12
%16 = OpTypePointer Output %12
%17 = OpTypePointer Output %13
%1 = OpVariable %14 Input
%2 = OpVariable %14 Input
%3 = OpVariable %15 Input
%4 = OpVariable %16 Output
%5 = OpVariable %17 Output
"""


class Directives(unittest.TestCase):
    def test_graphics_directives(self):
        d = directives.parse("// RUN: vertex=vs fragment=fs width=8 instances=2\n// DIFFCHECK-GEOMETRY: 1,2 3,4 5,6,0.25\n"
                             "// DIFFCHECK-POSITION: pos\n// DIFFCHECK: depth\n")
        self.assertEqual(d["run"], {"vertex": "vs", "fragment": "fs", "width": 8, "instances": 2})
        self.assertEqual(d["geometry"], [(1.0, 2.0, 0.5), (3.0, 4.0, 0.5), (5.0, 6.0, 0.25)])
        self.assertEqual((d["position"], d["depth"]), ("pos", True))

    def test_bad_values_are_refused(self):
        for text in ("// RUN: vertex=1x\n", "// RUN: instances=0\n", "// RUN: vertex=\n", "// DIFFCHECK-GEOMETRY: 1\n",
                     "// DIFFCHECK-GEOMETRY: a,b\n"):
            with self.assertRaises(ValueError, msg=text):
                directives.parse(text)


class Interface(unittest.TestCase):
    def test_types_flatness_and_the_selected_entry(self):
        r = spirvio.interface(DISASM, "fs")
        self.assertEqual(r["inputs"], {0: {"scalar": "float", "comps": 4, "flat": False},
                                       1: {"scalar": "uint", "comps": 1, "flat": True}})
        self.assertEqual(r["outputs"], {0: {"scalar": "uint", "comps": 1, "flat": False}})
        self.assertEqual(r["unsupported"], [])

    def test_an_array_output_is_unsupported_not_ignored(self):
        self.assertIn("output location 3", spirvio.interface(DISASM, "other")["unsupported"][0])

    def test_unknown_entry_is_an_error(self):
        with self.assertRaises(ValueError):
            spirvio.interface(DISASM, "nope")


class Geometry(unittest.TestCase):
    def test_default_is_unclipped_triangles(self):
        graphics.check_geometry(graphics.DEFAULT_GEOMETRY, 32, 32)

    def test_clipped_or_partial_geometry_is_refused(self):
        for points in ([(0, 5, .5), (5, 5, .5), (5, 9, .5)], [(1, 1, .5), (40, 1, .5), (5, 9, .5)],
                       [(1, 1, 2.0), (5, 5, .5), (5, 9, .5)], [(1, 1, .5), (5, 5, .5)], []):
            with self.assertRaises(graphics.GraphicsError, msg=points):
                graphics.check_geometry(points, 32, 32)

    def test_off_grid_vertices_and_non_power_of_two_targets_are_refused(self):
        graphics.check_geometry([(8.25, 8.5, .5), (24, 8, .5), (8, 24.00390625, .5)], 32, 32)
        for points, w, h in (([(8.3, 8, .5), (24, 8, .5), (8, 24, .5)], 32, 32), ([(8, 8, .5), (24, 8, .5), (8, 24.001, .5)], 32, 32),
                             ([(8, 8, .5), (24, 8, .5), (8, 24, .5)], 30, 32)):
            with self.assertRaises(graphics.GraphicsError, msg=points):
                graphics.check_geometry(points, w, h)

    def test_pixel_to_ndc_keeps_row_zero_on_top(self):
        self.assertEqual(graphics.ndc((8, 8, .5), 32, 16), (-.5, 0.0, .5))
        self.assertEqual(graphics.ndc((16, 4, .5), 32, 16)[1], .5)


class Outputs(unittest.TestCase):
    SRC = ("struct F { float4 a [[color(0)]]; uint2 b [[color(1)]]; float d [[depth(any)]]; };\n"
           "fragment F fs(int x) { }\n// fragment half4 other() {}\n")

    def test_struct_members_give_colours_and_depth(self):
        self.assertEqual(outputs.fragment_outputs(self.SRC, "fs"), ({0: ("float", 4), 1: ("uint", 2)}, True))

    def test_plain_return_types(self):
        self.assertEqual(outputs.fragment_outputs("fragment half4 fs() {}", "fs"), ({0: ("half", 4)}, False))
        self.assertEqual(outputs.fragment_outputs("[[fragment]] void fs() {}", "fs"), ({}, False))

    def test_unreadable_outputs_are_unsupported_not_guessed(self):
        for src in ("fragment Mystery fs() {}", "fragment uchar4 fs() {}", "typedef int T;\nfragment T fs() {}",
                    "struct F { float4 a [[color(1)]]; };\nfragment F fs() {}"):
            with self.assertRaises(outputs.Unsupported, msg=src):
                outputs.fragment_outputs(src, "fs")

    def test_a_module_that_drops_or_retypes_an_output_is_a_finding(self):
        colors = {0: ("float", 4), 1: ("uint", 2)}
        iface = {"outputs": {0: {"scalar": "float", "comps": 4}, 1: {"scalar": "uint", "comps": 2}}, "frag_depth": False}
        self.assertEqual(outputs.cross_check(colors, False, iface), [])
        dropped = dict(iface, outputs={0: iface["outputs"][0]})
        self.assertIn("[0, 1], mslc's module [0]", outputs.cross_check(colors, False, dropped)[0])
        retyped = dict(iface, outputs={0: iface["outputs"][0], 1: {"scalar": "int", "comps": 2}})
        self.assertIn("colour 1 is uint2", outputs.cross_check(colors, False, retyped)[0])
        self.assertIn("writes a depth output", outputs.cross_check(colors, True, iface)[0])
        self.assertEqual(outputs.cross_check({0: ("half", 4)}, False, {"outputs": {0: {"scalar": "float", "comps": 4}}, "frag_depth": False}), [])


class Data(unittest.TestCase):
    def test_position_attribute_carries_the_geometry_and_the_rest_is_seeded(self):
        import random
        attrs = {0: {"name": "pos", "scalar": "float", "comps": 4}, 1: {"name": "col", "scalar": "uint", "comps": 2}}
        a = graphics.vertex_data(attrs, [(8, 8, .25), (24, 8, .25), (8, 24, .25)], "pos", 32, 32, random.Random(1), "moderate")
        self.assertEqual(struct.unpack("<12f", a[0]), (-.5, .5, .25, 1.0, .5, .5, .25, 1.0, -.5, -.5, .25, 1.0))
        self.assertEqual(len(a[1]), 3 * 2 * 4)
        b = graphics.vertex_data(attrs, [(8, 8, .25), (24, 8, .25), (8, 24, .25)], "pos", 32, 32, random.Random(1), "moderate")
        self.assertEqual(a, b)

    def test_attachment_layout_compares_only_the_written_components(self):
        layout = graphics.attachment_layout("color0", "float", 2)
        self.assertEqual((layout["stride"], [l["off"] for l in layout["leaves"]]), (8, [0, 4]))
        self.assertEqual(graphics.color_format("float", 2), "rg32float")
        self.assertEqual(graphics.color_format("half", 4), "rgba16float")
        clear = graphics.clear_image("int", 2, 3)
        self.assertEqual(struct.unpack("<6i", clear), graphics.CLEAR["int"][:2] * 3)

    def test_a_drawn_pixel_counts_as_output_and_the_clear_colour_does_not(self):
        layout = {0: graphics.attachment_layout("color0", "float", 4)}
        clear = {0: graphics.clear_image("float", 4, 4)}
        drawn = {0: bytearray(clear[0])}
        self.assertFalse(checks.written_nonzero(layout, clear, drawn))
        drawn[0][16:20] = FLOAT(0.5)
        self.assertTrue(checks.written_nonzero(layout, clear, drawn))
        zero = {0: bytearray(clear[0])}
        zero[0][16:32] = bytes(16)
        self.assertFalse(checks.written_nonzero(layout, clear, zero))


class SplitLeaves(unittest.TestCase):
    def test_components_constant_per_primitive_are_exact_and_varying_ones_are_not(self):
        layout = graphics.attachment_layout("color0", "float", 3)
        pixels = [(FLOAT(i * 0.001) , FLOAT(0.375), FLOAT(float(i % 2))) for i in range(40)]
        apple = bytearray(b"".join(b"".join(p) for p in pixels))
        exact, smooth = graphics.split_leaves(layout, apple, primitives=3)
        self.assertEqual(([l["off"] for l in exact], [l["off"] for l in smooth]), ([4, 8], [0]))

    def test_integers_are_always_exact(self):
        layout = graphics.attachment_layout("color0", "uint", 1)
        apple = bytearray(b"".join(struct.pack("<I", i) for i in range(40)))
        self.assertEqual(graphics.split_leaves(layout, apple, 1), (layout["leaves"], []))


class Tolerance(unittest.TestCase):
    def test_integers_and_flat_floats_are_exact(self):
        self.assertEqual(graphics.tolerance(0, True, "uint"), (0, 0.0))
        self.assertEqual(graphics.tolerance(0, False, "float"), (0, 0.0))
        self.assertEqual(graphics.tolerance(4, False, "float"), (4, 0.0))

    def test_interpolated_floats_get_a_ulp_and_an_epsilon_bound(self):
        ulp, eps = graphics.tolerance(4, True, "float")
        self.assertEqual((ulp, eps), (graphics.INTERP_ULP, graphics.INTERP_EPS))
        near_zero = (FLOAT(1e-7), FLOAT(-1e-7))
        self.assertFalse(compare.leaf_equal(*near_zero, "float", 32))
        self.assertTrue(compare.leaf_equal(*near_zero, "float", ulp, eps))
        self.assertFalse(compare.leaf_equal(FLOAT(1.0), FLOAT(1.001), "float", ulp, eps))
        self.assertFalse(compare.leaf_equal(FLOAT(1.0), FLOAT(float("nan")), "float", ulp, eps))
        self.assertFalse(compare.leaf_equal(FLOAT(3.4e38), FLOAT(float("inf")), "float", ulp, eps))


if __name__ == "__main__":
    unittest.main()
