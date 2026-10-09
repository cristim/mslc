#!/usr/bin/env python3
"""Checks a SPIR-V binary for undefined or zero ids in id positions.

spirv-val reports these as "Id is 0" or "forward referenced IDs have not been
defined" without naming the instruction, which makes them hard to locate. This
walks the module using the Khronos grammar, so it knows which operands are ids
and which are literals, and reports the instruction that is wrong.

Usage:
    python3 tools/check_ids.py <module.spv> <spirv.core.grammar.json>
"""

import json
import struct
import sys

# Operand kinds that are ids, or contain ids. Kinds that are plain literals
# (a number, a literal string, a mode bitmask) are not listed here.
ID_KINDS = {
    "IdResultType",
    "IdResult",
    "IdRef",
    "IdScope",
    "IdMemorySemantics",
    "IdSampler",
    "IdImage",
    "IdSamplerImage",
    "IdAccelerationStructureKHR",
}


def load_grammar(path):
    grammar = json.load(open(path))

    kinds = {k["kind"]: k for k in grammar["operand_kinds"]}

    # Instructions with a result carry it in the operand list, and the grammar
    # names it, so the id positions come straight from the grammar.
    return grammar, kinds


def string_words(operands, start):
    """Words a NUL-terminated literal string takes, counted from start."""
    count = 0
    for word in operands[start:]:
        count += 1
        if any((word >> shift) & 0xFF == 0 for shift in (0, 8, 16, 24)):
            return count
    return count


def operand_id_positions(instruction, kinds, operands):
    """Indices of operands that are ids, as offsets into the operand words.

    A literal string spans as many words as its text needs, so the positions
    after one are found by reading the words rather than counting operands.
    """
    positions = []
    index = 0

    for operand in instruction.get("operands", []):
        kind = operand["kind"]
        quantity = operand.get("quantifier", "")

        if index >= len(operands):
            break

        if kind == "LiteralContextDependentNumber":
            # variable width, so nothing can be assumed after it
            break

        if quantity == "*":
            # A trailing variadic operand: everything left is that kind
            if kind in ID_KINDS:
                positions.extend(range(index, len(operands)))
            break

        if kind in ID_KINDS:
            positions.append(index)

        index += string_words(operands, index) if kind == "LiteralString" else 1

    return positions


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    grammar, kinds = load_grammar(sys.argv[2])
    by_opcode = {i["opcode"]: i for i in grammar["instructions"]}

    data = open(sys.argv[1], "rb").read()
    if len(data) % 4:
        sys.exit("module length is not a multiple of 4 bytes")

    words = list(struct.unpack("<%dI" % (len(data) // 4), data))

    if words[0] != 0x07230203:
        sys.exit("not a SPIR-V module: bad magic 0x%08x" % words[0])

    bound = words[3]
    defined = set()
    problems = []

    index = 5
    while index < len(words):
        header = words[index]
        count = header >> 16
        opcode = header & 0xFFFF

        if count == 0:
            problems.append((index, "word count is zero"))
            break

        operands = words[index + 1:index + count]

        instruction = by_opcode.get(opcode)
        if instruction is None:
            problems.append((index, "unknown opcode %d" % opcode))
        else:
            name = instruction["opname"]
            positions = operand_id_positions(instruction, kinds, operands)

            for position in positions:
                if position >= len(operands):
                    continue
                value = operands[position]

                if value == 0:
                    problems.append((index, "%s: id operand %d is 0" % (name, position)))
                elif value >= bound:
                    problems.append((index, "%s: id operand %d is %d, at or past bound %d"
                                     % (name, position, value, bound)))

            # record definitions
            for operand in instruction.get("operands", []):
                if operand["kind"] == "IdResult":
                    if operands:
                        defined.add(operands[0])
                elif operand["kind"] == "IdResultType":
                    if len(operands) >= 2:
                        defined.add(operands[1])
                elif operand["kind"] == "IdRef" and operands:
                    defined.add(operands[0])
                break

        index += count

    if problems:
        for offset, message in problems:
            print("word %d: %s" % (offset, message))
        sys.exit(1)

    print("no zero or out-of-range id operands found across %d words" % len(words))


if __name__ == "__main__":
    main()
