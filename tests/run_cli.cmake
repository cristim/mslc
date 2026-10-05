# Exercises the CLI's own paths, which no probe reaches: a probe drives the
# library through run_probe.cmake and never the argument handling, and
# --dump-tokens is what a review reaches for when it has a lexing question, so
# it being unexercised meant every lexing investigation started by hand.
#
# Each case is a command and what it must print or return. A case that is not
# listed here is not checked.
#
# Invoked by ctest as: cmake -DMSLC=... -DWORK_DIR=... -P run_cli.cmake

foreach(var MSLC WORK_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_cli.cmake: ${var} is not set")
	endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(input "${WORK_DIR}/input.metal")
file(WRITE "${input}"
"kernel void k(device float *out [[buffer(0)]], uint i [[thread_position_in_grid]])\n{ out[i] = 1.0; }\n")

# run(<name> <expect-status> <expect-substring> <args...>)
#
# An expected status of 0 or a positive number is an exact exit code. A
# substring of "nonzero" means any failure, which is how the cases that must not
# succeed are written.
function(run name expected)
	set(args ${ARGN})
	execute_process(
		COMMAND "${MSLC}" ${args}
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	set(got "${status}")
	# "nonzero" is how a case that must fail is written, so that the case does not
	# pin an exit code the CLI is free to change.
	if(expected STREQUAL "nonzero")
		if(got EQUAL 0)
			message(FATAL_ERROR "${name}: exit 0, expected a failure:\n${output}")
		endif()
	elseif(NOT got EQUAL expected)
		message(FATAL_ERROR "${name}: exit ${got}, expected ${expected}:\n${output}")
	endif()
	set(LAST_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(run_expects name needle)
	string(FIND "${LAST_OUTPUT}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "${name}: output lacks \"${needle}\":\n${LAST_OUTPUT}")
	endif()
endfunction()

# The token stream. A fixed width with a hex offset and the token kind, which is
# what makes a lexing question answerable by reading it rather than by guessing.
run(dump_tokens 0 "--dump-tokens;--output;${WORK_DIR}/out.spv;${input}")
run_expects(dump_tokens "identifier           kernel")

# No arguments at all: the usage is printed and the exit is 2, which is what a
# caller can tell from a compile failure.
run(no_arguments 2)
run_expects(no_arguments "usage: mslc")

# A flag with no value. The exit is 2 for the same reason, with the usage shown.
run(missing_output 2 "-o")
run_expects(missing_output "usage: mslc")
run(missing_reflect 2 "--reflect")
run_expects(missing_reflect "usage: mslc")

# An input that does not exist is a failure, not an empty module.
run(absent_input nonzero "${WORK_DIR}/does-not-exist.metal")

# A file-scope declaration on its own parses, so the failure is the missing entry
# point rather than a parse error, and the message names what it looked for.
set(bad "${WORK_DIR}/bad.metal")
file(WRITE "${bad}" "constant float x = 1.0;\n")
run(no_entry_point nonzero "-o;${WORK_DIR}/bad.spv;${bad}")
run_expects(no_entry_point "declares no kernel, vertex or fragment entry point")

# -V runs the validator, and a valid module passes it. That is the success side
# of the case run_validate_failure.cmake checks the failure of.
run(validate_ok 0 "-V;-o;${WORK_DIR}/validated.spv;${input}")
run_expects(validate_ok "spirv-val: PASS")
