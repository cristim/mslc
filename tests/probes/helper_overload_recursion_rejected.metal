// EXPECT: error recursive call
// An overload that calls itself: SPIR-V for Vulkan forbids recursion, whichever signature.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
float f(float3 x) { return f(x) + 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float3(1.0f)); }
