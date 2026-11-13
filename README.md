# mslc

[![CI](https://github.com/cristim/mslc/actions/workflows/ci.yml/badge.svg)](https://github.com/cristim/mslc/actions/workflows/ci.yml)

mslc compiles Metal Shading Language source to SPIR-V. It exists for runtime shader compilation in Darling's Metal-on-Vulkan stack, where an app hands Metal source to `newLibraryWithSource:` and Darling's [indium](https://github.com/darlinghq/indium) needs SPIR-V back. It's written in C++17 with a C ABI, and it's an early work in progress that handles a subset of Metal.

The subset is defined by measurement. [`tests/corpus.txt`](tests/corpus.txt) holds the Metal that Blender 4.5.14 calls through `newLibraryWithSource:`, and [`tests/indium_corpus.txt`](tests/indium_corpus.txt) lists indium's own test shaders. `mslc-corpus` reports how many of them reach SPIR-V that passes `spirv-val`.

## Build

You need CMake 3.16 or newer, a C++17 compiler and [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) (`spirv-val` and `spirv-dis`) for the tests.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The tests that validate SPIR-V fail when `spirv-val` isn't found. To include indium's corpus, pass `-DMSLC_INDIUM_DIR=<path to an indium checkout>`, and without it that test is skipped.

## Use the CLI

```sh
./build/mslc -V -o float_add.spv tests/probes/float_add.metal
```

That compiles a small compute kernel, runs `spirv-val` on the result (`-V`) and writes `float_add.spv`. Other options:

| Option | Effect |
|---|---|
| `-o`, `--output <path>` | Where to write the SPIR-V (default: next to the input) |
| `--reflect <path>` | Also write reflection JSON |
| `--stage <stage>` | `kernel`, `vertex` or `fragment`, inferred when omitted |
| `--local-size <x> <y> <z>` | Workgroup size for a kernel (default 1 1 1) |
| `-I <dir>` | Extra search directory for quoted `#include`, repeatable |
| `-E` | Print the preprocessed source and exit |
| `--dump-tokens` | Print the token stream and exit |
| `--compile-library` | Compile an empty translation unit to a Universal SPIR-V 1.5 Linkage library |

## Use the library

[`include/mslc/mslc.h`](include/mslc/mslc.h) is a plain C API: `mslc_default_options`, `mslc_translate`, `mslc_compile_library`, `mslc_validate`, `mslc_free` and `mslc_version`. The library has no C++ types in its public surface, so it can be built as a shared object for the Darling guest or linked into other tools.

## Layout

- `src/` has the lexer, preprocessor, parser, semantic analysis and SPIR-V writer.
- `tests/probes/` has one `.metal` file per behavior, each carrying its own expectation (`EXPECT: valid`, `EXPECT: error <text>`, disassembly and reflection checks).
- `tools/` has the corpus runner and `check_ids.py`, a SPIR-V id checker.

## Contributing

Read [AGENTS.md](AGENTS.md) first. Each capability gets an issue and a branch named after it, and a change has to state the corpus count before and after from a fresh build. Known problems are in [known-issues.md](known-issues.md).

## License

[MIT](LICENSE).
