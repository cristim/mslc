// EXPECT: error is an array, so ".x" needs an element
//
// Apple rejects "a.x" for an array a; "a->x" (element zero) is accepted.
#include <metal_stdlib>
using namespace metal;
struct S { float x; };
kernel void helper_thread_pointer_dot_on_array_rejected(device float *out [[buffer(0)]])
{ S a[2]; a.x = 1.0; out[0] = a[0].x; }
