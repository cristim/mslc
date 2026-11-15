"""Comment directives a .metal file may carry for diffcheck.

  // RUN: grid=64 local=8 seed=1 cases=3 elems=256   run parameters for this file
  // DIFFCHECK: writes-zero                          every output is legitimately all zero
  // DIFFCHECK-RANGE: <buffer> <lo> <hi>             integer leaves of that buffer are drawn from lo..hi,
                                                     for a buffer the kernel uses as an index
  // DIFFCHECK-FLOAT-RANGE: <buffer> <lo> <hi>       float leaves of that buffer are drawn from the integers lo..hi,
                                                     for a buffer whose value the kernel converts to an index or
                                                     a loop bound (so the conversion is defined)
  // DIFFCHECK-UNUSED: <texture> <reason>            the kernel legitimately ignores this texture (otherwise a
                                                     texture the output does not depend on is a failure)
  // DIFFCHECK-EXCLUDE: <buffer> <reason>            skip a buffer's comparison; the reason
                                                     is required and is printed in the report

Graphics (a vertex and a fragment function drawn into offscreen targets):
  // RUN: vertex=vs fragment=fs width=32 height=32 instances=2   makes the file a graphics pair
  // DIFFCHECK-GEOMETRY: 2,3 22,3 22,13 ...          triangle list in pixel coordinates, "x,y" or "x,y,z"
  // DIFFCHECK-POSITION: <attribute>|none            the stage_in field that carries the position
  // DIFFCHECK: depth                                also read back a depth attachment
The excludable names are color0, color1, ... and depth.
"""

import re

RUN_KEYS = {"grid", "local", "seed", "cases", "elems", "width", "height", "instances"}
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


def parse(source):
    result = {"run": {}, "writes_zero": False, "exclude": {}, "range": {}, "float_range": {}, "unused": {}, "geometry": None, "position": None,
              "depth": False}
    for line in source.splitlines():
        m = re.match(r"\s*//\s*RUN:\s*(.*)$", line)
        if m:
            for pair in m.group(1).split():
                key, _, value = pair.partition("=")
                if key in RUN_NAMES and re.fullmatch(r"[A-Za-z_]\w*", value):
                    result["run"][key] = value
                    continue
                if key not in RUN_KEYS or not value.isdigit() or int(value) < 1:
                    raise ValueError("bad RUN parameter %r (a positive number; keys: %s; a name for %s)" % (
                        pair, ", ".join(sorted(RUN_KEYS)), ", ".join(sorted(RUN_NAMES))))
                result["run"][key] = int(value)
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
