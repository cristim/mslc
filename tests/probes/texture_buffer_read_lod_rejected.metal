// EXPECT: error read on a texture_buffer takes one coordinate
// Apple refuses this texture_buffer call too (no matching read).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = r.read(0u, 0).x; }
