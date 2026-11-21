// EXPECT: error get_width takes no arguments
// Apple refuses this texture_buffer call too (no matching get_width).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = r.get_width(0); }
