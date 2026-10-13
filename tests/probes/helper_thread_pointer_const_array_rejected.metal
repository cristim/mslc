// EXPECT: error is const, and parameter "p" is a pointer that can change it
//
// Apple finds no function for f(a) when a const array is given to a non-const pointer.
#include <metal_stdlib>
using namespace metal;
float f(thread float *p) { return p[0]; }
kernel void helper_thread_pointer_const_array_rejected(device float *out [[buffer(0)]])
{ const float a[2] = { 1.0, 2.0 }; out[0] = f(a); }
