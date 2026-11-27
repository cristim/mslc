#!/usr/bin/env python3
"""Unit tests for corpus list parsing in tools/diffcheck/diffcheck.py. No GPU needed."""

import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import diffcheck  # noqa: E402
import directives  # noqa: E402
import graphics  # noqa: E402
import hosts  # noqa: E402


def listing(text):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "list.txt")
        with open(p, "w") as f:
            f.write(text)
        return diffcheck.corpus_files(p, "root")


class CorpusFiles(unittest.TestCase):
    def test_run_fields_parse_to_the_run_dict(self):
        (path, std, run), = listing("a.metal std=3.1 vertex=vs fragment=fs width=16\n")
        self.assertEqual((path, std, run), (os.path.join("root", "a.metal"), "3.1",
                                              {"vertex": "vs", "fragment": "fs", "width": 16}))

    def test_bare_line_has_an_empty_run(self):
        self.assertEqual(listing("a.metal\n")[0][2], {})

    def test_malformed_line_is_an_error_and_the_next_line_still_parses(self):
        files = listing("x.metal bogus=1\nx.metal vertex=\nx.metal width=0x\ny.metal width=4\n")
        self.assertEqual(len(files), 4)
        for bad in files[:3]:
            self.assertEqual(len(bad), 4)
            r, = diffcheck.check_one(None, *bad)
            self.assertEqual(r["status"], "error")
            self.assertIn("bad corpus list line", r["reason"])
        self.assertEqual(files[3][2], {"width": 4})

    def test_bad_field_is_named_in_the_reason(self):
        r, = diffcheck.check_one(None, *listing("x.metal bogus=1\n")[0])
        self.assertIn("bogus=1", r["reason"])

    def test_empty_std_is_an_error(self):
        r, = diffcheck.check_one(None, *listing("x.metal std=\n")[0])
        self.assertEqual(r["status"], "error")
        self.assertIn("std=", r["reason"])

    def test_field_in_path_position_is_an_error_naming_it(self):
        r, = diffcheck.check_one(None, *listing("vertex=vs a.metal\n")[0])
        self.assertEqual(r["status"], "error")
        self.assertIn("vertex=vs", r["reason"])

    def test_bare_field_line_is_an_error_naming_it(self):
        r, = diffcheck.check_one(None, *listing("vertex=vs\n")[0])
        self.assertEqual(r["status"], "error")
        self.assertIn("vertex=vs", r["reason"])

    def test_duplicate_std_is_an_error_naming_the_field(self):
        for line in ("x.metal std=3.1 std=\n", "x.metal std= std=3.1\n", "x.metal std=3.1 std=3.0\n"):
            r, = diffcheck.check_one(None, *listing(line)[0])
            self.assertEqual(r["status"], "error", line)
            self.assertIn("std=", r["reason"], line)
            self.assertIn("more than once", r["reason"], line)

    def test_error_takes_precedence_over_too_few_compared(self):
        with tempfile.TemporaryDirectory() as d:
            lst = os.path.join(d, "l.txt")
            with open(lst, "w") as f:
                f.write("x.metal bogus=1\n")
            self.assertEqual(diffcheck.main(["--corpus", lst, "--min-compared", "5"]), diffcheck.ERROR)

    def test_list_line_run_overrides_the_files_own_run_line(self):
        own = directives.parse("// RUN: width=8 height=8\nkernel void k() {}\n")["run"]
        listed = listing("a.metal width=16\n")[0][2]
        self.assertEqual(dict(own, **listed), {"width": 16, "height": 8})


class CorpusGraphicsDiscovery(unittest.TestCase):
    def _opts(self, corpus="list.txt"):
        opts = diffcheck.parser().parse_args(["--corpus", corpus] if corpus else ["s.metal"])
        opts.ranges, opts.metal, opts.vk = {}, "metalhost", "vkhost"
        return opts

    def _run_file(self, text, run_mslc, corpus="list.txt"):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "s.metal")
            with open(p, "w") as f:
                f.write(text)
            with mock.patch.object(hosts, "run_mslc", side_effect=run_mslc), \
                 mock.patch.object(graphics, "check_pair", return_value={"entry": "vs+fs", "status": "matched", "cases": [], "reason": None}) as cp, \
                 mock.patch.object(diffcheck, "check_entry", return_value={"entry": "k", "status": "matched", "cases": [], "reason": None}) as ce:
                res = diffcheck.check_file(self._opts(corpus), p)
            return res, cp, ce

    def test_unannotated_pair_discovers_graphics_with_inferred_position(self):
        stages = []
        refl = {"entry_points": [{"name": "vs", "stage": "vertex"}, {"name": "fs", "stage": "fragment"}]}
        res, cp, _ = self._run_file("vertex float4 vs() {}\nfragment float4 fs() {}\n",
                                    lambda m, s, w, i, stage="kernel": (stages.append(stage), ("spv", refl, None))[1])
        self.assertEqual(stages, [None])
        self.assertEqual(res[0]["status"], "matched")
        self.assertTrue(cp.call_args.args[4]["infer_position"])

        # Explicit vertex=vs fragment=fs in --corpus mode also infers position when unannotated.
        res, cp, _ = self._run_file("// RUN: vertex=vs fragment=fs\nvertex float4 vs() {}\nfragment float4 fs() {}\n",
                                    lambda *a: ("spv", refl, None))
        self.assertTrue(cp.call_args.args[4]["infer_position"])

    def test_multiple_or_unpaired_graphics_entry_points_skip(self):
        multi = {"entry_points": [{"name": "v1", "stage": "vertex"}, {"name": "v2", "stage": "vertex"}, {"name": "f1", "stage": "fragment"}]}
        res, cp, _ = self._run_file("vertex float4 v1() {}\n", lambda *a: ("spv", multi, None))
        self.assertEqual((res[0]["status"], cp.call_count), ("skipped", 0))
        self.assertIn("multiple vertex or fragment", res[0]["reason"])

        one = {"entry_points": [{"name": "v1", "stage": "vertex"}]}
        res, cp, _ = self._run_file("vertex float4 v1() {}\n", lambda *a: ("spv", one, None))
        self.assertEqual((res[0]["status"], res[0]["reason"]), ("skipped", "no matching fragment entry point"))

    def test_mixed_compute_and_graphics_file_and_kernel_fallback(self):
        mixed = {"entry_points": [{"name": "k", "stage": "compute"}, {"name": "vs", "stage": "vertex"}, {"name": "fs", "stage": "fragment"}]}
        stages = []
        res, cp, ce = self._run_file("kernel void k() {}\nvertex float4 vs() {}\n",
                                     lambda m, s, w, i, stage="kernel": (stages.append(stage), ("spv", mixed, None))[1])
        self.assertEqual(stages, [None, "kernel"])
        self.assertEqual(([r["entry"] for r in res], ce.call_count, cp.call_count), (["k", "vs+fs"], 1, 1))

        # Broken vertex stops combined compilation; compute entry still runs via --stage kernel.
        konly = {"entry_points": [{"name": "k", "stage": "compute"}]}
        res, cp, ce = self._run_file("kernel void k() {}\nvertex float4 vs() {}\n",
                                     lambda m, s, w, i, stage="kernel": (None, None, "bad vertex") if stage is None else ("spv", konly, None))
        self.assertEqual(([r["entry"] for r in res], ce.call_count, cp.call_count), (["k"], 1, 0))

    def test_single_file_mode_without_corpus_keeps_kernel_stage(self):
        stages = []
        self._run_file("kernel void k() {}\n", lambda m, s, w, i, stage="kernel": (stages.append(stage), (None, None, "err"))[1], corpus=None)
        self.assertEqual(stages, ["kernel"])


if __name__ == "__main__":
    unittest.main()
