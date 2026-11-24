"""Seeded input generation for diffcheck.

Every case uses one of three profiles, cycled by --cases:
  moderate  small values that most kernels handle the same way on any GPU
  wide      random bit patterns (floats limited to finite values)
  edge      half the leaves come from the edge set (0, 1, -1, INT_MIN/MAX, UINT_MAX,
            +-0, +-inf, NaN, +-1.2e-38, +-1e38; float32 denormals are never generated, see no_denormal_float), the rest from `wide`
bool leaves are only ever 0 or 1. Bytes that no leaf covers (struct padding) are
0, or 0xA5 with poison_padding, so a kernel that reads padding differs from one
that does not. With sentinel the whole buffer is 0x7f bytes instead (0x7f7f7f7f
is a finite float), so a store outside the bytes a kernel is expected to write
shows as a difference.
"""

import struct

from compare import SCALARS

PROFILES = ("moderate", "wide", "edge")
SENTINEL = 0x7f
POISON = 0xA5

def _bits(value, fmt):
    return int.from_bytes(struct.pack(fmt, value), "little")


def no_denormal_float(bits):
    """A float32 bit pattern with a denormal replaced by the zero of the same sign. Apple's GPU
    flushes a denormal in a compare or float-to-bool and mslc declares no DenormFlushToZero (#377),
    so a denormal in the seeded inputs would be a known difference on every compare. The one
    dedicated probe (tests/diffcheck/denormal_compare_flush.metal) builds its denormals in the
    kernel. Normals, zeros, infinities and NaNs pass through."""
    return bits & 0x80000000 if bits & 0x7f800000 == 0 else bits


_EDGE_FLOAT = {
    "float": [0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000,
              0x00800000, 0x7f7fffff, 0xff7fffff]
             + [_bits(v, "<f") for v in (1.2e-38, -1.2e-38, 1e38, -1e38)],
    "half": [0x0000, 0x8000, 0x3c00, 0xbc00, 0x7c00, 0xfc00, 0x7e00, 0x0001, 0x03ff, 0x0400, 0x7bff, 0xfbff],
}


def _edge_int(size):
    top = 1 << (size * 8 - 1)
    return [0, 1, (1 << (size * 8)) - 1, top, top - 1, top + 1]


def _wide_float(rng, scalar):
    # Exponent field away from 0 and all-ones: finite and normal.
    if scalar == "float":
        return (rng.getrandbits(1) << 31) | (rng.randrange(60, 195) << 23) | rng.getrandbits(23)
    return (rng.getrandbits(1) << 15) | (rng.randrange(3, 29) << 10) | rng.getrandbits(10)


def _step(walk, table, rng):
    """The next edge value: a walk through the table, so every entry appears within len(table)
    edge leaves and a short run still reaches NaN and the infinities; random without a walk."""
    if walk is None:
        return rng.choice(table)
    walk[0] += 1
    return table[(walk[0] + walk[1]) % len(table)]


def leaf_bits(rng, scalar, profile, bounds=None, walk=None, float_bounds=None):
    bits = _leaf_bits(rng, scalar, profile, bounds, walk, float_bounds)
    return no_denormal_float(bits) if scalar == "float" else bits


def _leaf_bits(rng, scalar, profile, bounds, walk, float_bounds):
    size, kind = SCALARS[scalar]
    if kind == "bool":
        return rng.getrandbits(1)
    if kind == "float":
        if float_bounds:
            if scalar == "half" and max(abs(b) for b in float_bounds) > 65504:
                raise ValueError("DIFFCHECK-FLOAT-RANGE %d..%d does not fit a half" % float_bounds)
            return _bits(float(rng.randint(*float_bounds)), "<f" if scalar == "float" else "<e")
        if profile == "moderate":
            quarter = rng.random() < 0.25
            value = rng.randrange(-16, 17) / 4 if quarter else rng.uniform(-4, 4)
            fmt = "<f" if scalar == "float" else "<e"
            return int.from_bytes(struct.pack(fmt, value), "little")
        if profile == "edge" and rng.random() < 0.5:
            return _step(walk, _EDGE_FLOAT[scalar], rng)
        return _wide_float(rng, scalar)
    mask = (1 << (size * 8)) - 1
    if bounds and kind == "int":
        return rng.randint(*bounds) & mask
    if profile == "moderate":
        low = 0 if scalar.startswith("u") else -100
        return rng.randrange(low, 201) & mask
    if profile == "edge" and rng.random() < 0.5:
        return _step(walk, _edge_int(size), rng)
    return rng.getrandbits(size * 8)


def fill_buffer(layout, elements, rng, profile, sentinel=False, poison_padding=False, bounds=None, phase=0,
                float_bounds=None):
    """A bytearray of elements * stride bytes for one buffer. `bounds` (lo, hi) limits its
    integer leaves, for a buffer a kernel uses as an index. `phase` (the case number) offsets the
    walk through the edge values, so different cases start at different ones. `float_bounds` does the
    same for float leaves (integer-valued)."""
    stride = layout["stride"]
    walk = [-1, phase]
    data = bytearray([SENTINEL if sentinel else POISON if poison_padding else 0]) * (elements * stride)
    for element in range(elements):
        for leaf in layout["leaves"]:
            scalar = leaf["scalar"]
            size = SCALARS[scalar][0]
            at = element * stride + leaf["off"]
            if sentinel and SCALARS[scalar][1] == "bool":
                data[at] = rng.getrandbits(1)
            elif sentinel and not ((bounds and SCALARS[scalar][1] == "int") or (float_bounds and SCALARS[scalar][1] == "float")):
                continue
            else:
                data[at:at + size] = leaf_bits(rng, scalar, "wide" if sentinel else profile, bounds, walk, float_bounds).to_bytes(size, "little")
    return data
