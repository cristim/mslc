"""Checks the driver applies around a comparison: reflection cross-check, the negative
control's perturbation, and the "something was actually written" test."""

import compare
import json
import struct


def cross_check(mslc_buffers, apple):
    """Findings where mslc's reflection and Apple's disagree about the buffers."""
    findings = []
    by_index = {b["index"]: b for b in apple["buffers"]}
    for b in mslc_buffers:
        a = by_index.get(b["metal_index"])
        if a is None:
            findings.append("mslc binds buffer(%d) %s, Apple's reflection has no buffer at that index" % (b["metal_index"], b["name"]))
        elif a["name"] != b["name"]:
            findings.append("buffer(%d) is %s in mslc and %s in Apple" % (b["metal_index"], b["name"], a["name"]))
    extra = set(by_index) - {b["metal_index"] for b in mslc_buffers}
    for i in sorted(extra):
        findings.append("Apple reflects buffer(%d) %s, mslc does not bind it" % (i, by_index[i]["name"]))
    return findings


def perturb(layouts, outputs, reference=None):
    """A copy of `outputs` with one leaf made different, for the negative control.
    The new value is never equal to `reference`'s leaf either (when given), so the control
    cannot pass by turning mslc's value into Apple's: an infinity with one bit flipped is a
    NaN, which equals an Apple NaN. Returns (copy, buffer, offset), or None when no buffer
    has a leaf to change."""
    for index, layout in sorted(layouts.items()):
        if layout["access"] == "read" or not layout["leaves"] or not outputs[index]:
            continue
        leaf = layout["leaves"][0]
        size = compare.SCALARS[leaf["scalar"]][0]
        scalar = leaf["scalar"]
        raw = bytes(outputs[index][leaf["off"]:leaf["off"] + size])
        ref = bytes(reference[index][leaf["off"]:leaf["off"] + size]) if reference else None
        for n in range(1, 64):
            new = (int.from_bytes(raw, "little") ^ n).to_bytes(size, "little")
            if compare.SCALARS[scalar][1] == "float":
                # Three finite values, so one is neither the original nor Apple's.
                new = struct_float(scalar, n)
            if not compare.leaf_equal(new, raw, scalar, 0) and (ref is None or not compare.leaf_equal(new, ref, scalar, 0)):
                break
        copy = {i: bytearray(o) for i, o in outputs.items()}
        copy[index][leaf["off"]:leaf["off"] + size] = new
        return copy, layout["name"], leaf["off"]
    return None


def struct_float(scalar, n):
    """The n-th of a few distinct finite values (1.0, 2.0, 3.0, ...) as float or half bytes."""
    return struct.pack("<f" if scalar == "float" else "<e", float(n))


def written_nonzero(layouts, before, after):
    """True when some leaf of a writable buffer changed to a non-zero value. Counting only
    changed leaves keeps untouched input elements from making a kernel that writes zeros
    (or nothing) look like it produced output."""
    for index, layout in layouts.items():
        if layout["access"] == "read":
            continue
        for element in range(len(after[index]) // max(layout["stride"], 1)):
            for leaf in layout["leaves"]:
                size = compare.SCALARS[leaf["scalar"]][0]
                at = element * layout["stride"] + leaf["off"]
                new = bytes(after[index][at:at + size])
                if any(new) and new != bytes(before[index][at:at + size]):
                    return True
    return False


def control_holds(groups, mine, apple):
    """True when every non-empty output group compares unequal after a single-leaf perturbation."""
    held = []
    for g in groups:
        fired = perturb(g, mine, apple)
        if fired:
            copy, name, _ = fired
            index = next(i for i, l in g.items() if l["name"] == name)
            held.append(bool(compare.compare(g[index], apple[index], copy[index], 0)))
    return bool(held) and all(held)


def verdict(cases, nonzero, writes_zero, unused=()):
    """(status, reason) for the cases of one kernel. A control that did not fire outranks a
    match, because a comparison that cannot fail proves nothing. `unused` names the textures the
    output does not depend on and the file did not declare."""
    if any(not c["negative_control"] for c in cases):
        return "control-failed", "the perturbed copy compared equal to Apple's output, or nothing could be perturbed"
    first = next((c for c in cases if c["diff_count"]), None)
    if first:
        return "mismatched", "first difference: %s" % json.dumps(first["diffs"][0])
    if unused:
        return "unused-texture", ("changing the texels of %s left every compared output the same on Apple's side, so "
                                  "the texture binding is untested (declare `// DIFFCHECK-UNUSED: <name> <reason>` "
                                  "if the kernel ignores it)" % ", ".join(unused))
    if not nonzero and not writes_zero:
        return "zero-output", ("no output leaf was written to a non-zero value on either backend "
                               "(declare `// DIFFCHECK: writes-zero` if that is right)")
    return "matched", None
