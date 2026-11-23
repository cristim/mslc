#!/usr/bin/env python3
"""Half fragment fwidth (#173) against values computed on the host, run on lavapipe.

mslc compiles tests/diffcheck/graphics/half_fwidth_special.metal, lavapipe draws it, and every
covered pixel of both half targets is compared with numpy float16 arithmetic: dx and dy over the pixel's
2x2 quad taken in float32, each narrowed to half, then |dx| + |dy| added in half. A NaN must be a NaN and an
infinity an infinity (an exact bit match otherwise). Planted miscompiles of the lowering (OpDPdx taken as OpDPdy, the final OpFAdd turned into an OpFSub, made by diffcheck_mutate_mslc.py) must be reported, so a
comparison that cannot fail is caught.

The expectation assumes lavapipe's fine derivative convention: dx is taken inside the pixel's own row
and dy inside its own column of the 2x2 quad. SPIR-V leaves coarse or fine unspecified, so this test
pins lavapipe's behaviour, not a rule of the language.

tests/half_fwidth_wide.metal has a |dx| that is not a half (1024 next to 2^-10), so narrowing each term
first gives a different result from the float32 sum narrowed once (1026 against 1025 at |dy| = 1.5). Apple's
GPU gives 1026 (measured with tools/diffcheck); mslc follows it. The test checks that the two policies differ
and that mslc gives the first.
Exits 77 (ctest SKIP) when lavapipe is not available.

usage: half_fwidth_expected_test.py <mslc> <vkhost>
"""

import json
import os
import struct
import subprocess
import sys
import tempfile

try:
    import numpy as np
except ImportError:
    print("SKIP: numpy is not installed")
    sys.exit(77)

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "diffcheck"))
import hostenv  # noqa: E402
import hosts  # noqa: E402

mslc, vkhost = sys.argv[1:3]
SOURCE = os.path.join(HERE, "diffcheck", "graphics", "half_fwidth_special.metal")
WIDE = os.path.join(HERE, "half_fwidth_wide.metal")
MUTATE = os.path.join(HERE, "diffcheck_mutate_mslc.py")
W = H = 32
RECT = [(2, 3), (22, 3), (22, 13), (2, 3), (22, 13), (2, 13)]
XS, YS = range(2, 22), range(3, 13)  # the pixels the rectangle covers
CLEAR = (-1.5, -2.5, -3.5, -4.5)
f32, f16 = np.float32, np.float16


def inputs(x, y):
    """The six half inputs of pixel (x, y), as the shader builds them."""
    with np.errstate(all="ignore"):
        rounded = f16(f32(x) * f32(81.3125) + f32(y) * f32(219.0625))
        tiny = f16((f32(x) * f32(3) + f32(y) * f32(5)) * f32(5.9604645e-08))
        lin = f16(f32(x) + f32(y))
        over = f16(f32(x & 1) * f32(40000) - f32(y & 1) * f32(40000))
        wide = f16(f32(1024) if x & 1 else f32(0.0009765625) + f32(y & 1) * f32(1.5))
    inf_in = f16(np.inf) if x == 6 else lin
    nan_in = f16(np.nan) if x == 10 else lin
    inf_inf = f16(np.inf) if x in (14, 15) else lin
    return [rounded, tiny, inf_in, nan_in, inf_inf, over, wide]


def expected(x, y, float_sum=False):
    """Seven half fwidth results as Apple computes them: |dx| and |dy| each narrowed to half, then added
    (a half add is the exact sum rounded once). `float_sum` adds in float32 and narrows once instead (the
    wrong policy, used to prove the test can tell)."""
    x0, y0 = x & ~1, y & ~1
    out = []
    with np.errstate(all="ignore"):
        for i in range(7):
            dx = f32(inputs(x | 1, y)[i]) - f32(inputs(x0, y)[i])
            dy = f32(inputs(x, y | 1)[i]) - f32(inputs(x, y0)[i])
            if not float_sum:
                dx, dy = f32(f16(dx)), f32(f16(dy))
            out.append(f16(abs(dx) + abs(dy)))
    return out


def draw(mslc_cmd, source=SOURCE):
    with tempfile.TemporaryDirectory() as d:
        spv = {}
        for stage in ("vertex", "fragment"):
            path, _, error = hosts.run_mslc(mslc_cmd, source, d, [], stage)
            assert path, "%s: %s" % (stage, error)
            spv[stage] = path
        verts = b"".join(struct.pack("<4f", 2 * x / W - 1, 1 - 2 * y / H, 0.5, 1.0) for x, y in RECT)
        open(os.path.join(d, "attr0.bin"), "wb").write(verts)
        outs = [os.path.join(d, "c0.bin"), os.path.join(d, "c1.bin")]
        clear = " ".join(repr(v) for v in CLEAR)
        manifest = os.path.join(d, "m.txt")
        open(manifest, "w").write("\n".join([
            "mode draw", "spv vertex " + spv["vertex"], "spv fragment " + spv["fragment"], "vertex vs", "fragment fs",
            "target %d %d" % (W, H), "draw %d 1" % len(RECT), "attr 0 float4 " + os.path.join(d, "attr0.bin"),
            "color 0 rgba16float %s %s" % (clear, outs[0])] + (
            ["color 1 rg16float %s %s" % (clear, outs[1])] if source == SOURCE else [])) + "\n")
        hosts.run_vulkan(vkhost, manifest)
        c0 = np.frombuffer(open(outs[0], "rb").read(), "<u2").reshape(H, W, 4)
        if source == WIDE:
            return c0, None
        return c0, np.frombuffer(open(outs[1], "rb").read(), "<u2").reshape(H, W, 2)


def mismatches(color0, color1, indices, float_sum=False):
    """Differences between the drawn targets and the host values `indices` of `expected`."""
    bad, nonfinite = [], 0
    for y in range(H):
        for x in range(W):
            got = (list(color0[y, x]) + (list(color1[y, x]) if color1 is not None else []))[:len(indices)]
            if x in XS and y in YS:
                want = [expected(x, y, float_sum)[k] for k in indices]
            else:
                want = [f16(v) for v in (CLEAR[:4] + CLEAR[:2])[:len(indices)]]
            for i, (g, w) in enumerate(zip(got, want)):
                gv = np.array([g], "<u2").view(f16)[0]
                nonfinite += x in XS and y in YS and not np.isfinite(w)
                same = (np.isnan(gv) and np.isnan(w)) or (np.isinf(gv) and gv == w) or g == np.array([w]).view("<u2")[0]
                if not same:
                    bad.append((x, y, i, float(gv), float(w)))
    return bad, nonfinite


def check(mslc_cmd, source=SOURCE, float_sum=False):
    indices = list(range(6)) if source == SOURCE else [6]
    return mismatches(*draw(mslc_cmd, source), indices=indices, float_sum=float_sum)


try:
    if hostenv.vulkan_env() is None:
        raise hosts.Unavailable("no lavapipe ICD")
    bad, nonfinite = check(mslc)
    assert not bad, ("baseline differs from the host value", bad[:8], len(bad))
    # Narrowing each term first is enforced: a |dx| that is not a half must give the Apple result, and the
    # wrong policy (float32 sum narrowed once) must be told apart from it.
    bad, _ = check(mslc, WIDE)
    assert not bad, ("wide case differs from the host value", bad[:8], len(bad))
    wrong, _ = check(mslc, WIDE, float_sum=True)
    assert wrong, "a float32 sum gives the same values as narrowing each term: the test cannot see the policy"
    # Vacuity: the NaN, infinity and overflow cases are really among the expected values.
    assert nonfinite > 0, "no non-finite expected value was compared"
    for replacement, edits in (("dx taken as dy", [[r"OpDPdx %float", "OpDPdy %float"]]), ("add becomes subtract", [[r"OpFAdd %half", "OpFSub %half"]])):
        os.environ.update(MSLC_REAL=mslc, MUT=json.dumps(edits), MUT_STAGE="fragment")
        for source in (SOURCE, WIDE):
            bad, _ = check(MUTATE, source)
            assert bad, ("mutant not detected", replacement, source)
except hosts.Unavailable as e:
    print("SKIP:", e)
    sys.exit(77)
print("PASS")
