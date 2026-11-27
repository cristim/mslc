# The library's sources and the include directories they need. Builds that
# compile mslc without this repository's CMakeLists.txt (Darling's
# Metal.framework does) include() this file rather than copying the list, so it
# always matches the checked-out commit.
#
# The include directories are exported alongside the sources because they are
# part of the same hand-maintained copy this file exists to replace: src/mslc.cpp
# includes "mslc/mslc.h" from include/ and the private headers from src/, so a
# build given only MSLC_LIBRARY_SOURCES fails to compile without them.
#
# The variables are set in the including scope, which is the directory that
# include()s this file: CMake's include() does not create a scope, so an
# embedder that include()s at the top level of its own CMakeLists.txt reads them
# directly. PARENT_SCOPE would be wrong here, since there is no parent at top
# level.
#
# include_guard() is the directory-scoped form rather than GLOBAL. GLOBAL would
# let the first of two sibling directories that include this file take the lists
# and leave the second with none, since the guard is not per directory.
include_guard()
set(MSLC_LIBRARY_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/src/lexer.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor/directives.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor/expand.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor/expr.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor/files.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/ast.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/core.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/declarations.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/enums_constants.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/expr_ops.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/expr_primary.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/functions.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/lookup.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/names.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/scope.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/stmt_control.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/stmt_decl.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/structs.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/templates_class.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/templates_decl.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/templates_inst.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser/types.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/layout.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/atomics_calls.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/common.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/construct_init.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/convert.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/fold_matrix.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/fold_scalar.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/globals_expr.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/helpers_call.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/helpers_emit_math.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/helpers_validate.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/index_element.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/local_decls.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/member_swizzle.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/module.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/operators.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/overloads.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/parameters.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/resources_samplers.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/sizeof_places.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/spec_constants.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/stage_entry.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/stage_in_out.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/statements.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/templates.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/textures_core.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/textures_sample_read.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/types_core.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema/types_structs.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/spirv.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/mslc.cpp
)
# The public headers are what an embedder's own sources include, so they are
# PUBLIC; src/ holds the library's private headers and is not.
set(MSLC_PUBLIC_INCLUDE_DIRS
	${CMAKE_CURRENT_LIST_DIR}/include
)
set(MSLC_PRIVATE_INCLUDE_DIRS
	${CMAKE_CURRENT_LIST_DIR}/src
)