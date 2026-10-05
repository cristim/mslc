# A source that uses macros must compile to the same module as the source with
# the macros expanded by hand. The disassembly is compared whole, so a macro that
# expands to something subtly different (a parenthesis, an order of evaluation)
# shows up as a different instruction stream.
#
# Invoked by ctest as:
#   cmake -DMSLC=... -DSPIRV_DIS=... -DMACRO=... -DEXPANDED=... -DOUT_DIR=... -P run_equivalence.cmake

foreach(var MSLC SPIRV_DIS MACRO EXPANDED OUT_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_equivalence.cmake: ${var} is not set")
	endif()
endforeach()

if(NOT SPIRV_DIS)
	message(FATAL_ERROR "spirv-dis not found; install SPIRV-Tools and re-run cmake")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

function(disassemble source label)
	set(spv "${OUT_DIR}/${label}.spv")
	file(REMOVE "${spv}")
	execute_process(COMMAND "${MSLC}" -o "${spv}" "${source}"
		RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE output)
	if(NOT status EQUAL 0)
		message(FATAL_ERROR "${source}: mslc exited ${status}:\n${output}")
	endif()
	execute_process(COMMAND "${SPIRV_DIS}" "${spv}"
		RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE text)
	if(NOT status EQUAL 0)
		message(FATAL_ERROR "${source}: spirv-dis failed:\n${text}")
	endif()
	set(${label}_TEXT "${text}" PARENT_SCOPE)
endfunction()

get_filename_component(name "${MACRO}" NAME)
disassemble("${MACRO}" macro)
disassemble("${EXPANDED}" expanded)

# An empty disassembly on both sides would compare equal without comparing anything.
string(LENGTH "${macro_TEXT}" length)
if(length LESS 100)
	message(FATAL_ERROR "${name}: the disassembly is empty or implausibly short")
endif()

if(NOT macro_TEXT STREQUAL expanded_TEXT)
	file(WRITE "${OUT_DIR}/macro.dis" "${macro_TEXT}")
	file(WRITE "${OUT_DIR}/expanded.dis" "${expanded_TEXT}")
	message(FATAL_ERROR "${name}: sources differ. See ${OUT_DIR}/macro.dis and expanded.dis")
endif()
