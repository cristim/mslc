#!/usr/bin/env python3
"""Unit tests for the graphics-mode helpers of tools/diffcheck (directives, spirvio, graphics,
compare's epsilon). No GPU needed."""

import os
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import checks  # noqa: E402
import compare  # noqa: E402
import directives  # noqa: E402
import graphics  # noqa: E402
import hosts  # noqa: E402
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


class VertexAttributes(unittest.TestCase):
    def test_matching_types_have_no_findings(self):
        mine = {0: {"scalar": "float", "comps": 4}, 2: {"scalar": "uint", "comps": 1}}
        apple = [{"index": 2, "scalar": "uint", "comps": 1}, {"index": 0, "scalar": "float", "comps": 4}]
        self.assertEqual(graphics.vertex_attribute_findings(mine, apple), [])

    def test_type_width_and_presence_differences_are_findings(self):
        mine = {0: {"scalar": "float", "comps": 4}, 1: {"scalar": "int", "comps": 2}}
        apple = [{"index": 0, "scalar": "float", "comps": 3}, {"index": 1, "scalar": "uint", "comps": 2},
                 {"index": 5, "scalar": "float", "comps": 1}]
        self.assertEqual(len(graphics.vertex_attribute_findings(mine, apple)), 3)
        self.assertEqual(len(graphics.vertex_attribute_findings({}, apple[:1])), 1)

    def test_half_attributes_match_widened_spirv_inputs_only_when_apple_is_half(self):
        half_mine = {0: {"scalar": "float", "comps": 1}, 1: {"scalar": "float", "comps": 3}}
        half_apple = [{"index": 0, "scalar": "half", "comps": 1}, {"index": 1, "scalar": "half", "comps": 3}]
        self.assertEqual(graphics.vertex_attribute_findings(half_mine, half_apple), [])
        self.assertEqual(len(graphics.vertex_attribute_findings({0: {"scalar": "half", "comps": 2}},
                                                               [{"index": 0, "scalar": "float", "comps": 2}])), 1)
        self.assertEqual(len(graphics.vertex_attribute_findings({0: {"scalar": "int", "comps": 1}}, half_apple[:1])), 1)


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


class Resources(unittest.TestCase):
    EMBEDDED = {"s_address": "Repeat", "t_address": "ClampToZero", "r_address": "MirrorRepeat", "mag_filter": "Linear",
                "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": True, "compare_function": "Never",
                "anisotropy": 1, "lod_min": 0, "lod_max": 65504}
    PARAMS = {"width": 32, "height": 32, "instances": 1, "seed": 1, "cases": 1, "elems": None, "std": None,
              "exclude": {}, "range": {}, "writes_zero": False, "geometry": None, "position": "none",
              "depth": False, "float_range": {}, "ulp": {}}
    OPTS = type("Opts", (), {"mslc": "mslc", "metal": "metalhost", "vk": "vkhost", "apple_contract": False,
                             "gfx_profiles": ["moderate"], "corrupt_mslc": None})()
    SOURCE = "vertex float4 vs() { return float4(0); }\nfragment float4 fs() { return float4(0); }\n"
    IFACE = {"inputs": {}, "outputs": {0: {"scalar": "float", "comps": 4, "flat": False}}, "frag_depth": False, "unsupported": []}

    def _refl(self, v_bindings=(), f_bindings=(), v_emb=(), f_emb=()):
        return {
            "vertex": {"entry_points": [{"name": "vs", "stage": "vertex", "bindings": list(v_bindings), "embedded_samplers": list(v_emb)}]},
            "fragment": {"entry_points": [{"name": "fs", "stage": "fragment", "bindings": list(f_bindings), "embedded_samplers": list(f_emb)}]},
        }

    def _apple(self, v_tex=(), v_samp=(), f_tex=(), f_samp=()):
        return {
            "vertex_attributes": [],
            "vertex": {"buffers": [], "textures": list(v_tex), "samplers": list(v_samp), "unsupported": []},
            "fragment": {"buffers": [], "textures": list(f_tex), "samplers": list(f_samp), "unsupported": []},
        }

    def _run_prepare(self, refl, apple, params=None):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.metal")
            with open(src, "w") as f:
                f.write(self.SOURCE)
            with mock.patch.object(hosts, "run_mslc", side_effect=lambda *a: ("s.spv", refl[a[4]], None)), \
                 mock.patch.object(spirvio, "disassemble", return_value=""), \
                 mock.patch.object(spirvio, "interface", return_value=self.IFACE), \
                 mock.patch.object(hosts, "apple_source_lines", return_value=["source " + src]), \
                 mock.patch.object(hosts, "apple_gfx_reflection", return_value=apple):
                return graphics._prepare(self.OPTS, src, "vs", "fs", params or self.PARAMS, d, [])

    def test_draw_lines_forwards_backend_for_esampler_nonfloat_and_non2d(self):
        shape = graphics.pipeline_lines(32, 32, "vs", "fs", {}, {0: ("float", 4)}, False)
        tex = [(0, 0, 0, "vt.bin", "Read", "short"), (0, 1, 0, "ft.bin", "Sample", "half"),
               (1, 1, 1, "cube.bin", "Sample", "float", "cube")]
        samp = [(1, 0, 1), (None, 1, 1, (0, 3, 1), True)]
        vk = graphics.draw_lines(shape, graphics.DEFAULT_GEOMETRY, 1, {}, {0: "c.bin"}, {0: ("float", 4)}, None, [], tex, samp, "mslc")
        ap = graphics.draw_lines(shape, graphics.DEFAULT_GEOMETRY, 1, {}, {0: "c.bin"}, {0: ("float", 4)}, None, [], tex, samp, "apple")
        self.assertIn("storagetexture 0 0 0 4 4 vt.bin short", vk)
        self.assertIn("texture 0 1 0 4 4 ft.bin half", vk)
        self.assertIn("image cube sample 1 1 1 4 4 1 1 float cube.bin", vk)
        self.assertIn("image cube sample 1 1 1 4 4 1 1 float cube.bin", ap)
        self.assertIn("sampler 1 0 1", vk)
        self.assertIn("esampler 1 1 0 3 1 1", vk)
        self.assertFalse(any(l.startswith("esampler ") for l in ap))

    def test_prepare_accepts_vertex_resources_nonfloat_non2d_and_embedded_samplers(self):
        v_kinds = ("1d", "1d_array", "2d")
        f_kinds = ("2d_array", "3d", "cube", "cube_array")
        refl = self._refl(
            v_bindings=[{"kind": "Texture", "metal_index": i, "descriptor": {"set": 0, "binding": i},
                         "texture_access": "Read" if i else "Sample", "name": "vt%d" % i} for i in range(len(v_kinds))]
            + [{"kind": "Sampler", "descriptor": {"set": 0, "binding": 3}, "embedded_sampler": 0, "name": "vs_emb"}],
            f_bindings=[{"kind": "Texture", "metal_index": i, "descriptor": {"set": 1, "binding": i},
                         "texture_access": "Read" if i < 2 else "Sample", "name": "ft%d" % i} for i in range(len(f_kinds))]
            + [{"kind": "Sampler", "metal_index": 0, "descriptor": {"set": 1, "binding": 4}, "name": "fs_rt"}],
            v_emb=[self.EMBEDDED],
        )
        apple = self._apple(
            v_tex=[{"index": i, "name": "vt%d" % i, "type": k, "data": "short" if i else "float", "access": "read"}
                   for i, k in enumerate(v_kinds)],
            f_tex=[{"index": i, "name": "ft%d" % i, "type": k, "data": "half" if i == 1 else "float", "access": "read"}
                   for i, k in enumerate(f_kinds)],
            f_samp=[{"index": 0, "name": "fs_rt"}],
        )
        plan, early = self._run_prepare(refl, apple)
        self.assertIsNone(early)
        self.assertEqual(plan["apple"], apple)

    def test_prepare_infers_position_in_corpus_mode_and_errors_without_it(self):
        single = {0: {"scalar": "float", "comps": 2, "flat": False}, 1: {"scalar": "uint", "comps": 1, "flat": True}}
        named = {0: {"scalar": "float", "comps": 4, "flat": False}, 1: {"scalar": "float", "comps": 4, "flat": False}}
        self.assertEqual(graphics._infer_position({0: dict(single[0], name="p"), 1: dict(single[1], name="id")}), "p")
        self.assertEqual(graphics._infer_position({0: dict(named[0], name="pos"), 1: dict(named[1], name="col")}), "pos")
        self.assertIsNone(graphics._infer_position({0: dict(named[0], name="a"), 1: dict(named[1], name="b")}))
        refl = self._refl(v_bindings=[{"kind": "VertexInput", "location": 0, "name": "a"}, {"kind": "VertexInput", "location": 1, "name": "b"}])
        v_iface = {"inputs": named, "outputs": {}, "frag_depth": False, "unsupported": []}
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.metal")
            with open(src, "w") as f:
                f.write(self.SOURCE)
            ifaces = {"vertex": v_iface, "fragment": self.IFACE}
            with mock.patch.object(hosts, "run_mslc", side_effect=lambda *a: (a[4] + ".spv", refl[a[4]], None)), \
                 mock.patch.object(spirvio, "disassemble", side_effect=lambda spv: spv[:-4]), \
                 mock.patch.object(spirvio, "interface", side_effect=lambda tag, _: ifaces[tag]):
                _, err_res = graphics._prepare(self.OPTS, src, "vs", "fs", dict(self.PARAMS, position=None), d, [])
                _, skip_res = graphics._prepare(self.OPTS, src, "vs", "fs", dict(self.PARAMS, position=None, infer_position=True), d, [])
        self.assertEqual((err_res["status"], skip_res["status"]), ("error", "skipped"))
        self.assertIn("DIFFCHECK-POSITION", skip_res["reason"])

    def test_prepare_skips_writable_depth_and_bad_embedded_samplers(self):
        refl_w = self._refl(f_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 1, "binding": 0}, "texture_access": "Write", "name": "w"}])
        _, early = self._run_prepare(refl_w, self._apple())
        self.assertEqual((early["status"], early["reason"]), ("skipped", "unsupported: texture w access Write"))

        refl_d = self._refl(v_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 0, "binding": 0}, "texture_access": "Sample", "name": "td"}])
        apple_d = self._apple(v_tex=[{"index": 0, "name": "td", "type": "depth", "data": "float", "access": "read"}])
        _, early = self._run_prepare(refl_d, apple_d)
        self.assertEqual(early["status"], "skipped")
        self.assertIn("unsupported type depth", early["reason"])

        refl_bad_emb = self._refl(f_bindings=[{"kind": "Sampler", "descriptor": {"set": 1, "binding": 0}, "embedded_sampler": 0, "name": "s"}],
                                  f_emb=[dict(self.EMBEDDED, mip_filter="Linear")])
        _, early = self._run_prepare(refl_bad_emb, self._apple())
        self.assertEqual(early["status"], "skipped")
        self.assertIn("mip_filter is Linear", early["reason"])

    def test_prepare_checks_stage_descriptor_set_and_unsupported_before_cross_check(self):
        refl_set = self._refl(v_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 1, "binding": 0},
                                           "texture_access": "Sample", "name": "vt"}])
        apple_ok = self._apple(v_tex=[{"index": 0, "name": "vt", "type": "2d", "data": "float", "access": "read"}])
        _, early = self._run_prepare(refl_set, apple_ok)
        self.assertEqual(early["status"], "reflection-mismatch")
        self.assertIn("descriptor set 1, expected 0", early["reason"])

        refl_both = self._refl(
            v_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 0, "binding": 0}, "texture_access": "Sample", "name": "vt"}],
            f_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 1, "binding": 0}, "texture_access": "Sample", "name": "ft"}],
        )
        apple_mismatch_plus_skip = self._apple(
            v_tex=[{"index": 0, "name": "wrong", "type": "2d", "data": "float", "access": "read"}],
            f_tex=[{"index": 0, "name": "ft", "type": "other", "data": "float", "access": "read"}],
        )
        _, early = self._run_prepare(refl_both, apple_mismatch_plus_skip)
        self.assertEqual(early["status"], "skipped")
        self.assertIn("unsupported type other", early["reason"])

    def test_check_pair_seeds_per_stage_and_skips_host_refusal(self):
        refl = self._refl(
            v_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 0, "binding": 0}, "texture_access": "Read", "name": "vt"}],
            f_bindings=[{"kind": "Texture", "metal_index": 0, "descriptor": {"set": 1, "binding": 0}, "texture_access": "Sample", "name": "ft"},
                        {"kind": "Sampler", "descriptor": {"set": 1, "binding": 1}, "embedded_sampler": 0, "name": "es"}],
            f_emb=[self.EMBEDDED],
        )
        apple = self._apple(
            v_tex=[{"index": 0, "name": "vt", "type": "2d", "data": "ushort", "access": "read"}],
            f_tex=[{"index": 0, "name": "ft", "type": "2d", "data": "half", "access": "read"}],
        )
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.metal")
            with open(src, "w") as f:
                f.write(self.SOURCE)
            seen = {}

            def run_host(binary, manifest):
                with open(manifest) as mf:
                    text = mf.read()
                seen[binary] = text
                if binary == "vkhost":
                    raise hosts.Refused("unsupported: lavapipe: no storage feature")
                for line in text.splitlines():
                    if line.startswith("color "):
                        with open(line.split()[-1], "wb") as out:
                            out.write(bytes(32 * 32 * 16))

            with mock.patch.object(hosts, "run_mslc", side_effect=lambda *a: ("s.spv", refl[a[4]], None)), \
                 mock.patch.object(spirvio, "disassemble", return_value=""), \
                 mock.patch.object(spirvio, "interface", return_value=self.IFACE), \
                 mock.patch.object(hosts, "apple_source_lines", return_value=["source " + src]), \
                 mock.patch.object(hosts, "apple_gfx_reflection", return_value=apple), \
                 mock.patch.object(hosts, "run_metal", side_effect=run_host), \
                 mock.patch.object(hosts, "run_vulkan", side_effect=run_host):
                res = graphics.check_pair(self.OPTS, src, "vs", "fs", self.PARAMS, d, [])
            self.assertEqual(res["status"], "skipped")
            self.assertIn("no storage feature", res["reason"])
            self.assertIn("storagetexture 0 0 0 4 4 ", seen["metalhost"])
            self.assertIn("ushort", seen["metalhost"])
            self.assertNotIn("esampler ", seen["metalhost"])
            self.assertIn("texture 0 1 0 4 4 ", seen["vkhost"])
            self.assertIn("half", seen["vkhost"])
            self.assertIn("esampler 1 1 0 3 1 1", seen["vkhost"])

    def test_prepare_upgrades_half_vertex_attributes_and_guards_position(self):
        refl = self._refl(v_bindings=[{"kind": "VertexInput", "location": 0, "name": "pos"},
                                      {"kind": "VertexInput", "location": 1, "name": "h1"},
                                      {"kind": "VertexInput", "location": 2, "name": "h3"}])
        v_iface = {"inputs": {0: {"scalar": "float", "comps": 2, "flat": False},
                              1: {"scalar": "float", "comps": 1, "flat": False},
                              2: {"scalar": "float", "comps": 3, "flat": False}},
                   "outputs": {}, "frag_depth": False, "unsupported": []}
        apple = dict(self._apple(), vertex_attributes=[
            {"index": 0, "name": "pos", "scalar": "float", "comps": 2},
            {"index": 1, "name": "h1", "scalar": "half", "comps": 1},
            {"index": 2, "name": "h3", "scalar": "half", "comps": 3},
        ])
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.metal")
            with open(src, "w") as f:
                f.write(self.SOURCE)
            ifaces = {"vertex": v_iface, "fragment": self.IFACE}
            with mock.patch.object(hosts, "run_mslc", side_effect=lambda *a: (a[4] + ".spv", refl[a[4]], None)), \
                 mock.patch.object(spirvio, "disassemble", side_effect=lambda spv: spv[:-4]), \
                 mock.patch.object(spirvio, "interface", side_effect=lambda tag, _: ifaces[tag]), \
                 mock.patch.object(hosts, "apple_source_lines", return_value=["source " + src]), \
                 mock.patch.object(hosts, "apple_gfx_reflection", return_value=apple):
                plan, early = graphics._prepare(self.OPTS, src, "vs", "fs", dict(self.PARAMS, position="pos"), d, [])
                self.assertIsNone(early)
                self.assertEqual(plan["attributes"][1]["scalar"], "half")
                self.assertIn("attr 1 half", plan["shape"])
                self.assertIn("attr 2 half3", plan["shape"])
                _, bad = graphics._prepare(self.OPTS, src, "vs", "fs", dict(self.PARAMS, position="h3"), d, [])
                self.assertEqual(bad["status"], "error")
                self.assertIn("is half", bad["reason"])
                apple_half_pos = dict(apple, vertex_attributes=[
                    {"index": 0, "name": "pos", "scalar": "half", "comps": 2},
                    {"index": 1, "name": "h1", "scalar": "half", "comps": 1},
                    {"index": 2, "name": "h3", "scalar": "half", "comps": 3},
                ])
                with mock.patch.object(hosts, "apple_gfx_reflection", return_value=apple_half_pos):
                    _, skipped = graphics._prepare(self.OPTS, src, "vs", "fs", dict(self.PARAMS, position=None, infer_position=True), d, [])
                    self.assertEqual(skipped["status"], "skipped")
                    self.assertIn("without // DIFFCHECK-POSITION", skipped["reason"])

    def test_writable_buffers_require_single_instance_and_read_back(self):
        buf = {"kind": "Buffer", "metal_index": 0, "descriptor": {"set": 1, "binding": 0}, "member": 0, "name": "fout"}
        ro = {"kind": "Buffer", "metal_index": 1, "descriptor": {"set": 1, "binding": 1}, "member": 1, "name": "fin"}
        refl = self._refl(f_bindings=[buf, ro])
        lay_w = {"index": 0, "name": "fout", "access": "readwrite", "stride": 4, "leaves": [{"off": 0, "scalar": "uint"}]}
        lay_r = {"index": 1, "name": "fin", "access": "read", "stride": 4, "leaves": [{"off": 0, "scalar": "uint"}]}
        apple = self._apple()
        apple["fragment"]["buffers"] = [lay_w, lay_r]
        _, skip = self._run_prepare(refl, apple, dict(self.PARAMS, instances=2))
        self.assertEqual(skip["status"], "skipped")
        self.assertIn("instances > 1", skip["reason"])

        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.metal")
            with open(src, "w") as f:
                f.write(self.SOURCE)
            manifests = []

            def run_host(binary, manifest):
                with open(manifest) as mf:
                    text = mf.read()
                manifests.append(text)
                for line in text.splitlines():
                    w = line.split()
                    if w[0] == "color":
                        with open(w[-1], "wb") as out:
                            out.write(struct.pack("<4f", 1.0, 2.0, 3.0, 4.0) * (32 * 32))
                    elif w[0] == "gbuffer" and len(w) == 8:
                        with open(w[6], "rb") as src_f, open(w[7], "wb") as out:
                            out.write(struct.pack("<I", 42) + src_f.read()[4:])

            with mock.patch.object(hosts, "run_mslc", side_effect=lambda *a: ("s.spv", refl[a[4]], None)), \
                 mock.patch.object(spirvio, "disassemble", return_value=""), \
                 mock.patch.object(spirvio, "interface", return_value=self.IFACE), \
                 mock.patch.object(hosts, "apple_source_lines", return_value=["source " + src]), \
                 mock.patch.object(hosts, "apple_gfx_reflection", return_value=apple), \
                 mock.patch.object(hosts, "run_metal", side_effect=run_host), \
                 mock.patch.object(hosts, "run_vulkan", side_effect=run_host):
                res = graphics.check_pair(self.OPTS, src, "vs", "fs", self.PARAMS, d, [])
                self.assertEqual(res["status"], "matched")
                self.assertTrue(res["cases"][0]["negative_control"])
                self.assertEqual(res["buffers"], ["color0", "fin", "fout"])
                gbuf_lines = [l for l in manifests[0].splitlines() if l.startswith("gbuffer ")]
                self.assertEqual([len(l.split()) for l in gbuf_lines], [8, 7])
                corrupt_opts = type("O", (type(self.OPTS),), {"corrupt_mslc": "fout:0"})()
                os.makedirs(os.path.join(d, "bad"))
                bad = graphics.check_pair(corrupt_opts, src, "vs", "fs", self.PARAMS, os.path.join(d, "bad"), [])
                self.assertEqual(bad["status"], "mismatched")
                self.assertEqual(bad["cases"][0]["diffs"][0]["buffer"], "fout")


if __name__ == "__main__":
    unittest.main()
