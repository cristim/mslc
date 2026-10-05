# Exercises the CLI's own paths, which no probe reaches: a probe drives the
# library through run_probe.cmake and never the argument handling, and
# --dump-tokens is what a review reaches for when it has a lexing question, so
# it being unexercised meant every lexing investigation started by hand.
#
# Each case is a command and what it must print or return. A case that is not
# listed here is not checked.
#
# Invoked by ctest as: cmake -DMSLC=... -DWORK_DIR=... -P run_cli.cmake

foreach(var MSLC WORK_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_cli.cmake: ${var} is not set")
	endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(input "${WORK_DIR}/input.metal")
file(WRITE "${input}"
"kernel void k(device float *out [[buffer(0)]], uint i [[thread_position_in_grid]])\n{ out[i] = 1.0; }\n")

# run(<name> <expect-status> <expect-substring> <args...>)
#
# An expected status of 0 or a positive number is an exact exit code. A
# substring of "nonzero" means any failure, which is how the cases that must not
# succeed are written.
function(run name expected)
	set(args ${ARGN})
	execute_process(
		COMMAND "${MSLC}" ${args}
		RESULT_VARIABLE status
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	set(got "${status}")
	# "nonzero" is how a case that must fail is written, so that the case does not
	# pin an exit code the CLI is free to change.
	if(expected STREQUAL "nonzero")
		if(got EQUAL 0)
			message(FATAL_ERROR "${name}: exit 0, expected a failure:\n${output}")
		endif()
	elseif(NOT got EQUAL expected)
		message(FATAL_ERROR "${name}: exit ${got}, expected ${expected}:\n${output}")
	endif()
	set(LAST_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(run_expects name needle)
	string(FIND "${LAST_OUTPUT}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "${name}: output lacks \"${needle}\":\n${LAST_OUTPUT}")
	endif()
endfunction()

# The token stream. A fixed width with a hex offset and the token kind, which is
# what makes a lexing question answerable by reading it rather than by guessing.
run(dump_tokens 0 "--dump-tokens;--output;${WORK_DIR}/out.spv;${input}")
run_expects(dump_tokens "identifier           kernel")

# No arguments at all: the usage is printed and the exit is 2, which is what a
# caller can tell from a compile failure.
run(no_arguments 2)
run_expects(no_arguments "usage: mslc")

# A flag with no value. The exit is 2 for the same reason, with the usage shown.
run(missing_output 2 "-o")
run_expects(missing_output "usage: mslc")
run(missing_reflect 2 "--reflect")
run_expects(missing_reflect "usage: mslc")

# An input that does not exist is a failure, not an empty module.
run(absent_input nonzero "${WORK_DIR}/does-not-exist.metal")

# A file-scope declaration on its own parses, so the failure is the missing entry
# point rather than a parse error, and the message names what it looked for.
set(bad "${WORK_DIR}/bad.metal")
file(WRITE "${bad}" "constant float x = 1.0;\n")
run(no_entry_point nonzero "-o;${WORK_DIR}/bad.spv;${bad}")
run_expects(no_entry_point "declares no kernel, vertex or fragment entry point")

# -V runs the validator, and a valid module passes it. That is the success side
# of the case run_validate_failure.cmake checks the failure of.
run(validate_ok 0 "-V;-o;${WORK_DIR}/validated.spv;${input}")
run_expects(validate_ok "spirv-val: PASS")

# ---------------------------------------------------------------------------
# The preprocessor: -E prints what the parser would be given, and -I adds
# directories a quoted #include is searched in.
set(pp "${WORK_DIR}/pp")
file(REMOVE_RECURSE "${pp}")
file(MAKE_DIRECTORY "${pp}/inc" "${pp}/other" "${WORK_DIR}/secret")

file(WRITE "${pp}/expand.metal" [==[
#define S(x) #x
#define CAT(a, b) a##b
#define PASTE(a, b) [a ## b]
#define F(x) x F
#define ARGS(...) (__VA_ARGS__)
#define V(a, ...) f(a, ## __VA_ARGS__)
S( a  +  b ) S("q\"r\\s") S('\n') CAT(x, y) F(1)(2) ARGS(1, 2) V(1) V(1, 2) V(1,)
PASTE(, y) PASTE(x, ) PASTE(, )
__COUNTER__ __COUNTER__ __INCLUDE_LEVEL__ __LINE__
]==])
run(expand 0 "-E;${pp}/expand.metal")
run_expects(expand [=["a + b" "\"q\\\"r\\\\s\"" "'\\n'" xy 1 F(2) (1, 2) f(1) f(1, 2) f(1,)]=])
run_expects(expand "[y] [x] []")
run_expects(expand "0 1 0 9")

# __FILE__ and __BASE_FILE__ are the path the source was given.
file(WRITE "${pp}/file.metal" "__FILE__ __BASE_FILE__\n")
run(file_macro 0 "-E;${pp}/file.metal")
run_expects(file_macro "pp/file.metal\" \"")

# -I, in both spellings, and the including file's directory comes before it.
file(WRITE "${pp}/inc/h.h" "from_inc\n")
file(WRITE "${pp}/other/h.h" "from_other\n")
file(WRITE "${pp}/uses_h.metal" "#include \"h.h\"\n")
run(include_dir_separate 0 "-E;-I;${pp}/inc;${pp}/uses_h.metal")
run_expects(include_dir_separate "from_inc")
run(include_dir_attached 0 "-E;-I${pp}/other;${pp}/uses_h.metal")
run_expects(include_dir_attached "from_other")
run(include_dirs_in_order 0 "-E;-I;${pp}/inc;-I;${pp}/other;${pp}/uses_h.metal")
run_expects(include_dirs_in_order "from_inc")
file(WRITE "${pp}/h.h" "from_beside\n")
run(including_file_first 0 "-E;-I;${pp}/inc;${pp}/uses_h.metal")
run_expects(including_file_first "from_beside")
file(REMOVE "${pp}/h.h")

# Without -I the same include is not found, and a directory that does not exist is
# an error rather than a silent no-op.
run(include_not_found nonzero "-E;${pp}/uses_h.metal")
run_expects(include_not_found "#include \"h.h\" not found")
run(include_dir_missing nonzero "-E;-I;${pp}/no-such-dir;${pp}/uses_h.metal")
run_expects(include_dir_missing "does not exist")

# A symlink inside an include directory that leads outside every allowed root (here
# WORK_DIR/secret, beside the source's directory and not in it) is the way past a
# lexical check on "..".
file(WRITE "${WORK_DIR}/secret/s.h" "secret\n")
file(CREATE_LINK "${WORK_DIR}/secret" "${pp}/inc/link" SYMBOLIC)
file(WRITE "${pp}/via_link.metal" "#include \"link/s.h\"\n")
run(include_through_symlink_out nonzero "-E;-I;${pp}/inc;${pp}/via_link.metal")
run_expects(include_through_symlink_out "resolves outside")

# An error names the line the source gave, though a continuation joined two of
# them, and the column of the directive's name.
file(WRITE "${pp}/line.metal" "#define A 1 \\\n  + 2\n#bogus\n")
run(error_line_after_continuation nonzero "-E;${pp}/line.metal")
run_expects(error_line_after_continuation "line.metal:3:2: unknown preprocessor directive")

# The compile path preprocesses too, so a macro-built kernel compiles.
file(WRITE "${pp}/kernel.metal"
"#define BODY out[i] = VALUE;\n#define VALUE 7u\nkernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])\n{ BODY }\n")
run(compile_with_macros 0 "-V;-o;${pp}/kernel.spv;${pp}/kernel.metal")
run_expects(compile_with_macros "spirv-val: PASS")

# __FILE__ is the path of the file the token is in, and #line can rename it.
file(WRITE "${pp}/inc/f.h" "__FILE__\n")
file(WRITE "${pp}/file_in_header.metal" "#include \"f.h\"\n#line 10 \"renamed.metal\"\n__FILE__ __LINE__\n")
run(file_macro_in_header 0 "-E;-I;${pp}/inc;${pp}/file_in_header.metal")
run_expects(file_macro_in_header "inc/f.h\"")
run_expects(file_macro_in_header "\"renamed.metal\" 10")

# The classic rescan example: f(2)(9) is 2*9*g, because the g that f's expansion
# produces stays painted when the (9) after it arrives.
file(WRITE "${pp}/rescan.metal" "#define f(a) a*g\n#define g(a) f(a)\nf(2)(9)\n")
run(rescan 0 "-E;${pp}/rescan.metal")
run_expects(rescan "2*9*g")

# A header over the size limit is refused rather than read into memory.
string(REPEAT "0123456789abcdef" 2200000 big)
file(WRITE "${pp}/big.h" "${big}")
file(WRITE "${pp}/uses_big.metal" "#include \"big.h\"\n")
run(include_too_large nonzero "-E;${pp}/uses_big.metal")
run_expects(include_too_large "is larger than")
file(REMOVE "${pp}/big.h")
