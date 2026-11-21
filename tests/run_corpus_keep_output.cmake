# mslc-corpus --keep-output retains one module per manifest entry, named by the
# entry's position in the manifest and not by which entries compiled before it,
# including entries whose source files share a basename, or differ only in case.
#
# Invoked by ctest as:
#   cmake -DCORPUS=... -DROOT=... -DSPIRV_VAL=... -DOUT_DIR=... -P run_corpus_keep_output.cmake
# where ROOT holds a/k.metal, b/k.metal, c/K.metal (all valid, all different) and
# z/k.metal (does not compile).

foreach(var CORPUS ROOT SPIRV_VAL OUT_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_corpus_keep_output.cmake: ${var} is not set")
	endif()
endforeach()

# Compiles the entries (a ;-list) into its own directory, expecting the compile
# of all of them to succeed unless EXPECT_STATUS says otherwise.
function(run label entries)
	set(dir "${OUT_DIR}/${label}")
	file(REMOVE_RECURSE "${dir}")
	file(MAKE_DIRECTORY "${dir}")
	string(REPLACE ";" "\n" text "${entries}")
	file(WRITE "${dir}.txt" "${text}\n")
	execute_process(
		COMMAND "${CORPUS}" "${dir}.txt" --root "${ROOT}" --spirv-val "${SPIRV_VAL}" --keep-output "${dir}"
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	set(${label}_status "${status}" PARENT_SCOPE)
	set(${label}_output "${output}" PARENT_SCOPE)
endfunction()

# The hash of the module a lone entry compiles to, which is the ground truth for
# what each retained file must contain.
function(hash_of label entry)
	run(${label} "${entry}")
	if(NOT ${label}_status EQUAL 0)
		message(FATAL_ERROR "${entry} alone failed: ${${label}_output}")
	endif()
	string(REGEX REPLACE ".*/" "" base "${entry}")
	file(SHA256 "${OUT_DIR}/${label}/${base}.spv" hash)
	set(${label}_hash "${hash}" PARENT_SCOPE)
endfunction()

function(expect_file label name want)
	set(path "${OUT_DIR}/${label}/${name}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "${label}: ${name} was not retained:\n${${label}_output}")
	endif()
	file(SHA256 "${path}" got)
	if(NOT got STREQUAL "${want}")
		message(FATAL_ERROR "${label}: ${name} holds the wrong module\n${${label}_output}")
	endif()
endfunction()

function(expect_count label n)
	file(GLOB modules "${OUT_DIR}/${label}/*.spv")
	list(LENGTH modules count)
	if(NOT count EQUAL n)
		message(FATAL_ERROR "${label}: expected ${n} retained modules, found ${count}: ${modules}\n${${label}_output}")
	endif()
endfunction()

hash_of(alone_a a/k.metal)
hash_of(alone_b b/k.metal)
hash_of(alone_c c/K.metal)

# Equal basenames from different directories.
run(same "a/k.metal;b/k.metal")
if(NOT same_status EQUAL 0)
	message(FATAL_ERROR "same: exited ${same_status}:\n${same_output}")
endif()
expect_count(same 2)
expect_file(same k.metal.spv "${alone_a_hash}")
expect_file(same k.metal.2.spv "${alone_b_hash}")

# An early entry that fails to compile must not shift the others' names.
run(failing "z/k.metal;a/k.metal;b/k.metal")
if(failing_status EQUAL 0)
	message(FATAL_ERROR "failing: a manifest with an entry that does not compile exited 0")
endif()
expect_count(failing 2)
expect_file(failing k.metal.2.spv "${alone_a_hash}")
expect_file(failing k.metal.3.spv "${alone_b_hash}")

# Names that differ only in case are one file on a case-insensitive filesystem.
run(folded "c/K.metal;b/k.metal")
if(NOT folded_status EQUAL 0)
	message(FATAL_ERROR "folded: exited ${folded_status}:\n${folded_output}")
endif()
expect_count(folded 2)
expect_file(folded K.metal.spv "${alone_c_hash}")
expect_file(folded k.metal.2.spv "${alone_b_hash}")

# The table says which file each entry went to.
if(NOT same_output MATCHES "-> k.metal.2.spv")
	message(FATAL_ERROR "the summary does not name the output file of the second entry:\n${same_output}")
endif()
