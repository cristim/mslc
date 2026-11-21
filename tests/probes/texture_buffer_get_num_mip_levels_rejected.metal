// EXPECT: error has read, write and get_width only
// Apple refuses this texture_buffer call too (no member named get_num_mip_levels).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = r.get_num_mip_levels(); }
