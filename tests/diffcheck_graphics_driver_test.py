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
    ("half_fwidth_special", "fragment", [["OpFAdd %half", "OpFSub %half"]]),
    ("vertex_texture_sampler", "vertex", [["Binding 2$", "Binding 9"], ["Binding 3$", "Binding 2"], ["Binding 9$", "Binding 3"]]),
    ("vertex_texture_sampler", "vertex", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]]),
    ("vertex_texture_sampler", "fragment", [["OpConstant %float 1.375", "OpConstant %float 0.375"]]),
    ("texture_nonfloat", "fragment", [["Binding 2$", "Binding 9"], ["Binding 3$", "Binding 2"], ["Binding 9$", "Binding 3"]]),
    ("texture_nonfloat", "fragment", [["Binding 4$", "Binding 9"], ["Binding 5$", "Binding 4"], ["Binding 9$", "Binding 5"]]),
    ("texture_nonfloat", "fragment", [["OpFAdd %v4half", "OpFSub %v4half"]]),
    ("embedded_sampler", "vertex", [["OpConstant %float 1.375", "OpConstant %float 0.375"]]),
    ("embedded_sampler", "fragment", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]]),
    ("embedded_sampler", "fragment", [["Binding 2$", "Binding 9"], ["Binding 3$", "Binding 2"], ["Binding 9$", "Binding 3"]]),
    ("texture_non2d", "vertex", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]]),
    ("texture_non2d", "vertex", [[r"(OpImageQuerySize(?:Lod)? %v2uint %\d+(?: %\S+)?\n\s+%\d+ = OpCompositeExtract %uint %\d+) 1$", r"\1 0"]]),
    ("texture_non2d", "fragment", [[r"(OpCompositeConstruct %v3uint %\S+) (%\S+) (%\S+)", r"\1 \3 \2"]]),
    ("texture_non2d", "fragment", [[r"(OpImageQuerySize(?:Lod)? %v3uint %\d+(?: %\S+)?\n\s+%\d+ = OpCompositeExtract %uint %\d+) 2$", r"\1 1"]]),
    ("writable_buffer", "vertex", [["OpISub %int", "OpIAdd %int"]]),
    ("writable_buffer", "vertex", [["OpConstant %float 0.25", "OpConstant %float 0.75"]]),
    ("writable_buffer", "fragment", [[r"(OpAccessChain %_ptr_PhysicalStorageBuffer_v4uint %\d+ %\w+ )%\d+(\n\s+%\d+ = OpLoad %v4uint %\d+\n\s+OpStore)", r"\1%uint_960\2"]]),
    ("vertex_attr_half", "vertex", [["Location 1$", "Location 9"], ["Location 2$", "Location 1"], ["Location 9$", "Location 2"]], "reflection-mismatch"),
    ("vertex_attr_half", "vertex", [[r"(OpVectorShuffle %v2half %\d+ %\d+) 2 3", r"\1 3 2"]]),
    ("vertex_attr_half", "vertex", [["OpFAdd %half", "OpFSub %half"]]),
    ("sparse_color_interp", "fragment", [[r"(OpDecorate %\d+ )Location 3(?=\n\s+OpDecorate %\d+ Location 6)", r"\1Location 2"]], "reflection-mismatch"),
    ("sparse_color_interp", "fragment", [["OpIMul %uint", "OpIAdd %uint"]]),
    ("sparse_color_interp", "vertex", [["OpFAdd %v2float", "OpFSub %v2float"]]),
    ("sparse_color_interp", "fragment", [[r"^\s*OpDecorate %\d+ NoPerspective\n", ""]]),
    ("indexed_draw", "vertex", [["BuiltIn VertexIndex", "BuiltIn SubgroupSize"],
                                ["BuiltIn InstanceIndex", "BuiltIn VertexIndex"],
                                ["BuiltIn SubgroupSize", "BuiltIn InstanceIndex"]]),
    ("indexed_draw", "vertex", [["OpBitwiseXor %uint", "OpBitwiseOr %uint"]]),
    ("indexed_draw", "vertex", [["OpConstant %uint 16", "OpConstant %uint 17"]]),
]

rc, out, data = run(probe("flat_colour"))
if rc == 77:
    print(out)
    sys.exit(77)

for name in ("flat_colour", "interpolated_varying", "integer_output", "texture_sample", "instancing", "depth_output",
             "multi_target", "mixed_components", "half_fwidth", "half_fwidth_special",
             "vertex_texture_sampler", "texture_nonfloat", "embedded_sampler", "texture_non2d",
             "writable_buffer", "vertex_attr_half", "sparse_color_interp", "indexed_draw"):
    rc, out, data = run(probe(name))
    assert rc == 0 and data["status"] == "matched", (name, rc, out)
    assert data["cases"] and all(c["negative_control"] for c in data["cases"]), (name, data)
rc, out, data = run(os.path.join(HERE, "half_fwidth_wide.metal"))
assert rc == 0 and data["status"] == "matched" and all(c["negative_control"] for c in data["cases"]), (rc, out)
rc, out, data = run(os.path.join(HERE, "half_fwidth_wide.metal"), mslc_cmd=MUTATE,
                    env={"MSLC_REAL": mslc, "MUT": json.dumps([["OpFAdd %half", "OpFSub %half"]]), "MUT_STAGE": "fragment"})
assert rc == 1 and data["status"] == "mismatched", (rc, out)

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
rc, out, data = run(fixture("gfx_writable_buffer"), "--instances", "1")
assert rc == 0 and data["status"] == "matched" and "log" in data["buffers"], (rc, out)

# Unsupported graphics textures/samplers and host refusals skip with the reason, never error or pass.
with tempfile.TemporaryDirectory() as d:
    cases = [
        ("write_tex.metal", "access Write",
         "// RUN: vertex=vs fragment=fs\n// DIFFCHECK-POSITION: pos\n#include <metal_stdlib>\nusing namespace metal;\n"
         "struct VIn { float4 pos [[attribute(0)]]; };\nvertex float4 vs(VIn in [[stage_in]]) { return in.pos; }\n"
         "fragment float4 fs(texture2d<float, access::write> t [[texture(0)]]) { t.write(float4(1), uint2(0)); return float4(1); }\n"),
        ("depth2d.metal", "unsupported type depth",
         "// RUN: vertex=vs fragment=fs\n// DIFFCHECK-POSITION: pos\n#include <metal_stdlib>\nusing namespace metal;\n"
         "struct VIn { float4 pos [[attribute(0)]]; };\nvertex float4 vs(VIn in [[stage_in]]) { return in.pos; }\n"
         "fragment float4 fs(depth2d<float> t [[texture(0)]]) { return float4(t.read(uint2(0))); }\n"),
        ("bad_esampler.metal", "differs from mag_filter",
         "// RUN: vertex=vs fragment=fs\n// DIFFCHECK-POSITION: pos\n#include <metal_stdlib>\nusing namespace metal;\n"
         "struct VIn { float4 pos [[attribute(0)]]; };\nvertex float4 vs(VIn in [[stage_in]]) { return in.pos; }\n"
         "fragment float4 fs(texture2d<float> t [[texture(0)]]) { constexpr sampler s(mag_filter::linear, min_filter::nearest); return t.sample(s, float2(0.5)); }\n"),
    ]
    for fname, needle, body in cases:
        p = os.path.join(d, fname)
        with open(p, "w") as f:
            f.write(body)
        rc, out, data = run(p)
        assert rc == 3 and data["status"] == "skipped" and needle in data["reason"], (fname, rc, out)

    stub = os.path.join(d, "refuse-vk")
    with open(stub, "w") as f:
        f.write("#!/bin/sh\necho 'vkhost: unsupported: no storage image feature' >&2\nexit 2\n")
    os.chmod(stub, 0o755)
    rc, out, data = run(probe("texture_nonfloat"), vk=stub)
    assert rc == 3 and data["status"] == "skipped" and "no storage image feature" in data["reason"], (rc, out)

# A host that crashes or hangs is an error naming the cause, not a skip or a pass.
with tempfile.TemporaryDirectory() as d:
    for name, body, expect in (("crash", "kill -SEGV $$", "exit -11"), ("hang", "exec sleep 60", "timed out")):
        stub = os.path.join(d, name)
        with open(stub, "w") as f:
            f.write("#!/bin/sh\n%s\n" % body)
        os.chmod(stub, 0o755)
        rc, out, data = run(probe("flat_colour"), "--timeout", "20" if name == "crash" else "3", vk=stub)
        assert rc == 2 and data["status"] == "error" and expect in data["reason"], (name, rc, out)

# Non-indexed draw with non-zero base_vertex and base_instance must match on both hosts.
with tempfile.TemporaryDirectory() as d:
    base_k = os.path.join(d, "base_nonindexed.metal")
    with open(base_k, "w") as f:
        f.write("// RUN: vertex=vs fragment=fs instances=2 base_vertex=2 base_instance=4 cases=1\n"
                "// DIFFCHECK-POSITION: pos\n"
                "// DIFFCHECK-GEOMETRY: 4,4 24,4 4,25\n"
                "#include <metal_stdlib>\nusing namespace metal;\n"
                "struct VIn { float2 pos [[attribute(0)]]; uint tag [[attribute(1)]]; };\n"
                "struct VOut { float4 pos [[position]]; uint4 val [[flat]]; };\n"
                "vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]], uint iid [[instance_id]]) {\n"
                "  VOut o; o.pos = float4(in.pos.x + 0.125f * float(iid - 4u), in.pos.y, 0.5f, 1.0f);\n"
                "  o.val = uint4(vid, iid, in.tag, 7u); return o;\n}\n"
                "fragment uint4 fs(VOut in [[stage_in]]) { return in.val; }\n")
    rc, out, data = run(base_k)
    assert rc == 0 and data["status"] == "matched", (rc, out, data)
print("PASS")
