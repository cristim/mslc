// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
float f(float);
float f(float) { return 1.0f; }
float g(void) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f) + g(); }
