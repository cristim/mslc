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
