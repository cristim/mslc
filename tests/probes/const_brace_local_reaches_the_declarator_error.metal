// EXPECT: error this expression cannot be used as a value
//
// Apple accepts "const int j{3};". Parsing now reaches value lowering.
#include <metal_stdlib>
using namespace metal;
kernel void const_brace_local_reaches_the_declarator_error(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const int j{3}; out[i] = i; }
