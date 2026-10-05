# A source that uses macros must compile to the same module as the source with
# the macros expanded by hand. The disassembly is compared whole, so a macro that
# expands to something subtly different (a parenthesis, an order of evaluation)
# shows up as a different instruction stream. The reflection is compared too: a
# buffer index lives there and nowhere in the instructions, so an enumerator that
# folded to the wrong index would otherwise go unseen.
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
	set(json "${OUT_DIR}/${label}.json")
	file(REMOVE "${json}")
	execute_process(COMMAND "${MSLC}" -o "${spv}" --reflect "${json}" "${source}"
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
	file(READ "${json}" reflection)
	set(${label}_REFLECTION "${reflection}" PARENT_SCOPE)
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

if(NOT macro_REFLECTION STREQUAL expanded_REFLECTION)
	file(WRITE "${OUT_DIR}/macro.json" "${macro_REFLECTION}")
	file(WRITE "${OUT_DIR}/expanded.json" "${expanded_REFLECTION}")
	message(FATAL_ERROR "${name}: reflection differs. See ${OUT_DIR}/macro.json and expanded.json")
endif()
