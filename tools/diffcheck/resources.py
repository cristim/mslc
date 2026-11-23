"""Textures, samplers and texel buffers as both modes feed them: the same seeded texels on both sides, bound by
Apple's index and by the set and binding of mslc's reflection.

A texture is 2D, RGBA32Float, shader-read; a sampler is nearest, normalised, no mips, with an
address mode picked by its Metal index (0 clamp to edge, 1 repeat, 2 mirrored repeat). Probes should sample at a texel centre (an edge or a filter is where the backends may
legitimately differ). Anything else is reported by `unsupported` and the file is skipped.

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
READABLE = {"Sample", "Read"}
TEXEL_TYPES = ("float", "half", "int", "uint", "short", "ushort")
# Apple's binding access -> mslc's texture_access and descriptor type for a texel buffer.
TEXEL_ACCESS = {"read": ("Read", "UniformTexelBuffer"), "readwrite": ("ReadWrite", "StorageTexelBuffer"),
                "write": ("Write", "StorageTexelBuffer")}
# One entry of a texel buffer: its output key in the comparison (negative, so it never meets a device
# buffer's Metal index), the layout of one texel and where mslc binds it.
TexelBuffer = collections.namedtuple("TexelBuffer", "index set binding uniform scalar components layout")


def texture_data(rng, profile):
    return bytearray(b"".join(inputs.leaf_bits(rng, "float", profile).to_bytes(4, "little")
                              for _ in range(4 * TEXTURE_SIZE * TEXTURE_SIZE)))


def cross_check(bindings, apple, label=""):
    """Findings where mslc's texture, sampler and texel buffer bindings differ from Apple's by index
    or name (and, for a texel buffer, by access and descriptor type)."""
    findings = []
    for kind, key, texel in (("Texture", "textures", False), ("TextureBuffer", "textures", True), ("Sampler", "samplers", False)):
        # An embedded sampler has no Metal index (and is skipped by `unsupported` before this runs).
        mine = sorted((b["metal_index"], b["name"]) for b in bindings if b["kind"] == kind and "metal_index" in b)
        theirs = sorted((t["index"], t["name"]) for t in apple[key] if key != "textures" or (t["type"] == "texturebuffer") == texel)
        if mine != theirs:
            findings.append("%s%s bindings differ: mslc %s, Apple %s" % (label, kind.lower(), mine, theirs))
    for t in apple["textures"]:
        if t["type"] != "texturebuffer":
            continue
        for b in bindings:
            if b["kind"] == "TextureBuffer" and b.get("metal_index") == t["index"] and (b.get("texture_access"), b.get("descriptor_type")) != TEXEL_ACCESS[t["access"]]:
                findings.append("%stexture_buffer %s is %s in Apple, mslc has access %s as %s" % (
                    label, t["name"], t["access"], b.get("texture_access"), b.get("descriptor_type")))
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


def unsupported(entry, apple):
    """Reasons this entry point's textures and samplers cannot be fed (empty: they can)."""
    reasons = ["texture %s access %s" % (b["name"], b.get("texture_access")) for b in entry["bindings"]
               if b["kind"] == "Texture" and b.get("texture_access") not in READABLE]
    plain = [t for t in apple["textures"] if t["type"] != "texturebuffer"]
    reasons += ["texture %s is not a 2D float texture" % t["name"] for t in plain if t["type"] != "2d" or t["data"] != "float"]
    reasons += ["texture %s is written (Apple access %s)" % (t["name"], t["access"]) for t in plain if t["access"] != "read"]
    reasons += ["texture_buffer %s has element type %s" % (t["name"], t["data"]) for t in apple["textures"]
                if t["type"] == "texturebuffer" and t["data"] not in TEXEL_TYPES]
    for state in entry["embedded_samplers"]:
        reasons += ["embedded sampler %s is %s, the Vulkan host builds only nearest or linear, no mips, no compare, normalised"
                    % (k, state[k]) for k, ok in EMBEDDED_PLAIN.items() if state[k] != ok]
        if state["min_filter"] != state["mag_filter"]:
            reasons.append("embedded sampler min_filter %s differs from mag_filter %s, the Vulkan host sets one filter for both"
                           % (state["min_filter"], state["mag_filter"]))
        reasons += ["embedded sampler %s is %s, the Vulkan host builds the full lod range" % (k, state[k])
                    for k, ok in EMBEDDED_LOD.items() if state[k] != ok]
        reasons += ["embedded sampler address %s" % state[a] for a in ("s_address", "t_address", "r_address") if state[a] not in VK_ADDRESS]
    return reasons


# What an embedded sampler must say for the Vulkan host to build it, and the VkSamplerAddressMode of each address
# (ClampToZero is a transparent black border, Apple's clamp_to_zero).
EMBEDDED_PLAIN = {"mip_filter": "None", "compare_function": "Never", "normalized_coordinates": True, "anisotropy": 1}
EMBEDDED_LOD = {"lod_min": 0, "lod_max": 65504}
VK_ADDRESS = {"Repeat": 0, "MirrorRepeat": 1, "ClampToEdge": 2, "ClampToZero": 3}


def seed(bindings, rng, profile, base, embedded=()):
    """Writes one texture file per Texture binding to `base`+name and returns the manifest
    tuples ([(index, set, binding, file, access)], [(index, set, binding)]), in binding order;
    `access` is mslc's texture_access: a `Read` texture is a storage image in SPIR-V, the rest are sampled.
    A sampler tuple is (index, set, binding), or for an embedded sampler (None, set, binding, (s, t, r) modes, linear)
    from the entry's `embedded` states."""
    textures, samplers = [], []
    for b in bindings:
        if b["kind"] == "Texture":
            path = "%stex%d.bin" % (base, b["metal_index"])
            hostenv.write_bytes(path, texture_data(rng, profile))
            textures.append((b["metal_index"], b["descriptor"]["set"], b["descriptor"]["binding"], path, b["texture_access"]))
        elif b["kind"] == "Sampler" and "embedded_sampler" in b:
            e = embedded[b["embedded_sampler"]]
            samplers.append((None, b["descriptor"]["set"], b["descriptor"]["binding"],
                             tuple(VK_ADDRESS[e[a]] for a in ("s_address", "t_address", "r_address")), e["mag_filter"] == "Linear"))
        elif b["kind"] == "Sampler":
            samplers.append((b["metal_index"], b["descriptor"]["set"], b["descriptor"]["binding"]))
    return textures, samplers


def lines(textures, samplers, backend="vulkan"):
    """The manifest lines both hosts read; Apple has an embedded sampler in the shader, so only Vulkan is told of it."""
    return (["%s %d %d %d %d %d %s" % ("storagetexture" if a == "Read" else "texture", i, s, b, TEXTURE_SIZE, TEXTURE_SIZE, f)
             for i, s, b, f, a in textures]
            + ["sampler %d %d %d" % s for s in samplers if s[0] is not None]
            + ["esampler %d %d %d %d %d %d" % (s[1], s[2], *s[3], s[4]) for s in samplers if s[0] is None and backend != "apple"])


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
