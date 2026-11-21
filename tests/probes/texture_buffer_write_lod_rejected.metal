// EXPECT: error mip arguments are unsupported
// Apple refuses this texture_buffer call too (no matching write).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> r [[texture(0)]], device float* o [[buffer(0)]]) { r.write(float4(1), 0u, 0u); }
