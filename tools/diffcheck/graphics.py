"""Graphics mode: a vertex and a fragment function drawn into offscreen targets on both
backends, the targets read back and compared per pixel.

One fixed draw: a triangle list (DEFAULT_GEOMETRY or // DIFFCHECK-GEOMETRY, in pixel
coordinates) with `instances` instances into float or integer colour targets chosen from the
fragment's output types, plus a depth target when the fragment writes depth or the file says
`// DIFFCHECK: depth`. Everything stays inside the target: lavapipe returns NaN for
noperspective varyings on a guard-band-clipped triangle, and Apple clips differently.

Vertex data: the field named by // DIFFCHECK-POSITION carries the geometry (w = 1); every other
stage_in field (including scalar and vector half), [[buffer]] argument and texture is seeded
like a kernel's buffers. One tightly packed vertex buffer per attribute; the Metal vertex
descriptor and the Vulkan one use the same bytes. A runtime sampler is nearest, normalised,
mip-nearest (address mode by Metal index); an embedded sampler is built on Vulkan from mslc's
reflection. Textures may be 1d, 1d_array, 2d, 2d_array, 3d, cube or cube_array RGBA (float,
half, int, uint, short or ushort; sample or access::read), with optional `// DIFFCHECK-MIPS`.

Both hosts get one manifest body (graphics_manifest); Apple's gets the source lines, Vulkan's
the SPIR-V. Attachments (color0, color1, ... and depth) and, at single instance, writable
[[buffer]] arguments are read back and compared with compare.compare.
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
BUFFER_INDEX = 200  # writable vertex/fragment buffers sit above attachment indices in compare maps
# Interpolated floats: barycentric interpolation is not specified to the bit. Absolute epsilon
# because a ULP bound is useless near zero; measured on the probes, then rounded up.
INTERP_ULP, INTERP_EPS = 32, 2e-5
DEPTH_EPS = 1e-6
SUBPIXEL = 256  # vertex positions must sit on this grid, in pixels
SLOT_TOP = 30  # attribute at location n is vertex buffer slot 30 - n, away from [[buffer(n)]] arguments
MAX_COLOR = 8
MAX_BASE_VERTEX = 65536
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


def check_geometry(points, width, height, indexes=None):
    if any(n < 2 or n & (n - 1) for n in (width, height)):
        raise GraphicsError("width and height must be powers of two, so every pixel coordinate converts to clip "
                            "space exactly (got %dx%d)" % (width, height))
    if indexes is None:
        if not points or len(points) % 3:
            raise GraphicsError("DIFFCHECK-GEOMETRY needs a multiple of 3 points, got %d" % len(points))
    else:
        if not points:
            raise GraphicsError("DIFFCHECK-GEOMETRY needs at least one vertex")
        if not indexes or len(indexes) % 3:
            raise GraphicsError("DIFFCHECK-INDEXES needs a multiple of 3 indices, got %d" % len(indexes))
        for idx in indexes:
            if not (0 <= idx < len(points)):
                raise GraphicsError("DIFFCHECK-INDEXES index %d is out of range for %d vertices" % (idx, len(points)))
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


def vertex_data(attributes, points, position, width, height, rng, profile, base_vertex=0):
    """{location: bytes}: one tightly packed array per attribute, `base_vertex + len(points)` vertices."""
    if base_vertex < 0:
        raise ValueError("base_vertex must be non-negative, got %d" % base_vertex)
    if base_vertex and not points:
        raise ValueError("base_vertex requires at least one vertex point")
    data = {}
    verts = ([points[0]] * base_vertex if base_vertex else []) + list(points)
    for location, a in sorted(attributes.items()):
        size = compare.SCALARS[a["scalar"]][0]
        out = bytearray()
        for p in verts:
            if a["name"] == position:
                x, y, z = ndc(p, width, height)
                out += b"".join(struct.pack("<f", v) for v in (x, y, z, 1.0)[:a["comps"]])
            else:
                for _ in range(a["comps"]):
                    out += inputs.leaf_bits(rng, a["scalar"], profile).to_bytes(size, "little")
        data[location] = out
    return data


def index_data(indexes):
    """Little-endian uint32 bytes for an index buffer."""
    return struct.pack("<%dI" % len(indexes), *indexes)


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


def draw_lines(shape, points, instances, attr_files, color_outs, colors, depth_out, buffers, textures, samplers, backend="vulkan",
               base_vertex=0, base_instance=0, index_file=None, index_count=0, input_attachments=()):
    """`shape` plus the data both hosts read: `attr_files` {location: file}, `color_outs` {index: file},
    `buffers` [(stage, metal index, set, member, bytes, file[, out_file])], `textures` and `samplers` in
    resources.seed's shape, `input_attachments` [(color_index, set, binding)]."""
    lines = [l for l in shape if not l.startswith(("attr ", "color ", "depth"))]
    draw = "draw %d %d" % (len(points), instances)
    if base_vertex or base_instance:
        draw += " %d %d" % (base_vertex, base_instance)
    lines.append(draw)
    if index_file:
        lines.append("index %d %s" % (index_count, index_file))
    for l in shape:
        if l.startswith("attr "):
            lines.append("%s %s" % (l, attr_files[int(l.split()[1])]))
        elif l.startswith("color "):
            i = int(l.split()[1])
            lines.append("%s %s %s" % (l, " ".join(repr(float(v)) if colors[i][0] in ("float", "half") else str(v) for v in CLEAR[colors[i][0]]), color_outs[i]))
    if depth_out:
        lines.append("depth %r %s" % (DEPTH_CLEAR, depth_out))
    lines += ["gbuffer " + " ".join(str(x) for x in b) for b in buffers]
    if backend != "apple":
        lines += ["inputattachment %d %d %d" % ia for ia in input_attachments]
    return lines + resources.lines(textures, samplers, backend=backend)


def _skipped(res, reason):
    res.update(status="skipped", reason=reason)
    return res


def _infer_position(attributes):
    """Unambiguous stage_in position attribute for an unannotated corpus file, else None."""
    floats = [a["name"] for a in attributes.values() if a["scalar"] == "float" and a["comps"] >= 2]
    if len(floats) == 1:
        return floats[0]
    named = [n for n in floats if n.lower() in ("pos", "position")]
    return named[0] if len(named) == 1 else None


def _wbuf_key(stage, index):
    return BUFFER_INDEX + (0 if stage == "vertex" else 100) + index


def _attr_match(mine, theirs):
    """mslc widens half/halfN stage_in inputs to float/floatN in SPIR-V; Apple feeds the 16-bit format."""
    return mine == theirs or (theirs is not None and theirs[0] == "half" and mine == ("float", theirs[1]))


def vertex_attribute_findings(attributes, apple):
    """Findings where mslc's stage_in attribute types (read from its SPIR-V) differ from Apple's
    MTLFunction.vertexAttributes, by location, scalar kind or component count. An attribute only
    one side has is a finding too."""
    mine = {loc: (a["scalar"], a["comps"]) for loc, a in attributes.items()}
    theirs = {a["index"]: (a["scalar"], a["comps"]) for a in apple}
    return ["vertex attribute %d: mslc has %s x%d, Apple has %s x%d" % (
        loc, *(mine.get(loc) or ("nothing", 0)), *(theirs.get(loc) or ("nothing", 0)))
        for loc in sorted(set(mine) | set(theirs)) if not _attr_match(mine.get(loc), theirs.get(loc))]


def _prepare(opts, source, vs, fs, params, work, includes):
    """Compile both stages and decide everything the draw needs. Returns (plan, None) or
    (None, result) when the file is skipped or erroneous."""
    res = {"entry": "%s+%s" % (vs, fs), "status": None, "cases": [], "reason": None}
    width, height = params["width"], params["height"]
    points = DEFAULT_GEOMETRY if params["geometry"] is None else params["geometry"]
    indexes = params.get("indexes")
    base_vertex, base_instance = params.get("base_vertex", 0), params.get("base_instance", 0)
    try:
        check_geometry(points, width, height, indexes)
        if base_vertex < 0 or base_vertex > MAX_BASE_VERTEX or base_instance < 0 or base_instance > 0xffffffff:
            raise GraphicsError("base_vertex %d (max %d) or base_instance %d out of range" % (base_vertex, MAX_BASE_VERTEX, base_instance))
    except GraphicsError as e:
        res.update(status="error", reason=str(e))
        return None, res
    compiled, problems = {}, []
    allowed = {"vertex": {"Buffer", "VertexInput", "StageOutput", "Texture", "Sampler"},
               "fragment": {"Buffer", "StageInput", "Texture", "Sampler", "InputAttachment"}}
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
    try:
        colors, src_depth = outputs.fragment_outputs(hostenv.read_text(source), fs)
    except outputs.Unsupported as e:
        problems.append(str(e))
        colors, src_depth = {}, False
    input_attachments = [b for b in compiled["fragment"][1]["bindings"] if b["kind"] == "InputAttachment"]
    for b in input_attachments:
        cidx = b["input_attachment_index"]
        if cidx not in colors:
            sc = b.get("scalar", "float")
            if sc not in ("float", "int", "uint", "half") or not (0 <= cidx < MAX_COLOR):
                problems.append("input attachment %s at colour %d has unsupported scalar %s" % (b["name"], cidx, sc))
            else:
                colors[cidx] = (sc, 4)
    if input_attachments and params["instances"] > 1:
        problems.append("a fragment function with framebuffer fetch has instances > 1 (overlapping primitives would be unordered)")
    depth = params["depth"] or src_depth
    problems += ["colour %d has 3 components, which no pixel format holds" % i for i, (_, c) in colors.items() if c == 3]
    if not colors and not depth:
        problems.append("the fragment function writes no colour or depth output")
    if problems:
        return None, _skipped(res, "unsupported: " + "; ".join(problems))
    src_colors, _ = outputs.fragment_outputs(hostenv.read_text(source), fs)
    findings = outputs.cross_check(src_colors, src_depth, fif)
    for b in input_attachments:
        cidx = b["input_attachment_index"]
        if b["metal_index"] != cidx:
            findings.append("fragment: input attachment %s has metal_index %d, expected %d" % (b["name"], b["metal_index"], cidx))
        if cidx in src_colors:
            want_sc = "float" if src_colors[cidx][0] == "half" else src_colors[cidx][0]
            if b.get("scalar") != want_sc:
                findings.append("fragment: input attachment %s at colour %d has scalar %s, output is %s" % (
                    b["name"], cidx, b.get("scalar"), src_colors[cidx][0]))
    if findings:
        res.update(status="reflection-mismatch", reason="; ".join(findings))
        return None, res
    position = params["position"]
    names = {a["name"]: a for a in attributes.values()}
    if attributes and position is None:
        if params.get("infer_position"):
            position = _infer_position(attributes)
            if position is None:
                return None, _skipped(res, "the vertex function has stage_in attributes without // DIFFCHECK-POSITION")
        else:
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
        reasons = resources.unsupported(ep, a, params.get("mips"))
        if reasons:
            return None, _skipped(res, "unsupported %s argument: %s" % (stage, "; ".join(reasons)))
        layouts[stage] = {l["index"]: l for l in a["buffers"]}
        if params["instances"] > 1 and any(l["access"] != "read" for l in layouts[stage].values()):
            return None, _skipped(res, "a %s buffer is writable with instances > 1 (invocation order would make it racy)" % stage)
        if any(l["stride"] == 0 or not l["leaves"] for l in layouts[stage].values()):
            return None, _skipped(res, "unsupported argument: buffer element has no scalar layout")
    findings = vertex_attribute_findings(attributes, apple["vertex_attributes"])
    for stage in ("vertex", "fragment"):
        ep, a = compiled[stage][1], apple[stage]
        want_set = 0 if stage == "vertex" else 1
        for b in ep["bindings"]:
            if b["kind"] in ("Buffer", "Texture", "Sampler", "InputAttachment") and b["descriptor"]["set"] != want_set:
                findings.append("%s: %s %s has descriptor set %d, expected %d" % (
                    stage, b["kind"].lower(), b["name"], b["descriptor"]["set"], want_set))
        bufs = [b for b in ep["bindings"] if b["kind"] == "Buffer"]
        findings += ["%s: %s" % (stage, f) for f in checks.cross_check(bufs, a)]
        findings += resources.cross_check(ep["bindings"], a, stage + ": ")
    if findings:
        res.update(status="reflection-mismatch", reason="; ".join(findings))
        return None, res
    for a in apple["vertex_attributes"]:
        if a["index"] in attributes and a["scalar"] == "half":
            if attributes[a["index"]]["name"] == position:
                if params.get("infer_position") and params["position"] is None:
                    return None, _skipped(res, "the vertex function has stage_in attributes without // DIFFCHECK-POSITION")
                res.update(status="error", reason="DIFFCHECK-POSITION %s is half, must be float2 to float4" % position)
                return None, res
            attributes[a["index"]]["scalar"] = "half"
    shape = pipeline_lines(width, height, vs, fs, attributes, colors, depth)
    return dict(res=res, compiled=compiled, attributes=attributes, position=position, colors=colors, depth=depth,
                points=points, indexes=indexes, shape=shape, lines=lines, layouts=layouts, apple=apple), None


def check_pair(opts, source, vs, fs, params, work, includes):
    """Compare one vertex + fragment pair; returns a result dict."""
    plan, early = _prepare(opts, source, vs, fs, params, work, includes)
    if early:
        return early
    res, width, height = plan["res"], params["width"], params["height"]
    pixels = width * height
    indexes = plan["indexes"]
    base_vertex, base_instance = params.get("base_vertex", 0), params.get("base_instance", 0)
    primitives = (len(indexes) if indexes else len(plan["points"])) // 3 * params["instances"]
    index_file = None
    if indexes:
        index_file = os.path.join(work, "indices.bin")
        hostenv.write_bytes(index_file, index_data(indexes))
    src_colors, _ = outputs.fragment_outputs(hostenv.read_text(source), fs)
    attach = {i: attachment_layout("color%d" % i, s, c) for i, (s, c) in src_colors.items()}
    if plan["depth"]:
        attach[DEPTH_INDEX] = depth_layout()
    writable = {_wbuf_key(stage, b["metal_index"]): plan["layouts"][stage][b["metal_index"]]
                for stage in ("vertex", "fragment")
                for b in sorted((x for x in plan["compiled"][stage][1]["bindings"] if x["kind"] == "Buffer"),
                                key=lambda x: x["member"])
                if plan["layouts"][stage][b["metal_index"]]["access"] != "read"}
    excluded = dict(params["exclude"])
    compared = {i: l for i, l in attach.items() if l["name"] not in excluded}
    wcompared = {i: l for i, l in writable.items() if l["name"] not in excluded}
    before = {i: clear_image(*plan["colors"][i], pixels) for i in plan["colors"]}
    if plan["depth"]:
        before[DEPTH_INDEX] = bytearray(struct.pack("<f", DEPTH_CLEAR) * pixels)
    ulp = compare.ulp_bound_for_source(hostenv.read_text(source))
    ulp_float = compare.ulp_bound_for_source(hostenv.read_text(source), scalar="float")
    res["buffers"] = sorted([l["name"] for l in attach.values()] + [l["name"] for s in plan["layouts"].values() for l in s.values()])
    res["textures"] = sorted({t["name"] for s in ("vertex", "fragment") for t in plan["apple"][s]["textures"]})
    res["unused_textures"] = []  # graphics mode has no unused-texture pass; reject DIFFCHECK-UNUSED as unknown
    res["ulp_bound"] = ulp
    profiles = [opts.gfx_profiles[c % len(opts.gfx_profiles)] for c in range(params["cases"])]
    nonzero = False
    for case, profile in enumerate(profiles):
        rng = random.Random(params["seed"] * 1000 + case)
        base = os.path.join(work, "case%d-" % case)
        case_before = {i: bytearray(v) for i, v in before.items()}
        attr_files, buffers, textures, samplers = {}, [], [], []
        for loc, data in vertex_data(plan["attributes"], plan["points"], plan["position"], width, height, rng, profile, base_vertex).items():
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
                k = _wbuf_key(stage, b["metal_index"])
                if k in writable:
                    case_before[k] = bytearray(data)
                buffers.append((stage, b["metal_index"], b["descriptor"]["set"], b["member"], len(data), path))
            stage_textures, stage_samplers = resources.seed(
                ep["bindings"], rng, profile, base + stage + "-", ep["embedded_samplers"], plan["apple"][stage], params.get("mips"))
            textures += stage_textures
            samplers += stage_samplers
        outs = {}
        for backend, runner in (("apple", hosts.run_metal), ("mslc", hosts.run_vulkan)):
            color_outs = {i: base + "%s-color%d.bin" % (backend, i) for i in plan["colors"]}
            depth_out = base + "%s-depth.bin" % backend if plan["depth"] else None
            buf_outs = {_wbuf_key(b[0], b[1]): base + "%s-%s-buf%d-out.bin" % (backend, b[0], b[1])
                        for b in buffers if _wbuf_key(b[0], b[1]) in writable}
            backend_buffers = [b + ((buf_outs[_wbuf_key(b[0], b[1])],) if _wbuf_key(b[0], b[1]) in buf_outs else ())
                               for b in buffers]
            input_attachments = [(b["input_attachment_index"], b["descriptor"]["set"], b["descriptor"]["binding"])
                                 for b in plan["compiled"]["fragment"][1]["bindings"] if b["kind"] == "InputAttachment"]
            body = draw_lines(plan["shape"], plan["points"], params["instances"], attr_files, color_outs, plan["colors"],
                              depth_out, backend_buffers, textures, samplers, backend, base_vertex, base_instance,
                              index_file, len(indexes or ()), input_attachments)
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
            for k, p in buf_outs.items():
                outs[backend][k] = bytearray(hostenv.read_bytes(p))
        if opts.corrupt_mslc:
            name, _, at = opts.corrupt_mslc.partition(":")
            for i, l in {**attach, **writable}.items():
                if l["name"] == name:
                    outs["mslc"][i][int(at)] ^= 0xff
        diffs = []
        for i, layout in sorted(compared.items()):
            exact, smooth = split_leaves(layout, outs["apple"][i], primitives)
            default_ulp = ulp if any(lf["scalar"] == "half" for lf in layout["leaves"]) else ulp_float
            bound = params["ulp"].get(layout["name"], default_ulp)
            for leaves, (u, e) in ((exact, (bound, 0.0)), (smooth, tolerance(bound, True, "float"))):
                if leaves:
                    sub = dict(layout, leaves=leaves)
                    diffs += [d.to_json() for d in compare.compare(sub, outs["apple"][i], outs["mslc"][i], u, eps=DEPTH_EPS if i == DEPTH_INDEX else e)]
        for k, layout in sorted(wcompared.items()):
            explicit = params["ulp"].get(layout["name"])
            diffs += [d.to_json() for d in compare.compare(layout, outs["apple"][k], outs["mslc"][k],
                                                           ulp if explicit is None else explicit,
                                                           float_ulp=ulp_float if explicit is None else explicit)]
        all_compared = {**compared, **wcompared}
        nonzero = nonzero or any(checks.written_nonzero(all_compared, case_before, outs[b]) for b in ("apple", "mslc"))
        control_ok = checks.control_holds([g for g in (compared, wcompared) if g], outs["mslc"], outs["apple"])
        res["cases"].append({"case": case, "profile": profile, "diffs": diffs[:16], "diff_count": len(diffs), "negative_control": control_ok})
    res["status"], res["reason"] = checks.verdict(res["cases"], nonzero, params["writes_zero"])
    if excluded:
        res["excluded"] = excluded
    return res
