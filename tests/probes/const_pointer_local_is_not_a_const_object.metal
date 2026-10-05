// EXPECT: error a local of type const uint* is not lowered yet
//
// A pointer to const is not itself const, so the missing initialiser is not the answer here; the pointer local is.
#include <metal_stdlib>
using namespace metal;
kernel void const_pointer_local_is_not_a_const_object(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const device uint* p; out[i] = i; }
