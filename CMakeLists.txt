cmake_minimum_required(VERSION 3.16)
project(mslc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The library is C++ but exposes a C ABI, so it has no C++ types in its public
# surface and can be built as a shared object for the Darling guest or used
# directly by the CLI.
add_library(mslc
	src/lexer.cpp
	src/preprocessor.cpp
	src/ast.cpp
	src/parser.cpp
	src/sema.cpp
	src/spirv.cpp
	src/mslc.cpp
)
target_include_directories(mslc
	PUBLIC include
	PRIVATE src
)
set_target_properties(mslc PROPERTIES
	POSITION_INDEPENDENT_CODE ON
)

# CLI driver. Kept separate from the library so the library stays embeddable.
add_executable(mslc-cli src/main.cpp)
target_link_libraries(mslc-cli PRIVATE mslc)
set_target_properties(mslc-cli PROPERTIES OUTPUT_NAME mslc)

# The corpus runner: compiles every .metal file given and reports which reach
# structurally valid SPIR-V. Progress on the subset is measured with this
# rather than asserted.
add_executable(mslc-corpus tools/corpus.cpp)
target_link_libraries(mslc-corpus PRIVATE mslc)

enable_testing()

# The library's preprocessing options on their own; the CLI always sets them
# together.
add_executable(mslc-api-test tests/preprocess_api.cpp)
target_link_libraries(mslc-api-test PRIVATE mslc)
add_test(NAME api/preprocess COMMAND mslc-api-test ${CMAKE_CURRENT_SOURCE_DIR}/tests/probes/include)

# Tests that validate SPIR-V fail, never skip, when this is not found.
find_program(SPIRV_VAL spirv-val)
if(NOT SPIRV_VAL)
	message(WARNING "spirv-val not found: the SPIR-V validating tests will fail. Install SPIRV-Tools.")
endif()
find_program(SPIRV_DIS spirv-dis)

add_test(NAME corpus COMMAND mslc-corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus.txt
	--spirv-val ${SPIRV_VAL})

set(MSLC_INDIUM_DIR "" CACHE PATH "indium checkout; enables the indium-corpus test")
if(MSLC_INDIUM_DIR)
	add_test(NAME indium-corpus COMMAND mslc-corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/indium_corpus.txt
		--root ${MSLC_INDIUM_DIR} --spirv-val ${SPIRV_VAL})
else()
	add_test(NAME indium-corpus COMMAND ${CMAKE_COMMAND} -E echo "SKIPPED: set MSLC_INDIUM_DIR to an indium checkout")
	set_tests_properties(indium-corpus PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")
endif()

add_test(NAME validate-failure-exits-nonzero COMMAND ${CMAKE_COMMAND}
	-DMSLC=$<TARGET_FILE:mslc-cli>
	-DFAKE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/failing-spirv-val
	-DPROBE=${CMAKE_CURRENT_SOURCE_DIR}/tests/probes/float_add.metal
	-DOUT=${CMAKE_CURRENT_BINARY_DIR}/validate_failure.spv
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_validate_failure.cmake)

# One test per tests/probes/*.metal; each file carries its own expectation.
file(GLOB probes CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/probes/*.metal)
foreach(probe IN LISTS probes)
	get_filename_component(name ${probe} NAME_WE)
	add_test(NAME probe/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_VAL=${SPIRV_VAL}
		-DSPIRV_DIS=${SPIRV_DIS}
		-DPROBE=${probe}
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/probes
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_probe.cmake)
endforeach()

# A source that uses macros must compile to the same module as the source with them
# expanded by hand: one test per tests/equivalence/<name>.macro.metal, against its
# <name>.expanded.metal.
file(GLOB equivalences CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/*.macro.metal)
foreach(macro IN LISTS equivalences)
	get_filename_component(name ${macro} NAME)
	string(REGEX REPLACE "\\.macro\\.metal$" "" name ${name})
	add_test(NAME equivalence/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_DIS=${SPIRV_DIS}
		-DMACRO=${macro}
		-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/${name}.expanded.metal
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/${name}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
endforeach()

# "*p" is "p[0]": one test per tests/equivalence/<name>.deref.metal, against its
# <name>.index.metal, which is the same program with every dereference written as
# an index. The two have to disassemble identically, not just both compile.
file(GLOB dereferences CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/*.deref.metal)
foreach(deref IN LISTS dereferences)
	get_filename_component(name ${deref} NAME)
	string(REGEX REPLACE "\\.deref\\.metal$" "" name ${name})
	add_test(NAME equivalence/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_DIS=${SPIRV_DIS}
		-DMACRO=${deref}
		-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/${name}.index.metal
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/${name}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
endforeach()

# "x op= y" is "x = x op y": one test per tests/equivalence/<name>.compound.metal,
# against <name>.plain.metal, which spells every compound assignment out. Where the
# two have the same operand types they have to disassemble identically, so a result
# that is computed and then not stored cannot pass.
file(GLOB compounds CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/*.compound.metal)
foreach(compound IN LISTS compounds)
	get_filename_component(name ${compound} NAME)
	string(REGEX REPLACE "\\.compound\\.metal$" "" name ${name})
	add_test(NAME equivalence/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_DIS=${SPIRV_DIS}
		-DMACRO=${compound}
		-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/${name}.plain.metal
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/${name}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
endforeach()

# An implicit arithmetic conversion is the explicit one: one test per
# tests/equivalence/<name>.implicit.metal, against <name>.explicit.metal, which
# spells the conversions out. "x + 1.5f" must disassemble as "float(x) + 1.5f".
file(GLOB conversions CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/*.implicit.metal)
foreach(conversion IN LISTS conversions)
	get_filename_component(name ${conversion} NAME)
	string(REGEX REPLACE "\\.implicit\\.metal$" "" name ${name})
	add_test(NAME equivalence/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_DIS=${SPIRV_DIS}
		-DMACRO=${conversion}
		-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/${name}.explicit.metal
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/${name}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
endforeach()

# A qualifier written after the type name is the one written before it: one test per
# tests/equivalence/<name>.trailing.metal, against <name>.leading.metal.
file(GLOB trailings CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/*.trailing.metal)
foreach(trailing IN LISTS trailings)
	get_filename_component(name ${trailing} NAME)
	string(REGEX REPLACE "\\.trailing\\.metal$" "" name ${name})
	add_test(NAME equivalence/${name} COMMAND ${CMAKE_COMMAND}
		-DMSLC=$<TARGET_FILE:mslc-cli>
		-DSPIRV_DIS=${SPIRV_DIS}
		-DMACRO=${trailing}
		-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/${name}.leading.metal
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/${name}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
endforeach()

# Negative control: a pair that does differ must fail, or the comparison above
# could not tell a macro that expands wrongly from one that expands right.
add_test(NAME equivalence-negative-control/differs COMMAND ${CMAKE_COMMAND}
	-DMSLC=$<TARGET_FILE:mslc-cli>
	-DSPIRV_DIS=${SPIRV_DIS}
	-DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/negative/differs.macro.metal
	-DEXPANDED=${CMAKE_CURRENT_SOURCE_DIR}/tests/equivalence/negative/differs.expanded.metal
	-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/equivalence/negative
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_equivalence.cmake)
set_tests_properties(equivalence-negative-control/differs PROPERTIES
	PASS_REGULAR_EXPRESSION "sources differ")

# Harness self-test: a compiler that prints the expected diagnostic and then
# crashes must fail an "error" probe, not pass it.
add_test(NAME harness/crash_after_diag COMMAND ${CMAKE_COMMAND}
	-DMSLC=${CMAKE_CURRENT_SOURCE_DIR}/tests/harness/crash_after_diag.sh
	-DPROBE=${CMAKE_CURRENT_SOURCE_DIR}/tests/harness/crash_after_diag.metal
	-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/harness
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_probe.cmake)
set_tests_properties(harness/crash_after_diag PROPERTIES
	PASS_REGULAR_EXPRESSION "crash_after_diag: mslc crashed")

# The CLI's own paths: argument handling, --dump-tokens, and the exit codes. A
# probe drives the library, so none of this runs otherwise, and --dump-tokens is
# what a review reaches for when it has a lexing question.
add_test(NAME cli/arguments COMMAND ${CMAKE_COMMAND}
	-DMSLC=$<TARGET_FILE:mslc-cli>
	-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli.cmake)
