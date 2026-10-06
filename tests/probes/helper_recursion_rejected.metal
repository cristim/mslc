// EXPECT: error recursive
// SPIR-V for Vulkan forbids recursion. Apple accepts it.
#include <metal_stdlib>
using namespace metal;
float f(float x) { if (x > 0.0f) { return f(x - 1.0f); } return 0.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2.0f); }
