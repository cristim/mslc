// EXPECT: error the coordinate of read has to be a uint or a ushort
// Apple refuses this texture_buffer call too (no matching read).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = r.read(uint2(0)).x; }
