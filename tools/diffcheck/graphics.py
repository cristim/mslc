"""Graphics mode: a vertex and a fragment function drawn into offscreen targets on both
backends, the targets read back and compared per pixel.

One fixed draw: a triangle list (DEFAULT_GEOMETRY or // DIFFCHECK-GEOMETRY, in pixel
coordinates) with `instances` instances into float or integer colour targets chosen from the
fragment's output types, plus a depth target when the fragment writes depth or the file says
`// DIFFCHECK: depth`. Everything stays inside the target: lavapipe returns NaN for
noperspective varyings on a guard-band-clipped triangle, and Apple clips differently.

Vertex data: the field named by // DIFFCHECK-POSITION carries the geometry (w = 1); every other
stage_in field, [[buffer]] argument and texture is seeded like a kernel's buffers. One tightly
packed vertex buffer per attribute; the Metal vertex descriptor and the Vulkan one use the
same bytes. A runtime sampler is nearest, normalised, no mips (address mode by Metal index);
an embedded sampler is built on Vulkan from mslc's reflection. Textures are 4x4 2D RGBA
(float, half, int, uint, short or ushort; sample or access::read).

Both hosts get one manifest body (graphics_manifest); Apple's gets the source lines, Vulkan's
the SPIR-V. Attachments are compared with compare.compare as if they were buffers named
color0, color1, ... and depth.
"""

import os
import random
import struct

import checks
import compare
import hostenv
import hosts
import inputs
import outputs
import resources
import spirvio

# (host format name, scalar of the attachment) per fragment output scalar.
COLOR_SUFFIX = {"float": "32float", "int": "32sint", "uint": "32uint", "half": "16float"}
CHANNELS = {1: "r", 2: "rg", 4: "rgba"}  # a pixel format needs as many channels as the output has components
# Clear values no plausible shader output equals, so "a pixel changed" means "a pixel was drawn".
CLEAR = {"float": (-1.5, -2.5, -3.5, -4.5), "half": (-1.5, -2.5, -3.5, -4.5),
         "int": (-12345, 54321, -777, 31), "uint": (123456, 654321, 77777, 4242)}
DEPTH_CLEAR = 0.375
DEPTH_INDEX = 100
# Interpolated floats: barycentric interpolation is not specified to the bit. Absolute epsilon
# because a ULP bound is useless near zero; measured on the probes, then rounded up.
INTERP_ULP, INTERP_EPS = 32, 2e-5
DEPTH_EPS = 1e-6
SUBPIXEL = 256  # vertex positions must sit on this grid, in pixels
SLOT_TOP = 30  # attribute at location n is vertex buffer slot 30 - n, away from [[buffer(n)]] arguments
MAX_COLOR = 8
DEFAULT_SIZE, DEFAULT_ELEMS = 32, 64
# Pixel coordinates of 32x32. Three shapes; no edge passes through a pixel centre (checked by
# hand: the slopes 1/2, 13/18 and 3/26 never meet k + 0.5), so top-left fill rules cannot
# make the backends disagree about a covered pixel.
DEFAULT_GEOMETRY = [(2, 3, .5), (22, 3, .5), (22, 13, .5), (2, 3, .5), (22, 13, .5), (2, 13, .5),
                    (3, 16, .5), (21, 16, .5), (21, 29, .5), (3, 16, .5), (21, 29, .5), (3, 29, .5),
                    (24, 2, .5), (30, 2, .5), (27, 28, .5)]


class GraphicsError(Exception):
    """The file cannot be drawn as asked; reported as an error, not a skip."""


def ndc(point, width, height):
    x, y, z = point
    return 2 * x / width - 1, 1 - 2 * y / height, z


def check_geometry(points, width, height):
    if any(n < 2 or n & (n - 1) for n in (width, height)):
        raise GraphicsError("width and height must be powers of two, so every pixel coordinate converts to clip "
                            "space exactly (got %dx%d)" % (width, height))
    if not points or len(points) % 3:
        raise GraphicsError("DIFFCHECK-GEOMETRY needs a multiple of 3 points, got %d" % len(points))
    for x, y, z in points:
        if not (0 < x < width and 0 < y < height and 0 <= z <= 1):
            raise GraphicsError("geometry point %g,%g,%g is not strictly inside the %dx%d target "
                                "(clipped geometry differs between the backends)" % (x, y, z, width, height))
        if x * SUBPIXEL != int(x * SUBPIXEL) or y * SUBPIXEL != int(y * SUBPIXEL):
            raise GraphicsError("geometry point %g,%g is off the 1/%d subpixel grid: the backends snap it "
                                "differently and the interpolated values drift apart" % (x, y, SUBPIXEL))


def format_name(scalar, comps):
    return scalar if comps == 1 else "%s%d" % (scalar, comps)


def color_format(scalar, comps):
    return CHANNELS[comps] + COLOR_SUFFIX[scalar]


def attachment_layout(name, scalar, comps):
    size = compare.SCALARS[scalar][0]
    return {"name": name, "stride": comps * size, "access": "readwrite",
            "leaves": [{"off": i * size, "scalar": scalar} for i in range(comps)]}


def depth_layout():
    return {"name": "depth", "stride": 4, "access": "readwrite", "leaves": [{"off": 0, "scalar": "float"}]}


def clear_image(scalar, comps, pixels):
    fmt = "<f" if scalar == "float" else "<e" if scalar == "half" else "<i" if scalar == "int" else "<I"
    return bytearray(b"".join(struct.pack(fmt, v) for v in CLEAR[scalar][:comps]) * pixels)


def vertex_data(attributes, points, position, width, height, rng, profile):
    """{location: bytes}: one tightly packed array per attribute, `len(points)` vertices."""
    data = {}
    for location, a in sorted(attributes.items()):
        size = compare.SCALARS[a["scalar"]][0]
        out = bytearray()
        for p in points:
            if a["name"] == position:
                x, y, z = ndc(p, width, height)
                out += b"".join(struct.pack("<f", v) for v in (x, y, z, 1.0)[:a["comps"]])
            else:
                for _ in range(a["comps"]):
                    out += inputs.leaf_bits(rng, a["scalar"], profile).to_bytes(size, "little")
        data[location] = out
    return data


def split_leaves(layout, apple, primitives):
    """(exact, smooth) leaves of one attachment. A float component whose Apple values take more
    distinct values than there are primitives (plus the clear value) varies across a primitive,
    so it is interpolated and gets a tolerance; a constant, a flat varying or anything that
    takes at most one value per primitive is compared bit for bit. Decided from Apple's image,
    not from mslc's module, so a miscompile cannot loosen its own comparison."""
    exact, smooth = [], []
    for leaf in layout["leaves"]:
        scalar = leaf["scalar"]
        size = compare.SCALARS[scalar][0]
        if compare.SCALARS[scalar][1] != "float":
            exact.append(leaf)
            continue
        seen = {bytes(apple[at:at + size]) for at in range(leaf["off"], len(apple), layout["stride"])}
        (smooth if len(seen) > primitives + 1 else exact).append(leaf)
    return exact, smooth


def tolerance(source_ulp, interpolated, scalar):
    """(ulp, eps) for one attachment: exact for integers and flat floats."""
    if compare.SCALARS[scalar][1] != "float" or not interpolated:
        return source_ulp, 0.0
    return max(source_ulp, INTERP_ULP), INTERP_EPS


def pipeline_lines(width, height, vs, fs, attributes, colors, depth):
    """The shape both Apple calls need: enough to build the render pipeline, no data."""
    lines = ["vertex " + vs, "fragment " + fs, "target %d %d" % (width, height)]
    lines += ["attr %d %s" % (loc, format_name(a["scalar"], a["comps"])) for loc, a in sorted(attributes.items())]
    lines += ["color %d %s" % (i, color_format(s, c)) for i, (s, c) in sorted(colors.items())]
    return lines + (["depth"] if depth else [])


def draw_lines(shape, points, instances, attr_files, color_outs, colors, depth_out, buffers, textures, samplers, backend="vulkan"):
    """`shape` plus the data both hosts read: `attr_files` {location: file}, `color_outs` {index: file},
    `buffers` [(stage, metal index, set, member, bytes, file)], `textures` and `samplers` in
    resources.seed's shape."""
    lines = [l for l in shape if not l.startswith(("attr ", "color ", "depth"))]
    lines.append("draw %d %d" % (len(points), instances))
    for l in shape:
        if l.startswith("attr "):
            lines.append("%s %s" % (l, attr_files[int(l.split()[1])]))
        elif l.startswith("color "):
            i = int(l.split()[1])
            lines.append("%s %s %s" % (l, " ".join(repr(float(v)) if colors[i][0] in ("float", "half") else str(v) for v in CLEAR[colors[i][0]]), color_outs[i]))
    if depth_out:
        lines.append("depth %r %s" % (DEPTH_CLEAR, depth_out))
    lines += ["gbuffer %s %d %d %d %d %s" % b for b in buffers]
    return lines + resources.lines(textures, samplers, backend=backend)


def _skipped(res, reason):
    res.update(status="skipped", reason=reason)
    return res


def vertex_attribute_findings(attributes, apple):
    """Findings where mslc's stage_in attribute types (read from its SPIR-V) differ from Apple's
    MTLFunction.vertexAttributes, by location, scalar kind or component count. An attribute only
    one side has is a finding too."""
    mine = {loc: (a["scalar"], a["comps"]) for loc, a in attributes.items()}
    theirs = {a["index"]: (a["scalar"], a["comps"]) for a in apple}
    return ["vertex attribute %d: mslc has %s x%d, Apple has %s x%d" % (
        loc, *(mine.get(loc) or ("nothing", 0)), *(theirs.get(loc) or ("nothing", 0)))
        for loc in sorted(set(mine) | set(theirs)) if mine.get(loc) != theirs.get(loc)]


def _prepare(opts, source, vs, fs, params, work, includes):
    """Compile both stages and decide everything the draw needs. Returns (plan, None) or
    (None, result) when the file is skipped or erroneous."""
    res = {"entry": "%s+%s" % (vs, fs), "status": None, "cases": [], "reason": None}
    width, height = params["width"], params["height"]
    points = params["geometry"] or DEFAULT_GEOMETRY
    try:
        check_geometry(points, width, height)
    except GraphicsError as e:
        res.update(status="error", reason=str(e))
        return None, res
    compiled, problems = {}, []
    allowed = {"vertex": {"Buffer", "VertexInput", "Texture", "Sampler"}, "fragment": {"Buffer", "Texture", "Sampler"}}
    for stage, name in (("vertex", vs), ("fragment", fs)):
        spv, refl, error = hosts.run_mslc(opts.mslc, source, work, includes, stage)
        if error:
            res["status"], res["reason"] = hosts.mslc_failure(error)
            return None, res
        ep = next((e for e in refl["entry_points"] if e["name"] == name and e["stage"] == stage), None)
        if ep is None:
            return None, _skipped(res, "no %s function named %s" % (stage, name))
        try:
            iface = spirvio.interface(spirvio.disassemble(spv), name)
        except ValueError as e:
            res.update(status="error", reason=str(e))
            return None, res
        problems += iface["unsupported"]
        problems += ["%s binding kind %s" % (stage, b["kind"]) for b in ep["bindings"] if b["kind"] not in allowed[stage]]
        problems += ["texture %s access %s" % (b["name"], b.get("texture_access")) for b in ep["bindings"]
                     if b["kind"] == "Texture" and b.get("texture_access") not in ("Sample", "Read")]
        compiled[stage] = (spv, ep, iface)
    vif, fif = compiled["vertex"][2], compiled["fragment"][2]
    attributes = {}
    for b in compiled["vertex"][1]["bindings"]:
        if b["kind"] == "VertexInput" and b["location"] in vif["inputs"]:
            attributes[b["location"]] = dict(vif["inputs"][b["location"]], name=b["name"])
    problems += ["half scalar vertex attribute %s" % a["name"] for a in attributes.values() if a["scalar"] == "half" and a["comps"] == 1]
    try:
        colors, src_depth = outputs.fragment_outputs(hostenv.read_text(source), fs)
    except outputs.Unsupported as e:
        problems.append(str(e))
        colors, src_depth = {}, False
    depth = params["depth"] or src_depth
    problems += ["colour %d has 3 components, which no pixel format holds" % i for i, (_, c) in colors.items() if c == 3]
    if not colors and not depth:
        problems.append("the fragment function writes no colour or depth output")
    if problems:
        return None, _skipped(res, "unsupported: " + "; ".join(problems))
    findings = outputs.cross_check(colors, src_depth, fif)
    if findings:
        res.update(status="reflection-mismatch", reason="; ".join(findings))
        return None, res
    position = params["position"]
    names = {a["name"]: a for a in attributes.values()}
    if attributes and position is None:
        res.update(status="error", reason="the vertex function has stage_in attributes: name the one that carries the "
                   "position with // DIFFCHECK-POSITION: <name> (or none)")
        return None, res
    if position == "none":
        position = None
    elif position and not (position in names and names[position]["scalar"] == "float" and names[position]["comps"] >= 2):
        res.update(status="error", reason="DIFFCHECK-POSITION %s is not a float2 to float4 attribute (attributes: %s)"
                   % (position, ", ".join(sorted(names)) or "none"))
        return None, res
    if any(b["kind"] == "Buffer" and b["metal_index"] in {SLOT_TOP - l for l in attributes}
           for b in compiled["vertex"][1]["bindings"]):
        return None, _skipped(res, "a vertex buffer argument uses a slot the vertex descriptor needs")
    shape = pipeline_lines(width, height, vs, fs, attributes, colors, depth)
    try:
        lines = hosts.apple_source_lines(source, work, includes, params.get("std"), opts.apple_contract)
        apple = hosts.apple_gfx_reflection(opts.metal, lines, shape, work)
    except hosts.HostError as e:
        return None, _skipped(res, str(e))
    layouts = {}
    for stage in ("vertex", "fragment"):
        ep, a = compiled[stage][1], apple[stage]
        if a["unsupported"]:
            return None, _skipped(res, "unsupported %s argument: %s" % (stage, "; ".join(a["unsupported"])))
        reasons = resources.unsupported(ep, a)
        if reasons:
            return None, _skipped(res, "unsupported %s argument: %s" % (stage, "; ".join(reasons)))
        layouts[stage] = {l["index"]: l for l in a["buffers"]}
        if any(l["access"] != "read" for l in layouts[stage].values()):
            return None, _skipped(res, "a %s buffer is writable (invocation order would make it racy)" % stage)
        if any(l["stride"] == 0 or not l["leaves"] for l in layouts[stage].values()):
            return None, _skipped(res, "unsupported argument: buffer element has no scalar layout")
    findings = vertex_attribute_findings(attributes, apple["vertex_attributes"])
    for stage in ("vertex", "fragment"):
        ep, a = compiled[stage][1], apple[stage]
        want_set = 0 if stage == "vertex" else 1
        for b in ep["bindings"]:
            if b["kind"] in ("Buffer", "Texture", "Sampler") and b["descriptor"]["set"] != want_set:
                findings.append("%s: %s %s has descriptor set %d, expected %d" % (
                    stage, b["kind"].lower(), b["name"], b["descriptor"]["set"], want_set))
        bufs = [b for b in ep["bindings"] if b["kind"] == "Buffer"]
        findings += ["%s: %s" % (stage, f) for f in checks.cross_check(bufs, a)]
        findings += resources.cross_check(ep["bindings"], a, stage + ": ")
    if findings:
        res.update(status="reflection-mismatch", reason="; ".join(findings))
        return None, res
    return dict(res=res, compiled=compiled, attributes=attributes, position=position, colors=colors, depth=depth,
                points=points, shape=shape, lines=lines, layouts=layouts, apple=apple), None


def check_pair(opts, source, vs, fs, params, work, includes):
    """Compare one vertex + fragment pair; returns a result dict."""
    plan, early = _prepare(opts, source, vs, fs, params, work, includes)
    if early:
        return early
    res, width, height = plan["res"], params["width"], params["height"]
    pixels = width * height
    primitives = len(plan["points"]) // 3 * params["instances"]
    attach = {i: attachment_layout("color%d" % i, s, c) for i, (s, c) in plan["colors"].items()}
    if plan["depth"]:
        attach[DEPTH_INDEX] = depth_layout()
    excluded = dict(params["exclude"])
    compared = {i: l for i, l in attach.items() if l["name"] not in excluded}
    before = {i: clear_image(*plan["colors"][i], pixels) for i in plan["colors"]}
    if plan["depth"]:
        before[DEPTH_INDEX] = bytearray(struct.pack("<f", DEPTH_CLEAR) * pixels)
    ulp = compare.ulp_bound_for_source(hostenv.read_text(source))
    res["buffers"] = sorted([l["name"] for l in attach.values()] + [l["name"] for s in plan["layouts"].values() for l in s.values()])
    res["ulp_bound"] = ulp
    profiles = [opts.gfx_profiles[c % len(opts.gfx_profiles)] for c in range(params["cases"])]
    nonzero = False
    for case, profile in enumerate(profiles):
        rng = random.Random(params["seed"] * 1000 + case)
        base = os.path.join(work, "case%d-" % case)
        attr_files, buffers, textures, samplers = {}, [], [], []
        for loc, data in vertex_data(plan["attributes"], plan["points"], plan["position"], width, height, rng, profile).items():
            attr_files[loc] = base + "attr%d.bin" % loc
            hostenv.write_bytes(attr_files[loc], data)
        for stage in ("vertex", "fragment"):
            ep = plan["compiled"][stage][1]
            for b in sorted((b for b in ep["bindings"] if b["kind"] == "Buffer"), key=lambda b: b["member"]):
                layout = plan["layouts"][stage][b["metal_index"]]
                data = inputs.fill_buffer(layout, params.get("elems") or DEFAULT_ELEMS, rng, profile, False, False,
                                          params["range"].get(layout["name"]), case,
                                          params["float_range"].get(layout["name"]))
                path = base + "%s-buf%d.bin" % (stage, b["metal_index"])
                hostenv.write_bytes(path, data)
                buffers.append((stage, b["metal_index"], b["descriptor"]["set"], b["member"], len(data), path))
            stage_textures, stage_samplers = resources.seed(
                ep["bindings"], rng, profile, base + stage + "-", ep["embedded_samplers"], plan["apple"][stage])
            textures += stage_textures
            samplers += stage_samplers
        outs = {}
        for backend, runner in (("apple", hosts.run_metal), ("mslc", hosts.run_vulkan)):
            color_outs = {i: base + "%s-color%d.bin" % (backend, i) for i in plan["colors"]}
            depth_out = base + "%s-depth.bin" % backend if plan["depth"] else None
            body = draw_lines(plan["shape"], plan["points"], params["instances"], attr_files, color_outs, plan["colors"],
                              depth_out, buffers, textures, samplers, backend)
            if backend == "apple":
                text, binary = ["mode draw"] + plan["lines"] + body, opts.metal
            else:
                text = ["mode draw", "spv vertex " + plan["compiled"]["vertex"][0], "spv fragment " + plan["compiled"]["fragment"][0]] + body
                binary = opts.vk
            manifest = base + "%s.txt" % backend
            hostenv.write_text(manifest, "\n".join(text) + "\n")
            try:
                runner(binary, manifest)
            except hosts.Refused as refused:
                return _skipped(res, str(refused))
            outs[backend] = {i: bytearray(hostenv.read_bytes(color_outs[i])) for i in plan["colors"]}
            if depth_out:
                outs[backend][DEPTH_INDEX] = bytearray(hostenv.read_bytes(depth_out))
        if opts.corrupt_mslc:
            name, _, at = opts.corrupt_mslc.partition(":")
            for i, l in attach.items():
                if l["name"] == name:
                    outs["mslc"][i][int(at)] ^= 0xff
        diffs = []
        for i, layout in sorted(compared.items()):
            exact, smooth = split_leaves(layout, outs["apple"][i], primitives)
            bound = params["ulp"].get(layout["name"], ulp)
            for leaves, (u, e) in ((exact, (bound, 0.0)), (smooth, tolerance(bound, True, "float"))):
                if leaves:
                    sub = dict(layout, leaves=leaves)
                    diffs += [d.to_json() for d in compare.compare(sub, outs["apple"][i], outs["mslc"][i], u, eps=DEPTH_EPS if i == DEPTH_INDEX else e)]
        nonzero = nonzero or any(checks.written_nonzero(compared, before, outs[b]) for b in ("apple", "mslc"))
        control = checks.perturb(compared, outs["mslc"], outs["apple"])
        control_ok = False
        if control is not None:
            copy, name, _ = control
            index = next(i for i, l in attach.items() if l["name"] == name)
            control_ok = bool(compare.compare(attach[index], outs["apple"][index], copy[index], 0))
        res["cases"].append({"case": case, "profile": profile, "diffs": diffs[:16], "diff_count": len(diffs), "negative_control": control_ok})
    res["status"], res["reason"] = checks.verdict(res["cases"], nonzero, params["writes_zero"])
    if excluded:
        res["excluded"] = excluded
    return res
