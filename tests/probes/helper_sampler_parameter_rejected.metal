// EXPECT: error parameter "s" of helper function "f" is a texture or sampler
#include <metal_stdlib>
using namespace metal;
float f(sampler s) { return 0.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ constexpr sampler s; out[i] = f(s); }
