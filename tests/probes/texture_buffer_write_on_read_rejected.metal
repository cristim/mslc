// EXPECT: error not lowered yet on textures that are not access::write
// Apple refuses this texture_buffer call too (no member named write).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { r.write(float4(1), 0u); }
