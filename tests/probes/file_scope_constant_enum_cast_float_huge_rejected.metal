// EXPECT: error does not fit the non-negative range of the enum Mode
//
// Apple folds this through LLVM's undefined-behaviour rules (not a language rule); mslc refuses it rather than fold a host-dependent value.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB };
constant Mode k = (Mode)1e30f;
kernel void file_scope_constant_enum_cast_float_huge_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)k; }
