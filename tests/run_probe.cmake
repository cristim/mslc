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
#   // DISASM-ORDER: <text>      a later DISASM-ORDER line appears after this one
#   // DISASM-MATCH: <regex>      spirv-dis output matches <regex>
#   // DISASM-NO-MATCH: <regex>   spirv-dis output does not match <regex>
#   // DISASM-ASCENDING: <text>   an instruction starting with <text> lists ids
#                                 that rise left to right, for a value built lane by
#                                 lane in lane order
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
	file(STRINGS "${PROBE}" ascendingNeedles REGEX "^// DISASM-ASCENDING: ")
	# Collected on whitespace after the prefix rather than one literal space, so a
	# tab-separated needle is collected instead of silently skipped. A line whose
	# prefix is not followed by whitespace is not a needle at all and is reported
	# below, since the REGEX above cannot see it.
	file(STRINGS "${PROBE}" orderNeedles REGEX "^// DISASM-ORDER:[ \t]")
	file(STRINGS "${PROBE}" malformedOrder REGEX "^// *DISASM-ORDER")
	if(wanted OR unwanted OR wantedPatterns OR unwantedPatterns OR ascendingNeedles OR orderNeedles OR malformedOrder)
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
		# A regex cannot compare two ids, so lane order is checked here: the lanes
		# are emitted in order, so the ids a construct lists must rise.
		foreach(line IN LISTS ascendingNeedles)
			string(REGEX REPLACE "^// DISASM-ASCENDING: " "" needle "${line}")
			string(REGEX MATCH "${needle}( %[0-9]+)+" found "${disassembly}")
			if(found STREQUAL "")
				message(FATAL_ERROR "${name}: disassembly lacks an instruction \"${needle}\" with ids:\n${disassembly}")
			endif()
			string(REGEX REPLACE "^${needle}" "" idText "${found}")
			string(REGEX MATCHALL "[0-9]+" ids "${idText}")
			set(previous -1)
			foreach(id IN LISTS ids)
				if(NOT id GREATER previous)
					message(FATAL_ERROR "${name}: the ids of \"${found}\" do not rise, so the lanes are out of order")
				endif()
				set(previous ${id})
			endforeach()
		endforeach()
		# Every other needle answers whether a string is present, which cannot say
		# where it is relative to another string. CMake's REGEX MATCH does not span
		# a newline either, so a pattern cannot reach across two lines to say it.
		# Where the claim is about position, the two needles go in consecutive
		# DISASM-ORDER lines and the first has to come first in the disassembly.
		# A needle that is only whitespace is as vacuous as an empty one, and
		# string(FIND) finds a space in the disassembly's own indentation, so the
		# comparison would hold whatever the second needle said.
		foreach(line IN LISTS orderNeedles)
			string(REGEX REPLACE "^// DISASM-ORDER:[ \t]+" "" value "${line}")
			string(STRIP "${value}" value)
			if(value STREQUAL "")
				message(FATAL_ERROR "${name}: DISASM-ORDER needle is empty or only whitespace: \"${line}\"")
			endif()
		endforeach()
		# A DISASM-ORDER line the REGEX above did not collect is one whose prefix is
		# not followed by whitespace, so it is not a needle and would be ignored
		# along with the ordering claim it was meant to state.
		foreach(line IN LISTS malformedOrder)
			if(NOT line MATCHES "^// DISASM-ORDER:[ \t]")
				message(FATAL_ERROR "${name}: \"${line}\" is not a DISASM-ORDER needle; the prefix is \"// DISASM-ORDER: <text>\"")
			endif()
		endforeach()
		list(LENGTH orderNeedles orderCount)
		math(EXPR orderOdd "${orderCount} % 2")
		if(orderOdd)
			message(FATAL_ERROR "${name}: DISASM-ORDER takes two lines at a time, found ${orderCount}, which is not a whole number of pairs")
		endif()
		math(EXPR orderLast "${orderCount} - 1")
		if(orderCount GREATER 0)
			foreach(i RANGE 0 ${orderLast} 2)
				math(EXPR j "${i} + 1")
				list(GET orderNeedles ${i} firstLine)
				list(GET orderNeedles ${j} secondLine)
				string(REGEX REPLACE "^// DISASM-ORDER:[ \t]+" "" first "${firstLine}")
				string(REGEX REPLACE "^// DISASM-ORDER:[ \t]+" "" second "${secondLine}")
				# Defended again here rather than only above, because the pair is what
				# is compared and this is the point where the comparison is made.
				string(STRIP "${first}" first)
				string(STRIP "${second}" second)
				if(first STREQUAL "" OR second STREQUAL "")
					message(FATAL_ERROR "${name}: a DISASM-ORDER needle is empty: \"${firstLine}\" / \"${secondLine}\"")
				endif()
				string(FIND "${disassembly}" "${first}" firstAt)
				string(FIND "${disassembly}" "${second}" secondAt)
				if(firstAt EQUAL -1)
					message(FATAL_ERROR "${name}: disassembly lacks \"${first}\":\n${disassembly}")
				endif()
				if(secondAt EQUAL -1)
					message(FATAL_ERROR "${name}: disassembly lacks \"${second}\":\n${disassembly}")
				endif()
				if(NOT firstAt LESS secondAt)
					message(FATAL_ERROR "${name}: \"${first}\" is at ${firstAt} and \"${second}\" is at ${secondAt}, so the first does not come first:\n${disassembly}")
				endif()
			endforeach()
		endif()
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
		# A document a reader cannot parse is not a reflection, and every needle
		# below would still be satisfied by one with a syntax error in it. This
		# checks the one way mslc produced an unparseable document, which is a comma
		# before the closing bracket: string(JSON) accepts a trailing comma, so
		# asking it to parse the document cannot be the check. json.load does not:
		# "Illegal trailing comma before end of array".
		# All whitespace removed before looking, because a comma and the bracket it
		# precedes are on different lines and CMake's regex does not match across
		# one. Removing every space is safe for a check: whitespace cannot appear
		# inside a token, so "x, ]" and "x,]" mean the same thing here.
		string(REGEX REPLACE "[ \t\n\r]" "" tight "${reported}")
		string(FIND "${tight}" ",]" commaBeforeBracket)
		string(FIND "${tight}" ",}" commaBeforeBrace)
		if(NOT commaBeforeBracket EQUAL -1 OR NOT commaBeforeBrace EQUAL -1)
			message(FATAL_ERROR "${name}: reflection has a comma before a closing bracket, which a reader rejects:\n${reported}")
		endif()
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
