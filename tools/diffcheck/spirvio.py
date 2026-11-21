"""What graphics mode needs from a compiled entry point: the types of its Input and Output
variables. mslc's reflection names the bindings but carries no types, and the vertex
descriptor and the render-target formats cannot be chosen without them.

Read from `spirv-dis --raw-id`, for the variables in the entry point's interface list only.
Only 32-bit float, int and uint scalars and vectors, and 16-bit float ones, are handled;
anything else is reported in "unsupported", which makes the file skipped with that reason.
"""

import re
import shutil
import subprocess

SCALARS = {("float", 32): "float", ("float", 16): "half", ("int", 32): "int", ("uint", 32): "uint"}


def disassemble(path):
    r = subprocess.run([shutil.which("spirv-dis") or "spirv-dis", "--raw-id", path], capture_output=True, text=True)
    if r.returncode != 0:
        raise ValueError("spirv-dis failed: " + r.stderr.strip())
    return r.stdout


def interface(text, entry):
    """{"inputs": {location: {"scalar", "comps", "flat"}}, "outputs": {...}, "frag_depth": bool,
    "unsupported": [reason]} for the entry point `entry` of a disassembly."""
    types, variables, location, flat, builtin = {}, {}, {}, set(), {}
    interface_ids = None
    for line in text.splitlines():
        line = line.strip()
        if (m := re.match(r'OpEntryPoint \w+ %(\d+) "([^"]*)"\s*(.*)$', line)) and m.group(2) == entry:
            interface_ids = re.findall(r"%(\d+)", m.group(3))
        elif m := re.match(r"OpDecorate %(\d+) Location (\d+)$", line):
            location[m.group(1)] = int(m.group(2))
        elif m := re.match(r"OpDecorate %(\d+) Flat$", line):
            flat.add(m.group(1))
        elif m := re.match(r"OpDecorate %(\d+) BuiltIn (\w+)$", line):
            builtin[m.group(1)] = m.group(2)
        elif m := re.match(r"%(\d+) = OpTypeFloat (\d+)", line):
            types[m.group(1)] = ("scalar", "float", int(m.group(2)))
        elif m := re.match(r"%(\d+) = OpTypeInt (\d+) (\d)", line):
            types[m.group(1)] = ("scalar", "int" if m.group(3) == "1" else "uint", int(m.group(2)))
        elif m := re.match(r"%(\d+) = OpTypeVector %(\d+) (\d+)", line):
            types[m.group(1)] = ("vector", m.group(2), int(m.group(3)))
        elif m := re.match(r"%(\d+) = OpTypePointer (\w+) %(\d+)", line):
            types[m.group(1)] = ("pointer", m.group(2), m.group(3))
        elif m := re.match(r"%(\d+) = OpVariable %(\d+) (\w+)", line):
            variables[m.group(1)] = (m.group(2), m.group(3))
    if interface_ids is None:
        raise ValueError("no entry point named %s in the module" % entry)

    result = {"inputs": {}, "outputs": {}, "frag_depth": False, "unsupported": []}
    for var in interface_ids:
        if builtin.get(var) == "FragDepth":
            result["frag_depth"] = True
        if var not in location:
            continue
        pointer, storage = variables[var]
        table = result["inputs"] if storage == "Input" else result["outputs"]
        pointee = types.get(types[pointer][2])
        comps = 1
        if pointee and pointee[0] == "vector":
            comps, pointee = pointee[2], types[pointee[1]]
        scalar = SCALARS.get((pointee[1], pointee[2])) if pointee and pointee[0] == "scalar" else None
        if scalar is None:
            result["unsupported"].append("%s location %d has a type that is not a 32-bit scalar or vector "
                                         "(or a half one)" % (storage.lower(), location[var]))
            continue
        table[location[var]] = {"scalar": scalar, "comps": comps, "flat": var in flat}
    return result
