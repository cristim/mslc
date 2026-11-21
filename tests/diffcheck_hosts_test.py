#!/usr/bin/env python3
"""Tests for tools/diffcheck/hosts.py: how the Apple side is compiled. Exits 77 (ctest
SKIP) without a Metal device or `xcrun metal`.

usage: diffcheck_hosts_test.py <metalhost>
"""

import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "diffcheck"))
import hosts  # noqa: E402

metalhost = sys.argv[1]
# a * b + c with a * b not representable: fused it is -2^-46, rounded in between it is 0.
KERNEL = """#include <metal_stdlib>
using namespace metal;
kernel void k(device const float* a [[buffer(0)]], device float* o [[buffer(1)]]) { o[0] = a[0] * a[1] + a[2]; }
"""
A = 1.0 + 2.0 ** -23
INPUT = struct.pack("<3f", A, 1.0 - 2.0 ** -24, -1.0)


def apple_result(work, lines):
    open(os.path.join(work, "a.bin"), "wb").write(INPUT)
    open(os.path.join(work, "o.bin"), "wb").write(bytes(4))
    open(os.path.join(work, "m.txt"), "w").write("\n".join(
        ["mode run"] + lines + ["entry k", "local 1 1 1", "groups 1 1 1",
         "buffer 0 12 %s %s" % (os.path.join(work, "a.bin"), os.path.join(work, "a.out")),
         "buffer 1 4 %s %s" % (os.path.join(work, "o.bin"), os.path.join(work, "o.out"))]) + "\n")
    hosts.run_metal(metalhost, os.path.join(work, "m.txt"))
    return struct.unpack("<f", open(os.path.join(work, "o.out"), "rb").read())[0]


with tempfile.TemporaryDirectory() as work:
    src = os.path.join(work, "k.metal")
    open(src, "w").write(KERNEL)
    try:
        lines = hosts.apple_source_lines(src, work, [], None)
        assert lines == ["source " + src, "contract off"], lines
        # The default reference does not contract a * b + c, so it matches mslc's separate multiply and add.
        got = apple_result(work, lines)
    except hosts.Unavailable as e:
        print("SKIPPED: %s" % e)
        sys.exit(77)
    assert got == 0.0, got
    # Why the default exists: with contraction left on, Apple fuses it (the result is 2^-24).
    on = apple_result(work, hosts.apple_source_lines(src, work, [], None, contract=True))
    assert on != 0.0, "Apple no longer contracts in safe math mode: the contract knob can go"
    assert hosts.apple_source_lines(src, work, [], "3.1", contract=True) == [
        "source " + src, "contract on", "std 3.1"]

    # A quoted #include cannot be resolved by an in-memory source, so it takes the xcrun route,
    # which must not contract either.
    inc = os.path.join(work, "inc")
    os.makedirs(inc)
    open(os.path.join(inc, "h.h"), "w").write(KERNEL.split("using namespace metal;\n")[1])
    open(src, "w").write('#include <metal_stdlib>\nusing namespace metal;\n#include "h.h"\n')
    lines = hosts.apple_source_lines(src, work, [inc], None)
    assert lines[0].startswith("metallib "), lines
    assert apple_result(work, lines) == 0.0

    # __has_include("...") has the same problem and takes the same route.
    open(src, "w").write('#if __has_include("h.h")\nkernel void k(device float* o [[buffer(0)]]) { o[0] = 1.0f; }\n#endif\n')
    assert hosts.apple_source_lines(src, work, [inc], None)[0].startswith("metallib ")

    # Apple rejecting the source is a HostError carrying Apple's message.
    open(src, "w").write("kernel void k() { nope; }\n")
    try:
        hosts.apple_reflection(metalhost, hosts.apple_source_lines(src, work, [], None), "k", work)
    except hosts.HostError as e:
        assert str(e).startswith("apple: "), e
    else:
        raise AssertionError("expected HostError")
print("PASS")
