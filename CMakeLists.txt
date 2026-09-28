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

# The corpus: other people's shaders, each with what it is expected to do. A
# fixture mslc supports has to keep compiling, and one it does not support yet
# has to keep saying which construct is in the way, so the test fails for a
# regression in either direction rather than asserting that everything compiles.
add_test(NAME corpus
	COMMAND mslc-corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus.txt)

# One fixture per construct, all of them expected to compile. This is the
# regression guard on mslc's own work rather than on the corpus.
add_test(NAME constructs
	COMMAND mslc-corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/constructs.txt)
