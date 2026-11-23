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

    def test_swapped_members_are_a_finding(self):
        # Two members exchange offsets: the set of offsets is unchanged, the layout is not.
        text = DISASM.replace("1 Offset 4", "1 Offset 99").replace("2 Offset 8", "2 Offset 4").replace("1 Offset 99", "1 Offset 8")
        findings = layout.cross_check(BUFFERS, APPLE, layout.spirv_layouts(text, "k", BUFFERS))
        self.assertEqual(len(findings), 1)

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
        text = DISASM.replace("OpDecorate %10 ArrayStride 16", "OpDecorate %10 ArrayStride 32\nOpMemberDecorate %12 0 MatrixStride 16\nOpMemberDecorate %12 0 ColMajor"
                              ).replace("%10 = OpTypeRuntimeArray %8", "%7 = OpTypeMatrix %8 2\n%10 = OpTypeRuntimeArray %7")
        got = layout.spirv_layouts(text, "k", BUFFERS)
        self.assertEqual(got[1]["stride"], 32)
        self.assertEqual([l[0] for l in got[1]["leaves"]], [0, 4, 8, 12, 16, 20, 24, 28])
        with self.assertRaises(ValueError):  # a square matrix read row major has the same offsets
            layout.spirv_layouts(text.replace("ColMajor", "RowMajor"), "k", BUFFERS)
        with self.assertRaises(ValueError):
            layout.spirv_layouts(text.replace("OpMemberDecorate %12 0 ColMajor", ""), "k", BUFFERS)

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
        plain0 = {"s_address": "Repeat", "t_address": "Repeat", "r_address": "Repeat", "mag_filter": "Nearest", "min_filter": "Nearest",
                  "mip_filter": "None", "normalized_coordinates": True, "compare_function": "Never", "anisotropy": 1,
                  "lod_min": 0, "lod_max": 65504}
        entry = {"bindings": [{"kind": "Texture", "name": "t", "texture_access": "Write"}], "embedded_samplers": [dict(plain0, mip_filter="Linear")]}
        apple_side = {"textures": [{"name": "t", "type": "3d", "data": "int", "access": "write"}]}
        text = "; ".join(resources.unsupported(entry, apple_side))
        for needle in ("access Write", "not a 2D float", "is written", "embedded sampler mip_filter is Linear"):
            self.assertIn(needle, text)
        plain = {"s_address": "Repeat", "t_address": "ClampToEdge", "r_address": "MirrorRepeat", "mag_filter": "Linear",
                 "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": True, "compare_function": "Never", "anisotropy": 1,
                 "lod_min": 0, "lod_max": 65504}
        embedded = {"bindings": [{"kind": "Sampler", "embedded_sampler": 0, "name": "s", "descriptor": {"set": 1, "binding": 2}}],
                    "embedded_samplers": [plain]}
        self.assertEqual(resources.unsupported(embedded, {"textures": []}), [])
        self.assertEqual(resources.seed(embedded["bindings"], random.Random(1), "moderate", "x-", embedded["embedded_samplers"]),
                         ([], [(None, 1, 2, (0, 2, 1), True)]))
        self.assertEqual(resources.lines([], [(None, 1, 2, (0, 2, 1), True)]), ["esampler 1 2 0 2 1 1"])
        self.assertEqual(resources.lines([], [(None, 1, 2, (0, 2, 1), True)], "apple"), [])
        odd = {"bindings": [], "embedded_samplers": [dict(plain, compare_function="Less", s_address="Bogus")]}
        text = "; ".join(resources.unsupported(odd, {"textures": []}))
        self.assertIn("compare_function is Less", text)
        self.assertIn("address Bogus", text)
        clamp = dict(plain, s_address="ClampToZero")
        self.assertEqual(resources.unsupported({"bindings": [], "embedded_samplers": [clamp]}, {"textures": []}), [])
        self.assertEqual(resources.seed(embedded["bindings"], random.Random(1), "moderate", "x-", [clamp])[1][0][3][0], 3)
        mixed = {"bindings": [], "embedded_samplers": [dict(plain, mag_filter="Nearest"), dict(plain, lod_min=1), dict(plain, lod_max=2.5)]}
        text = "; ".join(resources.unsupported(mixed, {"textures": []}))
        for needle in ("min_filter Linear differs from mag_filter Nearest", "lod_min is 1", "lod_max is 2.5"):
            self.assertIn(needle, text)
        self.assertEqual(resources.cross_check(embedded["bindings"], {"textures": [], "samplers": []}), [])
        ok = {"bindings": [{"kind": "Texture", "name": "t", "texture_access": "Read"}], "embedded_samplers": []}
        self.assertEqual(resources.unsupported(ok, {"textures": [{"name": "t", "type": "2d", "data": "float", "access": "read"}]}), [])

    def test_cross_check(self):
        mine = [{"kind": "Texture", "metal_index": 0, "name": "t"}, {"kind": "Sampler", "metal_index": 0, "name": "s"}]
        good = {"textures": [{"index": 0, "name": "t", "type": "2d"}], "samplers": [{"index": 0, "name": "s"}]}
        self.assertEqual(resources.cross_check(mine, good), [])
        bad = {"textures": [{"index": 1, "name": "t", "type": "2d"}], "samplers": []}
        self.assertEqual(len(resources.cross_check(mine, bad)), 2)

    TEXEL = {"kind": "TextureBuffer", "metal_index": 3, "name": "tb", "descriptor": {"set": 1, "binding": 4},
             "descriptor_type": "StorageTexelBuffer", "texture_access": "ReadWrite"}
    APPLE_TEXEL = {"index": 3, "name": "tb", "type": "texturebuffer", "data": "ushort", "access": "readwrite"}

    def test_texel_buffer_plan(self):
        plan = resources.texel_plan([self.TEXEL], {"textures": [self.APPLE_TEXEL]})
        self.assertEqual([(t.index, t.set, t.binding, t.uniform, t.scalar) for t in plan], [(3, 1, 4, False, "ushort")])
        lay = plan[0].layout
        self.assertEqual((lay["name"], lay["access"], lay["stride"]), ("tb", "readwrite", 8))
        self.assertEqual([(l["off"], l["scalar"]) for l in lay["leaves"]], [(0, "ushort"), (2, "ushort"), (4, "ushort"), (6, "ushort")])
        self.assertLess(resources.texel_key(plan[0]), 0)  # never a device buffer's Metal index

    def test_texel_buffer_components(self):
        for n in (1, 2, 4):
            plan = resources.texel_plan([self.TEXEL], {"textures": [self.APPLE_TEXEL]}, {"tb": n})
            self.assertEqual(plan[0].components, n)
            lay = plan[0].layout
            self.assertEqual((lay["stride"], [l["off"] for l in lay["leaves"]]), (2 * n, [2 * c for c in range(n)]))
            with tempfile.TemporaryDirectory() as d:
                seeded = resources.seed_texels(plan, 16, random.Random(5), "edge", os.path.join(d, "a-"))
                self.assertEqual(len(seeded[0][2]), (16 + 4 * 3) * 2 * n)
                self.assertIn("storage ushort %d 28 " % n, resources.texel_lines(seeded, {3: "o"})[0])

    def test_texel_buffer_seed_is_deterministic_and_lines(self):
        plan = resources.texel_plan([self.TEXEL], {"textures": [self.APPLE_TEXEL]})
        with tempfile.TemporaryDirectory() as d:
            a = resources.seed_texels(plan, 16, random.Random(5), "edge", os.path.join(d, "a-"))
            b = resources.seed_texels(plan, 16, random.Random(5), "edge", os.path.join(d, "b-"))
            self.assertEqual(a[0][2], b[0][2])
            self.assertEqual(len(a[0][2]), (16 + 4 * 3) * 8)  # 16 texels, 4 per Metal index, the case number
            with open(a[0][1], "rb") as f:
                self.assertEqual(f.read(), bytes(a[0][2]))
        self.assertEqual(resources.texel_lines(a, {3: "out.bin"}),
                         ["texelbuffer 3 1 4 storage ushort 4 28 %s out.bin" % a[0][1]])

    def test_texel_buffer_cross_check(self):
        good = {"textures": [self.APPLE_TEXEL], "samplers": []}
        self.assertEqual(resources.cross_check([self.TEXEL], good), [])
        # a texture_buffer and a 2D texture are different kinds even at one index
        self.assertEqual(len(resources.cross_check([dict(self.TEXEL, kind="Texture")], good)), 2)
        # mslc declares a sampled texel buffer where the source says read_write
        wrong = dict(self.TEXEL, descriptor_type="UniformTexelBuffer", texture_access="Read")
        self.assertIn("read_write", " ".join(resources.cross_check([wrong], good)).replace("readwrite", "read_write"))
        self.assertEqual(len(resources.cross_check([wrong], good)), 1)

    def test_texel_buffer_unsupported(self):
        entry = {"bindings": [self.TEXEL], "embedded_samplers": []}
        self.assertEqual(resources.unsupported(entry, {"textures": [self.APPLE_TEXEL]}), [])
        self.assertIn("element type other", "; ".join(resources.unsupported(entry, {"textures": [dict(self.APPLE_TEXEL, data="other")]})))


class Directives(unittest.TestCase):
    def test_texel(self):
        self.assertEqual(directives.parse("// DIFFCHECK-TEXEL: a 1\n// DIFFCHECK-TEXEL: b 2\n")["texel"], {"a": 1, "b": 2})
        self.assertEqual(directives.parse("")["texel"], {})
        for bad in ("// DIFFCHECK-TEXEL: a 3\n", "// DIFFCHECK-TEXEL: a\n", "// DIFFCHECK-TEXEL: a 8\n", "// DIFFCHECK-TEXEL: 2\n"):
            with self.assertRaises(ValueError, msg=bad):
                directives.parse(bad)

    def test_ulp_and_unused(self):
        d = directives.parse("// DIFFCHECK-ULP: exact 0\n// DIFFCHECK-ULP: loose 8\n// DIFFCHECK-UNUSED: t never read\n")
        self.assertEqual(d["ulp"], {"exact": 0, "loose": 8})
        self.assertEqual(d["unused"], {"t": "never read"})
        with self.assertRaises(ValueError):
            directives.parse("// DIFFCHECK-UNUSED: t\n")
        for bad in ("// DIFFCHECK-ULP: exact 0.5\n", "// DIFFCHECK-ULP: exact\n", "// DIFFCHECK-ULP: 3\n"):
            with self.assertRaises(ValueError, msg=bad):
                directives.parse(bad)


class Verdict(unittest.TestCase):
    CASES = [{"negative_control": True, "diff_count": 0}]

    def test_unused_texture_fails_a_match(self):
        self.assertEqual(checks.verdict(self.CASES, True, False, ["t"])[0], "unused-texture")
        self.assertEqual(checks.verdict(self.CASES, True, False, [])[0], "matched")

    def test_mismatch_outranks_unused(self):
        cases = [{"negative_control": True, "diff_count": 1, "diffs": [{"buffer": "o"}]}]
        self.assertEqual(checks.verdict(cases, True, False, ["t"])[0], "mismatched")


class Threadgroup(unittest.TestCase):
    MINE = [{"metal_index": 2, "spec_id": 3, "element_size": 4, "name": "a"},
            {"metal_index": 0, "spec_id": 4, "element_size": 16, "name": "b"}]

    def test_lengths_are_bytes_on_apple_and_elements_on_vulkan(self):
        self.assertEqual(resources.threadgroup_lines(self.MINE, 8, "apple"), ["threadgroup 2 32", "threadgroup 0 128"])
        self.assertEqual(resources.threadgroup_lines([dict(self.MINE[0], element_size=1)], 8, "apple"), ["threadgroup 2 16"])
        self.assertEqual(resources.threadgroup_lines(self.MINE, 8, "mslc"), ["spec 3 8", "spec 4 8"])

    def test_cross_check_compares_index_name_and_size(self):
        apple = {"threadgroups": [{"index": 0, "name": "b", "size": 16}, {"index": 2, "name": "a", "size": 4}]}
        self.assertEqual(resources.threadgroup_cross_check(self.MINE, apple), [])
        apple["threadgroups"][0]["size"] = 12
        self.assertEqual(len(resources.threadgroup_cross_check(self.MINE, apple)), 1)
        apple["threadgroups"].pop()
        self.assertEqual(len(resources.threadgroup_cross_check(self.MINE, apple)), 1)


if __name__ == "__main__":
    unittest.main()
