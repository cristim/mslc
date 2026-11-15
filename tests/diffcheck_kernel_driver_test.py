#!/usr/bin/env python3
"""End-to-end test of diffcheck's kernel resources (textures, samplers), the SPIR-V layout
cross-check and per-buffer ULP bounds: Apple's GPU against mslc's SPIR-V on lavapipe. Baselines
must match; each planted miscompile (an edit to the SPIR-V of the real mslc output, made by
diffcheck_mutate_mslc.py) must be reported; the vacuity guards must fire. Exits 77 (ctest SKIP)
when either side is unavailable.

usage: diffcheck_kernel_driver_test.py <mslc> <metalhost> <vkhost>
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
    return os.path.join(HERE, "diffcheck", "kernel", name + ".metal")


def fixture(name):
    return os.path.join(HERE, "diffcheck", name + ".metal")


def mutated(path, edits):
    env = {"MSLC_REAL": mslc, "MUT": json.dumps(edits)}
    return run(path, mslc_cmd=MUTATE, env=env)


BASELINES = ("texture_read", "texture_sample_two", "texture_unused", "layout_unread_member")
# (probe, edits, expected status): each is a change a real compiler bug could make.
MUTANTS = [
    # texture: the x and y of a texel fetch swapped
    ("texture_read", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # texture: the two textures change bindings
    ("texture_sample_two", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # texture: the swapped coordinates of the second sample are no longer swapped
    ("texture_sample_two", [[r"(OpVectorShuffle %v2float %\d+ %\d+) 1 0", r"\1 0 1"]], "mismatched"),
    # layout: a member the kernel never reads moves; no output changes, only the layout does
    ("layout_unread_member", [[r"(OpMemberDecorate %\w+ 1 Offset) 4", r"\1 12"]], "reflection-mismatch"),
    # layout: the element stride of an array of vectors
    ("texture_read", [[r"OpDecorate (%\w+) ArrayStride 16", r"OpDecorate \1 ArrayStride 32"]], "reflection-mismatch"),
]

rc, out, data = run(probe("texture_read"))
if rc == 77:
    print(out)
    sys.exit(77)

for name in BASELINES:
    rc, out, data = run(probe(name))
    assert rc == 0 and data["status"] == "matched", (name, rc, out)
    assert data["cases"] and all(c["negative_control"] for c in data["cases"]), (name, data)
assert run(probe("texture_unused"))[2]["unused"].keys() == {"ignored"}

for name, edits, status in MUTANTS:
    rc, out, data = mutated(probe(name), edits)
    assert rc == 1 and data["status"] == status, ("mutant not detected", name, edits, rc, out)

# A texture the output does not depend on fails unless declared; a written texture and an unknown name are refused.
rc, out, data = run(fixture("texture_unused_undeclared"))
assert rc == 1 and data["status"] == "unused-texture" and "ignored" in data["reason"], (rc, out)
rc, out, data = run(fixture("texture_write_kernel"))
assert rc == 3 and "access Write" in data["reason"], (rc, out)
rc, out, data = run(fixture("texture_kernel"))
assert rc == 0 and data["status"] == "matched", (rc, out)
with tempfile.TemporaryDirectory() as d:
    typo = os.path.join(d, "typo.metal")
    with open(probe("texture_unused")) as src, open(typo, "w") as dst:
        dst.write(src.read().replace("UNUSED: ignored", "UNUSED: ignord"))
    rc, out, data = run(typo)
    assert rc == 2 and "ignord" in out, (rc, out)

# A host that crashes or hangs on a kernel with textures is an error naming the cause, not a skip or a pass.
with tempfile.TemporaryDirectory() as d:
    for name, body, expect in (("crash", "kill -SEGV $$", "exit -11"), ("hang", "exec sleep 60", "timed out")):
        stub = os.path.join(d, name)
        with open(stub, "w") as f:
            f.write("#!/bin/sh\n%s\n" % body)
        os.chmod(stub, 0o755)
        rc, out, data = run(probe("texture_read"), "--timeout", "20" if name == "crash" else "3", vk=stub)
        assert rc == 2 and data["status"] == "error" and expect in data["reason"], (name, rc, out)
print("PASS")
