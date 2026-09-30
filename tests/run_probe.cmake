# Runs one regression probe: compiles PROBE with MSLC and checks the result
# against the probe's own expectation line, one of
#
#   // EXPECT: valid              compiles and passes spirv-val
#   // EXPECT: error <substring>  fails, with <substring> in the diagnostic
#
# A valid probe can also pin the disassembly, for a bug whose output validates
# but computes the wrong thing. Each line is one substring check:
#
#   // DISASM: <substring>        spirv-dis output contains <substring>
#   // DISASM-NOT: <substring>    spirv-dis output does not contain <substring>
#   // DISASM-MATCH: <regex>      spirv-dis output matches <regex>
#   // DISASM-NO-MATCH: <regex>   spirv-dis output does not match <regex>
#   // REFLECT: <substring>       the reflection JSON contains <substring>
#   // REFLECT-NOT: <substring>   the reflection JSON does not contain <substring>
#
# Invoked by ctest as:
#   cmake -DMSLC=... -DSPIRV_VAL=... -DSPIRV_DIS=... -DPROBE=... -DOUT_DIR=... -P run_probe.cmake

foreach(var MSLC PROBE OUT_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_probe.cmake: ${var} is not set")
	endif()
endforeach()

file(STRINGS "${PROBE}" expectations REGEX "^// EXPECT: ")
list(LENGTH expectations count)
if(NOT count EQUAL 1)
	message(FATAL_ERROR "${PROBE}: needs exactly one \"// EXPECT:\" line, found ${count}")
endif()
string(REGEX REPLACE "^// EXPECT: " "" expectation "${expectations}")

get_filename_component(name "${PROBE}" NAME_WE)
file(MAKE_DIRECTORY "${OUT_DIR}")
set(spv "${OUT_DIR}/${name}.spv")
file(REMOVE "${spv}")

# The reflection is requested on every probe, not only on the ones that check
# it, so that a probe which starts depending on it needs no harness change. It is
# the contract indium consumes, and nothing else in the suite looks at it.
set(reflection "${OUT_DIR}/${name}.json")
file(REMOVE "${reflection}")

execute_process(
	COMMAND "${MSLC}" -o "${spv}" --reflect "${reflection}" "${PROBE}"
	RESULT_VARIABLE status
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output
)

if(expectation STREQUAL "valid")
	if(NOT status EQUAL 0)
		message(FATAL_ERROR "${name}: expected to compile, mslc exited ${status}:\n${output}")
	endif()
	# Never skip: an unvalidated module must not count as a pass.
	if(NOT SPIRV_VAL)
		message(FATAL_ERROR "${name}: spirv-val not found; install SPIRV-Tools and re-run cmake")
	endif()
	execute_process(
		COMMAND "${SPIRV_VAL}" --target-env vulkan1.3 "${spv}"
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	if(NOT status EQUAL 0)
		message(FATAL_ERROR "${name}: spirv-val rejected the module:\n${output}")
	endif()

	file(STRINGS "${PROBE}" wanted REGEX "^// DISASM: ")
	file(STRINGS "${PROBE}" unwanted REGEX "^// DISASM-NOT: ")
	file(STRINGS "${PROBE}" wantedPatterns REGEX "^// DISASM-MATCH: ")
	file(STRINGS "${PROBE}" unwantedPatterns REGEX "^// DISASM-NO-MATCH: ")
	if(wanted OR unwanted OR wantedPatterns OR unwantedPatterns)
		if(NOT SPIRV_DIS)
			message(FATAL_ERROR "${name}: spirv-dis not found; install SPIRV-Tools and re-run cmake")
		endif()
		execute_process(
			COMMAND "${SPIRV_DIS}" "${spv}"
			RESULT_VARIABLE status
			OUTPUT_VARIABLE disassembly
			ERROR_VARIABLE disassembly
		)
		if(NOT status EQUAL 0)
			message(FATAL_ERROR "${name}: spirv-dis failed:\n${disassembly}")
		endif()
		foreach(line IN LISTS wanted)
			string(REGEX REPLACE "^// DISASM: " "" needle "${line}")
			string(FIND "${disassembly}" "${needle}" found)
			if(found EQUAL -1)
				message(FATAL_ERROR "${name}: disassembly lacks \"${needle}\":\n${disassembly}")
			endif()
		endforeach()
		foreach(line IN LISTS unwanted)
			string(REGEX REPLACE "^// DISASM-NOT: " "" needle "${line}")
			string(FIND "${disassembly}" "${needle}" found)
			if(NOT found EQUAL -1)
				message(FATAL_ERROR "${name}: disassembly contains \"${needle}\":\n${disassembly}")
			endif()
		endforeach()
		# A substring cannot say which instruction a trailing operand belongs to,
		# so a probe that has to name the operand of one particular opcode uses a
		# pattern. A bare None would otherwise be satisfied by any one of the
		# three control masks in the module.
		foreach(line IN LISTS wantedPatterns)
			string(REGEX REPLACE "^// DISASM-MATCH: " "" needle "${line}")
			string(REGEX MATCH "${needle}" found "${disassembly}")
			if(found STREQUAL "")
				message(FATAL_ERROR "${name}: disassembly does not match \"${needle}\":\n${disassembly}")
			endif()
		endforeach()
		foreach(line IN LISTS unwantedPatterns)
			string(REGEX REPLACE "^// DISASM-NO-MATCH: " "" needle "${line}")
			string(REGEX MATCH "${needle}" found "${disassembly}")
			if(NOT found STREQUAL "")
				message(FATAL_ERROR "${name}: disassembly matches \"${needle}\":\n${disassembly}")
			endif()
		endforeach()
	endif()

	file(STRINGS "${PROBE}" wantedReflection REGEX "^// REFLECT: ")
	file(STRINGS "${PROBE}" unwantedReflection REGEX "^// REFLECT-NOT: ")

	# A needle whose prefix is misspelled matches neither REGEX above, so it would
	# be ignored and the probe would pass without ever checking anything. The
	# search is deliberately wider than the form it then requires: anchoring the
	# search the way the extractors are anchored would make it blind exactly where
	# they are, which is the case worth catching. The case is what rejects a tab
	# after "//", leading whitespace, and a missing colon. "Reflection:" in prose
	# is unaffected, since the search is case-sensitive.
	file(STRINGS "${PROBE}" reflectionNeedles REGEX "REFLECT")
	foreach(line IN LISTS reflectionNeedles)
		if(NOT line MATCHES "^// REFLECT(-NOT)?: .+")
			message(FATAL_ERROR "${name}: \"${line}\" is not a reflection needle; "
				"the prefix is \"// REFLECT: \" or \"// REFLECT-NOT: \"")
		endif()
	endforeach()

	if(wantedReflection OR unwantedReflection)
		if(NOT EXISTS "${reflection}")
			message(FATAL_ERROR "${name}: mslc wrote no reflection, so it cannot be checked")
		endif()
		file(READ "${reflection}" reported)
		foreach(line IN LISTS wantedReflection)
			string(REGEX REPLACE "^// REFLECT: " "" needle "${line}")
			string(FIND "${reported}" "${needle}" found)
			if(found EQUAL -1)
				message(FATAL_ERROR "${name}: reflection lacks \"${needle}\":\n${reported}")
			endif()
		endforeach()
		foreach(line IN LISTS unwantedReflection)
			string(REGEX REPLACE "^// REFLECT-NOT: " "" needle "${line}")
			string(FIND "${reported}" "${needle}" found)
			if(NOT found EQUAL -1)
				message(FATAL_ERROR "${name}: reflection contains \"${needle}\":\n${reported}")
			endif()
		endforeach()
	endif()
elseif(expectation MATCHES "^error (.+)$")
	set(needle "${CMAKE_MATCH_1}")
	if(status EQUAL 0)
		message(FATAL_ERROR "${name}: expected a compile error containing \"${needle}\", but it compiled")
	endif()
	# On a signal RESULT_VARIABLE is a string such as "Segmentation fault", so
	# only a numeric nonzero exit counts as a diagnosed error.
	if(NOT status MATCHES "^[1-9][0-9]*$")
		message(FATAL_ERROR "${name}: mslc crashed (${status}):\n${output}")
	endif()
	string(FIND "${output}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "${name}: expected a diagnostic containing \"${needle}\", got:\n${output}")
	endif()
else()
	message(FATAL_ERROR "${name}: unknown expectation \"${expectation}\"")
endif()
