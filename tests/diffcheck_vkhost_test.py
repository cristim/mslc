#!/usr/bin/env python3
"""Runs tests/diffcheck/fma_kernel.metal through mslc and vkhost on lavapipe and checks
the result against a host computation. Exits 77 (ctest SKIP) without lavapipe.

usage: diffcheck_vkhost_test.py <mslc> <vkhost> <fma_kernel.metal>
"""

import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import hostenv  # noqa: E402

mslc, vkhost, source = sys.argv[1:4]
env = hostenv.vulkan_env()
if env is None:
    print("SKIPPED: no lavapipe ICD found")
    sys.exit(hostenv.SKIP)

N = 64
with tempfile.TemporaryDirectory() as d:
    def path(name):
        return os.path.join(d, name)

    subprocess.run([mslc, "-o", path("k.spv"), source], check=True)
    a = [i * 0.5 for i in range(N)]
    b = [3.0] * N
    for name, vals in (("a.bin", a), ("b.bin", b)):
        open(path(name), "wb").write(struct.pack("<%df" % N, *vals))
    open(path("o.in"), "wb").write(bytes([0x7f]) * (N * 4))
    open(path("m.txt"), "w").write(
        "spv %s\nentry k\nset 0\nlocal 8 1 1\ngroups 8 1 1\n"
        "buffer 0 %d %s %s\nbuffer 1 %d %s %s\nbuffer 2 %d %s %s\n"
        % (path("k.spv"), N * 4, path("a.bin"), path("a.out"), N * 4, path("b.bin"), path("b.out"),
           N * 4, path("o.in"), path("o.out")))
    run = subprocess.run([vkhost, path("m.txt")], env=env, capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stdout, run.stderr)
        sys.exit(1)
    got = struct.unpack("<%df" % N, open(path("o.out"), "rb").read())
    want = tuple(x * y + 1.0 for x, y in zip(a, b))
    if got != want:
        print("FAIL: lavapipe output differs from the host computation")
        print(got[:8], want[:8])
        sys.exit(1)
    # Every input buffer came back untouched (read-only kernel arguments).
    assert open(path("a.out"), "rb").read() == open(path("a.bin"), "rb").read()
    # Negative control: the check above must be able to fail.
    assert got != tuple(w + 1 for w in want)

    # A manifest that lists buffers out of member order is refused, not misbound.
    open(path("bad.txt"), "w").write("spv %s\nentry k\nbuffer 1 %d %s %s\n"
                                     % (path("k.spv"), N * 4, path("a.bin"), path("x.out")))
    bad = subprocess.run([vkhost, path("bad.txt")], env=env, capture_output=True, text=True)
    assert bad.returncode == 2 and "members 0..n-1" in bad.stderr, bad
print("PASS")
