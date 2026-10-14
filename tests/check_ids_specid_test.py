#!/usr/bin/env python3
"""tools/check_ids.py on a module with several kernels.

usage: check_ids_specid_test.py <mslc> <check_ids.py> <grammar.json> <source.metal> <control.spvasm>

A module whose kernels share one set of SpecId decorations is what mslc has to
emit, so the helper has to accept it.

The control is a checked-in module with a second set, one per entry point, which
is what the compiler emitted before the constants were shared. spirv-val ACCEPTS
it, and a driver then picks one of the two constants for SpecId 0, so the helper
has to report it or the first check proves nothing. The control is checked in as
SPIR-V assembly rather than built, because the bug it stands for is that a
perfectly valid module is wrong.
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# OpDecorate, and the SpecId decoration value in the SPIR-V 1.6 grammar.
OP_DECORATE = 71
SPECID = 1


def specid_owners(module_bytes):
    """Maps each SpecId to the ids that claim it."""
    words = list(struct.unpack("<%dI" % (len(module_bytes) // 4), module_bytes))
    owners = {}
    index = 5
    while index < len(words):
        count = words[index] >> 16
        opcode = words[index] & 0xFFFF
        if count == 0:
            break
        operands = words[index + 1:index + count]
        if opcode == OP_DECORATE and len(operands) >= 3 and operands[1] == SPECID:
            owners.setdefault(operands[2], []).append(operands[0])
        index += count
    return owners


def run_checker(checker, grammar, module):
    return subprocess.run([sys.executable, checker, str(module), str(grammar)],
                          capture_output=True, text=True)


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    mslc, checker, grammar, source, control = sys.argv[1:6]
    grammar = Path(grammar)

    with tempfile.TemporaryDirectory() as work:
        work = Path(work)

        # The real case: several kernels sharing one set of SpecId constants.
        good = work / "good.spv"
        built = subprocess.run([mslc, "-o", str(good), source],
                               capture_output=True, text=True)
        if built.returncode != 0:
            sys.exit("mslc did not compile %s: %s%s" % (source, built.stdout, built.stderr))

        owners = specid_owners(good.read_bytes())
        for spec_id in sorted(owners):
            if len(owners[spec_id]) != 1:
                sys.exit("the compiler emitted %d constants for SpecId %d, so this test "
                         "is not testing what it claims" % (len(owners[spec_id]), spec_id))

        result = run_checker(checker, grammar, good)
        if result.returncode != 0:
            sys.exit("the helper rejected a module with one constant per SpecId:\n%s%s"
                     % (result.stdout, result.stderr))

        # The control: a second set of SpecId decorations, one per entry point.
        bad = work / "bad.spv"
        assembled = subprocess.run(["spirv-as", "--target-env", "vulkan1.3", control,
                                    "-o", str(bad)], capture_output=True, text=True)
        if assembled.returncode != 0:
            sys.exit("the control assembly did not build:\n%s" % assembled.stderr)

        # It has to be a valid module, or the check is not worth anything: the
        # whole point is that spirv-val misses this.
        validated = subprocess.run(["spirv-val", "--target-env", "vulkan1.3", str(bad)],
                                   capture_output=True, text=True)
        if validated.returncode != 0:
            sys.exit("the control is not valid, so it does not show that spirv-val misses "
                     "a duplicated SpecId:\n%s" % validated.stderr)

        bad_owners = specid_owners(bad.read_bytes())
        if not any(len(v) > 1 for v in bad_owners.values()):
            sys.exit("the control has no duplicated SpecId, so it does not test the check")

        result = run_checker(checker, grammar, bad)
        if result.returncode == 0:
            sys.exit("the helper accepted a module with two constants for one SpecId")

        print("ok: one SpecId set accepted, a duplicated set reported, and spirv-val "
              "accepts the duplicate")


if __name__ == "__main__":
    main()