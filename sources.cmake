# The library's sources. Builds that compile mslc without this repository's
# CMakeLists.txt (Darling's Metal.framework does) include() this file rather than
# copying the list, so it always matches the checked-out commit.
set(MSLC_LIBRARY_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/src/lexer.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/preprocessor.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/ast.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/parser.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/sema.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/spirv.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/mslc.cpp
)
