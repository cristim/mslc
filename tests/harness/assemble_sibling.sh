#!/bin/sh
# Fake compiler for the harness self-tests: ignores the source and assembles
# the .spvasm next to the last argument (the probe) into the -o file, so a probe
# can be run against SPIR-V that mslc itself would never emit. $SPIRV_AS names
# the assembler.
while [ $# -gt 1 ]; do
	if [ "$1" = "-o" ]; then out=$2; fi
	shift
done
exec "$SPIRV_AS" --target-env vulkan1.3 "${1%.metal}.spvasm" -o "$out"
