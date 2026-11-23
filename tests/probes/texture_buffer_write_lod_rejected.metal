// EXPECT: error write on a texture_buffer takes a value and one coordinate: a texture_buffer has no mip levels
// Apple refuses this texture_buffer call too (no matching write).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> r [[texture(0)]], device float* o [[buffer(0)]]) { r.write(float4(1), 0u, 0u); }
