// EXPECT: valid
//
// A const typedef keeps its qualifier; it is a value that cannot be assigned, and it reads like the plain one.
#include <metal_stdlib>
using namespace metal;
typedef const float kScale;
kernel void typedef_const_alias(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ kScale s = 2.0; out[i] = s * 3.0; }
