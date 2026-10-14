// EXPECT: error is not lowered yet
// A threadgroup array of pointers, or of a texture, is still refused. What
// lowers is an array of scalars, vectors, matrices and structs in the
// function's own storage or in threadgroup storage; the resource and pointer
// cases are unchanged by that and keep their own diagnostic.
#include <metal_stdlib>
using namespace metal;

kernel void threadgroup_pointer_array_is_refused(device float *d [[buffer(0)]])
{
  threadgroup float *s[4];
  d[0] = 1.0f;
}