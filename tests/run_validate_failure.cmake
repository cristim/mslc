# Checks that "mslc -V" exits nonzero when spirv-val rejects the module. PATH
# points at a spirv-val stand-in that always fails, since mslc emits nothing
# invalid on purpose.
#
# Invoked by ctest as: cmake -DMSLC=... -DFAKE_DIR=... -DPROBE=... -DOUT=... -P run_validate_failure.cmake

execute_process(
	COMMAND "${CMAKE_COMMAND}" -E env "PATH=${FAKE_DIR}" "${MSLC}" -V -o "${OUT}" "${PROBE}"
	RESULT_VARIABLE status
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output
)

if(status EQUAL 0)
	message(FATAL_ERROR "mslc -V exited 0 although spirv-val failed:\n${output}")
endif()
string(FIND "${output}" "spirv-val: FAIL" found)
if(found EQUAL -1)
	message(FATAL_ERROR "mslc -V failed for another reason than validation:\n${output}")
endif()
