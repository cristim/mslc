#!/usr/bin/env python3
"""Pins the known difference of #377: Apple flushes a denormal float to zero in a compare and in
float-to-bool, mslc (no DenormFlushToZero) does not. Runs tests/diffcheck/denormal_compare_flush.metal
through diffcheck and expects exactly that difference: Apple 0, mslc 1 in o_ne and o_bool for every
element, and no difference in o_bits (a plain copy keeps the denormal on both). Exits 77 (ctest
SKIP) when either side is unavailable.

usage: diffcheck_denormal_test.py <mslc> <metalhost> <vkhost>
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
mslc, metalhost, vkhost = sys.argv[1:4]
DRIVER = os.path.join(HERE, "..", "tools", "diffcheck", "diffcheck.py")
PROBE = os.path.join(HERE, "diffcheck", "denormal_compare_flush.metal")
N = 8

with tempfile.TemporaryDirectory() as d:
    report = os.path.join(d, "r.json")
    r = subprocess.run([sys.executable, DRIVER, PROBE, "--mslc", mslc, "--metalhost", metalhost, "--vkhost", vkhost,
                        "--grid", str(N), "--local", str(N), "--cases", "1", "--report", report],
                       capture_output=True, text=True)
    if r.returncode == 77:
        print(r.stdout)
        sys.exit(77)
    result = json.load(open(report))["results"][0]

assert result["status"] == "mismatched", (r.returncode, r.stdout)
diffs = result["cases"][0]["diffs"]
got = {(x["buffer"], x["element"]): (x["apple"], x["mslc"]) for x in diffs}
want = {(b, i): (0, 1) for b in ("o_ne", "o_bool") for i in range(N)}
assert got == want, (got, r.stdout)
print("PASS")
