"""Comment directives a .metal file may carry for diffcheck.

  // RUN: grid=64 local=8 seed=1 cases=3 elems=256   run parameters for this file
  // DIFFCHECK: writes-zero                          every output is legitimately all zero
  // DIFFCHECK-RANGE: <buffer> <lo> <hi>             integer leaves of that buffer are drawn from lo..hi,
                                                     for a buffer the kernel uses as an index
  // DIFFCHECK-FLOAT-RANGE: <buffer> <lo> <hi>       float leaves of that buffer are drawn from the integers lo..hi,
                                                     for a buffer whose value the kernel converts to an index or
                                                     a loop bound (so the conversion is defined)
  // DIFFCHECK-ULP: <buffer> <n>                    ULP bound for that buffer's float leaves, replacing the one chosen
                                                     from the builtins the source calls (0 = bit-exact); an attachment
                                                     name in graphics. Tightens a buffer a loose neighbour would hide.
  // DIFFCHECK-TEXEL: <texture_buffer> <1|2|4>       components per texel of that texture_buffer (R, RG or RGBA;
                                                     4 when absent), so the one- and two-component formats are run
  // DIFFCHECK-UNUSED: <texture> <reason>            the kernel legitimately ignores this texture (otherwise a
                                                     texture the output does not depend on is a failure)
  // DIFFCHECK-EXCLUDE: <buffer> <reason>            skip a buffer's comparison; the reason
                                                     is required and is printed in the report

Graphics (a vertex and a fragment function drawn into offscreen targets):
  // RUN: vertex=vs fragment=fs width=32 height=32 instances=2 base_vertex=1 base_instance=2
  // DIFFCHECK-GEOMETRY: 2,3 22,3 22,13 ...          triangle list (or vertex list when DIFFCHECK-INDEXES is set) in pixel coordinates, "x,y" or "x,y,z"
  // DIFFCHECK-INDEXES: 0 1 2 0 2 3 ...              uint32 triangle index list into the geometry vertices
  // DIFFCHECK-POSITION: <attribute>|none            the stage_in field that carries the position
  // DIFFCHECK: depth                                also read back a depth attachment
The excludable names are color0, color1, ... and depth.
"""

import re

RUN_KEYS = {"grid", "local", "seed", "cases", "elems", "width", "height", "instances", "base_vertex", "base_instance"}
RUN_NONNEG = {"base_vertex", "base_instance"}
RUN_NAMES = {"vertex", "fragment"}


def _point(token):
    fields = token.split(",")
    try:
        values = [float(f) for f in fields]
    except ValueError:
        values = []
    if len(values) not in (2, 3):
        raise ValueError("DIFFCHECK-GEOMETRY point %r is not x,y or x,y,z" % token)
    return tuple(values) if len(values) == 3 else (values[0], values[1], 0.5)


def parse_run_pairs(pairs):
    """{key: value} for `key=value` tokens of a RUN line (or of a corpus list line)."""
    run = {}
    for pair in pairs:
        key, _, value = pair.partition("=")
        if key in RUN_NAMES and re.fullmatch(r"[A-Za-z_]\w*", value):
            run[key] = value
            continue
        minimum = 0 if key in RUN_NONNEG else 1
        if key not in RUN_KEYS or not re.fullmatch(r"[0-9]+", value) or int(value) < minimum:
            kind = "a non-negative integer" if key in RUN_NONNEG else "a positive number"
            raise ValueError("bad RUN parameter %r (%s; keys: %s; a name for %s)" % (
                pair, kind, ", ".join(sorted(RUN_KEYS)), ", ".join(sorted(RUN_NAMES))))
        run[key] = int(value)
    return run


def parse(source):
    result = {"run": {}, "writes_zero": False, "exclude": {}, "range": {}, "float_range": {}, "ulp": {}, "texel": {}, "unused": {}, "geometry": None, "indexes": None, "position": None,
              "depth": False}
    for line in source.splitlines():
        m = re.match(r"\s*//\s*RUN:\s*(.*)$", line)
        if m:
            result["run"].update(parse_run_pairs(m.group(1).split()))
            continue
        m = re.match(r"\s*//\s*DIFFCHECK:\s*writes-zero\s*$", line)
        if m:
            result["writes_zero"] = True
            continue
        m = re.match(r"\s*//\s*DIFFCHECK:\s*depth\s*$", line)
        if m:
            result["depth"] = True
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-POSITION:\s*(\w+)\s*$", line)
        if m:
            result["position"] = m.group(1)
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-GEOMETRY:\s*(.*)$", line)
        if m:
            result["geometry"] = [_point(t) for t in m.group(1).split()]
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-INDEXES:\s*(.*)$", line)
        if m:
            tokens = m.group(1).split()
            if not tokens:
                raise ValueError("DIFFCHECK-INDEXES needs at least one index")
            indexes = []
            for t in tokens:
                if not re.fullmatch(r"[0-9]+", t):
                    raise ValueError("DIFFCHECK-INDEXES index %r is not a non-negative integer" % t)
                v = int(t)
                if v > 0xFFFFFFFF:
                    raise ValueError("DIFFCHECK-INDEXES index %s exceeds uint32 range" % t)
                indexes.append(v)
            result["indexes"] = indexes
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-RANGE:\s*(\S+)\s+(-?\d+)\s+(-?\d+)\s*$", line)
        if m:
            lo, hi = int(m.group(2)), int(m.group(3))
            if lo > hi:
                raise ValueError("DIFFCHECK-RANGE %s: %d > %d" % (m.group(1), lo, hi))
            result["range"][m.group(1)] = (lo, hi)
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-FLOAT-RANGE:\s*(\S+)\s+(-?\d+)\s+(-?\d+)\s*$", line)
        if m:
            lo, hi = int(m.group(2)), int(m.group(3))
            if lo > hi:
                raise ValueError("DIFFCHECK-FLOAT-RANGE %s: %d > %d" % (m.group(1), lo, hi))
            result["float_range"][m.group(1)] = (lo, hi)
            continue
        if re.match(r"\s*//\s*DIFFCHECK-ULP:", line):
            m = re.match(r"\s*//\s*DIFFCHECK-ULP:\s*(\S+)\s+(\d+)\s*$", line)
            if not m:
                raise ValueError("bad DIFFCHECK-ULP line %r (want <buffer> <whole number>)" % line.strip())
            result["ulp"][m.group(1)] = int(m.group(2))
            continue
        if re.match(r"\s*//\s*DIFFCHECK-TEXEL:", line):
            m = re.match(r"\s*//\s*DIFFCHECK-TEXEL:\s*(\S+)\s+([124])\s*$", line)
            if not m:
                raise ValueError("bad DIFFCHECK-TEXEL line %r (want <texture_buffer> 1, 2 or 4)" % line.strip())
            result["texel"][m.group(1)] = int(m.group(2))
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-UNUSED:\s*(\S+)\s*(.*)$", line)
        if m:
            if not m.group(2).strip():
                raise ValueError("DIFFCHECK-UNUSED %s needs a reason" % m.group(1))
            result["unused"][m.group(1)] = m.group(2).strip()
            continue
        m = re.match(r"\s*//\s*DIFFCHECK-EXCLUDE:\s*(\S+)\s*(.*)$", line)
        if m:
            if not m.group(2).strip():
                raise ValueError("DIFFCHECK-EXCLUDE %s needs a reason" % m.group(1))
            result["exclude"][m.group(1)] = m.group(2).strip()
    return result
