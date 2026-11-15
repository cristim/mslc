#!/usr/bin/env python3
"""Unit tests for the kernel-mode helpers of tools/diffcheck: layout cross-check, textures and
samplers, the ULP and UNUSED directives and the unused-texture verdict. No GPU needed."""

import os
import random
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import checks  # noqa: E402
import directives  # noqa: E402
import layout  # noqa: E402
import resources  # noqa: E402

# One entry point `k` with a constant struct {float a; float unread; float b;} at member 0 and a
# runtime array of float4 at member 1.
DISASM = """
OpEntryPoint GLCompute %46 "k" %43
OpDecorate %10 ArrayStride 16
OpDecorate %12 Block
OpMemberDecorate %12 0 Offset 0
OpDecorate %26 Block
OpMemberDecorate %26 0 Offset 0
OpMemberDecorate %26 1 Offset 4
OpMemberDecorate %26 2 Offset 8
OpDecorate %38 Block
OpMemberDecorate %38 0 Offset 0
OpMemberDecorate %38 1 Offset 8
OpDecorate %43 DescriptorSet 0
OpDecorate %43 Binding 0
%9 = OpTypeFloat 32
%8 = OpTypeVector %9 4
%10 = OpTypeRuntimeArray %8
%12 = OpTypeStruct %10
%26 = OpTypeStruct %9 %9 %9
%36 = OpTypePointer PhysicalStorageBuffer %26
%37 = OpTypePointer PhysicalStorageBuffer %12
%38 = OpTypeStruct %36 %37
%42 = OpTypePointer Uniform %38
%43 = OpVariable %42 Uniform
"""
BUFFERS = [{"metal_index": 0, "name": "p", "member": 0, "descriptor": {"set": 0, "binding": 0}},
           {"metal_index": 1, "name": "out", "member": 1, "descriptor": {"set": 0, "binding": 0}}]


def apple(stride, offsets, scalar="float"):
    return {"stride": stride, "leaves": [{"off": o, "scalar": scalar} for o in offsets]}


APPLE = {0: apple(12, [0, 4, 8]), 1: apple(16, [0, 4, 8, 12])}


class Layout(unittest.TestCase):
    def test_reads_offsets_and_stride(self):
        got = layout.spirv_layouts(DISASM, "k", BUFFERS)
        self.assertEqual(got[0], {"stride": None, "leaves": [(0, 4, "float"), (4, 4, "float"), (8, 4, "float")]})
        self.assertEqual(got[1]["stride"], 16)
        self.assertEqual([l[0] for l in got[1]["leaves"]], [0, 4, 8, 12])
        self.assertEqual(layout.cross_check(BUFFERS, APPLE, got), [])

    def test_moved_member_is_a_finding(self):
        # Moving the unread member: the outputs would not change, the layout does.
        got = layout.spirv_layouts(DISASM.replace("1 Offset 4", "1 Offset 12"), "k", BUFFERS)
        findings = layout.cross_check(BUFFERS, APPLE, got)
        self.assertEqual(len(findings), 1)
        self.assertIn("buffer(0) p", findings[0])

    def test_stride_is_a_finding(self):
        got = layout.spirv_layouts(DISASM.replace("ArrayStride 16", "ArrayStride 32"), "k", BUFFERS)
        self.assertIn("stride is 32", layout.cross_check(BUFFERS, APPLE, got)[0])

    def test_kind_and_size_matter(self):
        got = layout.spirv_layouts(DISASM, "k", BUFFERS)
        wrong = {**APPLE, 0: apple(12, [0, 4, 8], "int")}
        self.assertTrue(layout.cross_check(BUFFERS, wrong, got))

    def test_bare_scalar_buffer(self):
        # `constant float &scale`: the pointer's pointee is the scalar itself, not a struct.
        text = DISASM.replace("%36 = OpTypePointer PhysicalStorageBuffer %26", "%36 = OpTypePointer PhysicalStorageBuffer %9")
        got = layout.spirv_layouts(text, "k", BUFFERS)
        self.assertEqual(got[0], {"stride": None, "leaves": [(0, 4, "float")]})

    def test_matrix_stride_of_a_runtime_array(self):
        # constant matrix_float2x2 *m: the block's member 0 carries MatrixStride, the array only ArrayStride.
        text = DISASM.replace("OpDecorate %10 ArrayStride 16", "OpDecorate %10 ArrayStride 32\nOpMemberDecorate %12 0 MatrixStride 16"
                              ).replace("%10 = OpTypeRuntimeArray %8", "%7 = OpTypeMatrix %8 2\n%10 = OpTypeRuntimeArray %7")
        got = layout.spirv_layouts(text, "k", BUFFERS)
        self.assertEqual(got[1]["stride"], 32)
        self.assertEqual([l[0] for l in got[1]["leaves"]], [0, 4, 8, 12, 16, 20, 24, 28])

    def test_unreadable_layout_raises(self):
        with self.assertRaises(ValueError):
            layout.spirv_layouts(DISASM.replace("OpMemberDecorate %26 2 Offset 8\n", ""), "k", BUFFERS)
        with self.assertRaises(ValueError):
            layout.spirv_layouts(DISASM, "nope", BUFFERS)


class Resources(unittest.TestCase):
    def test_seed_is_deterministic_and_binds_by_reflection(self):
        bindings = [{"kind": "Texture", "metal_index": 2, "descriptor": {"set": 1, "binding": 5}, "name": "t",
                     "texture_access": "Read"},
                    {"kind": "Sampler", "metal_index": 1, "descriptor": {"set": 1, "binding": 6}, "name": "s"},
                    {"kind": "Buffer", "metal_index": 0, "descriptor": {"set": 0, "binding": 0}, "name": "b"}]
        with tempfile.TemporaryDirectory() as d:
            a = resources.seed(bindings, random.Random(3), "moderate", os.path.join(d, "a-"))
            b = resources.seed(bindings, random.Random(3), "moderate", os.path.join(d, "b-"))
            self.assertEqual(a[0][0][:3], (2, 1, 5))
            self.assertEqual(a[0][0][4], "Read")
            self.assertEqual(a[1], [(1, 1, 6)])
            with open(a[0][0][3], "rb") as fa, open(b[0][0][3], "rb") as fb:
                first, second = fa.read(), fb.read()
            self.assertEqual(first, second)
            self.assertEqual(len(first), 4 * 4 * 16)
        lines = resources.lines(a[0], a[1])
        self.assertTrue(lines[0].startswith("storagetexture 2 1 5 4 4 "), lines)  # access::read is a storage image
        self.assertEqual(lines[1], "sampler 1 1 6")
        self.assertTrue(resources.lines([(0, 0, 0, "f", "Sample")], [])[0].startswith("texture 0 "))

    def test_unsupported(self):
        entry = {"bindings": [{"kind": "Texture", "name": "t", "texture_access": "Write"}], "embedded_samplers": [1]}
        apple_side = {"textures": [{"name": "t", "type": "3d", "data": "int", "access": "write"}]}
        text = "; ".join(resources.unsupported(entry, apple_side))
        for needle in ("access Write", "not a 2D float", "is written", "embedded samplers"):
            self.assertIn(needle, text)
        embedded = {"bindings": [{"kind": "Sampler", "embedded_sampler": 0, "name": "s"}], "embedded_samplers": []}
        self.assertEqual(resources.unsupported(embedded, {"textures": []}), ["embedded samplers"])
        self.assertEqual(resources.cross_check(embedded["bindings"], {"textures": [], "samplers": []}), [])
        ok = {"bindings": [{"kind": "Texture", "name": "t", "texture_access": "Read"}], "embedded_samplers": []}
        self.assertEqual(resources.unsupported(ok, {"textures": [{"name": "t", "type": "2d", "data": "float", "access": "read"}]}), [])

    def test_cross_check(self):
        mine = [{"kind": "Texture", "metal_index": 0, "name": "t"}, {"kind": "Sampler", "metal_index": 0, "name": "s"}]
        good = {"textures": [{"index": 0, "name": "t"}], "samplers": [{"index": 0, "name": "s"}]}
        self.assertEqual(resources.cross_check(mine, good), [])
        bad = {"textures": [{"index": 1, "name": "t"}], "samplers": []}
        self.assertEqual(len(resources.cross_check(mine, bad)), 2)


class Directives(unittest.TestCase):
    def test_ulp_and_unused(self):
        d = directives.parse("// DIFFCHECK-ULP: exact 0\n// DIFFCHECK-ULP: loose 8\n// DIFFCHECK-UNUSED: t never read\n")
        self.assertEqual(d["ulp"], {"exact": 0, "loose": 8})
        self.assertEqual(d["unused"], {"t": "never read"})
        with self.assertRaises(ValueError):
            directives.parse("// DIFFCHECK-UNUSED: t\n")


class Verdict(unittest.TestCase):
    CASES = [{"negative_control": True, "diff_count": 0}]

    def test_unused_texture_fails_a_match(self):
        self.assertEqual(checks.verdict(self.CASES, True, False, ["t"])[0], "unused-texture")
        self.assertEqual(checks.verdict(self.CASES, True, False, [])[0], "matched")

    def test_mismatch_outranks_unused(self):
        cases = [{"negative_control": True, "diff_count": 1, "diffs": [{"buffer": "o"}]}]
        self.assertEqual(checks.verdict(cases, True, False, ["t"])[0], "mismatched")


if __name__ == "__main__":
    unittest.main()
