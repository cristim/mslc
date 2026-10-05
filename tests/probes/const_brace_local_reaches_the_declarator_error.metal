// EXPECT: error after a declaration, found
//
// Apple accepts "const int j{3};". Same: the message must not claim the initialiser is missing.
#include <metal_stdlib>
using namespace metal;
kernel void const_brace_local_reaches_the_declarator_error(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const int j{3}; out[i] = i; }
