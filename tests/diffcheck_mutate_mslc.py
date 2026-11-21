#!/usr/bin/env python3
"""An mslc stand-in that plants a miscompile, to prove the differential harness can see one.

Runs the real mslc ($MSLC_REAL), then applies the regex substitutions in $MUT (a JSON list of
[pattern, replacement]) to the disassembly of its output and reassembles it. With $MUT_STAGE
set, only invocations with that --stage are changed. A pattern that matches nothing exits 99,
so a mutant that stopped applying cannot pass as "the harness missed nothing".
"""

import json
import os
import re
import subprocess
import sys

args = sys.argv[1:]
real = subprocess.run([os.environ["MSLC_REAL"]] + args)
if real.returncode != 0:
    sys.exit(real.returncode)
stage = args[args.index("--stage") + 1] if "--stage" in args else None
if os.environ.get("MUT_STAGE") not in (None, stage):
    sys.exit(0)
out = args[args.index("-o") + 1]
asm = subprocess.run(["spirv-dis", out], capture_output=True, text=True, check=True).stdout
for pattern, replacement in json.loads(os.environ["MUT"]):
    asm, n = re.subn(pattern, replacement, asm, flags=re.M)
    if n == 0:
        print("mutate-mslc: pattern %r did not match" % pattern, file=sys.stderr)
        sys.exit(99)
tmp = out + ".mut.spvasm"
with open(tmp, "w") as f:
    f.write(asm)
subprocess.run(["spirv-as", "--target-env", "vulkan1.3", tmp, "-o", out], check=True)
