// EXPECT: error not an integer constant expression
//
// Apple folds a file-scope constant int into an enumerator. mslc folds enumerators and literals only.
#include <metal_stdlib>
using namespace metal;
constant int kBase = 2;
enum One { A = kBase };
kernel void enum_value_not_constant_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
