"""Buffer layout of a compiled entry point, read from the Offset, ArrayStride and MatrixStride
decorations of mslc's SPIR-V, and its cross-check against Apple's reflection.

mslc's reflection JSON carries no layout, and a run only reads the bytes under Apple's leaves
on both sides, so a difference that lands in padding or in a member the kernel never reads is
invisible in the outputs. Here the offsets of every scalar of one element, and the stride of a
runtime array, are compared directly.

A leaf is (byte offset, size, kind) with kind "float" or "int" (bool and integers alike). Leaves are
compared in flattening order (members in declaration order, array elements, matrix columns), not as
a set, so two members that swap offsets are a difference.
"""

import re

import compare


def _kind(scalar):
    return "float" if compare.SCALARS[scalar][1] == "float" else "int"


def apple_leaves(layout):
    return [(l["off"], compare.SCALARS[l["scalar"]][0], _kind(l["scalar"])) for l in layout["leaves"]]


class Module:
    """Types, constants and decorations of a `spirv-dis --raw-id` disassembly."""

    def __init__(self, text):
        self.types, self.consts, self.arraystride, self.offset, self.matrixstride = {}, {}, {}, {}, {}
        self.binding, self.variables, self.interfaces, self.major = {}, {}, {}, {}
        for line in text.splitlines():
            line = line.strip()
            if m := re.match(r'OpEntryPoint \w+ %\d+ "([^"]*)"\s*(.*)$', line):
                self.interfaces[m.group(1)] = re.findall(r"%(\d+)", m.group(2))
            elif m := re.match(r"OpDecorate %(\d+) ArrayStride (\d+)$", line):
                self.arraystride[m.group(1)] = int(m.group(2))
            elif m := re.match(r"OpMemberDecorate %(\d+) (\d+) Offset (\d+)$", line):
                self.offset[(m.group(1), int(m.group(2)))] = int(m.group(3))
            elif m := re.match(r"OpMemberDecorate %(\d+) (\d+) MatrixStride (\d+)$", line):
                self.matrixstride[(m.group(1), int(m.group(2)))] = int(m.group(3))
            elif m := re.match(r"OpMemberDecorate %(\d+) (\d+) (RowMajor|ColMajor)$", line):
                self.major[(m.group(1), int(m.group(2)))] = m.group(3)
            elif m := re.match(r"OpDecorate %(\d+) (DescriptorSet|Binding) (\d+)$", line):
                self.binding.setdefault(m.group(1), {})[m.group(2)] = int(m.group(3))
            elif m := re.match(r"%(\d+) = OpConstant %\d+ (\d+)$", line):
                self.consts[m.group(1)] = int(m.group(2))
            elif m := re.match(r"%(\d+) = OpVariable %(\d+) (\w+)", line):
                self.variables[m.group(1)] = (m.group(2), m.group(3))
            elif m := re.match(r"%(\d+) = (OpType\w+)(?: (.*))?$", line):
                self.types[m.group(1)] = (m.group(2), re.findall(r"%?\w+", m.group(3) or ""))

    def size_kind(self, ty):
        op, args = self.types[ty]
        if op == "OpTypeFloat":
            return int(args[0]) // 8, "float"
        if op == "OpTypeInt":
            return int(args[0]) // 8, "int"
        if op == "OpTypeBool":
            raise ValueError("a bool in a buffer has no SPIR-V layout")
        raise ValueError("%s is not a scalar" % op)

    def leaves(self, ty, base=0, matrix_stride=None):
        """Every scalar of the type placed at byte `base`, in Metal's flattening."""
        op, args = self.types[ty]
        if op in ("OpTypeFloat", "OpTypeInt"):
            size, kind = self.size_kind(ty)
            return [(base, size, kind)]
        if op == "OpTypeVector":
            size, kind = self.size_kind(args[0][1:])
            return [(base + i * size, size, kind) for i in range(int(args[1]))]
        if op == "OpTypeMatrix":
            if not isinstance(matrix_stride, int):
                raise ValueError("a %s" % (matrix_stride or "matrix without MatrixStride"))
            return [l for c in range(int(args[1])) for l in self.leaves(args[0][1:], base + c * matrix_stride)]
        if op == "OpTypeArray":
            stride = self.arraystride.get(ty)
            if stride is None:
                raise ValueError("an array without ArrayStride")
            return [l for i in range(self.consts[args[1][1:]])
                    for l in self.leaves(args[0][1:], base + i * stride, matrix_stride)]
        if op == "OpTypeStruct":
            out = []
            for i, member in enumerate(args):
                if (ty, i) not in self.offset:
                    raise ValueError("a struct member without Offset")
                out += self.leaves(member[1:], base + self.offset[(ty, i)], self.matrix_stride(ty, i))
            return out
        raise ValueError("%s has no buffer layout" % op)

    def matrix_stride(self, struct, member):
        """The MatrixStride of a struct member, or an error for a layout Metal does not use: Metal
        matrices are column major, so RowMajor (or no majorness) is a layout difference."""
        major = self.major.get((struct, member))
        if major != "ColMajor":
            return "%s matrix member" % (major or "undecorated")
        return self.matrixstride.get((struct, member))

    def pointee(self, ty):
        op, args = self.types[ty]
        if op != "OpTypePointer":
            raise ValueError("%s is not a pointer" % op)
        return args[1][1:]


def spirv_layouts(text, entry, buffers):
    """{metal index: {"stride": runtime array stride or None, "leaves": [...]}} for the Buffer
    bindings `buffers` of `entry`, or a ValueError naming what could not be read. The address
    block is the Uniform variable of the entry's interface at the bindings' set and binding;
    its member `member` points to the buffer's block, which holds one runtime array of elements
    or, for a `constant T &`, the element itself (a struct, or a bare scalar or vector)."""
    mod = Module(text)
    if entry not in mod.interfaces:
        raise ValueError("no entry point named %s" % entry)
    result = {}
    for b in buffers:
        want = {"DescriptorSet": b["descriptor"]["set"], "Binding": b["descriptor"]["binding"]}
        blocks = [v for v in mod.interfaces[entry] if v in mod.variables and mod.variables[v][1] == "Uniform"
                  and mod.binding.get(v) == want]
        if len(blocks) != 1:
            raise ValueError("expected one address block at set %d binding %d, found %d" % (
                want["DescriptorSet"], want["Binding"], len(blocks)))
        block = mod.pointee(mod.variables[blocks[0]][0])
        members = mod.types[block][1]
        block_ty = mod.pointee(members[b["member"]][1:])
        op, inner = mod.types[block_ty]
        if op == "OpTypeStruct" and len(inner) == 1 and mod.types[inner[0][1:]][0] == "OpTypeRuntimeArray":
            array = inner[0][1:]
            element = mod.types[array][1][0][1:]
            # A matrix element's MatrixStride is decorated on the block's one member, not on the array.
            result[b["metal_index"]] = {"stride": mod.arraystride.get(array),
                                        "leaves": (mod.leaves(element, 0, mod.matrix_stride(block_ty, 0)))}
        else:
            result[b["metal_index"]] = {"stride": None, "leaves": mod.leaves(block_ty)}
    return result


def cross_check(buffers, apple_layouts, mslc_layouts):
    """Findings where mslc's SPIR-V lays out a buffer differently from Apple: the stride of a
    runtime array and the byte offset of every scalar of one element."""
    findings = []
    for b in buffers:
        i = b["metal_index"]
        apple, mine = apple_layouts[i], mslc_layouts[i]
        if mine["stride"] is not None and mine["stride"] != apple["stride"]:
            findings.append("buffer(%d) %s: element stride is %d in mslc's SPIR-V and %d in Apple's layout" % (
                i, b["name"], mine["stride"], apple["stride"]))
        want, got = apple_leaves(apple), mine["leaves"]
        if want != got:
            first = next((w, g) for w, g in zip(want + [None], got + [None]) if w != g)
            findings.append("buffer(%d) %s: scalar layout differs, first at (offset, size, kind) Apple %s, mslc %s "
                            "(%d scalars in Apple, %d in mslc)" % (i, b["name"], first[0], first[1], len(want), len(got)))
    return findings
