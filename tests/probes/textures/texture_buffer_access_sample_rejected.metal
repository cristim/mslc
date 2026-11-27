// EXPECT: error a texture_buffer has no sampler
// Apple: static_assert invalid access value for texture_buffer.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::sample> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = 0; }
