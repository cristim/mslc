// EXPECT: error "f" returns float, so a return needs a value
// Apple: "non-void function 'f' should return a value".
#include <metal_stdlib>
using namespace metal;
float f(float x) { return; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
