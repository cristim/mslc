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
import resources  # noqa: E402

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
    def test_unused(self):
        d = directives.parse("// DIFFCHECK-UNUSED: t never read\n")
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
