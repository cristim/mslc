"""Textures, samplers and texel buffers as both modes feed them: the same seeded texels on both sides, bound by
Apple's index and by the set and binding of mslc's reflection.

A texture in kernel mode may be 1d (4), 1d_array (4x3), 2d (4x4), 2d_array (4x4x3), 3d (4x4x2), cube (4x4x6) or
cube_array (4x4x6x2), RGBA of float, half, int, uint, short or ushort (access sample, read, write or read_write;
sample or access::read in graphics), with optional 2 or 3 mip levels (`DIFFCHECK-MIPS`) on non-1D read-only textures;
a sampler is nearest, normalised, mip-nearest, with an address mode picked
by its Metal index (0 clamp to edge, 1 repeat, 2 mirrored repeat). Probes should sample at a texel centre (an edge
or a filter is where the backends may legitimately differ). A writable texture's texels are read back and
compared like a buffer. Anything else is reported by `unsupported` and the file is skipped.

A texel buffer (`texture_buffer<T, access>`, kernel mode only) is a buffer of texels of 4 (the default), 2 or 1
components of T (float, half, int, uint, short or ushort; `DIFFCHECK-TEXEL` picks the count), seeded like a device
buffer, bound as a MTLTextureTypeTextureBuffer on Apple and as a VkBufferView on lavapipe, and compared after the
dispatch like a device buffer: its layout is one leaf of T per component.
The element type and access come from Apple's reflection, never from mslc's module.
"""

import collections

import compare
import hostenv
import inputs

TEXTURE_SIZE = 4
TEXTURE_SHAPES = {
    "1d": (TEXTURE_SIZE, 1, 1, 1),
    "1d_array": (TEXTURE_SIZE, 1, 1, 3),
    "2d": (TEXTURE_SIZE, TEXTURE_SIZE, 1, 1),
    "2d_array": (TEXTURE_SIZE, TEXTURE_SIZE, 1, 3),
    "3d": (TEXTURE_SIZE, TEXTURE_SIZE, 2, 1),
    "cube": (TEXTURE_SIZE, TEXTURE_SIZE, 1, 1),
    "cube_array": (TEXTURE_SIZE, TEXTURE_SIZE, 1, 2),
}
TEXTURE_ACCESS = {"read": ("Sample", "Read"), "write": ("Write",), "readwrite": ("ReadWrite",)}
TEXEL_TYPES = ("float", "half", "int", "uint", "short", "ushort")
# Apple's binding access -> mslc's texture_access and descriptor type for a texel buffer.
TEXEL_ACCESS = {"read": ("Read", "UniformTexelBuffer"), "readwrite": ("ReadWrite", "StorageTexelBuffer"),
                "write": ("Write", "StorageTexelBuffer")}
# One entry of a texel buffer: its output key in the comparison (negative, so it never meets a device
# buffer's Metal index), the layout of one texel and where mslc binds it.
TexelBuffer = collections.namedtuple("TexelBuffer", "index set binding uniform scalar components layout")


def mip_shape(kind, level):
    """(w, h, d, slices) for `kind` at mip level `level`."""
    w, h, d, layers = TEXTURE_SHAPES[kind]
    mw = max(1, w >> level)
    mh = 1 if kind in ("1d", "1d_array") else max(1, h >> level)
    md = max(1, d >> level) if kind == "3d" else 1
    slices = layers * (6 if kind in ("cube", "cube_array") else 1)
    return mw, mh, md, slices


def texture_texels(kind="2d", mips=1):
    return sum(mw * mh * md * slices for lv in range(mips) for mw, mh, md, slices in (mip_shape(kind, lv),))


def texture_data(rng, profile, scalar="float", kind="2d", mips=1):
    size = compare.SCALARS[scalar][0]
    return bytearray(b"".join(inputs.leaf_bits(rng, scalar, profile).to_bytes(size, "little")
                              for _ in range(4 * texture_texels(kind, mips))))


def cross_check(bindings, apple, label=""):
    """Findings where mslc's texture, sampler and texel buffer bindings differ from Apple's by index
    or name (and, for a texture or texel buffer, by access and descriptor type)."""
    findings = []
    for kind, key, texel in (("Texture", "textures", False), ("TextureBuffer", "textures", True), ("Sampler", "samplers", False)):
        # An embedded sampler has no Metal index (and is skipped by `unsupported` before this runs).
        mine = sorted((b["metal_index"], b["name"]) for b in bindings if b["kind"] == kind and "metal_index" in b)
        theirs = sorted((t["index"], t["name"]) for t in apple[key] if key != "textures" or (t["type"] == "texturebuffer") == texel)
        if mine != theirs:
            findings.append("%s%s bindings differ: mslc %s, Apple %s" % (label, kind.lower(), mine, theirs))
    for t in apple["textures"]:
        for b in bindings:
            if b.get("metal_index") != t["index"]:
                continue
            if t["type"] == "texturebuffer" and b["kind"] == "TextureBuffer" and (b.get("texture_access"), b.get("descriptor_type")) != TEXEL_ACCESS[t["access"]]:
                findings.append("%stexture_buffer %s is %s in Apple, mslc has access %s as %s" % (
                    label, t["name"], t["access"], b.get("texture_access"), b.get("descriptor_type")))
            elif t["type"] != "texturebuffer" and b["kind"] == "Texture" and "texture_access" in b and b.get("texture_access") not in TEXTURE_ACCESS[t["access"]]:
                findings.append("%stexture %s is %s in Apple, mslc has access %s" % (
                    label, t["name"], t["access"], b.get("texture_access")))
    return findings


def threadgroup_cross_check(threadgroups, apple):
    """Findings where mslc's ThreadgroupMemory bindings differ from Apple's threadgroup arguments by
    index, name or element size."""
    mine = sorted((b["metal_index"], b["name"], b["element_size"]) for b in threadgroups)
    theirs = sorted((t["index"], t["name"], t["size"]) for t in apple["threadgroups"])
    return [] if mine == theirs else ["threadgroup arguments differ: mslc %s, Apple %s" % (mine, theirs)]


def threadgroup_lines(threadgroups, local, backend):
    """Manifest lines giving every threadgroup argument room for one element per thread of the group:
    Apple's setThreadgroupMemoryLength takes the bytes (a multiple of 16, so rounded up), and mslc's reflection says the Vulkan side is
    the element count in the argument's spec constant (bytes / element_size)."""
    if backend == "apple":
        return ["threadgroup %d %d" % (b["metal_index"], -(-local * b["element_size"] // 16) * 16) for b in threadgroups]
    return ["spec %d %d" % (b["spec_id"], local) for b in threadgroups]


def unsupported(entry, apple, mips=None):
    """Reasons this entry point's textures and samplers cannot be fed (empty: they can)."""
    mips = mips or {}
    reasons = ["texture %s access %s" % (b["name"], b.get("texture_access")) for b in entry["bindings"]
               if b["kind"] == "Texture" and b.get("texture_access") not in ("Sample", "Read", "Write", "ReadWrite")]
    plain = [t for t in apple["textures"] if t["type"] != "texturebuffer"]
    reasons += ["texture %s has unsupported type %s" % (t["name"], t["type"]) for t in plain if t["type"] not in TEXTURE_SHAPES]
    reasons += ["texture %s has element type %s" % (t["name"], t["data"]) for t in plain if t["type"] in TEXTURE_SHAPES and t["data"] not in TEXEL_TYPES]
    reasons += ["texture_buffer %s has element type %s" % (t["name"], t["data"]) for t in apple["textures"]
                if t["type"] == "texturebuffer" and t["data"] not in TEXEL_TYPES]
    reasons += ["texture_buffer %s has no mip levels" % t["name"] for t in apple["textures"]
                if t["type"] == "texturebuffer" and t["name"] in mips]
    reasons += ["texture %s is %s, which has no mip levels" % (t["name"], t["type"]) for t in plain
                if t["name"] in mips and t["type"] in ("1d", "1d_array")]
    writable = ({b["name"] for b in entry["bindings"] if b["kind"] == "Texture" and b.get("texture_access") in ("Write", "ReadWrite")}
                | {t["name"] for t in plain if t.get("access") not in (None, "read")})
    reasons += ["texture %s is writable and has DIFFCHECK-MIPS (storage images have one level)" % name
                for name in sorted(writable) if name in mips]
    for state in entry["embedded_samplers"]:
        reasons += ["embedded sampler %s is %s, the Vulkan host builds only nearest or linear, no compare, normalised"
                    % (k, state[k]) for k, ok in EMBEDDED_PLAIN.items() if state[k] != ok]
        if state["mip_filter"] not in EMBEDDED_MIP:
            reasons.append("embedded sampler mip_filter is %s, the Vulkan host builds only none, nearest or linear"
                           % state["mip_filter"])
        if state["min_filter"] != state["mag_filter"]:
            reasons.append("embedded sampler min_filter %s differs from mag_filter %s, the Vulkan host sets one filter for both"
                           % (state["min_filter"], state["mag_filter"]))
        if state.get("border_color", "TransparentBlack") != "TransparentBlack":
            reasons.append("embedded sampler border_color is %s, the Vulkan host builds only transparent black" % state["border_color"])
        reasons += ["embedded sampler %s is %s, the Vulkan host builds the full lod range" % (k, state[k])
                    for k, ok in EMBEDDED_LOD.items() if state[k] != ok]
        reasons += ["embedded sampler address %s" % state[a] for a in ("s_address", "t_address", "r_address") if state[a] not in VK_ADDRESS]
    return reasons


# What an embedded sampler must say for the Vulkan host to build it, and the VkSamplerAddressMode of each address
# (ClampToZero is a transparent black border, Apple's clamp_to_zero).
EMBEDDED_PLAIN = {"compare_function": "Never", "normalized_coordinates": True, "anisotropy": 1}
EMBEDDED_MIP = {"None": 0, "Nearest": 1, "Linear": 2}
EMBEDDED_LOD = {"lod_min": 0, "lod_max": 65504}
VK_ADDRESS = {"Repeat": 0, "MirrorRepeat": 1, "ClampToEdge": 2, "ClampToZero": 3}


def seed(bindings, rng, profile, base, embedded=(), apple=None, mips=None):
    """Writes one texture file per Texture binding to `base`+name and returns the manifest
    tuples ([(index, set, binding, file, access, scalar[, kind[, mips]])], [(index, set, binding)]), in binding order;
    `access` is mslc's texture_access: a `Read` texture is a sampled image fetched from, like a `Sample` one,
    while a `Write` or `ReadWrite` texture is a storage image in SPIR-V; `scalar` and `kind` come from Apple's
    reflection ("float" and "2d" when `apple` is omitted, as in graphics, with `kind` appended when not "2d" or `mips > 1`).
    A sampler tuple is (index, set, binding), or for an embedded sampler
    (None, set, binding, (s, t, r) modes, linear[, mip_mode]) from the entry's `embedded` states."""
    mips = mips or {}
    by_index = {t["index"]: (t["data"], t["type"]) for t in (apple or {}).get("textures", ()) if t["type"] != "texturebuffer"}
    textures, samplers = [], []
    for b in bindings:
        if b["kind"] == "Texture":
            scalar, kind = by_index.get(b["metal_index"], ("float", "2d"))
            nmips = mips.get(b["name"], 1)
            path = "%stex%d.bin" % (base, b["metal_index"])
            hostenv.write_bytes(path, texture_data(rng, profile, scalar, kind, nmips))
            entry = (b["metal_index"], b["descriptor"]["set"], b["descriptor"]["binding"], path, b["texture_access"], scalar)
            textures.append(entry + (kind, nmips) if nmips > 1 else entry if kind == "2d" else entry + (kind,))
        elif b["kind"] == "Sampler" and "embedded_sampler" in b:
            e = embedded[b["embedded_sampler"]]
            mip = EMBEDDED_MIP[e["mip_filter"]]
            s = (None, b["descriptor"]["set"], b["descriptor"]["binding"],
                 tuple(VK_ADDRESS[e[a]] for a in ("s_address", "t_address", "r_address")), e["mag_filter"] == "Linear")
            samplers.append(s + (mip,) if mip else s)
        elif b["kind"] == "Sampler":
            samplers.append((b["metal_index"], b["descriptor"]["set"], b["descriptor"]["binding"]))
    return textures, samplers


def lines(textures, samplers, backend="vulkan", outputs=None):
    """The manifest lines both hosts read; Apple has an embedded sampler in the shader, so only Vulkan is told of it.
    A writable 2D texture (`outputs` maps its Metal index to an output file) is a `storagetexture` with `write` or `readwrite`
    and its output file at the end; a non-float 2D texture adds its scalar type after the input file; non-2D or multi-mip textures
    use `image <kind> <access> <index> <set> <binding> <w> <h> <d> <layers> <scalar> <file>[ <output>| mips=<n>]`."""
    outputs = outputs or {}
    tex_lines = []
    for i, s, b, f, a, *rest in textures:
        scalar = rest[0] if rest else "float"
        kind = rest[1] if len(rest) > 1 else "2d"
        nmips = rest[2] if len(rest) > 2 else 1
        if kind == "2d" and nmips == 1:
            tail = " %s %s %s" % (scalar, a.lower(), outputs[i]) if i in outputs else (" " + scalar if scalar != "float" else "")
            tex_lines.append("%s %d %d %d %d %d %s%s" % ("texture" if a in ("Sample", "Read") else "storagetexture", i, s, b, TEXTURE_SIZE, TEXTURE_SIZE, f, tail))
        else:
            w, h, d, layers = TEXTURE_SHAPES[kind]
            tail = (" " + outputs[i]) if i in outputs else (" mips=%d" % nmips if nmips > 1 else "")
            tex_lines.append("image %s %s %d %d %d %d %d %d %d %s %s%s" % (kind, a.lower(), i, s, b, w, h, d, layers, scalar, f, tail))
    return (tex_lines
            + ["sampler %d %d %d" % s for s in samplers if s[0] is not None]
            + ["esampler %d %d %d %d %d %d%s" % (s[1], s[2], *s[3], s[4], " %d" % s[5] if len(s) > 5 and s[5] else "")
               for s in samplers if s[0] is None and backend != "apple"])


def texture_key(index):
    return -129 - index


def texture_layouts(apple):
    """{texture_key(index): layout} for every writable texture in Apple's reflection."""
    return {texture_key(t["index"]): texel_layout(t["name"], t["data"], t["access"], 4)
            for t in apple["textures"] if t["type"] != "texturebuffer" and t["access"] != "read"}


def texel_layout(name, scalar, access, components=4):
    """The layout of a texel buffer in compare's terms: one element is one texel of `components` components."""
    size = compare.SCALARS[scalar][0]
    return {"name": name, "access": access, "stride": components * size, "pointer": False,
            "leaves": [{"off": c * size, "scalar": scalar} for c in range(components)]}


def texel_plan(bindings, apple, components=None):
    """The TexelBuffers of an entry point: mslc's binding, Apple's name, element type and access, and the component
    count `components` (a {buffer name: count} of DIFFCHECK-TEXEL) gives, 4 otherwise."""
    components = components or {}
    by_index = {t["index"]: t for t in apple["textures"] if t["type"] == "texturebuffer"}
    plan = []
    for b in bindings:
        if b["kind"] != "TextureBuffer":
            continue
        t = by_index[b["metal_index"]]
        n = components.get(t["name"], 4)
        plan.append(TexelBuffer(b["metal_index"], b["descriptor"]["set"], b["descriptor"]["binding"],
                                b["descriptor_type"] == "UniformTexelBuffer", t["data"], n, texel_layout(t["name"], t["data"], t["access"], n)))
    return plan


def texel_key(tb):
    return -1 - tb.index


def texel_count(texels, tb, case):
    """Texels of one buffer in one case: `texels` plus 4 per Metal index plus the case number, so that get_width of
    one buffer is not another's or a constant (a kernel indexing every buffer modulo the width of texture(0), the
    smallest, stays in range)."""
    return texels + 4 * tb.index + case


def seed_texels(plan, texels, rng, profile, base, sentinel=False, poison=False, ranges=None, float_ranges=None, phase=0):
    """[(TexelBuffer, input file, bytes)]: texel_count(texels, tb, phase) texels each, filled like a device buffer,
    file `base`+tb<index>.bin."""
    seeded = []
    for tb in plan:
        name = tb.layout["name"]
        data = inputs.fill_buffer(tb.layout, texel_count(texels, tb, phase), rng, profile, sentinel, poison, (ranges or {}).get(name), phase,
                                  (float_ranges or {}).get(name))
        path = "%stb%d.bin" % (base, tb.index)
        hostenv.write_bytes(path, data)
        seeded.append((tb, path, data))
    return seeded


def texel_lines(seeded, outputs):
    """The manifest lines both hosts read; `outputs` maps a texel buffer's Metal index to its output file."""
    return ["texelbuffer %d %d %d %s %s %d %d %s %s" % (tb.index, tb.set, tb.binding, "uniform" if tb.uniform else "storage",
                                                       tb.scalar, tb.components, len(data) // tb.layout["stride"], path, outputs[tb.index]) for tb, path, data in seeded]
