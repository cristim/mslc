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
include_guard(GLOBAL)
set(MSLC_LIBRARY_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/src/lexer.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/ast.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema.cpp
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