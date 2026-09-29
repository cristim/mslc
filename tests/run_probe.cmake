# Runs one regression probe: compiles PROBE with MSLC and checks the result
# against the probe's own expectation line, one of
#
#   // EXPECT: valid              compiles and passes spirv-val
#   // EXPECT: error <substring>  fails, with <substring> in the diagnostic
#
# Invoked by ctest as: cmake -DMSLC=... -DSPIRV_VAL=... -DPROBE=... -DOUT_DIR=... -P run_probe.cmake

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

execute_process(
	COMMAND "${MSLC}" -o "${spv}" "${PROBE}"
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
