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
# operations (+ - * / fma and the rounding functions) are not listed: they
# must match to the bit. A function that is not here is compared exactly.
# Values are MSL Specification section 8.4, Table 8.1 (single precision, safe math,
# which is how metalhost.mm compiles), the loosest value for float:
#   p369: acos asin acosh asinh cos cosh exp exp2 exp10 sin sinh cospi sinpi sincos log log2: 4
#   p369: atan atanh: 5; atan2: 6; fmod: 0
#   p370: log10: 4; pow powr: 16; tan tanpi: 6; tanh: 5; rsqrt: correctly rounded (unlisted)
# rsqrt and sqrt are the exceptions to the table: Apple is correctly rounded (Table 8.1)
# but lavapipe is not. Vulkan-Docs appendices/spirvenv.adoc allows 2 ULP for InverseSqrt,
# and sqrt inherits that bound (it is defined via inversesqrt). Issue #387: rsqrt differs
# by exactly 1 ULP in 3 cases (64 elements, seed 1); sqrt differs by exactly 1 ULP on 2-11
# elements per case (moderate, wide, edge; --grid 64 --local 8 --seed 1 --cases 3), while
# fma and plain multiply match exactly.
# mix smoothstep step length distance normalize dot cross are not in the table: starting
# values, to be tightened by measurement.
ULP_BOUNDS = {
    "sin": 4, "cos": 4, "tan": 6, "asin": 4, "acos": 4, "atan": 5, "atan2": 6,
    "sinh": 4, "cosh": 4, "tanh": 5, "asinh": 4, "acosh": 4, "atanh": 5,
    "sinpi": 4, "cospi": 4, "tanpi": 6, "sincos": 4,
    "exp": 4, "exp2": 4, "exp10": 4, "log": 4, "log2": 4, "log10": 4, "rsqrt": 2, "sqrt": 2,
    "pow": 16, "powr": 16, "fmod": 0, "mix": 4, "smoothstep": 4,
    "step": 0, "length": 4, "distance": 4, "normalize": 4, "dot": 4, "cross": 4,
}

# Half `x / y` and `1.0 / x` are specified correctly rounded (MSL Table 8.3, p373), but neither
# backend is: for a = 0x397b, 3.0h the exact quotient is 1870.667 ulp, correctly rounded 0x334f,
# Apple gives 0x334e and lavapipe (plain OpFDiv) 0x3350, 2 ULP apart (issue #389). Float divide
# is exact on both, so the bound only applies to a source that names `half` and divides, and in
# `compare()` it applies only to `half` leaves so `float` leaves in the same kernel stay bit-exact.
HALF_DIVIDE_ULP = 2


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


def _strip(source):
    # Comments and string literals are not calls.
    return re.sub(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"", " ", source, flags=re.S)


def _call_bound(text, scalar=None):
    has_half_div = "/" in text and (
        scalar == "half"
        or (scalar is None and bool(re.search(
            r"\bhalf[234]?\b|(?<![\w.])(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?h\b", text)))
    )
    bound = HALF_DIVIDE_ULP if has_half_div else 0
    for name, ulp in ULP_BOUNDS.items():
        if ulp > bound and re.search(r"(?<![\w.])(?:\w+::)*%s\s*\(" % name, text):
            bound = ulp
    return bound


def ulp_bound_for_source(source, scalar=None):
    """The loosest bound among the inexact functions the source calls."""
    return _call_bound(_strip(source), scalar=scalar)


_SCALAR = r"(?:bool|char|uchar|short|ushort|int|uint|long|ulong|half|float)[234]?"
_SCALAR_ONLY = r"(?:bool|char|uchar|short|ushort|int|uint|long|ulong|half|float)"
_DECL = re.compile(r"^(?:const\s+)?(%s)\s+(\w+)\s*(?:=\s*(.*))?$" % _SCALAR, re.S)
_STORE = re.compile(r"^(\w+)\s*((?:\[[^\[\]]*\]\s*)+(?:\.\s*[A-Za-z_]\w*\s*)*)?([-+*/%]?=(?!=))\s*(.*)$", re.S)
_FUNCTION = re.compile(r"^\s*(?:(?:kernel|vertex|fragment)\s+)\w[\w:<>]*\s+\w+\s*\(([^(){};]*)\)\s*\{([^{}]*)\}\s*$", re.S)
_BUFFER_LIKE = re.compile(r"[*&]|\b(?:device|constant|threadgroup|texture\w*|sampler|depth\w*)\b")
_UNSAFE_EXPR = (
    re.compile(r"[\w\])]\s*->\s*[A-Za-z_]"),  # pointer member access
    re.compile(r"(?<!&)&(?!&)"),  # address-of, reference
    re.compile(r"\b(?:\w*_cast|as_type\w*|decltype|auto|sizeof|typeof)\b"),
    re.compile(r"\([^()]*\*\s*\)"),  # pointer cast
    re.compile(r"\(\s*(?:const\s+)?%s\s*\)" % _SCALAR),  # C-style value cast
)


_STRUCT = re.compile(r"^\s*struct\s+(\w+)\s*\{\s*((?:%s\s+\w+\s*;\s*)+)\}\s*;\s*" % _SCALAR_ONLY)


def _simple_expr(expr, buffers, struct_fields):
    """No way to reach a buffer except by a subscripted read (or `.field` of a scalar struct field), and no hidden aliasing or casts."""
    if any(p.search(expr) for p in _UNSAFE_EXPR) or _split_comma(expr):
        return False
    scrubbed = re.sub(r"\b(\w+)\s*\[[^\[\]]*\]\s*\.\s*([A-Za-z_]\w*)(?!\s*[\.\[(])",
                      lambda m: " " if m.group(2) in struct_fields.get(m.group(1), ()) else m.group(0), expr)
    if re.search(r"[\w\])]\s*\.\s*[A-Za-z_]", scrubbed):
        return False
    for inner in re.findall(r"\[([^\[\]]*)\]", expr):
        if not _simple_expr(inner, buffers, struct_fields):
            return False
    return not any(re.search(r"(?<![\w.])%s\b(?!\s*\[)" % re.escape(b), expr) for b in buffers)


def _split_comma(text):
    depth = 0
    for ch in text:
        depth += ch in "(["
        depth -= ch in ")]"
        if ch == "," and depth == 0:
            return True
    return False


def _access_paths(text, buffers, locals_):
    """Every local or buffer (or `buffer.field`) read by `text`, plus any inner subscript identifiers."""
    paths = set()
    def sub(m):
        base, chain = m.group(1), m.group(2)
        for inner in re.findall(r"\[([^\[\]]*)\]", chain):
            paths.update(_access_paths(inner, buffers, locals_))
        fields = re.findall(r"\.\s*([A-Za-z_]\w*)", chain)
        if base in buffers:
            paths.add("%s.%s" % (base, ".".join(fields)) if fields else base)
        elif base in locals_:
            paths.add(base)
        return " "
    rest = re.sub(r"\b(\w+)((?:\s*\[[^\[\]]*\])+(?:\s*\.\s*[A-Za-z_]\w*)*)", sub, text)
    for ident in re.findall(r"\w+", rest):
        if ident in buffers or ident in locals_:
            paths.add(ident)
    return paths


def ulp_bounds_by_root(source):
    """{root: ULP bound} for what each kernel buffer parameter (`b` or `b.field`) or scalar local can hold,
    so a buffer or struct field written from sqrt does not inherit the bound of an unrelated pow elsewhere
    in the shader.

    An allowlist, deliberately looser than a precise analysis: the split applies only when the source
    is one entry point (optionally preceded by plain struct definitions of scalar fields)
    whose body is nothing but scalar or vector declarations and plain
    `local[...] = rhs;`, `buffer[...] = rhs;` or `buffer[...].field = rhs;` stores, with no braces,
    pointer `->`, method calls, address-of, cast, `auto`/`decltype`/`using`/`typedef`, pointer or
    reference, helper function, or bare buffer name in an expression. Any other source returns {} and
    callers use the whole-source bound, which is never tighter than the truth. A root's bound is the
    loosest bound among the right-hand sides stored to it and every other root those sides read."""
    stripped = re.sub(r"(?m)^\s*#\s*(?:include|pragma)\b.*$", " ", _strip(source))
    stripped = re.sub(r"\busing\s+namespace\s+metal\s*;", " ", stripped)
    stripped = re.sub(r"\[\[.*?\]\]", "", stripped, flags=re.S)
    struct_defs, struct_scalars = {}, {}
    while sm := _STRUCT.match(stripped):
        fields = [f.split() for f in sm.group(2).split(";") if f.strip()]
        struct_defs[sm.group(1)] = {w[-1] for w in fields}
        struct_scalars[sm.group(1)] = {w[-1]: w[0] for w in fields}
        stripped = stripped[sm.end():]
    m = _FUNCTION.match(stripped)
    if m is None:
        return {}
    buffers, struct_fields, target_scalar = set(), {}, {}
    for param in re.split(r",", m.group(1)):
        if param.strip() and _BUFFER_LIKE.search(param):
            ident = re.findall(r"\w+", param)
            if not ident:
                return {}
            buffers.add(ident[-1])
            if param.count("*") == 1 and "[" not in param and "&" not in param:
                for t in ident[:-1]:
                    if t in struct_defs:
                        struct_fields[ident[-1]] = struct_defs[t]
                        for fn, sc in struct_scalars[t].items():
                            target_scalar["%s.%s" % (ident[-1], fn)] = sc
                    elif re.match(r"^%s$" % _SCALAR, t):
                        target_scalar[ident[-1]] = re.sub(r"[234]$", "", t)
    stores, locals_ = [], set()
    for stmt in m.group(2).split(";"):
        if not stmt.strip():
            continue
        d = _DECL.match(stmt.strip())
        if d:
            decl_type, name, init = d.group(1), d.group(2), d.group(3)
            if name in buffers or name in locals_ or (init is not None and not _simple_expr(init, buffers, struct_fields)):
                return {}
            locals_.add(name)
            target_scalar[name] = re.sub(r"[234]$", "", decl_type)
            if init is not None:
                stores.append((name, init))
            continue
        st = _STORE.match(stmt.strip())
        if st is None or not _simple_expr(st.group(4), buffers, struct_fields):
            return {}
        root, subs, op, rhs_expr = st.group(1), (st.group(2) or "").strip(), st.group(3), st.group(4)
        inners = re.findall(r"\[([^\[\]]*)\]", subs)
        for inner in inners:
            if not _simple_expr(inner, buffers, struct_fields):
                return {}
        fields = re.findall(r"\.\s*([A-Za-z_]\w*)", subs)
        if root in locals_:
            if fields:
                return {}
            target = root
        elif root in buffers and subs and (not fields or (len(inners) == 1 and len(fields) == 1 and fields[0] in struct_fields.get(root, ()))):
            target = "%s.%s" % (root, fields[0]) if fields else root
        else:
            return {}
        stores.append((target, ("/ " if op == "/=" else "") + rhs_expr + " " + " ".join(inners)))
    stores_set = {t for t, _ in stores}
    if any(any(other != t and other.startswith(t + ".") for other in stores_set) for t in stores_set):
        return {}
    bound = {target: 0 for target, _ in stores}
    for target, rhs in stores:
        read_half = any(target_scalar.get(p) == "half" for p in _access_paths(rhs, buffers, locals_))
        sc = "half" if (target_scalar.get(target) == "half" or read_half) else target_scalar.get(target)
        bound[target] = max(bound[target], _call_bound(rhs, scalar=sc))
    changed = True
    while changed:
        changed = False
        for target, rhs in stores:
            for path in _access_paths(rhs, buffers, locals_):
                for cand, val in list(bound.items()):
                    if (cand == path or cand.startswith(path + ".") or path.startswith(cand + ".")) and val > bound[target]:
                        bound[target] = val
                        changed = True
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


def leaf_ulp(lay, leaf, explicit, by_root, default, float_default=None):
    """The ULP bound for one leaf of `lay`: an explicit DIFFCHECK-ULP on the buffer wins, then a per-field
    or per-buffer bound from `by_root`, then `default` (`float_default` when given and the leaf is not half)."""
    name = lay["name"]
    if name in explicit:
        return explicit[name]
    field = leaf.get("field")
    if field and "%s.%s" % (name, field) in by_root:
        return by_root["%s.%s" % (name, field)]
    if field and any(k.startswith(name + ".") for k in by_root):
        return 0
    fallback = default if (float_default is None or leaf["scalar"] == "half") else float_default
    return by_root.get(name, fallback)


def compare(layout, apple, mslc, ulp=0, limit=16, eps=0.0, ulp_by_root=None, explicit_ulp=None, float_ulp=None):
    """Compare two buffers of one layout; returns up to `limit` Diffs.

    layout: {"name", "stride", "leaves": [{"off", "scalar", ...}]}. Both buffers must have
    the same length, a multiple of the stride.
    """
    if len(apple) != len(mslc):
        raise ValueError("buffers differ in length: %d vs %d" % (len(apple), len(mslc)))
    diffs = []
    stride = layout["stride"]
    for element in range(len(apple) // stride):
        base = element * stride
        for leaf in layout["leaves"]:
            if ulp_by_root or explicit_ulp or float_ulp is not None:
                bound = leaf_ulp(layout, leaf, explicit_ulp or {}, ulp_by_root or {}, ulp, float_ulp)
            else:
                bound = ulp
            size = SCALARS[leaf["scalar"]][0]
            at = base + leaf["off"]
            a, b = bytes(apple[at:at + size]), bytes(mslc[at:at + size])
            if not leaf_equal(a, b, leaf["scalar"], bound, eps):
                diffs.append(Diff(layout["name"], element, at, leaf["scalar"], a, b))
                if len(diffs) >= limit:
                    return diffs
    return diffs
