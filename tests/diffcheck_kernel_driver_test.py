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


def run(path, *extra, mslc_cmd=None, vk=None, metal=None, env=None):
    with tempfile.TemporaryDirectory() as d:
        report = os.path.join(d, "r.json")
        r = subprocess.run([sys.executable, DRIVER, path, "--mslc", mslc_cmd or mslc, "--metalhost", metal or metalhost,
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


BASELINES = ("texture_read", "texture_sample_two", "texture_unused", "texture_write", "texture_read_write",
             "texture_nonfloat_read", "texture_nonfloat_write", "texture_non2d_read", "texture_non2d_write",
             "texture_cube_read_write", "sampler_modes", "atomic_device_ops", "atomic_buffer_only",
             "layout_unread_member", "ulp_per_buffer", "ulp_per_field", "threadgroup_argument", "texel_buffer_read", "texel_buffer_write",
             "texel_buffer_read_write", "texel_buffer_unused", "texel_buffer_r_read", "texel_buffer_rg_read",
             "texel_buffer_r_write", "texel_buffer_rg_write", "texel_buffer_narrow_read_write")
# (probe, edits, expected status): each is a change a real compiler bug could make.
MUTANTS = [
    # texture: the x and y of a texel fetch swapped
    ("texture_read", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # texture, write: the x and y of a write coordinate swapped
    ("texture_write", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # texture, write: two components of a written float4 swapped
    ("texture_write", [[r"(OpCompositeConstruct %v4float) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # texture, write: the two write-only textures change bindings
    ("texture_write", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # texture, read_write: the two read_write textures change bindings
    ("texture_read_write", [["Binding 0$", "Binding 9"], ["Binding 1$", "Binding 0"], ["Binding 9$", "Binding 1"]], "mismatched"),
    # texture, read_write: the x and y of the read_write coordinate swapped
    ("texture_read_write", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-float 2D texture, read: the x and y of a fetch swapped
    ("texture_nonfloat_read", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-float 2D texture, read: the int and uint 2D textures change bindings
    ("texture_nonfloat_read", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # non-float 2D texture, write: two components of a written int4 swapped
    ("texture_nonfloat_write", [[r"(OpCompositeConstruct %v4int) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-float 2D texture, write: the int and half write-only 2D textures change bindings
    ("texture_nonfloat_write", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # non-2D textures, read: 1D array coordinate and layer swapped
    ("texture_non2d_read", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-2D textures, read: 3D y and z coordinates swapped
    ("texture_non2d_read", [[r"(OpCompositeConstruct %v3uint %\d+) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-2D textures, read: 1D array get_array_size reads width (lane 0) instead of layer count (lane 1)
    ("texture_non2d_read", [[r"(OpImageQuerySize(?:Lod)? %v2uint %\d+(?: %\S+)?\n\s+%\d+ = OpCompositeExtract %uint %\d+) 1$", r"\1 0"]], "mismatched"),
    # non-2D textures, read: 2D array get_array_size and 3D get_depth read height (lane 1) instead of lane 2
    ("texture_non2d_read", [[r"(OpImageQuerySize(?:Lod)? %v3uint %\d+(?: %\S+)?\n\s+%\d+ = OpCompositeExtract %uint %\d+) 2$", r"\1 1"]], "mismatched"),
    # non-2D textures, write: 1D float write components x and y swapped
    ("texture_non2d_write", [[r"(OpCompositeConstruct %v4float) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-2D textures, write: 1D array / 2D array write coordinates swapped
    ("texture_non2d_write", [[r"(OpCompositeConstruct %v2uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-2D textures, write: 2D array uint write components x and y swapped
    ("texture_non2d_write", [[r"(OpCompositeConstruct %v4uint) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # non-2D textures, write: 3D read_write x and z coordinates swapped
    ("texture_non2d_write", [[r"(OpCompositeConstruct %v3uint) (%\d+) (%\d+) (%\d+)", r"\1 \4 \3 \2"]], "mismatched"),
    # cube/cube_array: cube array layer-face stride 5 instead of 6
    ("texture_cube_read_write", [[r"(OpIMul %uint %\d+) %uint_6\b", r"\1 %uint_5"]], "mismatched"),
    # cube/cube_array: half cube write components x and y swapped
    ("texture_cube_read_write", [[r"(OpCompositeConstruct %v4half) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # cube/cube_array: cube sample face 0 direction altered
    ("texture_cube_read_write", [[r"(OpCompositeConstruct %v3float) %float_1 (%\d+)", r"\1 \2 \2"]], "mismatched"),
    # texture: the two textures change bindings
    ("texture_sample_two", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # texture: the swapped coordinates of the second sample are no longer swapped
    ("texture_sample_two", [[r"(OpVectorShuffle %v2float %\d+ %\d+) 1 0", r"\1 0 1"]], "mismatched"),
    # sampler: two samplers (clamp and repeat) swap bindings
    ("sampler_modes", [["Binding 2$", "Binding 9"], ["Binding 3$", "Binding 2"], ["Binding 9$", "Binding 3"]], "mismatched"),
    # layout: a member the kernel never reads moves; no output changes, only the layout does
    ("layout_unread_member", [[r"(OpMemberDecorate %\w+ 1 Offset) 4", r"\1 12"]], "reflection-mismatch"),
    # layout: two unread members exchange offsets (the set of offsets is unchanged)
    ("layout_unread_member", [[r"(OpMemberDecorate %\w+ 1 Offset) 4$", r"\1 99"], [r"(OpMemberDecorate %\w+ 2 Offset) 8$", r"\1 4"],
                              [r"(OpMemberDecorate %\w+ 1 Offset) 99$", r"\1 8"]], "reflection-mismatch"),
    # layout: an unread square matrix is row major, with the same offsets
    ("layout_unread_member", [[r"ColMajor$", "RowMajor"]], "reflection-mismatch"),
    # layout: the element stride of an array of vectors
    ("texture_read", [[r"OpDecorate (%\w+) ArrayStride 16", r"OpDecorate \1 ArrayStride 32"]], "reflection-mismatch"),
    # per-buffer ULP: an exactly rounded result off by one ULP in a file with an inexact builtin
    ("ulp_per_buffer", [[r"OpConstant %float 3$", "OpConstant %float 3.0000002"]], "mismatched"),
    # per-field struct ULP: an exactly rounded struct field off by one ULP next to sqrt and exp fields
    ("ulp_per_field", [[r"OpConstant %float 3$", "OpConstant %float 3.0000002"]], "mismatched"),
    # threadgroup argument: the spec constant that sizes the first argument's array is no longer the id the reflection names
    ("threadgroup_argument", [[r"SpecId 3$", "SpecId 9"]], "mismatched"),
    # threadgroup argument: a thread reads the wrong neighbour's slot
    ("threadgroup_argument", [[r"(OpIAdd %uint %\d+) %uint_1$", r"\1 %uint_3"]], "mismatched"),
    # threadgroup argument: the memory is per invocation (Private) and not shared by the group
    ("threadgroup_argument", [[r"(OpTypePointer) Workgroup", r"\1 Private"], [r"(OpVariable %\S+) Workgroup", r"\1 Private"]], "mismatched"),
    # texel buffer, read: the first fetch's texel index is off by one
    ("texel_buffer_read", [[r"OpBitcast %uint %int_1$", "OpBitcast %uint %int_2"]], "mismatched"),
    # texel buffer, read: the float and the half buffer change bindings
    ("texel_buffer_read", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # texel buffer, read: get_width is folded to the constant 256 (the texel counts differ per buffer and case)
    ("texel_buffer_read", [[r"OpImageQuerySize %uint %\d+$", "OpBitcast %uint %int_256"]], "mismatched"),
    # texel buffer, read: the width that sizes the indices is the one of the half buffer, not of the float one
    ("texel_buffer_read", [[r"(%149 = OpLoad %83 )%85$", r"\1%88"]], "mismatched"),
    # texel buffer, write: every write lands two texels away from where it should
    ("texel_buffer_write", [[r"OpBitcast %uint %int_5$", "OpBitcast %uint %int_7"]], "mismatched"),
    # texel buffer, write: two components of an int4 swapped
    ("texel_buffer_write", [[r"(OpCompositeConstruct %v4int) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # texel buffer, write: the int and the uint buffer change bindings
    ("texel_buffer_write", [["Binding 3$", "Binding 9"], ["Binding 4$", "Binding 3"], ["Binding 9$", "Binding 4"]], "mismatched"),
    # texel buffer, read_write: the two float buffers change bindings
    ("texel_buffer_read_write", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # texel buffer, read_write: the mirrored read of the read-only buffer is one texel lower
    ("texel_buffer_read_write", [[r"OpBitcast %uint %int_1$", "OpBitcast %uint %int_2"]], "mismatched"),
    # R texel buffer, read: the float fetch returns its first component twice (a read that fills the missing ones from x)
    ("texel_buffer_r_read", [[r"(OpImageFetch %v4float %\d+ %\d+\n\s+)OpStore (%\d+) (%\d+)( Aligned 16)",
                              r"\1%9001 = OpVectorShuffle %v4float \3 \3 0 0 2 3\n OpStore \2 %9001\4"]], "mismatched"),
    # RG texel buffer, read: the uint fetch returns its second component in the third place as well
    ("texel_buffer_rg_read", [[r"(OpImageFetch %v4uint %\d+ %\d+\n\s+)OpStore (%\d+) (%\d+)( Aligned 16)",
                               r"\1%9001 = OpVectorShuffle %v4uint \3 \3 0 1 1 3\n OpStore \2 %9001\4"]], "mismatched"),
    # R texel buffer, write: the float4 stored into the float buffer has its x and y swapped, and only x is kept
    ("texel_buffer_r_write", [[r"(OpCompositeConstruct %v4float) (%\d+) (%\d+)", r"\1 \3 \2"]], "mismatched"),
    # RG texel buffer, write: the uint4 stored has its y and z swapped, and x and y are kept
    ("texel_buffer_rg_write", [[r"(OpCompositeConstruct %v4uint %\d+) (%\d+) (%\d+) (%\d+)", r"\1 \3 \2 \4"]], "mismatched"),
    # R and RG texel buffers, read_write: the R float and the RG float buffer change bindings
    ("texel_buffer_narrow_read_write", [["Binding 1$", "Binding 9"], ["Binding 2$", "Binding 1"], ["Binding 9$", "Binding 2"]], "mismatched"),
    # atomics: unsigned atomic fetch_sub replaced by fetch_add
    ("atomic_device_ops", [[r"OpAtomicISub %uint", "OpAtomicIAdd %uint"]], "mismatched"),
    # atomics: unsigned min/max confused with signed min/max
    ("atomic_device_ops", [[r"OpAtomicUMin %uint", "OpAtomicSMin %uint"]], "mismatched"),
    ("atomic_device_ops", [[r"OpAtomicUMax %uint", "OpAtomicSMax %uint"]], "mismatched"),
    # atomics: signed min/max confused with unsigned min/max
    ("atomic_device_ops", [[r"OpAtomicSMin %int", "OpAtomicUMin %int"]], "mismatched"),
    ("atomic_device_ops", [[r"OpAtomicSMax %int", "OpAtomicUMax %int"]], "mismatched"),
    # atomics: exchange replaced by fetch_add, xor by or, CAS equality flipped
    ("atomic_device_ops", [[r"OpAtomicExchange %uint", "OpAtomicIAdd %uint"]], "mismatched"),
    ("atomic_device_ops", [[r"OpAtomicXor %uint", "OpAtomicOr %uint"]], "mismatched"),
    ("atomic_device_ops", [[r"(OpAtomicCompareExchange %uint [^\n]+\n\s+OpStore %\d+ %\d+\n\s+%\d+ =) OpIEqual (%bool)", r"\1 OpINotEqual \2"]], "mismatched"),
    # atomic-only buffers: layout stride mismatch and wrong atomic op
    ("atomic_buffer_only", [[r"OpDecorate (%_runtimearr_uint\S*) ArrayStride 4", r"OpDecorate \1 ArrayStride 8"]], "reflection-mismatch"),
    ("atomic_buffer_only", [[r"OpAtomicIAdd %uint", "OpAtomicISub %uint"]], "mismatched"),
]

# Equivalent mutant: components past the texel format are not stored, so swapping z and w of a write to an RG buffer
# changes no byte on either side, and the harness must not call it a mismatch (else the format is not what is compared).
# Likewise y and z of a write to an R buffer: if the R directive silently fell back to RGBA, this would change bytes.
EQUIVALENT = [("texel_buffer_rg_write", [[r"(OpCompositeConstruct %v4uint %\d+ %\d+) (%\d+) (%\d+)", r"\1 \3 \2"]]),
              ("texel_buffer_r_write", [[r"(OpCompositeConstruct %v4float %\d+) (%\d+) (%\d+)", r"\1 \3 \2"]])]

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

for name, edits in EQUIVALENT:
    rc, out, data = mutated(probe(name), edits)
    assert rc == 0 and data["status"] == "matched", ("equivalent mutant reported", name, rc, out)

# DIFFCHECK-TEXEL on a name that is no texture_buffer is a typo, not a silent no-op.
with tempfile.TemporaryDirectory() as d:
    typo = os.path.join(d, "texel_typo.metal")
    with open(probe("texel_buffer_r_read")) as src, open(typo, "w") as dst:
        dst.write(src.read().replace("TEXEL: f 1", "TEXEL: ff 1"))
    rc, out, data = run(typo)
    assert rc == 2 and "ff" in out, (rc, out)

# Without the directive the per-buffer bound still holds the exactly rounded buffer to 0 ULP
# (ulp_bounds_by_root), so the one ULP error in it is caught; the loose buffer keeps its 4 ULP bound.
with tempfile.TemporaryDirectory() as d:
    loose = os.path.join(d, "ulp_loose.metal")
    with open(probe("ulp_per_buffer")) as src, open(loose, "w") as dst:
        dst.write("".join(l for l in src if "DIFFCHECK-ULP" not in l))
    rc, out, data = mutated(loose, next(edits for name, edits, _ in MUTANTS if name == "ulp_per_buffer"))
    assert rc != 0 and data is not None and data["status"] == "mismatched" and "exact" in json.dumps(data), (
        "the per-buffer bound no longer holds the exact buffer", rc, out)

# A texture the output does not depend on fails unless declared; a multisampled texture and an unknown name are refused.
rc, out, data = run(fixture("texture_unused_undeclared"))
assert rc == 1 and data["status"] == "unused-texture" and "ignored" in data["reason"], (rc, out)
rc, out, data = run(fixture("texture_unused_read_write"))
assert rc == 1 and data["status"] == "unused-texture" and "w" in data["reason"], (rc, out)
rc, out, data = run(fixture("texture_stale_unused"))
assert rc == 2 and data["status"] == "error" and "used" in data["reason"], (rc, out)
rc, out, data = run(fixture("texture_write_kernel"))
assert rc == 0 and data["status"] == "matched", (rc, out)
rc, out, data = run(fixture("texture_3d_kernel"))
assert rc == 0 and data["status"] == "matched", (rc, out)
rc, out, data = run(fixture("texture_multisample_kernel"))
assert rc == 3 and "unsupported type other" in data["reason"], (rc, out)
rc, out, data = run(fixture("texel_buffer_unused_undeclared"))
assert rc == 1 and data["status"] == "unused-texture" and "ignored" in data["reason"], (rc, out)
rc, out, data = run(fixture("texel_buffer_unused_read_write"))
assert rc == 1 and data["status"] == "unused-texture" and "w" in data["reason"], (rc, out)
# A read-only texel buffer swapped for a read_write one has a different descriptor type: the harness must not
# call that a match or a mismatch (lavapipe currently crashes on it, which is an error).
rc, out, data = mutated(probe("texel_buffer_read_write"), [["Binding 1$", "Binding 9"], ["Binding 5$", "Binding 1"], ["Binding 9$", "Binding 5"]])
assert rc == 2 and data["status"] == "error", (rc, out)
rc, out, data = run(fixture("texture_kernel"))
assert rc == 0 and data["status"] == "matched", (rc, out)
rc, out, data = run(probe("atomic_buffer_only"), "--sentinel")
assert rc == 0 and data["status"] == "matched" and data["cases"][-1]["profile"] == "sentinel", (rc, out)
with tempfile.TemporaryDirectory() as d:
    typo = os.path.join(d, "typo.metal")
    with open(probe("texture_unused")) as src, open(typo, "w") as dst:
        dst.write(src.read().replace("UNUSED: ignored", "UNUSED: ignord"))
    rc, out, data = run(typo)
    assert rc == 2 and "ignord" in out, (rc, out)

# A depth texture reflects as depth (not 2d/2d_array/cube) and is skipped; >8 textures or samplers on vkhost skips cleanly.
with tempfile.TemporaryDirectory() as d:
    depth_k = os.path.join(d, "depth.metal")
    with open(depth_k, "w") as f:
        f.write("#include <metal_stdlib>\nusing namespace metal;\n"
                "kernel void k(depth2d<float> d [[texture(0)]], sampler s [[sampler(0)]],"
                " device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) { o[i] = d.sample(s, float2(0.5f)); }\n")
    rc, out, data = run(depth_k)
    assert rc == 3 and "unsupported type depth" in data["reason"], (rc, out, data)
    many_k = os.path.join(d, "many_tex.metal")
    params = ", ".join(f"texture2d<float, access::read> t{n} [[texture({n})]]" for n in range(9))
    reads = " + ".join(f"t{n}.read(uint2(0))" for n in range(9))
    with open(many_k, "w") as f:
        f.write(f"#include <metal_stdlib>\nusing namespace metal;\n"
                f"kernel void k({params}, device float4 *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {{ o[i] = {reads}; }}\n")
    rc, out, data = run(many_k)
    assert rc == 3 and "too many textures" in data["reason"], (rc, out, data)
    many_s = os.path.join(d, "many_smp.metal")
    sparams = ", ".join(f"sampler s{n} [[sampler({n})]]" for n in range(9))
    sreads = " + ".join(f"t.sample(s{n}, float2(0.5f))" for n in range(9))
    with open(many_s, "w") as f:
        f.write(f"#include <metal_stdlib>\nusing namespace metal;\n"
                f"kernel void k(texture2d<float, access::sample> t [[texture(0)]], {sparams},"
                f" device float4 *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {{ o[i] = {sreads}; }}\n")
    rc, out, data = run(many_s)
    assert rc == 3 and "too many samplers" in data["reason"], (rc, out, data)

# A host that says it cannot make the texel buffer (a format or feature the device lacks) is a skip with that reason,
# never a pass.
with tempfile.TemporaryDirectory() as d:
    for side, host in (("lavapipe", "vk"), ("Apple", "metal")):
        stub = os.path.join(d, side)
        with open(stub, "w") as f:
            f.write("#!/bin/sh\necho 'x: unsupported: no such format' >&2\nexit 2\n".replace("x:", "vkhost:" if host == "vk" else "metalhost:"))
        os.chmod(stub, 0o755)
        rc, out, data = run(probe("texel_buffer_read"), **{host: stub})
        assert rc == 3 and data["status"] == "skipped" and ("unsupported: %s: no such format" % side) in data["reason"], (side, rc, out)

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
