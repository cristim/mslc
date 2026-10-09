#!/usr/bin/env python3
"""tools/check_ids.py on a module whose OpEntryPoint name spans several words.

usage: check_ids_test.py <mslc> <check_ids.py> <grammar.json> <source.metal>

The module is valid, so the helper has to accept it. A copy with an interface
id of the entry point moved past the bound is the control: the helper has to
report it, or the first check proves nothing.
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

OP_ENTRY_POINT = 15


def run_checker(checker, grammar, module):
    return subprocess.run([sys.executable, checker, str(module), grammar],
                          capture_output=True, text=True)


def entry_point_interface_word(words):
    """Index of the first interface id word of the first OpEntryPoint."""
    index = 5
    while index < len(words):
        count = words[index] >> 16
        if words[index] & 0xFFFF == OP_ENTRY_POINT:
            # word 1 execution model, word 2 function id, then the name
            position = index + 3
            while True:
                word = words[position]
                position += 1
                if any((word >> shift) & 0xFF == 0 for shift in (0, 8, 16, 24)):
                    break
            if position >= index + count:
                sys.exit("the entry point lists no interface id to corrupt")
            return position
        index += count
    sys.exit("no OpEntryPoint found")


def main():
    mslc, checker, grammar, source = sys.argv[1:5]
    with tempfile.TemporaryDirectory() as directory:
        module = Path(directory) / "module.spv"
        compiled = subprocess.run([mslc, "-o", str(module), source], capture_output=True, text=True)
        if compiled.returncode != 0:
            sys.exit("mslc failed: " + compiled.stderr)

        words = list(struct.unpack("<%dI" % (module.stat().st_size // 4), module.read_bytes()))
        accepted = run_checker(checker, grammar, module)
        if accepted.returncode != 0:
            sys.exit("a valid multiword entry point name was reported:\n" + accepted.stdout)

        words[entry_point_interface_word(words)] = words[3] + 5
        corrupted = Path(directory) / "corrupted.spv"
        corrupted.write_bytes(struct.pack("<%dI" % len(words), *words))
        rejected = run_checker(checker, grammar, corrupted)
        if rejected.returncode == 0 or "OpEntryPoint" not in rejected.stdout:
            sys.exit("an interface id past the bound was not reported:\n" + rejected.stdout)


if __name__ == "__main__":
    main()
