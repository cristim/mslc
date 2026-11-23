"""Output comparison rules for diffcheck.

A buffer is described by a layout (see Layout below). Only the bytes covered by a
leaf are compared; struct padding is an explicit exclusion, not an accident.

Rules, in order of strength:
  integers and bool   bit-exact
  float and half      bit-exact, or within a ULP bound when the source calls a
                      function whose result is not exactly rounded
  NaN                 two NaNs match (payloads differ legitimately); a NaN against
                      a number never matches and never passes a ULP bound, because
                      every ordered comparison with a NaN is false and would let it through
  infinities          must be the same infinity
"""

import re
import struct

# name -> (size in bytes, kind). kind is "float", "int" or "bool".
SCALARS = {
    "float": (4, "float"), "half": (2, "float"),
    "int": (4, "int"), "uint": (4, "int"), "short": (2, "int"), "ushort": (2, "int"),
    "char": (1, "int"), "uchar": (1, "int"), "long": (8, "int"), "ulong": (8, "int"),
    "bool": (1, "bool"),
}

_FLOAT = {"float": ("<f", "<I", 8, 23), "half": ("<e", "<H", 5, 10)}

# Bounds in ULP for results Apple and lavapipe round differently. Exactly rounded
# operations (+ - * / sqrt fma and the rounding functions) are not listed: they must
# match to the bit. Starting values, to be tightened by measurement; a function that
# is not here is compared exactly.
# sqrt is provisional at 2 ULP, from one measured 1-ulp difference (#387); replace it with the Metal spec bound.
ULP_BOUNDS = {
    "sin": 4, "cos": 4, "tan": 8, "asin": 8, "acos": 8, "atan": 8, "atan2": 8,
    "sinh": 8, "cosh": 8, "tanh": 8, "asinh": 8, "acosh": 8, "atanh": 8,
    "exp": 4, "exp2": 4, "exp10": 8, "log": 4, "log2": 4, "log10": 8,
    "pow": 16, "powr": 16, "rsqrt": 4, "sqrt": 2, "fmod": 0, "mix": 4, "smoothstep": 4,
    "step": 0, "length": 4, "distance": 4, "normalize": 4, "dot": 4, "cross": 4,
}


def _fields(scalar):
    return _FLOAT[scalar]


def is_nan(raw, scalar):
    fmt, _, ebits, mbits = _fields(scalar)
    v = int.from_bytes(raw, "little")
    exp = (v >> mbits) & ((1 << ebits) - 1)
    return exp == (1 << ebits) - 1 and (v & ((1 << mbits) - 1)) != 0


def is_inf(raw, scalar):
    _, _, ebits, mbits = _fields(scalar)
    v = int.from_bytes(raw, "little")
    return (v >> mbits) & ((1 << ebits) - 1) == (1 << ebits) - 1 and (v & ((1 << mbits) - 1)) == 0


def ulp_distance(a, b, scalar):
    """Distance in representable values; None when either is a NaN."""
    if is_nan(a, scalar) or is_nan(b, scalar):
        return None
    size = SCALARS[scalar][0]
    sign = 1 << (size * 8 - 1)

    def ordered(raw):
        v = int.from_bytes(raw, "little")
        return -(v & (sign - 1)) if v & sign else v

    return abs(ordered(a) - ordered(b))


def leaf_equal(a, b, scalar, ulp=0, eps=0.0):
    """True when the two raw little-endian values of one leaf agree. `eps` is an absolute
    bound for two finite floats, for interpolated values: a ULP bound alone is useless near zero."""
    kind = SCALARS[scalar][1]
    if kind != "float":
        return a == b
    nan_a, nan_b = is_nan(a, scalar), is_nan(b, scalar)
    if nan_a or nan_b:
        return nan_a and nan_b
    if a == b:
        return True
    # An infinity next to the largest finite value is one ULP away, and is an overflow bug.
    if (ulp == 0 and eps == 0) or is_inf(a, scalar) or is_inf(b, scalar):
        return False
    if eps and abs(struct.unpack(_fields(scalar)[0], a)[0] - struct.unpack(_fields(scalar)[0], b)[0]) <= eps:
        return True
    return ulp_distance(a, b, scalar) <= ulp


def ulp_bound_for_source(source):
    """The loosest bound among the inexact functions the source calls."""
    # Comments and string literals are not calls.
    source = re.sub(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"", " ", source, flags=re.S)
    bound = 0
    for name, ulp in ULP_BOUNDS.items():
        if ulp > bound and re.search(r"(?<![\w.])(?:\w+::)*%s\s*\(" % name, source):
            bound = ulp
    return bound


class Diff:
    def __init__(self, buffer, element, offset, scalar, apple, mslc):
        self.buffer, self.element, self.offset = buffer, element, offset
        self.scalar, self.apple, self.mslc = scalar, apple, mslc

    def value(self, raw):
        if SCALARS[self.scalar][1] == "float":
            return struct.unpack(_fields(self.scalar)[0], raw)[0]
        if SCALARS[self.scalar][1] == "int" and not self.scalar.startswith("u"):
            return int.from_bytes(raw, "little", signed=True)
        return int.from_bytes(raw, "little")

    def to_json(self):
        return {"buffer": self.buffer, "element": self.element, "offset": self.offset,
                "type": self.scalar, "apple": self.value(self.apple), "mslc": self.value(self.mslc),
                "apple_hex": self.apple[::-1].hex(), "mslc_hex": self.mslc[::-1].hex()}


def compare(layout, apple, mslc, ulp=0, limit=16, eps=0.0):
    """Compare two buffers of one layout; returns up to `limit` Diffs.

    layout: {"name", "stride", "leaves": [{"off", "scalar"}]}. Both buffers must have
    the same length, a multiple of the stride.
    """
    if len(apple) != len(mslc):
        raise ValueError("buffers differ in length: %d vs %d" % (len(apple), len(mslc)))
    diffs = []
    stride = layout["stride"]
    for element in range(len(apple) // stride):
        base = element * stride
        for leaf in layout["leaves"]:
            size = SCALARS[leaf["scalar"]][0]
            at = base + leaf["off"]
            a, b = bytes(apple[at:at + size]), bytes(mslc[at:at + size])
            if not leaf_equal(a, b, leaf["scalar"], ulp, eps):
                diffs.append(Diff(layout["name"], element, at, leaf["scalar"], a, b))
                if len(diffs) >= limit:
                    return diffs
    return diffs
