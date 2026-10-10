#/ Runs mslc --compile-library and checks the artifact against what an empty
#/ library is, rather than against what a shader is.
#/
#/ An empty library has no entry point, so it is validated under Universal
#/ SPIR-V 1.5. It is deliberately also checked to be REJECTED under Vulkan
#/ 1.3: a Linkage object is not an executable, and a check that only ever
#/ validated it would not notice if it stopped being one.
#/
#/ Every case states what it expects, and the controls at the bottom fail
#/ against a compiler that does the wrong thing, so this test can fail.
#/
#/ Invoked by ctest as:
#/   cmake -DMSLC=... -DSPIRV_VAL=... -DSPIRV_DIS=... -DTEST_DIR=...
#/         -DOUT_DIR=... -P run_library.cmake

foreach(var MSLC TEST_DIR OUT_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_library.cmake: ${var} is not set")
	endif()
endforeach()

if(NOT SPIRV_VAL)
	message(FATAL_ERROR "run_library.cmake: spirv-val not found")
endif()
if(NOT SPIRV_DIS)
	message(FATAL_ERROR "run_library.cmake: spirv-dis not found")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

# Runs MSLC with ARGS, expecting exit code EXPECT_STATUS. Returns the output in
# out_variable. A crash (a signal) is not an expected status: RESULT_VARIABLE is
# then a string, and comparing it to a number would silently succeed.
function(run_mslc out_variable expect_status)
	execute_process(
		COMMAND "${MSLC}" ${ARGN}
		RESULT_VARIABLE status
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
	)
	set(output "${stdout}${stderr}")
	if(NOT status MATCHES "^[0-9]+$")
		message(FATAL_ERROR "mslc crashed (${status}) running: ${ARGN}\n${output}")
	endif()
	if(NOT status EQUAL expect_status)
		message(FATAL_ERROR "expected exit ${expect_status}, got ${status}, running: ${ARGN}\n${output}")
	endif()
	set(${out_variable} "${output}" PARENT_SCOPE)
endfunction()

function(validate_file path target_env expect_ok label)
	execute_process(
		COMMAND "${SPIRV_VAL}" --target-env "${target_env}" "${path}"
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	if(expect_ok)
		if(NOT status EQUAL 0)
			message(FATAL_ERROR "${label}: ${target_env} rejected the module:\n${output}")
		endif()
	else()
		if(status EQUAL 0)
			message(FATAL_ERROR "${label}: ${target_env} accepted the module, but it should not:\n${output}")
		endif()
	endif()
endfunction()

set(empty "${TEST_DIR}/library/empty.metal")
if(NOT EXISTS "${empty}")
	message(FATAL_ERROR "run_library.cmake: no ${empty}")
endif()

# A real kernel, used to prove that a library links with one and that default
# mode still produces Vulkan executables.
set(shader "${OUT_DIR}/library_kernel.spv")
file(WRITE "${OUT_DIR}/library_kernel.metal"
"#include <metal_stdlib>\nusing namespace metal;\nkernel void addone(device int *out [[buffer(0)]], uint i [[thread_position_in_grid]]) {\n  out[i] = 41 + 1;\n}\n")

# 1. The empty translation unit compiles to a library.
set(lib "${OUT_DIR}/empty.spv")
file(REMOVE "${lib}")
run_mslc(output 0 --compile-library -V -o "${lib}" "${empty}")
if(NOT EXISTS "${lib}")
	message(FATAL_ERROR "--compile-library reported success but wrote no ${lib}:\n${output}")
endif()

# 2. It is a Universal SPIR-V 1.5 Linkage object.
validate_file("${lib}" spv1.5 TRUE "--compile-library on the empty translation unit")
validate_file("${lib}" vulkan1.3 FALSE "--compile-library on the empty translation unit")

execute_process(
	COMMAND "${SPIRV_DIS}" "${lib}"
	RESULT_VARIABLE status
	OUTPUT_VARIABLE disassembly
	ERROR_VARIABLE disassembly
)
if(NOT status EQUAL 0)
	message(FATAL_ERROR "spirv-dis failed on the library:\n${disassembly}")
endif()
foreach(needle IN ITEMS "OpCapability Linkage" "OpMemoryModel PhysicalStorageBuffer64 GLSL450")
	string(FIND "${disassembly}" "${needle}" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "the library disassembly lacks \"${needle}\":\n${disassembly}")
	endif()
endforeach()
# No entry point: that is what makes it a library rather than a shader.
string(FIND "${disassembly}" "OpEntryPoint" at)
if(NOT at EQUAL -1)
	message(FATAL_ERROR "an empty library must declare no entry point:\n${disassembly}")
endif()

# 3. Default mode still refuses the same file, with the entry-point reason.
run_mslc(output 1 -o "${OUT_DIR}/empty_executable.spv" "${empty}")
string(FIND "${output}" "declares no function" at)
if(at EQUAL -1)
	message(FATAL_ERROR "default mode should still refuse an empty translation unit for having no entry point, got:\n${output}")
endif()

# 4. Default mode still compiles a real kernel as a Vulkan executable.
run_mslc(output 0 -V -o "${shader}" "${OUT_DIR}/library_kernel.metal")
validate_file("${shader}" vulkan1.3 TRUE "default mode on a kernel")

# 5. Flags that describe an executable are refused with --compile-library.
#    Each is checked in both orders, because an implementation that decided
#    while parsing would accept the order where --compile-library is read
#    first and the other flag second.
#
#    --reflect takes a value, so "the other flag first" cannot put --compile-library
#    immediately after it: the argument order there is
#    --compile-library <reflect flag> <value>, which still exercises a parser
#    that has not yet seen --compile-library when it meets --reflect.
foreach(flag IN ITEMS "-E" "--dump-tokens")
	set(flag_args ${flag})
	run_mslc(output 2 --compile-library ${flag_args} -o "${OUT_DIR}/never.spv" "${empty}")
	string(FIND "${output}" "--compile-library does not take" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "expected a refusal naming --compile-library for ${flag_args}, got:\n${output}")
	endif()
	list(GET flag_args 0 first_flag)
	list(REMOVE_AT flag_args 0)
	run_mslc(output 2 ${first_flag} --compile-library ${flag_args} -o "${OUT_DIR}/never.spv" "${empty}")
	string(FIND "${output}" "--compile-library does not take" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "expected the same refusal for ${first_flag} first, got:\n${output}")
	endif()
endforeach()

# --reflect with a value of its own, so the reflection path is what --compile-library
# has to refuse rather than only the bare flag.
run_mslc(output 2 --compile-library --reflect "${OUT_DIR}/empty.json" -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --reflect" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected --compile-library to refuse --reflect, got:\n${output}")
endif()
run_mslc(output 2 --reflect "${OUT_DIR}/empty.json" --compile-library -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --reflect" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected the same refusal with --compile-library last, got:\n${output}")
endif()

# --stage and --local-size take values, so a run with --compile-library
# immediately after them cannot be built: the flag would be consumed as the
# stage name or as a size. They are checked with --compile-library first, and
# again after the value, which is the order a parser sees the flag second.
run_mslc(output 2 --compile-library --stage kernel -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --stage" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected --compile-library to refuse --stage, got:\n${output}")
endif()
run_mslc(output 2 --stage kernel --compile-library -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --stage" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected the same refusal with --compile-library last, got:\n${output}")
endif()
# The stage flag before --compile-library still gets its value consumed, so the
# refusal is reported rather than "--compile-library" being read as the stage.
run_mslc(output 2 --stage --compile-library -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "unknown stage" at)
if(at EQUAL -1)
	message(FATAL_ERROR "a value-taking flag should consume the next argument, got:\n${output}")
endif()
run_mslc(output 2 --compile-library --local-size 2 2 2 -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --local-size" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected --compile-library to refuse --local-size, got:\n${output}")
endif()
run_mslc(output 2 --local-size 2 2 2 --compile-library -o "${OUT_DIR}/never.spv" "${empty}")
string(FIND "${output}" "--compile-library does not take --local-size" at)
if(at EQUAL -1)
	message(FATAL_ERROR "expected the same refusal with --compile-library last, got:\n${output}")
endif()

# 7. Controls: a library is not a shader, and unsupported nonempty input fails
#    without producing output.
set(nonempty "${OUT_DIR}/nonempty.metal")
file(WRITE "${nonempty}" "#include <metal_stdlib>\nusing namespace metal;\nconstant float k = 1.0;\n")
file(REMOVE "${OUT_DIR}/nonempty.spv")
run_mslc(output 1 --compile-library -o "${OUT_DIR}/nonempty.spv" "${nonempty}")
if(EXISTS "${OUT_DIR}/nonempty.spv")
	message(FATAL_ERROR "--compile-library accepted a nonempty translation unit, or wrote output for a rejected one")
endif()

set(realshader "${OUT_DIR}/library_kernel.metal")
file(REMOVE "${OUT_DIR}/real.spv")
run_mslc(output 1 --compile-library -o "${OUT_DIR}/real.spv" "${realshader}")
if(EXISTS "${OUT_DIR}/real.spv")
	message(FATAL_ERROR "--compile-library accepted a real shader, or wrote output for a rejected one")
endif()

# 8. A library links with a compiler-produced shader, and the result is a valid
#    Vulkan executable. This is the property the capability exists for.
find_program(SPIRV_LINK spirv-link)
set(linked "${OUT_DIR}/linked.spv")
file(REMOVE "${linked}")
execute_process(
	COMMAND "${SPIRV_LINK}" "${lib}" "${shader}" -o "${linked}"
	RESULT_VARIABLE status
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output
)
if(NOT status EQUAL 0)
	message(FATAL_ERROR "spirv-link could not link the library with the shader:\n${output}")
endif()
validate_file("${linked}" vulkan1.3 TRUE "the linked library and shader")
execute_process(COMMAND "${SPIRV_DIS}" "${linked}" OUTPUT_VARIABLE disassembly ERROR_VARIABLE disassembly)
	string(FIND "${disassembly}" "\"addone\"" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "the linked module lost the shader's entry point:\n${disassembly}")
	endif()
# spirv-link ships in the same SPIRV-Tools package as spirv-val and spirv-dis,
# which are already required above. A missing spirv-link therefore means a broken
# toolchain rather than an environment without it, and skipping here would let
# the summary below claim a link that was never attempted.
if(NOT SPIRV_LINK)
	message(FATAL_ERROR "run_library.cmake: spirv-link not found; the library-links-with-a-shader "
		"check is the point of this mode and cannot be skipped")
endif()

set(linked "${OUT_DIR}/linked.spv")
file(REMOVE "${linked}")
execute_process(
	COMMAND "${SPIRV_LINK}" "${lib}" "${shader}" -o "${linked}"
	RESULT_VARIABLE status
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output
)
if(NOT status EQUAL 0)
	message(FATAL_ERROR "spirv-link could not link the library with the shader:\n${output}")
endif()
validate_file("${linked}" vulkan1.3 TRUE "the linked library and shader")
execute_process(COMMAND "${SPIRV_DIS}" "${linked}" OUTPUT_VARIABLE disassembly ERROR_VARIABLE disassembly)
string(FIND "${disassembly}" "\"addone\"" at)
if(at EQUAL -1)
	message(FATAL_ERROR "the linked module lost the shader's entry point:\n${disassembly}")
endif()

# 9. Each mode passes the target environment it claims.
#
#    Comparing validator outcomes cannot do this. For a module mslc produces,
#    spv1.5 and vulkan1.3 accept exactly the same executables: every mslc module
#    needs SPIR-V 1.5 (PhysicalStorageBufferAddresses) and carries nothing that
#    only Vulkan allows, so a module accepted by one is accepted by the other.
#    Forcing the executable branch to spv1.5 therefore passes every outcome
#    check in this file, which is why it was pinned by observing the arguments
#    instead.
#
#    A recording shim earlier on PATH captures the --target-env the CLI chose.
#    The shim always succeeds, so this asserts what was requested rather than
#    what any validator thinks; the real checks above use the real spirv-val.
set(shim_dir "${OUT_DIR}/shim")
file(MAKE_DIRECTORY "${shim_dir}")
file(WRITE "${shim_dir}/spirv-val"
"#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"${shim_dir}/argv.txt\"\nexit 0\n")
execute_process(COMMAND chmod +x "${shim_dir}/spirv-val")

function(record_target_env expected_mode)
	file(REMOVE "${shim_dir}/argv.txt")
	# PATH is prepended so the shim is the spirv-val the CLI spawns.
	set(env "PATH=${shim_dir}:$ENV{PATH}")
	if(expected_mode STREQUAL "library")
		# A library only accepts an empty translation unit, so the two modes are
		# driven by different inputs as well as different flags.
		set(args --compile-library)
		set(input "${empty}")
	else()
		set(args "")
		set(input "${OUT_DIR}/library_kernel.metal")
	endif()
	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env "${env}" "${MSLC}" ${args} -V
			-o "${OUT_DIR}/shim_out.spv" "${input}"
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	if(NOT status EQUAL 0)
		message(FATAL_ERROR "the shimmed -V run failed for ${expected_mode} mode:\n${output}")
	endif()
	if(NOT EXISTS "${shim_dir}/argv.txt")
		message(FATAL_ERROR "the shimmed -V run never invoked spirv-val:\n${output}")
	endif()
	file(READ "${shim_dir}/argv.txt" recorded)
	string(STRIP "${recorded}" recorded)
	if(expected_mode STREQUAL "library")
		set(want "spv1.5")
	else()
		set(want "vulkan1.3")
	endif()
	string(FIND "${recorded}" "--target-env ${want}" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "expected --target-env ${want} for ${expected_mode} mode, the CLI asked for: ${recorded}")
	endif()
endfunction()

record_target_env("executable")
record_target_env("library")

message(STATUS "library: empty translation unit compiles, validates as Universal SPIR-V 1.5, links with a shader")
message(STATUS "library: each mode validates under its own target environment")
