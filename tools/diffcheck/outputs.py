"""A fragment function's outputs as the Metal source declares them.

Graphics mode must not take the set of attachments from mslc's own module: an output mslc drops
or retypes would then never be configured, never be read back and never be compared, and the
run would pass. The source is the record that is independent of the compiler under test, and
cross_check reports every disagreement with mslc's SPIR-V (read by spirvio) as a finding.

Handled: a fragment function returning void, a scalar or vector of float, half, int or uint
(colour 0), or a struct whose members carry [[color(n)]] and [[depth(...)]]. Anything else
(a typedef, a struct from an #include, other scalar types) raises Unsupported, which skips the file.
"""

import re

TYPE = re.compile(r"(float|half|int|uint)([234]?)$")


class Unsupported(Exception):
    pass


def _type(name):
    m = TYPE.match(name)
    if not m:
        raise Unsupported("fragment output type %s is not a float, half, int or uint scalar or vector" % name)
    return m.group(1), int(m.group(2) or 1)


def fragment_outputs(source, fragment):
    """({colour index: (scalar, components)}, writes_depth) for the function named `fragment`."""
    text = re.sub(r"//[^\n]*|/\*.*?\*/", " ", source, flags=re.S)
    m = re.search(r"(?:\bfragment|\[\[\s*fragment\s*\]\])\s+([\w:]+)\s+%s\s*\(" % re.escape(fragment), text)
    if not m:
        raise Unsupported("cannot read the return type of fragment function %s from the source" % fragment)
    ret = m.group(1)
    if ret == "void":
        return {}, False
    if TYPE.match(ret):
        return {0: _type(ret)}, False
    body = re.search(r"\bstruct\s+%s\s*\{(.*?)\}\s*;" % re.escape(ret), text, flags=re.S)
    if not body:
        raise Unsupported("the return type %s of %s is not defined in this file" % (ret, fragment))
    colors, depth = {}, False
    for member in body.group(1).split(";"):
        c = re.search(r"\[\[\s*color\s*\(\s*(\d+)\s*\)\s*\]\]", member)
        if c:
            decl = re.match(r"\s*([\w:]+)\s+\w+", member)
            colors[int(c.group(1))] = _type(decl.group(1)) if decl else _type("?")
        elif re.search(r"\[\[\s*depth\s*\(", member):
            depth = True
    if sorted(colors) != list(range(len(colors))) or len(colors) > 8:
        raise Unsupported("colour outputs %s are not 0..n-1" % sorted(colors))
    return colors, depth


def cross_check(colors, depth, interface):
    """Findings where mslc's module disagrees with the source about the fragment outputs."""
    findings = []
    theirs = {i: (o["scalar"], o["comps"]) for i, o in interface["outputs"].items()}
    if set(theirs) != set(colors):
        findings.append("the source writes colour outputs %s, mslc's module %s" % (sorted(colors), sorted(theirs)))
    for i in sorted(set(theirs) & set(colors)):
        # mslc writes a half output as a float; the render target is the source's half format.
        same = theirs[i] == colors[i] or (colors[i][0] == "half" and theirs[i] == ("float", colors[i][1]))
        if not same:
            findings.append("colour %d is %s%d in the source and %s%d in mslc's module" % (i, *colors[i], *theirs[i]))
    if depth != interface["frag_depth"]:
        findings.append("the source %s a depth output, mslc's module %s" % (
            "writes" if depth else "has no", "writes one" if interface["frag_depth"] else "does not"))
    return findings
