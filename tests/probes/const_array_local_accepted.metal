// EXPECT: valid
//
// Apple accepts "const float x[3] = {...};": a const local array with an initialiser list.
#include <metal_stdlib>
using namespace metal;
kernel void const_array_local_accepted(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const float x[3] = {1.0, 2.0, 3.0}; out[i] = uint(x[1]); }
