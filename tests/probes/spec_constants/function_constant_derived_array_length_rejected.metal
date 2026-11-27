// EXPECT: error is not an integer constant this source declares before this one
// Its value is known only when the function is made, so it is not an array length.
#include <metal_stdlib>
using namespace metal;
constant int x [[function_constant(0)]];
constant float fx [[function_constant(1)]];
constant int y = x + 1;
kernel void k(device float* out [[buffer(0)]]) { float a[y]; a[0] = 1; out[0] = a[0]; }
