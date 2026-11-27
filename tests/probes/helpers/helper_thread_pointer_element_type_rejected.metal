// EXPECT: error is an array of float, and parameter "p" points at int
//
// Apple finds no function for f(a) when the element types differ.
#include <metal_stdlib>
using namespace metal;
int f(thread int *p) { return p[0]; }
kernel void helper_thread_pointer_element_type_rejected(device float *out [[buffer(0)]])
{ float a[4] = { 1.0, 2.0, 3.0, 4.0 }; out[0] = float(f(a)); }
