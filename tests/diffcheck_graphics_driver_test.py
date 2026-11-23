#!/usr/bin/env python3
"""End-to-end test of diffcheck's graphics mode: Apple's GPU against mslc's SPIR-V on lavapipe.
Baselines must match; each planted miscompile (an edit to the SPIR-V of the real mslc output,
made by diffcheck_mutate_mslc.py) must be reported as a mismatch; the vacuity guards must fire.
Exits 77 (ctest SKIP) when either side is unavailable.

usage: diffcheck_graphics_driver_test.py <mslc> <metalhost> <vkhost>
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
mslc, metalhost, vkhost = sys.argv[1:4]
DRIVER = os.path.join(HERE, "..", "tools", "diffcheck", "diffcheck.py")
MUTATE = os.path.join(HERE, "diffcheck_mutate_mslc.py")


def run(path, *extra, mslc_cmd=None, vk=None, env=None):
    with tempfile.TemporaryDirectory() as d:
        report = os.path.join(d, "r.json")
        r = subprocess.run([sys.executable, DRIVER, path, "--mslc", mslc_cmd or mslc, "--metalhost", metalhost,
                            "--vkhost", vk or vkhost, "--report", report, *extra], capture_output=True, text=True,
                           env=dict(os.environ, **(env or {})))
        data = json.load(open(report))["results"][0] if os.path.exists(report) else None
    return r.returncode, r.stdout + r.stderr, data


def probe(name):
    return os.path.join(HERE, "diffcheck", "graphics", name + ".metal")


def fixture(name):
    return os.path.join(HERE, "diffcheck", name + ".metal")


# (probe, stage, regex edits): each is a change a real compiler bug could make.
MUTANTS = [
    ("flat_colour", "fragment", [["OpConstant %float 0.75", "OpConstant %float 0.625"]]),
    ("interpolated_varying", "vertex", [["OpFMul %v4float", "OpFAdd %v4float"]]),
    ("interpolated_varying", "fragment", [["OpFAdd %v4float", "OpFSub %v4float"]]),
    ("integer_output", "vertex", [["BuiltIn VertexIndex", "BuiltIn InstanceIndex"]]),
    ("texture_sample", "fragment", [["OpConstant %float 0.625", "OpConstant %float 0.875"]]),
    ("instancing", "vertex", [["BuiltIn InstanceIndex", "BuiltIn VertexIndex"]]),
    ("depth_output", "fragment", [["OpFMul %float", "OpFAdd %float"]]),
    # The last colour target is written by the source but moved to another location by the module:
    # it must not vanish from the comparison.
    ("multi_target", "fragment", [["Location 2", "Location 5"]], "reflection-mismatch"),
    ("multi_target", "fragment", [["OpConstant %uint 7", "OpConstant %uint 8"]]),
    # A constant output component sits next to interpolated ones: one and forty ULP must both show.
    ("mixed_components", "fragment", [["OpConstant %float 0.375", "OpConstant %float 0.37500003"]]),
    ("mixed_components", "fragment", [["OpConstant %float 0.375", "OpConstant %float 0.37500119"]]),
]

rc, out, data = run(probe("flat_colour"))
if rc == 77:
    print(out)
    sys.exit(77)

for name in ("flat_colour", "interpolated_varying", "integer_output", "texture_sample", "instancing", "depth_output",
             "multi_target", "mixed_components", "half_fwidth"):
    rc, out, data = run(probe(name))
    assert rc == 0 and data["status"] == "matched", (name, rc, out)
    assert data["cases"] and all(c["negative_control"] for c in data["cases"]), (name, data)

for name, stage, edits, *expect in MUTANTS:
    env = {"MSLC_REAL": mslc, "MUT": json.dumps(edits), "MUT_STAGE": stage}
    rc, out, data = run(probe(name), mslc_cmd=MUTATE, env=env)
    status = expect[0] if expect else "mismatched"
    assert rc == 1 and data["status"] == status, ("mutant not detected", name, stage, edits, rc, out)

# The hook that damages one byte of mslc's attachment is reported with the attachment and offset.
rc, out, data = run(probe("flat_colour"), "--corrupt-mslc", "color0:8")
assert rc == 1 and data["status"] == "mismatched", (rc, out)
diff = data["cases"][0]["diffs"][0]
assert (diff["buffer"], diff["offset"]) == ("color0", 8) and diff["apple_hex"] != diff["mslc_hex"], diff

# Vacuity guards: nothing drawn is zero-output; an excluded target's output is not counted.
rc, out, data = run(fixture("gfx_zero_output"))
assert rc == 1 and data["status"] == "zero-output", (rc, out)
rc, out, data = run(fixture("gfx_two_targets"))
assert rc == 0 and data["status"] == "matched", (rc, out)
rc, out, data = run(fixture("gfx_excluded_output"))
assert rc == 1 and data["status"] == "zero-output" and data["excluded"].keys() == {"color1"}, (rc, out)

# Refusals are errors or skips with the reason, never a pass.
rc, out, data = run(fixture("gfx_no_position"))
assert rc == 2 and "DIFFCHECK-POSITION" in data["reason"], (rc, out)
rc, out, data = run(fixture("gfx_clipped_geometry"))
assert rc == 2 and "strictly inside" in data["reason"], (rc, out)
rc, out, data = run(fixture("gfx_off_grid_geometry"))
assert rc == 2 and "subpixel grid" in data["reason"], (rc, out)
rc, out, data = run(fixture("gfx_writable_buffer"))
assert rc == 3 and "writable" in data["reason"], (rc, out)

# A host that crashes or hangs is an error naming the cause, not a skip or a pass.
with tempfile.TemporaryDirectory() as d:
    for name, body, expect in (("crash", "kill -SEGV $$", "exit -11"), ("hang", "exec sleep 60", "timed out")):
        stub = os.path.join(d, name)
        with open(stub, "w") as f:
            f.write("#!/bin/sh\n%s\n" % body)
        os.chmod(stub, 0o755)
        rc, out, data = run(probe("flat_colour"), "--timeout", "20" if name == "crash" else "3", vk=stub)
        assert rc == 2 and data["status"] == "error" and expect in data["reason"], (name, rc, out)
print("PASS")
