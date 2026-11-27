#!/usr/bin/env python3
"""End-to-end test of tools/diffcheck/diffcheck.py: the real Apple compiler and GPU against
mslc's SPIR-V on lavapipe. Exits 77 (ctest SKIP) when either side is unavailable.

usage: diffcheck_driver_test.py <mslc> <metalhost> <vkhost>
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

mslc, metalhost, vkhost = sys.argv[1:4]
DRIVER = os.path.join(HERE, "..", "tools", "diffcheck", "diffcheck.py")


def kernel(name):
    return os.path.join(HERE, "diffcheck", name)


def run(name, *extra):
    report = tempfile.mktemp(suffix=".json")
    r = subprocess.run([sys.executable, DRIVER, kernel(name), "--mslc", mslc, "--metalhost", metalhost,
                        "--vkhost", vkhost, "--report", report, *extra], capture_output=True, text=True)
    data = json.load(open(report)) if os.path.exists(report) else None
    return r.returncode, r.stdout, data


rc, out, data = run("fma_kernel.metal")
if rc == 77:
    print(out)
    sys.exit(77)
assert rc == 0 and data["results"][0]["status"] == "matched", (rc, out)
assert all(c["negative_control"] for c in data["results"][0]["cases"])
assert [c["profile"] for c in data["results"][0]["cases"]] == ["moderate", "wide", "edge"]

rc, out, data = run("struct_kernel.metal", "--sentinel", "--poison-padding")
assert rc == 0, out
assert data["results"][0]["cases"][-1]["profile"] == "sentinel"

# A difference between the backends is reported with the buffer, byte offset and both values.
rc, out, data = run("fma_kernel.metal", "--corrupt-mslc", "o:8")
assert rc == 1 and data["results"][0]["status"] == "mismatched", (rc, out)
diff = data["results"][0]["cases"][0]["diffs"][0]
assert (diff["buffer"], diff["offset"]) == ("o", 8) and diff["apple_hex"] != diff["mslc_hex"], diff

# Zero output on both backends fails unless the file declares it.
rc, out, data = run("zero_kernel.metal")
assert rc == 1 and data["results"][0]["status"] == "zero-output", (rc, out)
rc, out, data = run("zero_declared_kernel.metal")
assert rc == 0, out

# A kernel with a texture is compared (kernel resources: diffcheck_kernel_driver_test.py).
rc, out, data = run("texture_kernel.metal")
assert rc == 0 and data["results"][0]["status"] == "matched", (rc, out)

# A kernel that does not exist is skipped, not matched.
rc, out, data = run("fma_kernel.metal", "--entry", "nope")
assert rc == 3, (rc, out)
# An index buffer drawn from its declared range is read in bounds on both sides.
rc, out, data = run("range_kernel.metal")
assert rc == 0 and data["results"][0]["status"] == "matched", (rc, out)

# --profiles restricts the cases' input profiles; an unknown one is refused.
rc, out, data = run("fma_kernel.metal", "--profiles", "moderate", "--cases", "2")
assert rc == 0 and [c["profile"] for c in data["results"][0]["cases"]] == ["moderate", "moderate"], (rc, out)
rc, out, data = run("fma_kernel.metal", "--profiles", "bogus")
assert rc == 2, (rc, out)

# Buffer indices out of parameter order.
rc, out, data = run("permuted_index_kernel.metal")
assert rc == 0 and data["results"][0]["status"] == "matched", (rc, out)

# An excluded buffer is not the one the negative control perturbs: the control runs on `o`.
rc, out, data = run("excluded_first_kernel.metal")
assert rc == 0 and data["results"][0]["excluded"].keys() == {"junk"}, (rc, out)
assert all(c["negative_control"] for c in data["results"][0]["cases"])

# The same range given on the command line, for a file one does not edit; a malformed one is refused.
rc, out, data = run("range_flag_kernel.metal", "--range", "idx=0:127")
assert rc == 0 and data["results"][0]["status"] == "matched", (rc, out)
rc, out, data = run("range_flag_kernel.metal", "--range", "idx")
assert rc == 2 and data is None, (rc, out)

# Corpus mode: a table with a summary; skips do not fail the run, a mismatch does.
with tempfile.TemporaryDirectory() as d:
    listing = os.path.join(d, "list.txt")
    open(listing, "w").write("# comment\n%s\n%s std=3.1\n%s\n%s\n%s\n%s\n" % (
        kernel("fma_kernel.metal"), kernel("struct_kernel.metal"), kernel("texture_write_kernel.metal"),
        kernel("texture_multisample_kernel.metal"), os.path.join(d, "missing.metal"), kernel("zero_declared_kernel.metal")))
    open(os.path.join(d, "missing.metal"), "w").write("kernel void k() { nope(); }\n")
    report = os.path.join(d, "r.json")
    base = [sys.executable, DRIVER, "--corpus", listing, "--mslc", mslc, "--metalhost", metalhost,
            "--vkhost", vkhost, "--report", report, "--jobs", "3"]
    r = subprocess.run(base, capture_output=True, text=True)
    assert r.returncode == 0, r
    assert "6 kernels: 4 compared, 4 matched, 2 skipped" in r.stdout, r.stdout
    reasons = {os.path.basename(x["file"]): x["reason"] for x in json.load(open(report))["results"] if x["status"] == "skipped"}
    assert "unsupported type other" in reasons["texture_multisample_kernel.metal"], reasons
    assert "nope" in reasons["missing.metal"] or "mslc" in reasons["missing.metal"], reasons
    open(listing, "a").write("%s\n" % kernel("zero_kernel.metal"))
    r = subprocess.run(base, capture_output=True, text=True)
    assert r.returncode == 1 and "1 zero-output" in r.stdout, r
# Zero sizes are refused; a directive naming no buffer is an error, not a silent no-op.
for flag in ("--grid", "--local", "--cases"):
    rc, out, data = run("fma_kernel.metal", flag, "0")
    assert rc == 2, (flag, rc, out)
rc, out, data = run("unknown_exclude_kernel.metal")
assert rc == 2 and "scratchh" in json.dumps(data), (rc, out)
rc, out, data = run("fma_kernel.metal", "--range", "nosuch=0:3")
assert rc == 2, (rc, out)

# An mslc that dies on a signal is an error naming the signal, not a skipped file.
with tempfile.TemporaryDirectory() as d:
    crasher = os.path.join(d, "mslc-crash")
    with open(crasher, "w") as f:
        f.write("#!/bin/sh\nkill -SEGV $$\n")
    os.chmod(crasher, 0o755)
    r = subprocess.run([sys.executable, DRIVER, kernel("fma_kernel.metal"), "--mslc", crasher, "--metalhost", metalhost,
                        "--vkhost", vkhost], capture_output=True, text=True)
    assert r.returncode == 2 and "SIGSEGV" in r.stdout and r.stdout.startswith("error"), r

with tempfile.TemporaryDirectory() as d:
    listing = os.path.join(d, "list.txt")
    open(os.path.join(d, "missing.metal"), "w").write("kernel void k() { nope(); }\n")
    base = [sys.executable, DRIVER, "--corpus", listing, "--mslc", mslc, "--metalhost", metalhost, "--vkhost", vkhost]
    # An empty list is an error, and --min-compared turns a regression to skips into a failure.
    open(listing, "w").write("# nothing\n")
    assert subprocess.run(base, capture_output=True, text=True).returncode == 2
    open(listing, "w").write("%s\n%s\n" % (kernel("fma_kernel.metal"), os.path.join(d, "missing.metal")))
    assert subprocess.run(base, capture_output=True, text=True).returncode == 0
    r = subprocess.run(base + ["--min-compared", "2"], capture_output=True, text=True)
    assert r.returncode == 1 and "--min-compared" in r.stderr, r

    # Corpus graphics pairs without // RUN: vid pairs match, unambiguous stage_in infers position,
    # ambiguous stage_in and multi-pair files skip cleanly.
    g_vid = os.path.join(d, "g_vid.metal")
    g_pos = os.path.join(d, "g_pos.metal")
    g_amb = os.path.join(d, "g_amb.metal")
    g_multi = os.path.join(d, "g_multi.metal")
    open(g_vid, "w").write("#include <metal_stdlib>\nusing namespace metal;\n"
                            "constant float2 P[3]={{-0.5,-0.5},{0.5,-0.5},{0.0,0.5}};\n"
                            "vertex float4 vs(uint vid [[vertex_id]]) { return float4(P[vid % 3], 0.5, 1.0); }\n"
                            "fragment float4 fs() { return float4(0.25, 0.5, 0.75, 1.0); }\n")
    open(g_pos, "w").write("#include <metal_stdlib>\nusing namespace metal;\nstruct V { float2 p [[attribute(0)]]; };\n"
                            "vertex float4 vs(V in [[stage_in]]) { return float4(in.p, 0.5, 1.0); }\n"
                            "fragment float4 fs() { return float4(0.5, 0.25, 0.75, 1.0); }\n")
    open(g_amb, "w").write("#include <metal_stdlib>\nusing namespace metal;\nstruct V { float2 a [[attribute(0)]]; float2 b [[attribute(1)]]; };\n"
                            "vertex float4 vs(V in [[stage_in]]) { return float4(in.a + in.b, 0.5, 1.0); }\n"
                            "fragment float4 fs() { return float4(1.0); }\n")
    open(g_multi, "w").write("#include <metal_stdlib>\nusing namespace metal;\n"
                              "vertex float4 v1() { return float4(0); }\nvertex float4 v2() { return float4(0); }\n"
                              "fragment float4 fs() { return float4(1); }\n")
    g_rep = os.path.join(d, "g.json")
    open(listing, "w").write("%s\n%s\n%s\n%s\n" % (g_vid, g_pos, g_amb, g_multi))
    r = subprocess.run(base + ["--report", g_rep, "--cases", "1"], capture_output=True, text=True)
    assert r.returncode == 0 and "4 kernels: 2 compared, 2 matched, 2 skipped" in r.stdout, (r.returncode, r.stdout, r.stderr)
    by_base = {os.path.basename(x["file"]): x for x in json.load(open(g_rep))["results"]}
    assert by_base["g_vid.metal"]["status"] == "matched" and by_base["g_pos.metal"]["status"] == "matched", by_base
    assert "DIFFCHECK-POSITION" in by_base["g_amb.metal"]["reason"]
    assert "multiple vertex or fragment" in by_base["g_multi.metal"]["reason"]
print("PASS")
