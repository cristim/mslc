#!/usr/bin/env python3
"""Checks tools/diffcheck's Metal host against a host computation and against the
layout Apple's compiler must report for tests/diffcheck/struct_kernel.metal.
Exits 77 (ctest SKIP) when there is no Metal device.

usage: diffcheck_metalhost_test.py <metalhost> <fma_kernel.metal> <struct_kernel.metal>
"""

import json
import os
import struct
import subprocess
import sys
import tempfile

host, fma, structs = sys.argv[1:4]
N = 64

with tempfile.TemporaryDirectory() as d:
    def path(name):
        return os.path.join(d, name)

    def run(manifest):
        open(path("m.txt"), "w").write(manifest)
        return subprocess.run([host, path("m.txt")], capture_output=True, text=True)

    probe = run("mode reflect\nsource %s\nentry k\nreflection %s\n" % (fma, path("r.json")))
    if probe.returncode != 0 and "no Metal device" in probe.stderr:
        print("SKIPPED: no Metal device")
        sys.exit(77)
    assert probe.returncode == 0, probe

    # Layout of a struct holding float3, uint, bool, half and float2[2]: float3 packs
    # three floats and the next member starts at 16, not 12.
    assert run("mode reflect\nsource %s\nentry k\nreflection %s\n" % (structs, path("s.json"))).returncode == 0
    refl = json.load(open(path("s.json")))
    by_name = {b["name"]: b for b in refl["buffers"]}
    got = [(l["off"], l["scalar"]) for l in by_name["in"]["leaves"]]
    want = [(0, "float"), (4, "float"), (8, "float"), (16, "uint"), (20, "bool"), (22, "half"),
            (24, "float"), (28, "float"), (32, "float"), (36, "float")]
    assert got == want, got
    assert [l["field"] for l in by_name["in"]["leaves"]] == ["a", "a", "a", "n", "flag", "h", "p", "p", "p", "p"]
    assert by_name["in"]["stride"] == 48 and by_name["in"]["access"] == "read"
    assert by_name["out"]["access"] == "readwrite"

    # Execution: out[i] = a[i] * b[i] + 1 with the output pre-filled with a sentinel.
    a = [i * 0.5 for i in range(N)]
    open(path("a.bin"), "wb").write(struct.pack("<%df" % N, *a))
    open(path("b.bin"), "wb").write(struct.pack("<%df" % N, *([3.0] * N)))
    open(path("o.in"), "wb").write(bytes([0x7f]) * (N * 4))
    r = run("mode run\nsource %s\nentry k\nlocal 8 1 1\ngroups 8 1 1\n"
            "buffer 0 %d %s %s\nbuffer 1 %d %s %s\nbuffer 2 %d %s %s\n"
            % (fma, N * 4, path("a.bin"), path("a.out"), N * 4, path("b.bin"), path("b.out"),
               N * 4, path("o.in"), path("o.out")))
    assert r.returncode == 0, r
    out = struct.unpack("<%df" % N, open(path("o.out"), "rb").read())
    want = tuple(x * 3.0 + 1.0 for x in a)
    assert out == want, (out[:8], want[:8])
    assert out != tuple(w + 1 for w in want)  # negative control

    # A source Apple rejects is an error with Apple's message, not a silent success.
    open(path("bad.metal"), "w").write("kernel void k(device float* o [[buffer(0)]]) { o[0] = nope; }\n")
    bad = run("mode reflect\nsource %s\nentry k\n" % path("bad.metal"))
    assert bad.returncode == 2 and "Apple compile failed" in bad.stderr, bad
print("PASS")
