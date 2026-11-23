#!/usr/bin/env python3
"""Unit tests for corpus list parsing in tools/diffcheck/diffcheck.py. No GPU needed."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import diffcheck  # noqa: E402
import directives  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
